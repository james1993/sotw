#include "quests.h"
#include "entity.h"
#include "progression.h"
#include "items.h"
#include "world.h"
#include "ui_party.h"
#include "ui_font.h"
#include <math.h>
#include <stdio.h>

Quest g_quests[QUEST_COUNT] = {
    {
        .name = "Charr at the Gate",
        .objective = "Slay the Charr in Ashford Plains",
        .type = QTYPE_KILL,
        .killsRequired = 3,
        .rewardXP = 250,
        .rewardGold = 100,
        .state = QUEST_AVAILABLE,
        .prereq = -1,
    },
    {
        .name = "Scout the Eastern Ridge",
        .objective = "Reach the ridge marker east of the plains",
        .type = QTYPE_REACH,
        .targetPos = { 900, 0 },
        .reachRadius = 70.0f,
        .rewardXP = 300,
        .rewardGold = 150,
        .state = QUEST_AVAILABLE,
        .prereq = 0, // opens up after Charr at the Gate, GW1 chain-style
    },
};

static bool PrereqMet(int index) {
    int p = g_quests[index].prereq;
    return p < 0 || g_quests[p].state == QUEST_DONE;
}

int Quests_OfferableIndex(void) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        if (g_quests[i].state == QUEST_AVAILABLE && PrereqMet(i)) return i;
    }
    return -1;
}

int Quests_ReadyToTurnInIndex(void) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        if (g_quests[i].state == QUEST_READY_TO_TURN_IN) return i;
    }
    return -1;
}

bool Quests_GiverHasAttention(void) {
    return Quests_OfferableIndex() >= 0 || Quests_ReadyToTurnInIndex() >= 0;
}

void Quests_Accept(int index) {
    if (index < 0 || index >= QUEST_COUNT) return;
    if (g_quests[index].state == QUEST_AVAILABLE && PrereqMet(index)) {
        g_quests[index].state = QUEST_ACTIVE;
    }
}

void Quests_TurnIn(Entity *player, int index) {
    if (index < 0 || index >= QUEST_COUNT) return;
    if (g_quests[index].state != QUEST_READY_TO_TURN_IN) return;
    g_quests[index].state = QUEST_DONE;
    Progression_AwardXP(player, g_quests[index].rewardXP);
    g_gold += g_quests[index].rewardGold;
}

void Quests_NotifyMonsterKill(void) {
    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];
        if (q->type != QTYPE_KILL || q->state != QUEST_ACTIVE) continue;
        q->kills++;
        if (q->kills >= q->killsRequired) q->state = QUEST_READY_TO_TURN_IN;
    }
}

void Quests_Update(Entity *player) {
    if (!player || !player->alive) return;
    if (World_GetMode() != MODE_EXPLORABLE) return;

    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];
        if (q->type != QTYPE_REACH || q->state != QUEST_ACTIVE) continue;
        float dx = player->pos.x - q->targetPos.x;
        float dy = player->pos.y - q->targetPos.y;
        if (sqrtf(dx * dx + dy * dy) <= q->reachRadius) {
            q->state = QUEST_READY_TO_TURN_IN;
        }
    }
}

void Quests_DrawTracker(int screenWidth, int screenHeight) {
    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;

    int font = (int)(11 * scale);
    int pad = (int)(8 * scale);
    int w = (int)(220 * scale);
    // Upper-right, GW1's spot for mission goals. In outposts the party
    // window also lives on the right, so stack below it there.
    int x = screenWidth - w - (int)(14 * scale);
    int y = (int)(80 * scale);
    if (World_GetMode() == MODE_OUTPOST) {
        int below = (int)UI_PartyPanelBottom() + (int)(12 * scale);
        if (below > y) y = below;
    }

    for (int i = 0; i < QUEST_COUNT; i++) {
        Quest *q = &g_quests[i];
        if (q->state != QUEST_ACTIVE && q->state != QUEST_READY_TO_TURN_IN) continue;

        char line2[80];
        if (q->state == QUEST_READY_TO_TURN_IN) {
            snprintf(line2, sizeof(line2), "Return to Captain Osric");
        } else if (q->type == QTYPE_KILL) {
            snprintf(line2, sizeof(line2), "%s (%d/%d)", q->objective, q->kills, q->killsRequired);
        } else {
            snprintf(line2, sizeof(line2), "%s", q->objective);
        }

        int h = pad * 2 + font * 2 + 6;
        DrawRectangle(x, y, w, h, (Color){ 20, 22, 30, 210 });
        DrawRectangleLines(x, y, w, h, (Color){ 120, 120, 140, 255 });
        UIText(q->name, x + pad, y + pad, font, GOLD);
        UIText(line2, x + pad, y + pad + font + 4, font,
               q->state == QUEST_READY_TO_TURN_IN ? (Color){ 130, 220, 130, 255 } : LIGHTGRAY);
        y += h + (int)(6 * scale);
    }
}
