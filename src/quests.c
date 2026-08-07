#include "quests.h"
#include "audio.h"
#include "character.h"
#include "entity.h"
#include "progression.h"
#include "items.h"
#include "world.h"
#include "ui_party.h"
#include "ui_compass.h"
#include "ui_hit.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "ui_hints.h"
#include "skill.h"
#include "skillbook.h"
#include "save.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Item rewards, GW1-style "take this for your trouble" gear. Static so
// quests can point at them; turn-in copies them into the inventory.
static const Item g_rewardWarhammer = { .kind = ITEM_WEAPON, .name = "Warmaster's Hammer",
                                        .dmgMin = 22, .dmgMax = 38, .range = 30.0f,
                                        .attackInterval = 1.75f, .count = 1, .twoHanded = true };
static const Item g_rewardIdKit = { .kind = ITEM_KIT_ID, .name = "Identification Kit", .count = 25 };

Quest g_quests[QUEST_COUNT] = {
    // --- Ashford Abbey ---
    // Abbot Ciglo's tithe. A real pre-Searing quest name; the abbot
    // standing in for its giver is the demake's choice.
    //
    // Canon names are used where the CONTENT matches too. "Further
    // Adventures" is a real pre-Searing quest, but it happens in
    // Lakeside and has nothing to do with a tithe - borrowing the name
    // for this would be canon on the label and invented underneath.
    {
        .name = "Tithe for Ashford Abbey",
        .objective = "Bring 3 Skale Fins to Ashford Abbey",
        .offerText = "The abbey lives on charity, and charity is thin this season. "
                     "Bring me three skale fins from the lake and we'll call it a tithe.",
        .giverName = "Abbot Ciglo",
        .type = QTYPE_COLLECT,
        .killsRequired = 3,
        .collectMaterial = "Skale Fin",
        .rewardXP = 150,
        .rewardGold = 80,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },

    // --- Ascalon City: Sir Tydus ---
    // The canon quest that sends you to find a second profession. It is
    // the gate on the profession trainers, so it comes early and asks
    // almost nothing of you.
    {
        .name = "A Second Profession",
        .objective = "Find a profession trainer and take a second profession",
        .offerText = "You've a fine right arm, but one calling won't hold the wall. "
                     "Find a trainer - they're scattered across the county - and "
                     "learn a second.",
        .giverName = "Sir Tydus",
        .type = QTYPE_KILL,
        .killsRequired = 2,
        .targetName = "Skale",
        .rewardXP = 200,
        .rewardGold = 60,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },

    // --- Ascalon City: Prince Rurik ---
    // GW1's pre-Searing primary chain, in its own order and under its
    // own names: A Second Profession, Unsettling Rumors, The Path to
    // Glory, and finally Ascalon Academy, which ends the campaign.
    {
        .name = "Unsettling Rumors",
        .objective = "Find out what walks in the Catacombs",
        .offerText = "Scouts say something stirs beneath the abbey - and worse, that "
                     "the Charr have been seen going down there. Find out what walks "
                     "in the Catacombs.",
        .giverName = "Prince Rurik",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Skeleton",
        .rewardXP = 300,
        .rewardGold = 120,
        .rewardItem = &g_rewardIdKit,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "The Path to Glory",
        .objective = "Drive the Charr back in the Northlands",
        .offerText = "The Charr have crossed into the Northlands. Come with me and "
                     "we'll send them back over the wall.",
        .giverName = "Prince Rurik",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Charr",
        .rewardXP = 450,
        .rewardGold = 200,
        .rewardItem = &g_rewardWarhammer,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = 1, // after Sir Tydus has sent you for a second profession
    },

    // --- Regent Valley: Duke Barradin ---
    {
        .name = "Bandit Raid",
        .objective = "Break up the bandits holding the Regent Valley road",
        .offerText = "Highwaymen have taken my road, and my tenants can't reach the "
                     "market. Break them.",
        .giverName = "Duke Barradin",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Bandit",
        .rewardXP = 350,
        .rewardGold = 160,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "Unnatural Growths",
        .objective = "Destroy the aloes in Wizard's Folly",
        .offerText = "The wizard's old ground is sprouting things that shouldn't "
                     "sprout. Cut them down before they seed any further.",
        .giverName = "Duke Barradin",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Aloe",
        .rewardXP = 300,
        .rewardGold = 140,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = 4, // the duke trusts you once his road is clear
    },

    // --- Piken Square: Reforged Mode only ---
    // Warmaster Riga has nothing to say to a character who can't reach
    // her, which is the point: this line only exists for Reforged.
    {
        .name = "Hold the Square",
        .objective = "Slay Charr Axe Fiends in the Northlands",
        .offerText = "You fought your way in, so you know what's out there. The axe "
                     "fiends are the ones that break a shield wall. Thin them.",
        .giverName = "Warmaster Riga",
        .type = QTYPE_KILL,
        .killsRequired = 2,
        .targetName = "Axe Fiend",
        .rewardXP = 500,
        .rewardGold = 250,
        .rewardSkill = QUEST_REWARD_ANY_SKILL,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    // --- Ascalon City: the end of pre-Searing ---
    // Sir Tydus offers the Ascalon Guards, you take the Academy trial,
    // and the Charr come. In GW1 this is the point of no return, which
    // is why it needs every other thread finished first.
    {
        .name = "Ascalon Academy",
        .objective = "Take Sir Tydus' trial at the Ascalon Academy",
        .offerText = "You've done enough for Ascalon to be asked properly. Take the "
                     "trial at the Academy and you'll wear the Guards' colours. "
                     "There's no coming back from it - once you're sworn, you're ours.",
        .giverName = "Sir Tydus",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Charr",
        .rewardXP = 800,
        .rewardGold = 400,
        .rewardSkill = -1,
        .state = QUEST_AVAILABLE,
        .prereq = 3, // after The Path to Glory - the whole city thread
        .endsCampaign = true,
    },

    {
        .name = "Hides for the Watch",
        .objective = "Bring 4 Charr Carvings to Piken Square",
        .offerText = "Winter comes early this far north. Four Charr hides for the "
                     "wall-watchers and I'll see you paid.",
        .giverName = "Warmaster Riga",
        .type = QTYPE_COLLECT,
        .killsRequired = 4,
        .collectMaterial = "Charr Carving",
        .rewardXP = 400,
        .rewardGold = 300,
        .rewardSkill = -1,
        .state = QUEST_AVAILABLE,
        .prereq = 6,
    },
};

static bool PrereqMet(int index) {
    int p = g_quests[index].prereq;
    return p < 0 || g_quests[p].state == QUEST_DONE;
}

static bool GiverMatches(int index, const char *giverName) {
    return !giverName || strcmp(g_quests[index].giverName, giverName) == 0;
}

int Quests_OfferableIndexFor(const char *giverName) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        if (g_quests[i].state == QUEST_AVAILABLE && PrereqMet(i) &&
            GiverMatches(i, giverName)) return i;
    }
    return -1;
}

int Quests_ReadyToTurnInIndexFor(const char *giverName) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        if (g_quests[i].state == QUEST_READY_TO_TURN_IN &&
            GiverMatches(i, giverName)) return i;
    }
    return -1;
}

int Quests_ResolveRewardSkill(const Quest *q, const Entity *player) {
    if (!q || q->rewardSkill == -1) return -1;
    if (q->rewardSkill == QUEST_REWARD_ANY_SKILL) {
        if (!player) return -1;
        return Skillbook_PickReward(player->primaryProfession, player->secondaryProfession);
    }
    return q->rewardSkill;
}

bool Quests_IsDoneByName(const char *name) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        if (strcmp(g_quests[i].name, name) == 0) return g_quests[i].state == QUEST_DONE;
    }
    return false;
}

bool Quests_GiverHasAttentionFor(const char *giverName) {
    return Quests_OfferableIndexFor(giverName) >= 0 ||
           Quests_ReadyToTurnInIndexFor(giverName) >= 0;
}

void Quests_Reset(void) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        g_quests[i].state = QUEST_AVAILABLE;
        g_quests[i].kills = 0;
    }
}

void Quests_Accept(int index) {
    if (index < 0 || index >= QUEST_COUNT) return;
    if (g_quests[index].state == QUEST_AVAILABLE && PrereqMet(index)) {
        g_quests[index].state = QUEST_ACTIVE;
        Save_Write(); // the quest log is part of the saved character
    }
}

bool Quests_TurnIn(Entity *player, int index) {
    if (index < 0 || index >= QUEST_COUNT) return false;
    Quest *q = &g_quests[index];
    if (q->state != QUEST_READY_TO_TURN_IN) return false;

    // An item reward needs a bag slot - GW1 makes you clear one first.
    if (q->rewardItem && g_inventoryCount >= MAX_INVENTORY) return false;

    // Collect quests hand the goods over on turn-in.
    if (q->type == QTYPE_COLLECT) {
        if (!Items_ConsumeMaterial(q->collectMaterial, q->killsRequired)) {
            q->state = QUEST_ACTIVE; // sold them since the check? back to work
            return false;
        }
    }

    if (q->rewardItem) Items_AddToInventory(*q->rewardItem);
    q->state = QUEST_DONE;

    // The skill is the headline reward - announce it, because a new
    // skill changes what builds are open to you far more than the gold.
    int taught = Quests_ResolveRewardSkill(q, player);
    if (taught >= 0 && Skillbook_Unlock(taught)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Skill learned:  %s", g_skillDB[taught].name);
        UI_Notify(msg);
    }

    Audio_Play(SFX_QUEST_DONE);
    Progression_AwardXP(player, q->rewardXP); // Reforged's XP bonus is applied there
    g_gold += Character_IsReforged() ? (q->rewardGold * 105 / 100) : q->rewardGold;

    // The end of pre-Searing. main.c watches this flag and hands over to
    // the Searing; the flag is saved, so the character stays finished.
    if (q->endsCampaign) World_SetSearingHappened(true);

    Save_Write();
    return true;
}

void Quests_NotifyMonsterKill(const Entity *victim) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];
        if (q->type != QTYPE_KILL || q->state != QUEST_ACTIVE) continue;
        // Two kill quests can be active at once; each counts only its
        // own targets.
        if (q->targetName && (!victim || !strstr(victim->name, q->targetName))) continue;
        q->kills++;
        if (q->kills >= q->killsRequired) q->state = QUEST_READY_TO_TURN_IN;
    }
}

void Quests_Update(Entity *player) {
    if (!player || !player->alive) return;

    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];

        // Collect progress tracks the bag live, both ways: selling the
        // hides after "objective met" drops the quest back to active.
        if (q->type == QTYPE_COLLECT &&
            (q->state == QUEST_ACTIVE || q->state == QUEST_READY_TO_TURN_IN)) {
            bool have = Items_CountMaterial(q->collectMaterial) >= q->killsRequired;
            q->state = have ? QUEST_READY_TO_TURN_IN : QUEST_ACTIVE;
        }

        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        if (q->targetZone != World_GetZoneId()) continue; // marker lives elsewhere
        float dx = player->pos.x - q->targetPos.x;
        float dy = player->pos.y - q->targetPos.y;
        if (sqrtf(dx * dx + dy * dy) <= q->reachRadius) {
            q->state = QUEST_READY_TO_TURN_IN;
        }
    }
}

void Quests_DrawTracker(int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);

    int font = (int)(11 * scale);
    int pad = (int)(8 * scale);
    int w = (int)(220 * scale);
    // Top-left, which is where GW1 puts the quest log's tracked entries.
    // The right side belongs to the compass and the party window.
    (void)screenWidth;
    int x = (int)(14 * scale);
    int y = (int)(28 * scale);

    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];
        if (q->state != QUEST_ACTIVE && q->state != QUEST_READY_TO_TURN_IN) continue;

        char line2[80];
        if (q->state == QUEST_READY_TO_TURN_IN) {
            snprintf(line2, sizeof(line2), "Return to %s", q->giverName);
        } else if (q->type == QTYPE_KILL) {
            snprintf(line2, sizeof(line2), "%s (%d/%d)", q->objective, q->kills, q->killsRequired);
        } else if (q->type == QTYPE_COLLECT) {
            snprintf(line2, sizeof(line2), "%s (%d/%d)", q->objective,
                     Items_CountMaterial(q->collectMaterial), q->killsRequired);
        } else {
            snprintf(line2, sizeof(line2), "%s", q->objective);
        }

        int h = pad * 2 + font * 2 + 6;
        UIHit_Claim((Rectangle){ (float)x, (float)y, (float)w, (float)h });
        UI_ThemePanel((Rectangle){ (float)x, (float)y, (float)w, (float)h }, scale, 0);
        UIText(q->name, x + pad, y + pad, font, GOLD);
        UIText(line2, x + pad, y + pad + font + 4, font,
               q->state == QUEST_READY_TO_TURN_IN ? (Color){ 130, 220, 130, 255 } : LIGHTGRAY);
        y += h + (int)(6 * scale);
    }
}
