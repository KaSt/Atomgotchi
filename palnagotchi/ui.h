#ifdef ARDUINO_M5STACK_CARDPUTER
  #include "M5Cardputer.h"
#endif
#include "mood.h"
#include "pwngrid.h"

// Forward declarations for state management
void enterAPConfigMode();
void exitAPConfigMode();
uint8_t getDeviceState();

void initUi();
void wakeUp();
void drawMood(String face, String phrase, bool broken = false);
void drawTopCanvas();
void drawBottomCanvas(uint8_t friends_run = 0, uint8_t friends_tot = 0,
                      String last_friend_name = "", signed int rssi = -1000);
void drawMenu();
void updateUi(bool show_toolbars = false);
