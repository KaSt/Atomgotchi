#include "ui.h"
#include "device_state.h"
#include "config.h"
#include "ap_config.h"

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

struct menu {
  char name[25];
  int command;
};

menu main_menu[] = {
  { "Friends", 2 },
  { "Settings", 4 },
  { "About", 8 },
  { "Back", 99 }  // Added back option
};

menu settings_menu[] = {
  { "Config (AP)", 40 },
  { "Personality", 42 },
  { "Ninja Mode", 45 },
  { "Back", 41 },
};

menu nearby_menu[] = {
  { "Back", 41 },
};

menu personality_menu[] = {
  { "Friendly", 43 },
  { "AI", 44 },
  { "Back", 41 },
};

int main_menu_len = sizeof(main_menu) / sizeof(menu);
int settings_menu_len = sizeof(settings_menu) / sizeof(menu);
int personality_menu_len = sizeof(personality_menu) / sizeof(menu);

bool menu_open = false;
uint8_t menu_current_cmd = 0;
uint8_t menu_current_opt = 0;
uint8_t menu_max_opt = 0;  // Track max options for current menu

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
}

bool keyboard_changed = false;

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
    //Serial.println("UI - Long press detected");

    // If in AP config mode, exit it
    if (isAPModeActive()) {
      exitAPConfigMode();
      menu_open = false;
      menu_current_cmd = 0;
      menu_current_opt = 0;
      return;
    }

    // If menu is NOT open, open it
    if (!menu_open) {
      //Serial.println("UI - Opening menu");
      menu_open = true;
      menu_current_cmd = 0;
      menu_current_opt = 0;
      menu_max_opt = main_menu_len;
    }
    // If menu IS open, select current option
    else {
      //Serial.printf("UI - Selecting menu option: cmd=%d, opt=%d\n", menu_current_cmd, menu_current_opt);

      // Handle selection based on current menu
      switch (menu_current_cmd) {
        case 0:  // Main menu
          menu_current_cmd = main_menu[menu_current_opt].command;
          menu_current_opt = 0;

          // Set max options for new menu
          if (menu_current_cmd == 2) {                     // Nearby menu
            menu_max_opt = getPwngridRunTotalPeers() + 1;  // +1 for Back
          } else if (menu_current_cmd == 4) {              // Settings menu
            menu_max_opt = settings_menu_len;
          } else if (menu_current_cmd == 8) {  // About - just back
            menu_max_opt = 1;
          } else if (menu_current_cmd == 99) {  // Back to main
            menu_open = false;
            menu_current_cmd = 0;
            menu_current_opt = 0;
          }
          break;

        case 2:  // Nearby menu
          // If "Back" is selected (last option)
          if (menu_current_opt >= getPwngridRunTotalPeers()) {
            menu_current_cmd = 0;
            menu_current_opt = 0;
            menu_max_opt = main_menu_len;
          }
          // Otherwise viewing a peer, just stay here
          break;

        case 4:  // Settings menu
          menu_current_cmd = settings_menu[menu_current_opt].command;
          menu_current_opt = 0;

          if (menu_current_cmd == 40) {  // WiFi Config
            // Will be handled in drawMenu
          } else if (menu_current_cmd == 41) {  // Back
            menu_current_cmd = 0;
            menu_current_opt = 0;
            menu_max_opt = main_menu_len;
          }
          break;

        case 8:  // About menu - long press goes back
          menu_current_cmd = 0;
          menu_current_opt = 0;
          menu_max_opt = main_menu_len;
          break;

        case 42:  // Personality menu - Choose Gotchi's personality.
          menu_current_cmd = personality_menu[menu_current_opt].command;
          menu_current_opt = 0;

          if (menu_current_cmd == 43) {
            Serial.println("Clicked on Personality [Friendly]");
            setPersonality(FRIENDLY);
          } else if (menu_current_cmd == 44) {
            Serial.println("Clicked on Personality [AI]");
            setPersonality(AI);
          }
          break;

        default:
          menu_current_cmd = 0;
          menu_current_opt = 0;
          menu_max_opt = main_menu_len;
          break;
      }
    }
  }

  // Short press cycles through options
  if (isNextPressed() && menu_open) {
    //Serial.println("UI - Short press - next option");
    menu_current_opt++;
    if (menu_current_opt >= menu_max_opt) {
      menu_current_opt = 0;  // Wrap around
    }
    //Serial.printf("UI - New option: %d (max: %d)\n", menu_current_opt, menu_max_opt);
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

  char display_str[50] = "";
  for (uint8_t i = 0; i < main_menu_len; i++) {
    snprintf(display_str, sizeof(display_str), "%s %s",
             (menu_current_opt == i) ? ">" : " ",
             main_menu[i].name);
    int y = PADDING + 20 + (i * ROW_SIZE / 2);
    canvas_main.drawString(display_str, PADDING, y);
  }

  /*
  // Help text at bottom
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(bottom_center);
  canvas_main.drawString("Press: Next | Hold: Select", canvas_center_x, canvas_h - 5);
  */
}

void drawNearbyMenu() {

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
  uint8_t len = getPwngridRunTotalPeers();

  if (len == 0) {
    canvas_main.setTextColor(TFT_DARKGRAY);
    canvas_main.setCursor(PADDING, PADDING + 20);
    canvas_main.println("No friends nearby");
    canvas_main.println("found yet...");
  } else {
    char display_str[50] = "";
    for (uint8_t i = 0; i < len; i++) {
      snprintf(display_str, sizeof(display_str), "%s %s [%s]",
               (menu_current_opt == i) ? ">" : " ",
               pwngrid_peers[i].name,
               getRssiBars(pwngrid_peers[i].rssi).c_str());
      int y = PADDING + 20 + (i * ROW_SIZE / 2);
      canvas_main.drawString(display_str, PADDING, y);
    }
  }

  // Back option
  char back_str[50] = "";
  snprintf(back_str, sizeof(back_str), "%s Back",
           (menu_current_opt == len) ? ">" : " ");
  int back_y = PADDING + 20 + (len * ROW_SIZE / 2);
  canvas_main.drawString(back_str, PADDING, back_y);

  /*
  // Help text
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(bottom_center);
  canvas_main.drawString("Press: Next | Hold: Select", canvas_center_x, canvas_h - 5);
  */
}

void drawSettingsMenu() {

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

  char display_str[50] = "";
  for (uint8_t i = 0; i < settings_menu_len; i++) {
    snprintf(display_str, sizeof(display_str), "%s %s",
             (menu_current_opt == i) ? ">" : " ",
             settings_menu[i].name);
    int y = PADDING + 20 + (i * ROW_SIZE / 2);
    canvas_main.drawString(display_str, PADDING, y);
  }

  /*
  // Help text
  canvas_main.setTextColor(TFT_DARKGRAY);
  canvas_main.setTextSize(1);
  canvas_main.setTextDatum(bottom_center);
  canvas_main.drawString("Press: Next | Hold: Select", canvas_center_x, canvas_h - 5);
  */
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

  char display_str[50] = "";
  for (uint8_t i = 0; i < personality_menu_len; i++) {
    snprintf(display_str, sizeof(display_str), "%s %s",
             (menu_current_opt == i) ? ">" : " ",
             personality_menu[i].name);
    int y = PADDING + 20 + (i * ROW_SIZE / 2);
    canvas_main.drawString(display_str, PADDING, y);
  }
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

  // Draw appropriate menu based on current command
  switch (menu_current_cmd) {
    case 0:
      drawMainMenu();
      break;
    case 2:
      drawNearbyMenu();
      break;
    case 4:
      drawSettingsMenu();
      break;
    case 8:
      drawAboutMenu();
      break;
    case 40:
      // WiFi Config (AP) selected - enter AP mode
      enterAPConfigMode();
      break;
    case 41:
      // Back selected - should have been handled in updateUi
      menu_current_cmd = 0;
      menu_current_opt = 0;
      drawMainMenu();
      break;
    case 42:
      // Personality setting
      drawPersonalityMenu();
      break;
    case 99:
      // Back to main screen
      menu_open = false;
      menu_current_cmd = 0;
      menu_current_opt = 0;
      break;
    default:
      drawMainMenu();
      break;
  }
}

void setNinjaMode(bool _ninjaMode) {
  ninjaMode = _ninjaMode;
}

bool getNinjaMode() {
  return ninjaMode;
}