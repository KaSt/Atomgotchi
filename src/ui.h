#ifdef ARDUINO_M5STACK_CARDPUTER
  #include "M5Cardputer.h"
#endif
#include "mood.h"
#include "pwn.h"

#define SCREEN_TIMEOUT 30000

// Forward declarations for state management
void enterAPConfigMode();
void exitAPConfigMode();
uint8_t getDeviceState();

void initUi();
void initMenus();
void wakeUp();
void drawMood(String face, String phrase, bool broken = false,
                      String last_friend_name = "", signed int rssi = -1000);
void drawTopCanvas(int channel);
void drawBottomCanvas(uint8_t friends_run = 0, uint8_t friends_tot = 0, uint8_t pwned_run = 0, uint8_t pwned_tot = 0);
void drawMenu();
void updateUi(bool show_toolbars = false);

void setNinjaMode(bool _ninjaMode);
bool getNinjaMode();

bool getIsDisplayOn();
void setIsDisplayOn(bool _isDisplayOn);