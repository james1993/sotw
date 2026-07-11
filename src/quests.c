#include "quests.h"
#include "entity.h"
#include "progression.h"
#include "items.h"
#include "ui_font.h"
#include <stdio.h>

Quest g_quest = {
    .name = "Charr at the Gate",
    .objective = "Slay the Charr in Ashford Plains",
    .killsRequired = 3,
    .kills = 0,
    .rewardXP = 250,
    .rewardGold = 100,
    .state = QUEST_AVAILABLE,
};

void Quests_Accept(void) {
    if (g_quest.state == QUEST_AVAILABLE) {
        g_quest.state = QUEST_ACTIVE;
    }
}

void Quests_NotifyMonsterKill(void) {
    if (g_quest.state != QUEST_ACTIVE) return;
    g_quest.kills++;
    if (g_quest.kills >= g_quest.killsRequired) {
        g_quest.state = QUEST_READY_TO_TURN_IN;
    }
}

void Quests_TurnIn(Entity *player) {
    if (g_quest.state != QUEST_READY_TO_TURN_IN) return;
    g_quest.state = QUEST_DONE;
    Progression_AwardXP(player, g_quest.rewardXP);
    g_gold += g_quest.rewardGold;
}

void Quests_DrawTracker(int screenWidth, int screenHeight) {
    if (g_quest.state == QUEST_AVAILABLE || g_quest.state == QUEST_DONE) return;

    float scale = (float)screenHeight / 800.0f;
    if (scale < 0.85f) scale = 0.85f;
    if (scale > 5.0f) scale = 5.0f;

    int font = (int)(11 * scale);
    int pad = (int)(8 * scale);
    int w = (int)(220 * scale);
    // Sits below the party panel on the right edge, where GW1 keeps its
    // quest log.
    int x = screenWidth - w - (int)(14 * scale);
    int y = (int)(280 * scale);

    char line2[64];
    const char *line1 = g_quest.name;
    if (g_quest.state == QUEST_READY_TO_TURN_IN) {
        snprintf(line2, sizeof(line2), "Return to Captain Osric");
    } else {
        snprintf(line2, sizeof(line2), "%s (%d/%d)", g_quest.objective,
                 g_quest.kills, g_quest.killsRequired);
    }

    int h = pad * 2 + font * 2 + 6;
    DrawRectangle(x, y, w, h, (Color){ 20, 22, 30, 210 });
    DrawRectangleLines(x, y, w, h, (Color){ 120, 120, 140, 255 });
    UIText(line1, x + pad, y + pad, font, GOLD);
    UIText(line2, x + pad, y + pad + font + 4, font,
           g_quest.state == QUEST_READY_TO_TURN_IN ? (Color){ 130, 220, 130, 255 } : LIGHTGRAY);
}
