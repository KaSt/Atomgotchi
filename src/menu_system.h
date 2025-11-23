#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <vector>
#include <functional>
#include "M5Unified.h"

// Menu item structure
struct MenuItem {
    String label;
    std::function<void()> action;
    bool selected = false;
    bool enabled = true;
    
    MenuItem(const String& lbl, std::function<void()> act) 
        : label(lbl), action(act) {}
};

// Menu configuration for different screen sizes
struct MenuConfig {
    int itemHeight;
    int fontSize;
    int padding;
    int maxVisibleItems;
    int scrollbarWidth;
};

// Menu system class
class MenuSystem {
private:
    M5Canvas* canvas;
    std::vector<MenuItem> items;
    int currentIndex;
    int scrollOffset;
    MenuConfig config;
    String title;
    uint16_t primaryColor;
    uint16_t secondaryColor;
    uint16_t backgroundColor;
    uint16_t highlightColor;
    
    void autoConfigureForScreen();
    void drawScrollbar();
    void drawMenuItem(int index, int yPos, bool isSelected);
    int getVisibleStartIndex();
    int getVisibleEndIndex();
    
public:
    MenuSystem(M5Canvas* canv);
    
    void setTitle(const String& t);
    void addItem(const String& label, std::function<void()> action);
    void addBackItem(std::function<void()> action);
    void clearItems();
    
    void setColors(uint16_t primary, uint16_t secondary, uint16_t bg, uint16_t highlight);
    
    void draw();
    void navigateUp();
    void navigateDown();
    void select();
    
    int getCurrentIndex() const { return currentIndex; }
    void setCurrentIndex(int idx);
    
    bool isEmpty() const { return items.empty(); }
    int getItemCount() const { return items.size(); }
};

#endif // MENU_SYSTEM_H
