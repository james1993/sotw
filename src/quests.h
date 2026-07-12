#ifndef QUESTS_H
#define QUESTS_H

#include "raylib.h"
#include <stdbool.h>

struct Entity;

// GW1-style quest loop: accept from an NPC, objective tracked on the
// right side of the screen (where GW1 puts its quest log), then return
// to the giver to turn in for an XP + gold reward. Quests can chain -
// a quest with a prerequisite only becomes available once it's done,
// like GW1's quest lines.
typedef enum {
    QTYPE_KILL,  // slay N monsters
    QTYPE_REACH  // scout/reach a marked location in the explorable
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
    const char *giverName;  // who to return to for the reward
    QuestType type;
    int killsRequired;
    int kills;
    const char *targetName; // QTYPE_KILL: only kills whose victim's name
                            // contains this substring count (NULL = any)
    Vector2 targetPos;  // QTYPE_REACH: where to go (explorable coords)
    float reachRadius;
    int rewardXP;
    int rewardGold;
    QuestState state;
    int prereq; // index of a quest that must be DONE first, -1 = none
} Quest;

#define QUEST_COUNT 2
extern Quest g_quests[QUEST_COUNT];

// The first offerable (available + prerequisite met) / turn-in-ready
// quest index, or -1.
int Quests_OfferableIndex(void);
int Quests_ReadyToTurnInIndex(void);

// True when the quest giver has anything for the player - drives the
// green "!" marker.
bool Quests_GiverHasAttention(void);

void Quests_Accept(int index);
void Quests_TurnIn(struct Entity *player, int index);

// Called on every monster death; each active kill quest counts the
// victim only if it matches the quest's targetName filter.
void Quests_NotifyMonsterKill(const struct Entity *victim);

// Per-frame objective checks (reach quests). Only meaningful in the
// explorable zone.
void Quests_Update(struct Entity *player);

// Right-side objective tracker, drawn under the party panel.
void Quests_DrawTracker(int screenWidth, int screenHeight);

#endif
