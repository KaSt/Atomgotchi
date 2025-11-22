#include "ai.h"
#include "pwn.h"
#include "config.h"

// Timing constants
constexpr int MIN_STEP_MS = 5000;
constexpr int MAX_STEP_MS = 30000;
constexpr int BASE_STEP_MS = 10000;

// Action names for logging
const char* actionNames[] = {"STAY", "NEXT", "PREV", "DEAUTH", "IDLE"};

// State tracking
long observeSince = 0;
long idleSince = 0;
bool observing = false;
TaskHandle_t brainTask;

class QLearningAgent {
private:
  static constexpr int NUM_STATES = 13 * 3 * 2 * 4;  // 312 states
  static constexpr int NUM_ACTIONS = 5;
  
  float Q[NUM_STATES][NUM_ACTIONS];
  
  // Hyperparameters
  float alpha = 0.15f;
  float gamma = 0.90f;
  float epsilon = 0.4f;
  float epsilon_min = 0.05f;
  float epsilon_decay = 0.999f;
  
  // Statistics (epoch now persisted in EEPROM)
  uint32_t epoch = 0;
  uint32_t total_reward = 0;
  uint32_t handshakes_captured = 0;
  uint32_t pmkids_captured = 0;
  int current_step_duration = BASE_STEP_MS;

public:
  QLearningAgent() {
    // Optimistic initialization
    for (int s = 0; s < NUM_STATES; s++) {
      for (int a = 0; a < NUM_ACTIONS; a++) {
        Q[s][a] = 0.5f;
      }
    }
    loadFromNVS();
  }

  // Getters
  uint32_t getEpochCount() const { return epoch; }
  uint32_t getHandshakes() const { return handshakes_captured; }
  uint32_t getPMKIDs() const { return pmkids_captured; }
  float getEpsilon() const { return epsilon; }
  int getStepDuration() const { return current_step_duration; }

  Action selectAction(const State& state) {
    int s = state.toIndex();
    
    // Epsilon-greedy exploration
    if (random(0, 10000) < epsilon * 10000) {
      Action action;
      do {
        action = (Action)random(0, NUM_ACTIONS);
      } while (action == IDLE_MODE && random(0, 100) < 70);
      return action;
    }
    
    // Exploitation: select best action
    int best = 0;
    for (int a = 1; a < NUM_ACTIONS; a++) {
      if (Q[s][a] > Q[s][best]) {
        best = a;
      }
    }
    return (Action)best;
  }

  void update(const State& state, Action action, float reward, const State& nextState) {
    int s = state.toIndex();
    int a = (int)action;
    int s_next = nextState.toIndex();
    
    // Find max Q-value for next state
    float maxQ = Q[s_next][0];
    for (int i = 1; i < NUM_ACTIONS; i++) {
      if (Q[s_next][i] > maxQ) {
        maxQ = Q[s_next][i];
      }
    }
    
    // Q-learning update: Q(s,a) ← Q(s,a) + α[r + γ·max Q(s',a') - Q(s,a)]
    Q[s][a] += alpha * (reward + gamma * maxQ - Q[s][a]);
    
    // Decay exploration
    if (epsilon > epsilon_min) {
      epsilon *= epsilon_decay;
    }
    
    epoch++;
    total_reward += (int)reward;
    
    // Periodic logging
    if (epoch % 10 == 0) {
      Serial.printf("🧠 Epoch: %u | ε: %.3f | Avg R: %.2f | HS: %u | PMKID: %u\n",
                    epoch, epsilon, total_reward / 50.0f, handshakes_captured, pmkids_captured);
      total_reward = 0;
      saveToNVS();
    }
  }

  float computeReward(const Environment& env, Action action) {
    float reward = 0.0f;
    
    // Major rewards
    if (env.got_handshake) {
      reward += 100.0f;
      handshakes_captured++;
      Serial.println("🎯 HANDSHAKE! +100");
    }
    if (env.got_pmkid) {
      reward += 80.0f;
      pmkids_captured++;
      Serial.println("🔑 PMKID! +80");
    }
    
    // Medium rewards
    if (env.eapol_packets > 0) {
      reward += 5.0f * env.eapol_packets;
      Serial.printf("📡 EAPOL: %d | +%.1f\n", env.eapol_packets, 5.0f * env.eapol_packets);
    }
    if (env.new_aps_found > 0) {
      reward += 2.0f * env.new_aps_found;
    }
    
    // Small rewards
    if (env.ap_count > 0 && action == STAY_CHANNEL) {
      reward += 0.5f;
    }
    
    // Penalties
    if (env.ap_count == 0) {
      reward -= 2.0f;
    }
    if (env.time_on_channel > 20000 && !env.got_handshake && env.eapol_packets == 0) {
      reward -= 3.0f;
    }
    if (action == IDLE_MODE && env.ap_count > 2) {
      reward -= 5.0f;
    }
    
    // Efficiency bonuses
    if (action == IDLE_MODE && env.ap_count <= 1) {
      reward += 1.0f;
    }
    if ((action == NEXT_CHANNEL || action == PREV_CHANNEL) && 
        env.ap_count == 0 && env.time_on_channel < 8000) {
      reward += 1.0f;
    }
    
    return reward;
  }

  int calculateStepDuration(const Environment& env, Action action) {
    int duration = BASE_STEP_MS;
    
    if (env.eapol_packets > 0) {
      duration = MAX_STEP_MS;
    } else if (env.ap_count > 3) {
      duration = 20000;
    } else if (env.ap_count > 0) {
      duration = 15000;
    } else {
      duration = MIN_STEP_MS;
    }
    
    // Action-specific adjustments
    if (action == IDLE_MODE) {
      duration = MIN_STEP_MS;
    } else if (action == STAY_CHANNEL && env.got_handshake) {
      duration = MAX_STEP_MS;
    } else if (action == AGGRESSIVE_MODE) {
      duration = 25000;
    }
    
    current_step_duration = duration;
    return duration;
  }

  void saveToNVS() {
    nvs_handle_t handle;
    if (nvs_open("qlearn", NVS_READWRITE, &handle) != ESP_OK) {
      Serial.println("❌ NVS open failed");
      return;
    }
    
    // Save Q-table in chunks
    constexpr int CHUNK_SIZE = 1024;
    int numChunks = (sizeof(Q) + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    for (int i = 0; i < numChunks; i++) {
      char key[16];
      snprintf(key, sizeof(key), "q_%d", i);
      int offset = i * CHUNK_SIZE;
      int size = min(CHUNK_SIZE, (int)sizeof(Q) - offset);
      nvs_set_blob(handle, key, ((uint8_t*)Q) + offset, size);
    }
    
    // Save metadata to NVS (except epoch which goes to EEPROM)
    nvs_set_u32(handle, "handshakes", handshakes_captured);
    nvs_set_u32(handle, "pmkids", pmkids_captured);
    nvs_set_blob(handle, "epsilon", &epsilon, sizeof(epsilon));
    
    nvs_commit(handle);
    nvs_close(handle);
    
    // Save epoch to EEPROM for persistence consistency
    setEpoch(epoch);
    
    Serial.println("💾 Brain saved");
  }

  void loadFromNVS() {
    nvs_handle_t handle;
    if (nvs_open("qlearn", NVS_READONLY, &handle) != ESP_OK) {
      Serial.println("📝 Fresh brain initialized");
      epoch = getEpoch();  // Load epoch from EEPROM even if Q-table is new
      return;
    }
    
    // Load Q-table chunks
    constexpr int CHUNK_SIZE = 1024;
    int numChunks = (sizeof(Q) + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    for (int i = 0; i < numChunks; i++) {
      char key[16];
      snprintf(key, sizeof(key), "q_%d", i);
      size_t size = CHUNK_SIZE;
      int offset = i * CHUNK_SIZE;
      nvs_get_blob(handle, key, ((uint8_t*)Q) + offset, &size);
    }
    
    // Load metadata
    nvs_get_u32(handle, "handshakes", &handshakes_captured);
    nvs_get_u32(handle, "pmkids", &pmkids_captured);
    size_t eps_size = sizeof(epsilon);
    nvs_get_blob(handle, "epsilon", &epsilon, &eps_size);
    
    nvs_close(handle);
    
    // Load epoch from EEPROM (authoritative source)
    epoch = getEpoch();
    
    Serial.printf("✅ Brain loaded | Epoch: %u | ε: %.3f | HS: %u | PMKID: %u\n", 
                  epoch, epsilon, handshakes_captured, pmkids_captured);
  }

  void printPolicy() {
    Serial.println("\n=== Learned Policy ===");
    for (int ch = 0; ch < 13; ch++) {
      State s;
      s.channel = ch;
      s.ap_density = 1;
      s.recent_success = 0;
      s.time_bucket = 1;
      
      int idx = s.toIndex();
      Action a = selectAction(s);
      float bestQ = Q[idx][(int)a];
      float secondBest = -999.0f;
      
      for (int i = 0; i < NUM_ACTIONS; i++) {
        if (i != (int)a && Q[idx][i] > secondBest) {
          secondBest = Q[idx][i];
        }
      }
      
      Serial.printf("Ch %2d: %s (Q=%.2f) | Δ=%.2f\n", 
                    ch + 1, actionNames[a], bestQ, bestQ - secondBest);
    }
    Serial.println("======================\n");
  }
};

QLearningAgent agent;
State currentState;
Action currentAction;

void applyAction(Action action, Environment& env);

void think(void* pv) {
  while (true) {
    Environment& env = getEnv();
    currentState = State::fromObservation(env);
    currentState.epoch = agent.getEpochCount();
    
    currentAction = agent.selectAction(currentState);
    
    Serial.printf("\n📊 E%u | Ch%d | APs:%d | %s\n",
                  currentState.epoch, currentState.channel + 1,
                  currentState.ap_density, actionNames[currentAction]);
    
    int stepDuration = agent.calculateStepDuration(env, currentAction);
    Serial.printf("⏱️  %dms\n", stepDuration);
    
    applyAction(currentAction, env);
    
    env.reset();
    delay(stepDuration);
    env.update();
    
    State nextState = State::fromObservation(env);
    float reward = agent.computeReward(env, currentAction);
    
    Serial.printf("💰 R=%.2f\n", reward);
    
    agent.update(currentState, currentAction, reward, nextState);
    
    if (agent.getEpochCount() % 200 == 0) {
      agent.printPolicy();
    }
  }
}

void startBrain() {
  Serial.println("🧠 Starting Brain...");
  xTaskCreate(think, "think", 8192, NULL, 1, &brainTask);
}

void stopBrain() {
  vTaskDelete(brainTask);
}

void applyAction(Action action, Environment& env) {
  int currentCh = wifi_get_channel();
  env.action = action;
  
  switch (action) {
    case STAY_CHANNEL:
      Serial.printf("➡️  Stay Ch%d\n", currentCh);
      break;
      
    case NEXT_CHANNEL:
      currentCh = (currentCh % 13) + 1;
      wifi_set_channel(currentCh);
      env.reset();
      Serial.printf("⏭️  Ch%d\n", currentCh);
      break;
      
    case PREV_CHANNEL:
      currentCh = (currentCh - 2 + 13) % 13 + 1;
      wifi_set_channel(currentCh);
      env.reset();
      Serial.printf("⏮️  Ch%d\n", currentCh);
      break;
      
    case AGGRESSIVE_MODE:
      Serial.println("⚡ Aggressive");
      // performDeauthCycle();
      break;
      
    case IDLE_MODE:
      Serial.println("😴 Nap");
      env.idle_time = agent.getStepDuration();
      delay(env.idle_time);
      break;
  }
}
