#ifndef TITLES_H
#define TITLES_H

#include <stdbool.h>

struct Entity;

// GW1's title tracks, reduced to the one that pre-Searing actually has.
//
// Legendary Defender of Ascalon is a single-rank title earned by
// reaching level 20 without ever leaving pre-Searing - 140,600
// experience, all of it from an area whose monsters cap out around
// level 8. There is no tier ladder: you either did it or you didn't,
// which is exactly what makes it the pre-Searing achievement.
//
// The track stays hidden until level 12, GW1's own rule. Before that
// the panel says a title exists rather than showing a progress bar you
// can't meaningfully move yet.
typedef enum {
    TITLE_DEFENDER_OF_ASCALON = 0,
    TITLE_COUNT
} TitleId;

#define TITLE_LDOA_LEVEL   20
#define TITLE_LDOA_REVEAL  12

const char *Titles_Name(TitleId id);

// Have the requirements been met? Reads live off the player.
bool Titles_IsEarned(TitleId id, const struct Entity *player);

// Should the track be listed at all yet?
bool Titles_IsRevealed(TitleId id, const struct Entity *player);

// 0..1 progress toward earning it, for the panel's bar.
float Titles_Progress(TitleId id, const struct Entity *player);

// Which title the character wears under their name, or -1 for none.
// Setting one you haven't earned is refused.
int Titles_Displayed(void);
void Titles_SetDisplayed(int titleId);

// The text to draw under the player's nameplate, or NULL when nothing
// is being displayed (or the displayed title is no longer earned).
const char *Titles_DisplayedText(const struct Entity *player);

void Titles_Reset(void);

#endif
