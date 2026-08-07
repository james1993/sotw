#ifndef AUDIO_H
#define AUDIO_H

#include "skill.h"
#include <stdbool.h>

// Sound effects.
//
// Deliberately keyed by what a thing IS, not by which skill did it. GW1
// has hundreds of skills and a handful of sounds; keying off the data
// already on `Skill` (its type, and what its effect steps actually do)
// means every skill added later gets audio for free instead of silently
// getting none - which is exactly the failure the icons had before they
// moved to an atlas.
typedef enum {
    SFX_UI_MOVE = 0,    // focus stepped to another item; the quietest thing here
    SFX_UI_CLICK,
    SFX_UI_OPEN,
    SFX_UI_CLOSE,
    SFX_UI_DENY,        // can't afford it, can't do it
    SFX_UI_CONFIRM,     // purchase, skill learned, profession taken
    SFX_LOOT,
    SFX_LEVEL_UP,
    SFX_QUEST_DONE,
    SFX_SWING,          // melee attack
    SFX_BOW,            // ranged attack
    SFX_HIT,
    SFX_HIT_HEAVY,
    SFX_DEATH,
    SFX_CAST_FIRE,      // offensive magic
    SFX_CAST_HEAL,
    SFX_CAST_HEX,
    SFX_CAST_ARCANE,    // spells that neither heal nor hex; signets, shouts
    SFX_COUNT
} SoundId;

// Opens the audio device and loads the bank. Safe to call when there is
// no sound hardware at all: everything below turns into a no-op rather
// than failing, the same policy the font and icon loaders follow.
void Audio_Init(void);
void Audio_Unload(void);
void Audio_Update(float dt);

void Audio_Play(SoundId id);

// As above, but panned by how far the source sits from the player on the
// x axis (world units, negative = left). Distant sources are also
// quieter, so a fight across the zone doesn't sound like it's on top of
// you.
void Audio_PlayAt(SoundId id, float dxFromPlayer);

// Picks the sound for a skill from its type and its effect steps: a
// skill that heals sounds like healing whichever profession owns it.
void Audio_PlaySkill(const Skill *skill);

// Master volume, 0-100. Persisted in the save file, changed from the
// pause menu, and applied immediately.
int Audio_GetVolume(void);
void Audio_SetVolume(int percent);

#endif
