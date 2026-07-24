#ifndef SKILLBOOK_H
#define SKILLBOOK_H

#include "attributes.h"
#include <stdbool.h>

// Which skills this character actually owns.
//
// In GW1 your eight-slot bar is the *output* of a collection game: you
// start with almost nothing, earn skills from quests, buy them from
// trainers with skill points and gold, and capture elites off bosses.
// Handing the player a full bar at character creation skips the entire
// build-crafting loop, so the bar starts nearly empty and everything on
// it has to be earned.
//
// Ownership is per-character and saved; the bar itself is just a
// selection of eight owned skills.

// Fresh character: only the starting skill(s) known, no points spent.
void Skillbook_Reset(void);

bool Skillbook_IsUnlocked(int skillId);

// Adds a skill to the book. Returns true only if it wasn't already
// known, so callers can announce genuinely new acquisitions.
bool Skillbook_Unlock(int skillId);

int Skillbook_UnlockedCount(void);

// Whether a character of these professions could ever use the skill -
// its attribute has to belong to one of their two professions. Monster
// skills sit on attributes nobody can reach, which keeps Claw Swipe and
// friends out of every trainer and skill list automatically.
bool Skillbook_IsUsableBy(int skillId, Profession primary, Profession secondary);

// --- Skill points: earned on level up, spent at trainers (GW1's rule) ---
extern int g_skillPoints;
extern int g_skillsPurchased;

// Gold price of the next trainer purchase. Climbs with each skill
// bought, like GW1's escalating trainer costs.
int Skillbook_TrainerGoldCost(void);

// Buying requires a skill point AND the gold. Elite skills are never
// sold - those are capture-only, again GW1's rule.
bool Skillbook_CanBuy(int skillId, Profession primary, Profession secondary);
bool Skillbook_Buy(int skillId, Profession primary, Profession secondary);

// Save/restore support: the unlock set as a compact "0110..." string
// indexed by SkillId.
void Skillbook_WriteMask(char *out, int outSize);
void Skillbook_ReadMask(const char *mask);

#endif
