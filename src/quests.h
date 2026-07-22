#ifndef QUESTS_H
#define QUESTS_H

#include "raylib.h"
#include "world.h"
#include "items.h"
#include <stdbool.h>

struct Entity;

// GW1-style quest loop: accept from an NPC, objective tracked on the
// right side of the screen (where GW1 puts its quest log), then return
// to the giver to turn in for an XP + gold (+ sometimes item) reward.
// Quests can chain - a quest with a prerequisite only becomes available
// once it's done, like GW1's quest lines. Each giver only offers their
// own quests, so Ashford's captain and Piken's warmaster have separate
// books of work.
typedef enum {
    QTYPE_KILL,    // slay N matching monsters
    QTYPE_REACH,   // scout/reach a marked location in an explorable
    QTYPE_COLLECT  // hold N of a crafting material; turning in consumes them
} QuestType;

typedef enum {
    QUEST_AVAILABLE,
    QUEST_ACTIVE,
    QUEST_READY_TO_TURN_IN, // objective met, reward waiting at the giver
    QUEST_DONE
} QuestState;

typedef struct {
    const char *name;
    const char *objective;
    const char *offerText;  // what the giver says when offering it
    const char *giverName;  // who offers it AND takes the turn-in
    QuestType type;
    int killsRequired;      // QTYPE_KILL: kills; QTYPE_COLLECT: items needed
    int kills;
    const char *targetName; // QTYPE_KILL: only kills whose victim's name
                            // contains this substring count (NULL = any)
    const char *collectMaterial; // QTYPE_COLLECT: material name
    Vector2 targetPos;  // QTYPE_REACH: where to go
    float reachRadius;
    ZoneId targetZone;  // QTYPE_REACH: zone the marker lives in
    int rewardXP;
    int rewardGold;
    const Item *rewardItem; // optional extra reward, NULL = none
    QuestState state;
    int prereq; // index of a quest that must be DONE first, -1 = none
} Quest;

#define QUEST_COUNT 5
extern Quest g_quests[QUEST_COUNT];

// The first offerable (available + prerequisite met) / turn-in-ready
// quest index FOR THIS GIVER, or -1. NULL matches any giver.
int Quests_OfferableIndexFor(const char *giverName);
int Quests_ReadyToTurnInIndexFor(const char *giverName);

// True when this quest giver has anything for the player - drives the
// green "!" marker over their head.
bool Quests_GiverHasAttentionFor(const char *giverName);

// Back to the fresh-start quest log (everything available, no kills
// counted) - for New Game from the menu.
void Quests_Reset(void);

void Quests_Accept(int index);

// Hands out the rewards and completes the quest. Returns false without
// completing when the reward item doesn't fit in the inventory (GW1
// makes you clear a slot first).
bool Quests_TurnIn(struct Entity *player, int index);

// Called on every monster death; each active kill quest counts the
// victim only if it matches the quest's targetName filter.
void Quests_NotifyMonsterKill(const struct Entity *victim);

// Per-frame objective checks (reach + collect quests).
void Quests_Update(struct Entity *player);

// Right-side objective tracker, drawn under the party panel.
void Quests_DrawTracker(int screenWidth, int screenHeight);

#endif
