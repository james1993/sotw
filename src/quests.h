#ifndef QUESTS_H
#define QUESTS_H

struct Entity;

// Minimal GW1-style quest loop: accept from an NPC, objective tracked
// on the right side of the screen (where GW1 puts its quest log), then
// return to the giver to turn in for an XP + gold reward.
typedef enum {
    QUEST_AVAILABLE,
    QUEST_ACTIVE,
    QUEST_READY_TO_TURN_IN, // objective met, reward waiting at the giver
    QUEST_DONE
} QuestState;

typedef struct {
    const char *name;
    const char *objective;
    int killsRequired;
    int kills;
    int rewardXP;
    int rewardGold;
    QuestState state;
} Quest;

// The one authored quest, "Charr at the Gate" (a real pre-Searing GW1
// quest name), given by Captain Osric.
extern Quest g_quest;

void Quests_Accept(void);
void Quests_NotifyMonsterKill(void);
void Quests_TurnIn(struct Entity *player); // grants rewards

// Right-side objective tracker, drawn under the party panel.
void Quests_DrawTracker(int screenWidth, int screenHeight);

#endif
