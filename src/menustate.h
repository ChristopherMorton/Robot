#ifndef MENU_STATE_H__
#define MENU_STATE_H__

// MenuState handles where in the game we are
typedef unsigned int MenuState;

extern MenuState menu_state; // stored in app.cpp

namespace sf { class Font; };
extern sf::Font *menu_font; // stored in app.cpp
extern sf::Font *ingame_font; // stored in app.cpp

// MenuState masks:
// startup:
#define MENU_PRELOAD 0x1
#define MENU_LOADING 0x2
#define MENU_POSTLOAD 0x3
#define MENU_MAIN 0x4
// where in the app are we?
#define MENU_PRI_SPLASH 0x100
#define MENU_PRI_NEW_GAME 0x200
#define MENU_PRI_INGAME 0x400
#define MENU_PRI_ANIMATION 0x800
#define MENU_PRI_LEVEL_EDITOR 0x1000
// which secondary windows are up?
#define MENU_SEC_OPTIONS 0x10000
#define MENU_SEC_AV_OPTIONS 0x20000
#define MENU_SEC_INPUT_OPTIONS 0x40000
#define MENU_SEC_SAVE_LOAD 0x80000
// other
#define MENU_ERROR_DIALOG 0x1000000

#endif
