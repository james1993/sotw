#ifndef PROGRESSION_H
#define PROGRESSION_H

struct Entity;

// XP needed to go from `level` to `level + 1`. Approximates GW1's curve
// (rising ~600 XP per level); the exact retail table isn't reproduced,
// but the shape - early levels fast, later levels slow, hard cap at
// 20 - is. See docs/research/gw1-mechanics.md #1.
int Progression_XPToNext(int level);

// Party-wide kill XP, scaled by the level difference between the
// monster and the player, like GW1: higher-level kills give more,
// far-lower-level kills give almost nothing. Handles level-ups: +20 max
// HP per level (GW1's actual per-level health gain) and attribute
// points on GW1's schedule (+5/level up to 10, +10 to 15, +15 to 20).
void Progression_AwardKillXP(struct Entity *player, int monsterLevel);

// Direct XP grant (quest rewards etc.); runs the same level-up logic.
void Progression_AwardXP(struct Entity *player, int amount);

#define MAX_LEVEL 20

#endif
