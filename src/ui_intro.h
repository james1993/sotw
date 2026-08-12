#ifndef UI_INTRO_H
#define UI_INTRO_H

#include <stdbool.h>

// The opening cinematic, "The Last Day Dawns" - GW1 Prophecies' own words,
// played once when a new character is created, before they wake in
// Ascalon City. A timed, skippable text sequence over a dawn sky and the
// Great Northern Wall, the same shape as the Searing sequence that ends
// the prototype.

// Rewind to the first frame. Call before switching into the intro.
void UI_IntroReset(void);

// Draw one frame; returns true when the cinematic has finished or been
// skipped, at which point the caller hands over to gameplay.
bool UI_DrawIntro(int screenWidth, int screenHeight, float dt);

#endif
