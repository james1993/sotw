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

// The in-game pause menu (P / gamepad Start), drawn over the frozen
// world: Resume, Quit to Main Menu, Quit Game. Same navigation as the
// main menu. The game autosaves regardless of which exit is taken.
typedef enum {
    PAUSE_NONE = 0,
    PAUSE_RESUME,
    PAUSE_QUIT_TO_MENU,
    PAUSE_QUIT_GAME
} PauseAction;

PauseAction UI_DrawPauseMenu(int screenWidth, int screenHeight);

#endif
