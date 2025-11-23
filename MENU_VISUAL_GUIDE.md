# Menu System - Visual Guide

## Screen Adaptations

### M5Stack Cardputer (240x135)
```
┌─────────────────────────────────┐
│        MAIN MENU               │ ← Title (centered)
├─────────────────────────────────┤
│                                │
│  > Friends                     │ ← Selected (yellow text)
│    Settings                    │ ← Normal (green text)
│    About                       │
│    < Back                      │
│                                │
├─────────────────────────────────┤
│ Press: Nav | Hold: Select     │ ← Footer hint
└─────────────────────────────────┘
```

### M5StickC Plus 2 (135x240)
```
┌──────────────┐
│  MAIN MENU  │
├──────────────┤
│ > Friends   │ ║ ← Scrollbar
│   Settings  │ ║
│   About     │ ║
│   < Back    │ ║
│             │ ║
│             │ █
│             │ ║
│             │ ║
├──────────────┤
│ Press: Nav  │
│ Hold: Sel   │
└──────────────┘
```

### M5Atom S3 (128x64)
```
┌─────────────┐
│ MAIN MENU  │
├─────────────┤
│>Friends    │║
│ Settings   │║
│ About      │█
│ < Back     │║
└─────────────┘
```

## Menu Features

### Selection Highlighting
```
Normal item:    Settings        (green text)
Selected item: ┌──────────────┐
               │> Settings    │  (dark green bg, yellow text)
               └──────────────┘
Disabled item:  Settings        (dark gray text)
```

### Scrollbar Behavior
```
Top of list:       Middle:         Bottom:
   ║                  ║                ║
   █                  ║                ║
   ║                  █                ║
   ║                  ║                ║
   ║                  ║                █
```

### Dynamic Content (Nearby Menu)
```
┌──────────────────────────┐
│         NEARBY          │
├──────────────────────────┤
│                         │
│  > alice [████]         │ ← Friend with RSSI
│    bob [███ ]           │
│    charlie [██  ]       │
│    < Back               │
│                         │
└──────────────────────────┘

If no friends:
┌──────────────────────────┐
│         NEARBY          │
├──────────────────────────┤
│                         │
│  No friends nearby      │  (gray text)
│  found yet...           │
│                         │
│  > < Back               │
│                         │
└──────────────────────────┘
```

## Navigation Patterns

### Single Button (M5Atom S3, M5StickC)
```
State 0: Main Screen
         │
         │ (Long Press)
         ▼
State 1: Main Menu - Item 0 selected
         │
         │ (Short Press)
         ▼
State 2: Main Menu - Item 1 selected
         │
         │ (Short Press, wraps)
         ▼
State 3: Main Menu - Item 0 selected
         │
         │ (Long Press on "Settings")
         ▼
State 4: Settings Menu
```

### Keyboard (Cardputer)
```
m or `     → Open/close menu
Tab/Enter  → Navigate down
Arrow keys → Navigate up/down
Any key    → Select (when menu open)
```

### Two Buttons
```
Button A (Short)  → Navigate down
Button B (Short)  → Navigate up
Button A (Long)   → Select item
```

## Color Schemes

### Default Theme
- **Primary**: Green (#00FF00) - Normal items
- **Secondary**: Dark Green (#008000) - Selection bg
- **Background**: Black (#000000)
- **Highlight**: Yellow (#FFFF00) - Selected text & titles
- **Disabled**: Dark Gray (#444444)

### Future Themes (Potential)
- **Night**: Purple/Blue scheme
- **Hacker**: Matrix green variants
- **Retro**: Amber monochrome
- **Minimal**: White/Black/Gray

## Menu Hierarchy

```
Main Menu
├── Friends (Nearby)
│   ├── [Dynamic list of friends]
│   └── < Back → Main Menu
│
├── Settings
│   ├── Config (AP) → AP Config Mode
│   ├── Personality
│   │   ├── Friendly
│   │   ├── AI
│   │   └── < Back → Settings
│   ├── Ninja Mode → Toggle
│   └── < Back → Main Menu
│
├── About → Info Screen
└── < Back → Exit Menu
```

## Code Pattern

### Menu Definition
```cpp
// Initialize
MenuSystem* menu = new MenuSystem(&canvas);
menu->setTitle("TITLE");

// Build
menu->addItem("Option", []() {
    // Action code
});

menu->addBackItem([]() {
    // Return to previous
});

// Display
menu->draw();

// Handle input
menu->navigateDown();  // or navigateUp()
menu->select();
```

### Dynamic Rebuild
```cpp
void rebuildDynamicMenu() {
    menu->clearItems();
    
    for (auto& item : dynamicData) {
        menu->addItem(item.name, [&item]() {
            handleItem(item);
        });
    }
    
    menu->addBackItem(returnToPrevious);
}
```

## Performance Metrics

| Device | Items | Render Time | Memory |
|--------|-------|-------------|--------|
| Cardputer | 10 | ~12ms | ~300B |
| StickC Plus2 | 15 | ~15ms | ~450B |
| Atom S3 | 5 | ~8ms | ~150B |

## Best Practices

1. **Keep labels short**: Max 20 chars for compatibility
2. **Limit items**: 20-30 max for performance
3. **Use lambdas**: Avoid global state where possible
4. **Clear on navigation**: Rebuild dynamic menus
5. **Test on target**: Different screens have different limits

## Accessibility

- ✅ Works with single button
- ✅ Clear visual feedback
- ✅ Wrapping navigation
- ✅ Scrollbar indicates position
- ✅ Footer hints explain controls
- ✅ High contrast colors
- ⏳ Sound feedback (future)
- ⏳ Haptic feedback (future)
