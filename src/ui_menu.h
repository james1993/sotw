#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdbool.h>

// The main menu, shown before the world exists: Continue (only when a
// save is on disk), New Game, Quit. Immediate-mode like the rest of the
// UI - draws itself and reports what was clicked this frame. Enter or
// gamepad A picks the default action (Continue when there's a save,
// otherwise New Game).
typedef enum {
    MENU_NONE = 0,
    MENU_CONTINUE,
    MENU_NEW_GAME,
    MENU_QUIT
} MenuAction;

MenuAction UI_DrawMainMenu(int screenWidth, int screenHeight, bool hasSave);

#endif
