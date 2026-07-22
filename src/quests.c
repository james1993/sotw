#include "quests.h"
#include "entity.h"
#include "progression.h"
#include "items.h"
#include "world.h"
#include "ui_party.h"
#include "ui_compass.h"
#include "ui_hit.h"
#include "ui_font.h"
#include "ui_theme.h"
#include "save.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Item rewards, GW1-style "take this for your trouble" gear. Static so
// quests can point at them; turn-in copies them into the inventory.
static const Item g_rewardWarhammer = { ITEM_WEAPON, "Warmaster's Hammer", 22, 38, 30.0f, 1.75f, 0, 1, false };
static const Item g_rewardIdKit = { ITEM_KIT_ID, "Identification Kit", 0, 0, 0, 0, 0, 25, false };

Quest g_quests[QUEST_COUNT] = {
    // --- Captain Osric's line (Ashford Camp) ---
    {
        .name = "Charr at the Gate",
        .objective = "Slay the Charr in Ashford Plains",
        .offerText = "The Charr prowl our plains. Will you thin them out?",
        .giverName = "Captain Osric",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Charr", // only Charr kills advance this quest
        .rewardXP = 250,
        .rewardGold = 100,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "Scout the Eastern Ridge",
        .objective = "Reach the ridge marker east of the plains",
        .offerText = "With the Charr culled, we need eyes on the eastern ridge.",
        .giverName = "Captain Osric",
        .type = QTYPE_REACH,
        .targetPos = { 1340, 40 }, // past the prowler's beat - you'll meet it on the road
        .reachRadius = 70.0f,
        .targetZone = ZONE_ASHFORD_PLAINS,
        .rewardXP = 300,
        .rewardGold = 150,
        .state = QUEST_AVAILABLE,
        .prereq = 0, // opens up after Charr at the Gate, GW1 chain-style
    },

    // --- Warmaster Grast's book (Piken Watch, past the foothills) ---
    {
        .name = "Silence the Shamans",
        .objective = "Slay Charr Shamans in the foothills",
        .offerText = "Their shamans burn my scouts from across the field. Silence them.",
        .giverName = "Warmaster Grast",
        .type = QTYPE_KILL,
        .killsRequired = 2,
        .targetName = "Charr Shaman",
        .rewardXP = 400,
        .rewardGold = 150,
        .rewardItem = &g_rewardWarhammer,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "Clear the Gullies",
        .objective = "Slay Devourers in the foothills",
        .offerText = "Devourers nest in the gullies south of the road. Burn them out.",
        .giverName = "Warmaster Grast",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .targetName = "Devourer",
        .rewardXP = 350,
        .rewardGold = 120,
        .rewardItem = &g_rewardIdKit,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "Hides for the Watch",
        .objective = "Bring 4 Charr Hides to Piken Watch",
        .offerText = "Winter comes early up here. Bring me 4 Charr Hides for the wall-watchers.",
        .giverName = "Warmaster Grast",
        .type = QTYPE_COLLECT,
        .killsRequired = 4, // items needed
        .collectMaterial = "Charr Hide",
        .rewardXP = 300,
        .rewardGold = 200,
        .state = QUEST_AVAILABLE,
        .prereq = 2, // the warmaster trusts you after the shaman work
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
    Progression_AwardXP(player, q->rewardXP);
    g_gold += q->rewardGold;
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
    // Right side under the compass, GW1's spot for mission goals. In
    // outposts the party window is also on the right, so stack below it.
    int x = screenWidth - w - (int)(14 * scale);
    int y = (int)UI_CompassBottom(screenHeight) + (int)(12 * scale);
    if (World_GetMode() == MODE_OUTPOST) {
        int below = (int)UI_PartyPanelBottom() + (int)(12 * scale);
        if (below > y) y = below;
    }

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
