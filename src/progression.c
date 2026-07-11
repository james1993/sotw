#include "progression.h"
#include "entity.h"

int Progression_XPToNext(int level) {
    return 1400 + 600 * level;
}

static int AttrPointsForLevel(int newLevel) {
    if (newLevel <= 10) return 5;
    if (newLevel <= 15) return 10;
    return 15;
}

void Progression_AwardXP(Entity *player, int amount) {
    if (!player || !player->alive || player->level >= MAX_LEVEL) return;

    player->xp += amount;
    while (player->level < MAX_LEVEL && player->xp >= Progression_XPToNext(player->level)) {
        player->xp -= Progression_XPToNext(player->level);
        player->level++;
        player->maxHp += 20; // GW1's real per-level health gain
        player->hp += 20;
        player->attributePoints += AttrPointsForLevel(player->level);
    }
    if (player->level >= MAX_LEVEL) player->xp = 0;
}

void Progression_AwardKillXP(Entity *player, int monsterLevel) {
    if (!player) return;
    int diff = monsterLevel - player->level;
    int xp = 100 + 24 * diff;
    if (xp < 16) xp = 16;
    if (xp > 400) xp = 400;
    Progression_AwardXP(player, xp);
}
