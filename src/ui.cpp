#include "ui.h"
#include "device_state.h"
#include "config.h"
#include "ap_config.h"
#include "menu_system.h"
#include "storage.h"

M5Canvas canvas_top(&M5.Display);
M5Canvas canvas_main(&M5.Display);
M5Canvas canvas_bot(&M5.Display);

int32_t display_w;
int32_t display_h;
int32_t canvas_h;
int32_t canvas_center_x;
int32_t canvas_top_h;
int32_t canvas_bot_h;
int32_t canvas_peers_menu_h;
int32_t canvas_peers_menu_w;

bool ninjaMode = false;

long displayOnSince = millis();
bool isDisplayOn = true;

// New menu system instances
MenuSystem* mainMenu = nullptr;
MenuSystem* settingsMenu = nullptr;
MenuSystem* nearbyMenu = nullptr;
MenuSystem* personalityMenu = nullptr;
MenuSystem* storageMenu = nullptr;
MenuSystem* currentMenu = nullptr;

enum MenuState {
  MENU_NONE = 0,
  MENU_MAIN = 1,
  MENU_FRIENDS = 2,
  MENU_SETTINGS = 4,
  MENU_ABOUT = 8,
  MENU_AP_CONFIG = 40,
  MENU_PERSONALITY = 42,
  MENU_NINJA_MODE = 45,
  MENU_BACK = 99
};

MenuState activeMenuState = MENU_NONE;
bool menu_open = false;

bool getIsDisplayOn() {
  return isDisplayOn;
}

void setIsDisplayOn(bool _isDisplayOn) {
  isDisplayOn = _isDisplayOn;
  if (!isDisplayOn) {
    M5.Display.sleep();
  } else {
    M5.Display.wakeup(); 
  }
}

void initUi() {
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }
  M5.Display.setTextFont(&fonts::Font0);
  M5.Display.setTextSize(4);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(GREEN);
  M5.Display.setColor(GREEN);

  display_w = M5.Display.width();
  display_h = M5.Display.height();
  canvas_h = display_h * .8;
  canvas_center_x = display_w / 2;
  canvas_top_h = display_h * .1;
  canvas_bot_h = display_h * .2;
  canvas_peers_menu_h = display_h * .8;
  canvas_peers_menu_w = display_w * .8;

  canvas_top.createSprite(display_w, canvas_top_h);
  canvas_bot.createSprite(display_w, canvas_bot_h);
  canvas_main.createSprite(display_w, canvas_h);
  
  // Initialize menus
  initMenus();
}

bool keyboard_changed = false;

// Forward declarations for menu actions
void closeMenu();
void openMainMenu();
void openFriendsMenu();
void openSettingsMenu();
void openAboutMenu();
void openPersonalityMenu();
void openAPConfigMenu();
void openStorageMenu();
void toggleNinjaMode();

// Forward declarations for button functions
bool toggleMenuBtnPressed();
bool isNextPressed();
String getRssiBars(signed int rssi);
void setPersonalityFriendly();
void setPersonalityAI();
void formatSDCard();
void switchToSDStorage();
void switchToInternalStorage();
void showStorageInfo();

// Initialize all menus
void initMenus() {
  mainMenu = new MenuSystem(&canvas_main);
  mainMenu->setTitle("MAIN MENU");
  mainMenu->setColors(TFT_GREEN, TFT_DARKGREEN, TFT_BLACK, TFT_YELLOW);
  
  settingsMenu = new MenuSystem(&canvas_main);
  settingsMenu->setTitle("SETTINGS");
  settingsMenu->setColors(TFT_GREEN, TFT_DARKGREEN, TFT_BLACK, TFT_YELLOW);
  
  nearbyMenu = new MenuSystem(&canvas_main);
  nearbyMenu->setTitle("NEARBY");
  nearbyMenu->setColors(TFT_GREEN, TFT_DARKGREEN, TFT_BLACK, TFT_YELLOW);
  
  personalityMenu = new MenuSystem(&canvas_main);
  personalityMenu->setTitle("PERSONALITY");
  personalityMenu->setColors(TFT_GREEN, TFT_DARKGREEN, TFT_BLACK, TFT_YELLOW);
  
  storageMenu = new MenuSystem(&canvas_main);
  storageMenu->setTitle("STORAGE");
  storageMenu->setColors(TFT_GREEN, TFT_DARKGREEN, TFT_BLACK, TFT_YELLOW);
  
  // Build main menu
  mainMenu->addItem("Friends", openFriendsMenu);
  mainMenu->addItem("Settings", openSettingsMenu);
  mainMenu->addItem("About", openAboutMenu);
  mainMenu->addBackItem(closeMenu);
  
  // Build settings menu
  settingsMenu->addItem("Config (AP)", openAPConfigMenu);
  settingsMenu->addItem("Personality", openPersonalityMenu);
  settingsMenu->addItem("Storage", openStorageMenu);
  settingsMenu->addItem("Ninja Mode", toggleNinjaMode);
  settingsMenu->addBackItem(openMainMenu);
  
  // Build personality menu
  personalityMenu->addItem("Friendly", setPersonalityFriendly);
  personalityMenu->addItem("AI", setPersonalityAI);
  personalityMenu->addBackItem(openSettingsMenu);
}

// Menu action implementations
void closeMenu() {
  menu_open = false;
  activeMenuState = MENU_NONE;
  currentMenu = nullptr;
}

void openMainMenu() {
  activeMenuState = MENU_MAIN;
  currentMenu = mainMenu;
  menu_open = true;
}

void openFriendsMenu() {
  activeMenuState = MENU_FRIENDS;
  
  // Rebuild nearby menu with current peers
  nearbyMenu->clearItems();
  
  pwngrid_peer* pwngrid_peers = getPwngridPeers();
  uint64_t len = getPwngridRunTotalPeers();
  
  if (len == 0) {
    nearbyMenu->addItem("No friends yet...", [](){});
  } else {
    for (uint8_t i = 0; i < len && i < 20; i++) {  // Limit to 20 items
      String peerLabel = String(pwngrid_peers[i].name) + " [" + getRssiBars(pwngrid_peers[i].rssi) + "]";
      nearbyMenu->addItem(peerLabel, [](){});  // No action for now
    }
  }
  
  nearbyMenu->addBackItem(openMainMenu);
  currentMenu = nearbyMenu;
}

void openSettingsMenu() {
  activeMenuState = MENU_SETTINGS;
  currentMenu = settingsMenu;
}

void openAboutMenu() {
  activeMenuState = MENU_ABOUT;
  
  // Show about screen (not a menu)
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(top_center);
  canvas_main.drawString("Palnagotchi", canvas_center_x, 10);
  
  canvas_main.setTextColor(TFT_GREEN);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(top_left);
  canvas_main.setCursor(10, 40);
  canvas_main.println("AI-powered WiFi companion");
  canvas_main.println("");
  canvas_main.println("Features:");
  canvas_main.println("- RL-based channel hopping");
  canvas_main.println("- Handshake collection");
  canvas_main.println("- Friend discovery");
  canvas_main.println("");
  canvas_main.setTextColor(TFT_DARKGREY);
  canvas_main.println("Hold button to go back");
  
  canvas_main.pushSprite(0, canvas_top_h);
  
  // Wait for button press to go back
  delay(300);  // Debounce
  while (!toggleMenuBtnPressed()) {
    M5.update();
    delay(10);
  }
  
  openMainMenu();
}

void openPersonalityMenu() {
  activeMenuState = MENU_PERSONALITY;
  currentMenu = personalityMenu;
}

void openAPConfigMenu() {
  activeMenuState = MENU_AP_CONFIG;
  closeMenu();
  enterAPConfigMode();
}

void toggleNinjaMode() {
  ninjaMode = !ninjaMode;
  setNinjaMode(ninjaMode);
  
  // Show confirmation
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString(ninjaMode ? "Ninja: ON" : "Ninja: OFF", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  delay(1000);
  
  openSettingsMenu();
}

void setPersonalityFriendly() {
  setPersonality(FRIENDLY);
  
  // Show confirmation
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_GREEN);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString("Set: Friendly", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  delay(1000);
  
  openPersonalityMenu();
}

void setPersonalityAI() {
  setPersonality(AI);
  
  // Show confirmation
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_GREEN);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString("Set: AI", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  delay(1000);
  
  openPersonalityMenu();
}

void openStorageMenu() {
  activeMenuState = MENU_SETTINGS;  // Part of settings
  
  // Rebuild storage menu based on current state
  storageMenu->clearItems();
  
  // Show current storage info
  String currentInfo = String("Current: ") + storage.getStorageTypeName();
  storageMenu->addItem(currentInfo, showStorageInfo);
  
  // SD card options
  if (storage.isSDAvailable()) {
    if (storage.isSDFormatted()) {
      if (!storage.isSDActive()) {
        storageMenu->addItem("Switch to SD", switchToSDStorage);
      }
      storageMenu->addItem("SD Card Info", showStorageInfo);
    } else {
      storageMenu->addItem("Format SD Card", formatSDCard);
    }
  } else {
    storageMenu->addItem("No SD Card", [](){});  // Disabled
  }
  
  // Internal storage option
  if (storage.isSDActive()) {
    storageMenu->addItem("Switch to Internal", switchToInternalStorage);
  }
  
  storageMenu->addBackItem(openSettingsMenu);
  currentMenu = storageMenu;
}

void showStorageInfo() {
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(top_center);
  canvas_main.drawString("STORAGE INFO", canvas_center_x, 5);
  
  canvas_main.setTextColor(TFT_GREEN);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(top_left);
  canvas_main.setCursor(10, 30);
  
  canvas_main.printf("Active: %s\n", storage.getStorageTypeName());
  canvas_main.println();
  
  if (storage.isSDAvailable()) {
    canvas_main.println("SD Card:");
    canvas_main.printf("  Size: %llu MB\n", storage.getSDCardSize());
    canvas_main.printf("  Used: %llu MB\n", storage.getSDCardUsed());
    canvas_main.printf("  Free: %llu MB\n", storage.getSDCardFree());
    canvas_main.printf("  Status: %s\n", storage.isSDFormatted() ? "OK" : "Not formatted");
  } else {
    canvas_main.println("SD Card: Not detected");
  }
  
  canvas_main.setTextColor(TFT_DARKGREY);
  canvas_main.setCursor(10, canvas_h - 15);
  canvas_main.println("Hold button to go back");
  
  canvas_main.pushSprite(0, canvas_top_h);
  
  // Wait for button
  delay(300);
  while (!toggleMenuBtnPressed()) {
    M5.update();
    delay(10);
  }
  
  openStorageMenu();
}

void formatSDCard() {
  // Confirmation dialog
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString("FORMAT SD?", canvas_center_x, canvas_h / 3);
  
  canvas_main.setTextColor(TFT_RED);
  canvas_main.setTextSize(1);
  canvas_main.drawString("All data will be lost!", canvas_center_x, canvas_h / 2);
  
  canvas_main.setTextColor(TFT_GREEN);
  canvas_main.drawString("Short press: Cancel", canvas_center_x, canvas_h - 30);
  canvas_main.drawString("Long press: Format", canvas_center_x, canvas_h - 15);
  
  canvas_main.pushSprite(0, canvas_top_h);
  
  delay(500);  // Debounce
  
  // Wait for user input
  unsigned long startWait = millis();
  bool formatConfirmed = false;
  
  while (millis() - startWait < 10000) {  // 10 second timeout
    M5.update();
    
    if (isNextPressed()) {
      // Cancel
      break;
    }
    
    if (toggleMenuBtnPressed()) {
      formatConfirmed = true;
      break;
    }
    
    delay(10);
  }
  
  if (formatConfirmed) {
    // Show formatting message
    canvas_main.fillSprite(TFT_BLACK);
    canvas_main.setTextColor(TFT_YELLOW);
    canvas_main.setTextSize(2);
    canvas_main.setTextDatum(middle_center);
    canvas_main.drawString("Formatting...", canvas_center_x, canvas_h / 2);
    canvas_main.pushSprite(0, canvas_top_h);
    
    bool success = storage.formatSD();
    
    // Show result
    canvas_main.fillSprite(TFT_BLACK);
    canvas_main.setTextColor(success ? TFT_GREEN : TFT_RED);
    canvas_main.drawString(success ? "Success!" : "Failed!", canvas_center_x, canvas_h / 2);
    canvas_main.pushSprite(0, canvas_top_h);
    delay(2000);
  }
  
  openStorageMenu();
}

void switchToSDStorage() {
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString("Switching...", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  
  bool success = storage.switchToSD();
  
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(success ? TFT_GREEN : TFT_RED);
  canvas_main.drawString(success ? "Using SD Card" : "Failed!", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  delay(2000);
  
  openStorageMenu();
}

void switchToInternalStorage() {
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(TFT_YELLOW);
  canvas_main.setTextSize(2);
  canvas_main.setTextDatum(middle_center);
  canvas_main.drawString("Switching...", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  
  bool success = storage.switchToLittleFS();
  
  canvas_main.fillSprite(TFT_BLACK);
  canvas_main.setTextColor(success ? TFT_GREEN : TFT_RED);
  canvas_main.drawString(success ? "Using Internal" : "Failed!", canvas_center_x, canvas_h / 2);
  canvas_main.pushSprite(0, canvas_top_h);
  delay(2000);
  
  openStorageMenu();
}

// Long press to toggle menu or select item
bool toggleMenuBtnPressed() {
#if defined(ARDUINO_M5STACK_CARDPUTER)
  return M5Cardputer.BtnA.isPressed() || (keyboard_changed && (M5Cardputer.Keyboard.isKeyPressed('m') || M5Cardputer.Keyboard.isKeyPressed('`')));
#elif defined(ARDUINO_M5STACK_STICKC) || defined(ARDUINO_M5STACK_STICKC_PLUS) || defined(ARDUINO_M5STACK_STICKC_PLUS2)
  return M5.BtnA.wasHold();
#else
  return M5.BtnA.wasHold();
#endif
}

// Short press/click to navigate (was "OK")
bool isNextPressed() {
#if defined(ARDUINO_M5STACK_CARDPUTER)
  return keyboard_changed && (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/') || M5Cardputer.Keyboard.isKeyPressed(KEY_TAB) || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER));
#elif defined(ARDUINO_M5STACK_STICKC) || defined(ARDUINO_M5STACK_STICKC_PLUS) || defined(ARDUINO_M5STACK_STICKC_PLUS2)
  return M5.BtnA.wasClicked();
#else
  // For AtomS3: single click/press to navigate
  return M5.BtnA.wasClicked();
#endif
}

// Not used in single-button mode, but kept for compatibility
bool isPrevPressed() {
#if defined(ARDUINO_M5STACK_CARDPUTER)
  return keyboard_changed && (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed(';'));
#elif defined(ARDUINO_M5STACK_STICKC) || defined(ARDUINO_M5STACK_STICKC_PLUS) || defined(ARDUINO_M5STACK_STICKC_PLUS2)
  return M5.BtnB.wasClicked();
#else
  return false;  // Not used on AtomS3
#endif
}

void updateUi(bool show_toolbars) {
  ////Serial.println("UI - updateUi.");

  if ((M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnA.wasHold() || M5.BtnB.wasHold()) && !isDisplayOn) {
    setIsDisplayOn(true);
    displayOnSince = millis();
  }

  if (millis() - displayOnSince > SCREEN_TIMEOUT && isDisplayOn) {
    setIsDisplayOn(false);
  }

  if (!isDisplayOn) {
    return;
  }

#ifdef ARDUINO_M5STACK_CARDPUTER
  keyboard_changed = M5Cardputer.Keyboard.isChange();
#else
  keyboard_changed = false;
#endif

  // Long press toggles menu or selects item
  if (toggleMenuBtnPressed()) {
    // If in AP config mode, exit it
    if (isAPModeActive()) {
      exitAPConfigMode();
      menu_open = false;
      return;
    }

    // If menu is NOT open, open main menu
    if (!menu_open) {
      openMainMenu();
    }
    // If menu IS open, select current option
    else if (currentMenu != nullptr) {
      currentMenu->select();
    }
    
    displayOnSince = millis();  // Reset display timeout
  }

  // Short press cycles through options
  if (isNextPressed() && menu_open && currentMenu != nullptr) {
    currentMenu->navigateDown();
    displayOnSince = millis();
  }
  
  // Previous button (if available)
  if (isPrevPressed() && menu_open && currentMenu != nullptr) {
    currentMenu->navigateUp();
    displayOnSince = millis();
  }

  uint8_t mood_id = getCurrentMoodId();
  String mood_face = getCurrentMoodFace();
  String mood_phrase = getCurrentMoodPhrase();
  bool mood_broken = isCurrentMoodBroken();

  //Serial.println("UI - Draw canvas.");

  drawTopCanvas(wifi_get_channel());
  drawBottomCanvas(getPwngridRunTotalPeers(), getPwngridTotalPeers(), getPwngridRunPwned(), getPwngridTotalPwned());

  //Serial.println("UI - Menu or Mood.");

  if (menu_open || isAPModeActive()) {
    drawMenu();
  } else {
    drawMood(mood_face, mood_phrase, mood_broken, getPwngridLastFriendName(), getPwngridClosestRssi());
  }

  //Serial.println("UI - startWrite.");

  M5.Display.startWrite();
  if (show_toolbars) {
    canvas_top.pushSprite(0, 0);
    canvas_bot.pushSprite(0, canvas_top_h + canvas_h);
  }
  canvas_main.pushSprite(0, canvas_top_h);
  M5.Display.endWrite();

  //Serial.println("UI - Ended updateUi.");
}

void drawTopCanvas(int channel) {

  if (!isDisplayOn) {
    return;
  }

  canvas_top.fillSprite(BLACK);
  canvas_top.setTextSize(1);
  canvas_top.setTextColor(GREEN);
  canvas_top.setColor(GREEN);
  canvas_top.setTextDatum(top_left);

  char top_text[25] = "CH * [F]";
  String channel_text = String(channel).length() < 2 ? String("0") + String(channel) : String(channel);
  snprintf(top_text, sizeof(top_text), "CH %s [%s]", channel_text, (getPersonalityText()).c_str());
  canvas_top.drawString(top_text, 0, 3);
  canvas_top.setTextDatum(top_right);
  unsigned long ellapsed = millis() / 1000;
  int8_t h = ellapsed / 3600;
  int sr = ellapsed % 3600;
  int8_t m = sr / 60;
  int8_t s = sr % 60;
  if (display_w > 128) {
    char right_str[50] = "UPS 0%  UP 00:00:00";
    snprintf(right_str, sizeof(right_str), "UPS %i%% UP %02d:%02d:%02d",
             M5.Power.getBatteryLevel(), h, m, s);
    canvas_top.drawString(right_str, display_w, 3);
  } else {
    char right_str[50] = "UP 00:00:00";
    snprintf(right_str, sizeof(right_str), "UP %02d:%02d:%02d", h, m, s);
    canvas_top.drawString(right_str, display_w, 3);
  }
  canvas_top.drawLine(0, canvas_top_h - 1, display_w, canvas_top_h - 1);
}

String getRssiBars(signed int rssi) {
  String rssi_bars = "";

  if (rssi != -1000) {
    if (rssi >= -67) {
      rssi_bars = "||||";
    } else if (rssi >= -70) {
      rssi_bars = "|||";
    } else if (rssi >= -80) {
      rssi_bars = "||";
    } else {
      rssi_bars = "|";
    }
  }

  return rssi_bars;
}

void drawBottomCanvas(uint8_t friends_run, uint8_t friends_tot, uint8_t pwned_run, uint8_t pwned_tot) {

  if (!isDisplayOn) {
    return;
  }

  canvas_bot.fillSprite(BLACK);
  canvas_bot.setTextSize(1);
  canvas_bot.setTextColor(GREEN);
  canvas_bot.setColor(GREEN);
  canvas_bot.setTextDatum(top_left);

  if (getPersonality() == AI) {
    char sniffer_stats[25] = "F 0 (0) P 0 (0)";
    if (friends_run > 0 || pwned_run > 0 || pwned_tot > 0) {
      snprintf(sniffer_stats, sizeof(sniffer_stats), "F %d (%d) P %d (%d)",
               friends_run, friends_tot, pwned_run, pwned_tot);
    }
    canvas_bot.drawString(sniffer_stats, 0, 5);
  } else {
    char friendly_stats[25] = "FRIENDS 0 (0)";
    if (friends_run > 0) {
      snprintf(friendly_stats, sizeof(friendly_stats), "FRIENDS %d (%d)",
               friends_run, friends_tot);
    }

    canvas_bot.drawString(friendly_stats, 0, 5);
  }
  canvas_bot.setTextDatum(top_right);
  //if (display_w > 128) {
     if (getPersonality() == AI) {
      canvas_bot.drawString("AI", display_w, 5);
     }
  // }
  canvas_bot.drawLine(0, 0, display_w, 0);
}

void drawMood(String face, String phrase, bool broken,
              String last_friend_name, signed int rssi) {

  if (!isDisplayOn) {
    return;
  }

  if (broken == true) {
    canvas_main.setTextColor(RED);
  } else {
    canvas_main.setTextColor(GREEN);
  }

  canvas_main.setTextSize(4);
  canvas_main.setTextDatum(middle_center);
  canvas_main.fillSprite(BLACK);
  canvas_main.drawString(face, canvas_center_x, canvas_h / 3);
  canvas_main.setTextDatum(bottom_center);
  canvas_main.setTextSize(1);
  canvas_main.drawString(phrase, canvas_center_x, canvas_h - 35);

  // Add hint text at bottom
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setTextSize(1);
  canvas_main.drawString("Long press for menu", canvas_center_x, canvas_h - 20);

  canvas_main.setTextDatum(bottom_left);
  canvas_main.setTextColor(GREEN);
  String rssi_bars = getRssiBars(rssi);
  char friend_txt[45] = "";
  if (last_friend_name.length() > 0) {
    snprintf(friend_txt, sizeof(friend_txt), "[%s] %s", rssi_bars.c_str(), last_friend_name.c_str());
  }
  canvas_main.drawString(friend_txt, 0, canvas_h - 5);
}

#define ROW_SIZE 40
#define PADDING 10

void drawMainMenu() {
  // Legacy function - now handled by MenuSystem
  if (!isDisplayOn) {
    return;
  }

  canvas_main.fillSprite(BLACK);
  if (display_w > 128) {
    canvas_main.setTextSize(2);
  } else {
    canvas_main.setTextSize(1.5);
  }
  canvas_main.setTextColor(GREEN);
  canvas_main.setColor(GREEN);
  canvas_main.setTextDatum(top_left);

  // Title
  canvas_main.setTextColor(YELLOW);
  canvas_main.drawString("MAIN MENU", PADDING, PADDING);
  canvas_main.setTextColor(GREEN);
}

void drawNearbyMenu() {
  // Legacy function - now handled by MenuSystem
  if (!isDisplayOn) {
    return;
  }

  canvas_main.clear(BLACK);
  canvas_main.setTextSize(1.5);
  canvas_main.setTextColor(GREEN);
  canvas_main.setColor(GREEN);
  canvas_main.setTextDatum(top_left);

  // Title
  canvas_main.setTextColor(YELLOW);
  canvas_main.drawString("NEARBY", PADDING, PADDING);
  canvas_main.setTextColor(GREEN);

  pwngrid_peer* pwngrid_peers = getPwngridPeers();
  uint64_t len = getPwngridRunTotalPeers();

  if (len == 0) {
    canvas_main.setTextColor(TFT_DARKGRAY);
    canvas_main.setCursor(PADDING, PADDING + 20);
    canvas_main.println("No friends nearby");
    canvas_main.println("found yet...");
  }
}

void drawSettingsMenu() {
  // Legacy function - now handled by MenuSystem
  if (!isDisplayOn) {
    return;
  }

  canvas_main.fillSprite(BLACK);
  canvas_main.setTextSize(1.5);
  canvas_main.setTextColor(GREEN);
  canvas_main.setColor(GREEN);
  canvas_main.setTextDatum(top_left);

  // Title
  canvas_main.setTextColor(YELLOW);
  canvas_main.drawString("SETTINGS", PADDING, PADDING);
  canvas_main.setTextColor(GREEN);
}

void drawAboutMenu() {

  if (!isDisplayOn) {
    return;
  }

  canvas_main.clear(BLACK);

  // Title
  canvas_main.setTextColor(YELLOW);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(top_center);
  canvas_main.drawString("ABOUT", canvas_center_x, PADDING);

  canvas_main.qrcode("https://github.com/KaSt/Atomgotchi",
                     (display_w / 2) - (display_h * 0.3), PADDING + 15,
                     display_h * 0.5);

  /*
  // Help text
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(bottom_center);
  canvas_main.drawString("Hold to go back", canvas_center_x, canvas_h - 5);
  */
}

void drawAPConfigMenu() {

  if (!isDisplayOn) {
    return;
  }

  canvas_main.fillSprite(BLACK);
  canvas_main.setTextSize(1.5);
  canvas_main.setTextColor(GREEN);
  canvas_main.setColor(GREEN);
  canvas_main.setTextDatum(top_left);

  int y = PADDING;
  canvas_main.setCursor(0, y);
  canvas_main.println("AP Active");
  y += 20;
  canvas_main.setCursor(0, y);
  canvas_main.println("");
  y += 20;
  canvas_main.setCursor(0, y);
  canvas_main.println("AP:");
  y += 15;
  canvas_main.setTextSize(1);
  canvas_main.setCursor(0, y);
  canvas_main.setTextColor(YELLOW);
  canvas_main.println("  SSID: Gotchi");
  y += 15;
  canvas_main.setCursor(0, y);
  canvas_main.println("  PASS: GotchiPass");
  y += 20;
  canvas_main.setTextColor(GREEN);
  canvas_main.setCursor(0, y);
  canvas_main.println("Then open browser:");
  y += 15;
  canvas_main.setCursor(0, y);
  canvas_main.setTextColor(YELLOW);
  canvas_main.println("  http://192.168.4.1");
  y += 20;
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setCursor(0, y);
  canvas_main.println("Auto-exits in 5 minutes");
  y += 15;
  canvas_main.setCursor(0, y);
  canvas_main.println("Hold button to exit");
}

void drawPersonalityMenu() {
  // Legacy function - now handled by MenuSystem
  if (!isDisplayOn) {
    return;
  }

  canvas_main.fillSprite(BLACK);
  canvas_main.setTextSize(1.5);
  canvas_main.setTextColor(GREEN);
  canvas_main.setColor(GREEN);
  canvas_main.setTextDatum(top_left);

  // Title
  canvas_main.setTextColor(YELLOW);
  canvas_main.drawString("PERSONALITY", PADDING, PADDING);
  canvas_main.setTextColor(GREEN);
}

void drawMenu() {
  if (!isDisplayOn) {
    return;
  }

  // Check if in AP config mode
  uint8_t device_state = getDeviceState();
  if (device_state == STATE_AP_CONFIG) {
    if (!isAPModeActive()) {
      startAPMode();
    }
    drawAPConfigMenu();
    return;
  }

  // Draw current menu using new menu system
  if (currentMenu != nullptr) {
    currentMenu->draw();
    canvas_main.pushSprite(0, canvas_top_h);
  } else {
    // Fallback to main menu if no menu is set
    openMainMenu();
  }
}

void setNinjaMode(bool _ninjaMode) {
  ninjaMode = _ninjaMode;
}

bool getNinjaMode() {
  return ninjaMode;
}