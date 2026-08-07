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

// "A skill of your calling": resolved against the player's professions
// at the moment it's shown or handed over. A quest that hard-named a
// Monk spell would be worthless to a Ranger, and there are six
// professions to serve off one quest chain.
#define QUEST_REWARD_ANY_SKILL (-2)

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
    int rewardSkill;        // SkillId taught on turn-in, -1 = none, or
                            // QUEST_REWARD_ANY_SKILL to teach whatever
                            // suits the character. Quests are the main
                            // early source of skills, so the bar fills
                            // as the story does.
    QuestState state;
    int prereq; // index of a quest that must be DONE first, -1 = none
    // Turning this one in ends the campaign. GW1's pre-Searing has
    // exactly one such quest - Sir Tydus' Academy trial - and the
    // Searing follows it immediately. Data rather than a name check, so
    // the ending isn't wired to a string.
    bool endsCampaign;
} Quest;

#define QUEST_COUNT 9
extern Quest g_quests[QUEST_COUNT];

// The first offerable (available + prerequisite met) / turn-in-ready
// quest index FOR THIS GIVER, or -1. NULL matches any giver.
int Quests_OfferableIndexFor(const char *giverName);
int Quests_ReadyToTurnInIndexFor(const char *giverName);

// Which skill this quest will actually teach THIS character, or -1 if
// none (or nothing suitable is left). Used both by the reward summary
// and by the turn-in, so the promise and the payout can't differ.
int Quests_ResolveRewardSkill(const Quest *q, const struct Entity *player);

// True once the named quest has been turned in. The profession trainer
// gates the second profession on this: GW1 makes you actually play the
// primary for a while before it lets you pick a second.
bool Quests_IsDoneByName(const char *name);

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
