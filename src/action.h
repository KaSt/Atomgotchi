#ifndef _ACTION_H_
#define _ACTION_H_

// Actions the agent can take
enum Action {
  STAY_CHANNEL = 0,  // Stay on current channel
  NEXT_CHANNEL = 1,  // Hop to next channel
  PREV_CHANNEL = 2,  // Hop to previous channel
  AGGRESSIVE_MODE = 3,   // Send deauth packets (ethical monitoring only)
  IDLE_MODE = 4      // Sleep to save power
};

#endif