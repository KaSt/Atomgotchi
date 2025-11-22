#include "ai.h"
#include "pwn.h"

const int MIN_STEP_DURATION_MS = 5000;   // Minimum observe time: 5 seconds
const int MAX_STEP_DURATION_MS = 30000;  // Maximum observe time: 30 seconds
const int BASE_STEP_DURATION_MS = 10000; // Base observe time: 10 seconds

const char* actionNames[] = {
  "STAY", "NEXT", "PREV", "DEAUTH", "IDLE"
};

long observeSince = 0;
long idleSince = 0;
bool observing = false;

TaskHandle_t brainTask;

class QLearningAgent {
private:
  static constexpr int NUM_STATES = 13 * 3 * 2 * 4;  // 312 states
  static constexpr int NUM_ACTIONS = 5;

  float Q[NUM_STATES][NUM_ACTIONS];

  // Hyperparameters - optimized for faster learning
  float alpha = 0.15f;    // Learning rate (slightly higher for faster learning)
  float gamma = 0.90f;    // Discount factor (slightly lower - focus on immediate rewards)
  float epsilon = 0.4f;   // Exploration rate (start higher)
  float epsilon_min = 0.05f;
  float epsilon_decay = 0.999f;  // Faster decay

  // Statistics
  uint32_t epoch = 0;
  uint32_t total_reward = 0;
  uint32_t handshakes_captured = 0;
  uint32_t pmkids_captured = 0;
  
  // Dynamic timing
  int current_step_duration = BASE_STEP_DURATION_MS;

public:
  QLearningAgent() {
    // Initialize Q-table with optimistic initialization
    // Encourages exploration of all actions early on
    for (int s = 0; s < NUM_STATES; s++) {
      for (int a = 0; a < NUM_ACTIONS; a++) {
        Q[s][a] = 0.5f;  // Optimistic init instead of random
      }
    }

    // Try to load from NVS
    loadFromNVS();
  }

  uint32_t getEpoch() {
    return epoch;
  }

  uint32_t getHandshakes() {
    return handshakes_captured;
  }

  uint32_t getPMKIDs() {
    return pmkids_captured;
  }

  float getEpsilon() {
    return epsilon;
  }

  int getStepDuration() {
    return current_step_duration;
  }

  // Epsilon-greedy action selection with temperature-based softmax option
  Action selectAction(const State& state) {
    int s = state.toIndex();

    // Exploration: epsilon-greedy
    if (random(0, 10000) < epsilon * 10000) {
      // Smart exploration: don't pick IDLE too often during exploration
      Action randomAction;
      do {
        randomAction = (Action)random(0, NUM_ACTIONS);
      } while (randomAction == IDLE_MODE && random(0, 100) < 70); // 70% chance to reroll IDLE
      
      return randomAction;
    }

    // Exploitation: choose best action
    int bestAction = 0;
    float bestValue = Q[s][0];

    for (int a = 1; a < NUM_ACTIONS; a++) {
      if (Q[s][a] > bestValue) {
        bestValue = Q[s][a];
        bestAction = a;
      }
    }

    return (Action)bestAction;
  }

  // Q-learning update
  void update(const State& state, Action action, float reward, const State& nextState) {
    int s = state.toIndex();
    int s_next = nextState.toIndex();
    int a = (int)action;

    // Find max Q value for next state
    float maxQ = Q[s_next][0];
    for (int a_next = 1; a_next < NUM_ACTIONS; a_next++) {
      if (Q[s_next][a_next] > maxQ) {
        maxQ = Q[s_next][a_next];
      }
    }

    // Q-learning update rule
    // Q(s,a) = Q(s,a) + α[r + γ max Q(s',a') - Q(s,a)]
    float oldQ = Q[s][a];
    Q[s][a] = oldQ + alpha * (reward + gamma * maxQ - oldQ);

    // Decay exploration
    if (epsilon > epsilon_min) {
      epsilon *= epsilon_decay;
    }

    epoch++;
    total_reward += (int)reward;

    // Periodic logging
    if (epoch % 10 == 0) {
      Serial.printf("🧠 Brain Stats | Epoch: %d | ε: %.3f | Avg Reward: %.2f | HS: %d | PMKID: %d\n",
                    epoch, epsilon, total_reward / 50.0f, handshakes_captured, pmkids_captured);
      total_reward = 0;
    }

    // Periodic save to NVS (every 10 epochs)
    if (epoch % 10 == 0) {
      saveToNVS();
    }
  }

  // Compute reward based on environment feedback
  float computeReward(const Environment& env, Action action) {
    float reward = 0.0f;

    // ===== MAJOR REWARDS =====
    if (env.got_handshake) {
      reward += 100.0f;  // Big reward for handshake!
      handshakes_captured++;
      Serial.println("🎯 HANDSHAKE CAPTURED! Reward: +100.0");
    }

    if (env.got_pmkid) {
      reward += 80.0f;  // PMKID is also great
      pmkids_captured++;
      Serial.println("🔑 PMKID CAPTURED! Reward: +80.0");
    }

    // ===== MEDIUM REWARDS =====
    if (env.eapol_packets > 0) {
      reward += 5.0f * env.eapol_packets;  // EAPOL traffic = potential handshake
      Serial.printf("📡 EAPOL packets: %d | Reward: +%.1f\n", env.eapol_packets, 5.0f * env.eapol_packets);
    }

    if (env.new_aps_found > 0) {
      reward += 2.0f * env.new_aps_found;  // New APs discovered
      Serial.printf("🔍 New APs: %d | Reward: +%.1f\n", env.new_aps_found, 2.0f * env.new_aps_found);
    }

    // ===== SMALL REWARDS =====
    if (env.ap_count > 0 && action == STAY_CHANNEL) {
      reward += 0.5f;  // Reward for staying when there's activity
    }

    // ===== PENALTIES =====
    if (env.ap_count == 0) {
      reward -= 2.0f;  // Empty channel = waste of time
    }

    if (env.time_on_channel > 20000 && !env.got_handshake && env.eapol_packets == 0) {
      reward -= 3.0f;  // Too long without any progress
      Serial.println("⏰ Wasting time on dead channel | Penalty: -3.0");
    }

    // Penalize excessive IDLE when there's potential
    if (action == IDLE_MODE && env.ap_count > 2) {
      reward -= 5.0f;  // Don't sleep when there are targets!
      Serial.println("😴 Sleeping when busy | Penalty: -5.0");
    }

    // ===== POWER EFFICIENCY BONUS =====
    if (action == IDLE_MODE && env.ap_count <= 1) {
      reward += 1.0f;  // Good decision to save power
      Serial.println("💤 Smart nap | Reward: +1.0");
    }

    // Reward efficient channel hopping
    if ((action == NEXT_CHANNEL || action == PREV_CHANNEL) && 
        env.ap_count == 0 && env.time_on_channel < 8000) {
      reward += 1.0f;  // Quick exit from empty channel
    }

    return reward;
  }

  // Calculate dynamic step duration based on environment and action
  int calculateStepDuration(const Environment& env, Action action) {
    int duration = BASE_STEP_DURATION_MS;

    // Longer observation if there's activity
    if (env.eapol_packets > 0) {
      duration = MAX_STEP_DURATION_MS;  // Stay longer for handshakes
    }
    else if (env.ap_count > 3) {
      duration = 20000;  // Busy channel needs more time
    }
    else if (env.ap_count > 0) {
      duration = 15000;  // Some activity
    }
    else {
      duration = MIN_STEP_DURATION_MS;  // Empty channel, move fast
    }

    // Action-specific adjustments
    if (action == IDLE_MODE) {
      duration = MIN_STEP_DURATION_MS;  // Short idle periods
    }
    else if (action == STAY_CHANNEL && env.got_handshake) {
      duration = MAX_STEP_DURATION_MS;  // Keep monitoring successful channel
    }
    else if (action == AGGRESSIVE_MODE) {
      duration = 25000;  // Give deauth time to work
    }

    current_step_duration = duration;
    return duration;
  }

  // Persistence to NVS (ESP32 non-volatile storage)
  void saveToNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("qlearn", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
      Serial.printf("❌ Failed to open NVS: %d\n", err);
      return;
    }

    // Save Q-table in chunks (NVS has size limits)
    const int CHUNK_SIZE = 1024;  // bytes
    int numChunks = (sizeof(Q) + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (int i = 0; i < numChunks; i++) {
      char key[16];
      snprintf(key, sizeof(key), "q_%d", i);

      int offset = i * CHUNK_SIZE;
      int size = min(CHUNK_SIZE, (int)sizeof(Q) - offset);

      nvs_set_blob(handle, key, ((uint8_t*)Q) + offset, size);
    }

    // Save metadata
    nvs_set_u32(handle, "epoch", epoch);
    nvs_set_u32(handle, "handshakes", handshakes_captured);
    nvs_set_u32(handle, "pmkids", pmkids_captured);
    nvs_set_blob(handle, "epsilon", &epsilon, sizeof(epsilon));

    nvs_commit(handle);
    nvs_close(handle);

    Serial.println("💾 Q-table saved to NVS");
  }

  void loadFromNVS() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("qlearn", NVS_READONLY, &handle);
    if (err != ESP_OK) {
      Serial.println("📝 No saved Q-table found, starting fresh");
      return;
    }

    // Load Q-table chunks
    const int CHUNK_SIZE = 1024;
    int numChunks = (sizeof(Q) + CHUNK_SIZE - 1) / CHUNK_SIZE;

    for (int i = 0; i < numChunks; i++) {
      char key[16];
      snprintf(key, sizeof(key), "q_%d", i);

      size_t size = CHUNK_SIZE;
      int offset = i * CHUNK_SIZE;
      nvs_get_blob(handle, key, ((uint8_t*)Q) + offset, &size);
    }

    // Load metadata
    nvs_get_u32(handle, "epoch", &epoch);
    nvs_get_u32(handle, "handshakes", &handshakes_captured);
    nvs_get_u32(handle, "pmkids", &pmkids_captured);

    size_t eps_size = sizeof(epsilon);
    nvs_get_blob(handle, "epsilon", &epsilon, &eps_size);

    nvs_close(handle);

    Serial.printf("✅ Q-table loaded | Epoch: %d | ε: %.3f | HS: %d | PMKID: %d\n", 
                  epoch, epsilon, handshakes_captured, pmkids_captured);
  }

  // Debug: print best actions for each channel
  void printPolicy() {
    Serial.println("\n=== Learned Policy ===");
    for (int ch = 0; ch < 13; ch++) {
      State s;
      s.channel = ch;
      s.ap_density = 1;  // Medium density
      s.recent_success = 0;
      s.time_bucket = 1;  // Afternoon

      int stateIdx = s.toIndex();
      Action a = selectAction(s);
      
      // Find second best action for comparison
      float bestQ = Q[stateIdx][(int)a];
      float secondBestQ = -999.0f;
      for (int i = 0; i < NUM_ACTIONS; i++) {
        if (i != (int)a && Q[stateIdx][i] > secondBestQ) {
          secondBestQ = Q[stateIdx][i];
        }
      }

      Serial.printf("Ch %2d: %s (Q=%.2f) | 2nd best: %.2f | Confidence: %.2f\n", 
                    ch + 1, actionNames[a], bestQ, secondBestQ, bestQ - secondBestQ);
    }
    Serial.println("======================\n");
  }
};

QLearningAgent agent;
State currentState;
Action currentAction;

void applyAction(Action action, Environment& env);
int wifi_get_channel();
void wifi_set_channel(int ch);

void think(void* pv) {
  while (true) {
    // 1. Observe current state
    Environment& env = getEnv();
    currentState = State::fromObservation(env);
    currentState.epoch = agent.getEpoch();

    // 2. Select action
    currentAction = agent.selectAction(currentState);

    Serial.printf("\n📊 Epoch %d | Ch %d | APs: %d | Success: %d | Time: %d | Action: %s\n",
                  currentState.epoch,
                  currentState.channel + 1,
                  currentState.ap_density,
                  currentState.recent_success,
                  currentState.time_bucket,
                  actionNames[currentAction]);

    // 3. Calculate dynamic step duration
    int stepDuration = agent.calculateStepDuration(env, currentAction);
    Serial.printf("⏱️  Observing for %d ms\n", stepDuration);

    // 4. Apply action
    applyAction(currentAction, env);

    // 5. Wait and observe
    env.reset();
    delay(stepDuration);
    env.update();

    // 6. Observe new state
    State nextState = State::fromObservation(env);

    // 7. Compute reward
    float reward = agent.computeReward(env, currentAction);
    Serial.printf("💰 Total Reward: %.2f\n", reward);

    // 8. Update Q-table
    agent.update(currentState, currentAction, reward, nextState);

    // 9. Periodic policy print
    if (agent.getEpoch() % 200 == 0) {
      agent.printPolicy();
    }
  }
}

void startBrain() {
  Serial.println("🧠 Initializing Brain...");
  //xTaskCreatePinnedToCore(think, "think", 8192, NULL, 1, NULL, 1);
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
      // Do nothing, keep monitoring
      Serial.printf("➡️  Staying on channel %d\n", currentCh);
      break;

    case NEXT_CHANNEL:
      currentCh = (currentCh % 13) + 1;
      wifi_set_channel(currentCh);
      env.reset();
      Serial.printf("⏭️  Next → Channel %d\n", currentCh);
      break;

    case PREV_CHANNEL:
      currentCh = (currentCh - 2 + 13) % 13 + 1;
      wifi_set_channel(currentCh);
      env.reset();
      Serial.printf("⏮️  Prev → Channel %d\n", currentCh);
      break;

    case AGGRESSIVE_MODE:
      Serial.println("⚡ Aggressive Mode");

      //performDeauthCycle();
      break;

    case IDLE_MODE:
      Serial.println("😴 Napping...");
      // Light sleep for power efficiency
      env.idle_time = agent.getStepDuration();
      
      // Uncomment for actual sleep (requires proper wake configuration)
      // esp_sleep_enable_timer_wakeup(env.idle_time * 1000);
      // esp_light_sleep_start();
      
      delay(env.idle_time);
      break;
  }
}