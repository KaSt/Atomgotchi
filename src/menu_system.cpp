#include "menu_system.h"

MenuSystem::MenuSystem(M5Canvas* canv) 
    : canvas(canv), currentIndex(0), scrollOffset(0), title("") {
    
    primaryColor = TFT_GREEN;
    secondaryColor = TFT_DARKGREEN;
    backgroundColor = TFT_BLACK;
    highlightColor = TFT_YELLOW;
    
    autoConfigureForScreen();
}

void MenuSystem::autoConfigureForScreen() {
    int screenWidth = canvas->width();
    int screenHeight = canvas->height();
    
    // Detect device type based on screen dimensions
    if (screenWidth == 135 && screenHeight == 240) {
        // M5StickC Plus 2
        config.itemHeight = 20;
        config.fontSize = 1;
        config.padding = 5;
        config.maxVisibleItems = 9;
        config.scrollbarWidth = 3;
    } 
    else if (screenWidth == 128 && screenHeight == 64) {
        // M5Atom S3 or similar small screen
        config.itemHeight = 12;
        config.fontSize = 1;
        config.padding = 2;
        config.maxVisibleItems = 4;
        config.scrollbarWidth = 2;
    }
    else if (screenWidth >= 240 && screenHeight >= 135) {
        // Cardputer or larger displays
        config.itemHeight = 18;
        config.fontSize = 1;
        config.padding = 8;
        config.maxVisibleItems = (screenHeight - 40) / config.itemHeight;
        config.scrollbarWidth = 4;
    }
    else {
        // Default configuration
        config.itemHeight = 18;
        config.fontSize = 1;
        config.padding = 5;
        config.maxVisibleItems = (screenHeight - 30) / config.itemHeight;
        config.scrollbarWidth = 3;
    }
}

void MenuSystem::setTitle(const String& t) {
    title = t;
}

void MenuSystem::addItem(const String& label, std::function<void()> action) {
    items.push_back(MenuItem(label, action));
}

void MenuSystem::addBackItem(std::function<void()> action) {
    MenuItem backItem("< Back", action);
    items.push_back(backItem);
}

void MenuSystem::clearItems() {
    items.clear();
    currentIndex = 0;
    scrollOffset = 0;
}

void MenuSystem::setColors(uint16_t primary, uint16_t secondary, uint16_t bg, uint16_t highlight) {
    primaryColor = primary;
    secondaryColor = secondary;
    backgroundColor = bg;
    highlightColor = highlight;
}

int MenuSystem::getVisibleStartIndex() {
    if (items.size() <= config.maxVisibleItems) {
        return 0;
    }
    
    // Keep current item in view
    if (currentIndex < scrollOffset) {
        scrollOffset = currentIndex;
    } else if (currentIndex >= scrollOffset + config.maxVisibleItems) {
        scrollOffset = currentIndex - config.maxVisibleItems + 1;
    }
    
    return scrollOffset;
}

int MenuSystem::getVisibleEndIndex() {
    int start = getVisibleStartIndex();
    int end = start + config.maxVisibleItems;
    if (end > items.size()) {
        end = items.size();
    }
    return end;
}

void MenuSystem::drawMenuItem(int index, int yPos, bool isSelected) {
    if (index < 0 || index >= items.size()) return;
    
    MenuItem& item = items[index];
    
    // Draw selection background
    if (isSelected) {
        canvas->fillRoundRect(
            config.padding, 
            yPos, 
            canvas->width() - config.padding * 2 - config.scrollbarWidth - 2,
            config.itemHeight - 2,
            3,
            secondaryColor
        );
    }
    
    // Set text properties
    canvas->setTextSize(config.fontSize);
    canvas->setTextDatum(middle_left);
    
    // Draw text
    uint16_t textColor = isSelected ? highlightColor : (item.enabled ? primaryColor : TFT_DARKGREY);
    canvas->setTextColor(textColor);
    
    String displayText = item.label;
    int maxChars = (canvas->width() - config.padding * 3 - config.scrollbarWidth) / 6;
    if (displayText.length() > maxChars) {
        displayText = displayText.substring(0, maxChars - 3) + "...";
    }
    
    canvas->drawString(
        displayText, 
        config.padding * 2, 
        yPos + config.itemHeight / 2
    );
}

void MenuSystem::drawScrollbar() {
    if (items.size() <= config.maxVisibleItems) return;
    
    int scrollbarX = canvas->width() - config.scrollbarWidth - 1;
    int scrollbarHeight = canvas->height() - 25;
    int scrollbarY = 23;
    
    // Draw scrollbar background
    canvas->drawRect(scrollbarX, scrollbarY, config.scrollbarWidth, scrollbarHeight, secondaryColor);
    
    // Calculate thumb position and size
    float itemRatio = (float)config.maxVisibleItems / items.size();
    int thumbHeight = max(10, (int)(scrollbarHeight * itemRatio));
    
    float scrollRatio = (float)scrollOffset / (items.size() - config.maxVisibleItems);
    int thumbY = scrollbarY + (int)((scrollbarHeight - thumbHeight) * scrollRatio);
    
    // Draw thumb
    canvas->fillRect(scrollbarX, thumbY, config.scrollbarWidth, thumbHeight, primaryColor);
}

void MenuSystem::draw() {
    canvas->fillSprite(backgroundColor);
    
    // Draw title
    if (title.length() > 0) {
        canvas->setTextSize(config.fontSize + 1);
        canvas->setTextDatum(top_center);
        canvas->setTextColor(highlightColor);
        canvas->drawString(title, canvas->width() / 2, config.padding);
        
        // Draw title underline
        canvas->drawLine(
            config.padding, 
            20, 
            canvas->width() - config.padding, 
            20, 
            secondaryColor
        );
    }
    
    // Calculate visible range
    int startIdx = getVisibleStartIndex();
    int endIdx = getVisibleEndIndex();
    
    // Draw menu items
    int yPos = 25;
    for (int i = startIdx; i < endIdx; i++) {
        drawMenuItem(i, yPos, i == currentIndex);
        yPos += config.itemHeight;
    }
    
    // Draw scrollbar
    drawScrollbar();
    
    // Draw footer hint (only on larger screens)
    if (canvas->height() > 100) {
        canvas->setTextSize(1);
        canvas->setTextDatum(bottom_center);
        canvas->setTextColor(TFT_DARKGREY);
        canvas->drawString("Press: Nav | Hold: Select", canvas->width() / 2, canvas->height() - 2);
    }
}

void MenuSystem::navigateUp() {
    if (items.empty()) return;
    
    currentIndex--;
    if (currentIndex < 0) {
        currentIndex = items.size() - 1;
        scrollOffset = max(0, (int)items.size() - config.maxVisibleItems);
    }
}

void MenuSystem::navigateDown() {
    if (items.empty()) return;
    
    currentIndex++;
    if (currentIndex >= items.size()) {
        currentIndex = 0;
        scrollOffset = 0;
    }
}

void MenuSystem::select() {
    if (items.empty() || currentIndex < 0 || currentIndex >= items.size()) return;
    
    MenuItem& item = items[currentIndex];
    if (item.enabled && item.action) {
        item.action();
    }
}

void MenuSystem::setCurrentIndex(int idx) {
    if (idx >= 0 && idx < items.size()) {
        currentIndex = idx;
        
        // Adjust scroll offset if needed
        if (currentIndex < scrollOffset) {
            scrollOffset = currentIndex;
        } else if (currentIndex >= scrollOffset + config.maxVisibleItems) {
            scrollOffset = currentIndex - config.maxVisibleItems + 1;
        }
    }
}
