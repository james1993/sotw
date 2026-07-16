#include "save.h"
#include "entity.h"
#include "items.h"
#include "quests.h"
#include "world.h"
#include "skill.h"
#include "attributes.h"
#include "progression.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYER_INDEX 0
#define SAVE_VERSION 1

static bool g_enabled = false;
static char g_saveDir[448];
static char g_savePath[512];

// Resolve the platform save directory once. Env-var based rather than
// hardcoded paths - the same portability rule as the font loading.
static void ResolvePaths(void) {
    if (g_savePath[0]) return;

    const char *base = NULL;
    static char fallback[400];
#if defined(_WIN32)
    base = getenv("APPDATA");
    if (!base || !base[0]) {
        const char *home = getenv("USERPROFILE");
        if (home && home[0]) {
            snprintf(fallback, sizeof(fallback), "%s", home);
            base = fallback;
        }
    }
#else
    base = getenv("XDG_DATA_HOME");
    if (!base || !base[0]) {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            snprintf(fallback, sizeof(fallback), "%s/.local/share", home);
            base = fallback;
        }
    }
#endif
    // Last resort (sandboxed/odd environments): next to the executable.
    if (!base || !base[0]) base = GetApplicationDirectory();

    snprintf(g_saveDir, sizeof(g_saveDir), "%s/sotw-demake", base);
    snprintf(g_savePath, sizeof(g_savePath), "%s/save.txt", g_saveDir);
}

const char *Save_GetPath(void) {
    ResolvePaths();
    return g_savePath;
}

bool Save_Exists(void) {
    ResolvePaths();
    return FileExists(g_savePath);
}

void Save_Enable(void) {
    g_enabled = true;
}

bool Save_Write(void) {
    if (!g_enabled) return false;
    ResolvePaths();

    Entity *p = Entity_Get(PLAYER_INDEX);
    if (!p) return false;

    MakeDirectory(g_saveDir); // no-op when it already exists
    FILE *f = fopen(g_savePath, "w");
    if (!f) {
        TraceLog(LOG_WARNING, "SAVE: cannot write %s", g_savePath);
        return false;
    }

    fprintf(f, "version=%d\n", SAVE_VERSION);
    fprintf(f, "outpost=%d\n", (int)World_GetLastOutpostId());
    fprintf(f, "level=%d\n", p->level);
    fprintf(f, "xp=%d\n", p->xp);
    fprintf(f, "attrPoints=%d\n", p->attributePoints);
    for (int i = 0; i < ATTR_COUNT; i++) {
        fprintf(f, "attr%d=%d\n", i, p->attributeRank[i]);
    }
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        fprintf(f, "skill%d=%d\n", i, p->skillBar[i]);
    }
    fprintf(f, "gold=%d\n", g_gold);
    fprintf(f, "thomHired=%d\n", World_IsThomHired() ? 1 : 0);
    for (int i = 0; i < QUEST_COUNT; i++) {
        fprintf(f, "quest%d=%d,%d\n", i, (int)g_quests[i].state, g_quests[i].kills);
    }
    fprintf(f, "equipWeapon=%d\n", g_equippedWeapon);
    fprintf(f, "equipArmor=%d\n", g_equippedArmor);
    fprintf(f, "itemCount=%d\n", g_inventoryCount);
    for (int i = 0; i < g_inventoryCount; i++) {
        const Item *it = &g_inventory[i];
        // Name goes last so it can contain spaces (never '|').
        fprintf(f, "item%d=%d|%d|%d|%.3f|%.3f|%d|%d|%s\n", i,
                (int)it->kind, it->dmgMin, it->dmgMax,
                it->range, it->attackInterval, it->armor,
                (it->count > 0 ? it->count : 1), it->name);
    }

    fclose(f);
    return true;
}

static int ClampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool Save_LoadAndApply(void) {
    ResolvePaths();
    FILE *f = fopen(g_savePath, "r");
    if (!f) return false;

    Entity *p = Entity_Get(PLAYER_INDEX);
    if (!p) {
        fclose(f);
        return false;
    }

    // The save's inventory replaces the starting kit World_Init handed out.
    g_inventoryCount = 0;
    g_equippedWeapon = -1;
    g_equippedArmor = -1;
    g_gold = 0;

    int outpost = (int)ZONE_ASHFORD_CAMP;
    bool thomHired = false;
    int equipWeapon = -1, equipArmor = -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';

        int idx;
        if (strcmp(key, "outpost") == 0) {
            outpost = atoi(val);
        } else if (strcmp(key, "level") == 0) {
            p->level = ClampInt(atoi(val), 1, MAX_LEVEL);
        } else if (strcmp(key, "xp") == 0) {
            p->xp = ClampInt(atoi(val), 0, 1000000);
        } else if (strcmp(key, "attrPoints") == 0) {
            p->attributePoints = ClampInt(atoi(val), 0, 300);
        } else if (sscanf(key, "attr%d", &idx) == 1 && idx >= 0 && idx < ATTR_COUNT) {
            p->attributeRank[idx] = ClampInt(atoi(val), 0, ATTRIBUTE_RANK_CAP);
        } else if (sscanf(key, "skill%d", &idx) == 1 && idx >= 0 && idx < SKILL_BAR_SIZE) {
            int s = atoi(val);
            p->skillBar[idx] = (s >= -1 && s < g_skillCount) ? s : -1;
        } else if (strcmp(key, "gold") == 0) {
            g_gold = ClampInt(atoi(val), 0, 1000000000);
        } else if (strcmp(key, "thomHired") == 0) {
            thomHired = atoi(val) != 0;
        } else if (sscanf(key, "quest%d", &idx) == 1 && idx >= 0 && idx < QUEST_COUNT) {
            int state = 0, kills = 0;
            sscanf(val, "%d,%d", &state, &kills);
            g_quests[idx].state = (QuestState)ClampInt(state, 0, (int)QUEST_DONE);
            g_quests[idx].kills = ClampInt(kills, 0, 999);
        } else if (strcmp(key, "equipWeapon") == 0) {
            equipWeapon = atoi(val);
        } else if (strcmp(key, "equipArmor") == 0) {
            equipArmor = atoi(val);
        } else if (sscanf(key, "item%d", &idx) == 1) {
            Item it;
            memset(&it, 0, sizeof(it));
            int kind = 0, consumed = 0;
            // New format carries a stack count before the name; old
            // saves lack it and fall through to the second parse.
            if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
                       &it.range, &it.attackInterval, &it.armor, &it.count, &consumed) >= 7
                && consumed > 0) {
                it.kind = (ItemKind)ClampInt(kind, 0, (int)ITEM_MATERIAL);
                it.count = ClampInt(it.count, 1, 9999);
                strncpy(it.name, val + consumed, sizeof(it.name) - 1);
                Items_AddToInventory(it);
            } else if (sscanf(val, "%d|%d|%d|%f|%f|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
                       &it.range, &it.attackInterval, &it.armor, &consumed) >= 6
                && consumed > 0) {
                it.kind = (ItemKind)ClampInt(kind, 0, (int)ITEM_MATERIAL);
                it.count = 1;
                strncpy(it.name, val + consumed, sizeof(it.name) - 1);
                Items_AddToInventory(it);
            }
        }
        // "version" / "itemCount" are informational; unknown keys are
        // skipped so older builds tolerate newer saves.
    }
    fclose(f);

    // Rebuild what's derived rather than stored: HP scales with level
    // (+20/level, GW1's rule); equipment re-applies its combat stats
    // through the same path the inventory UI uses.
    p->baseMaxHp = 100 + 20 * (p->level - 1);
    Entity_RecomputePenalizedStats(p);
    p->hp = p->maxHp;
    p->energy = p->maxEnergy;

    World_SetThomHired(thomHired);
    if (equipWeapon >= 0) Items_EquipWeapon(p, equipWeapon);
    if (equipArmor >= 0) Items_EquipArmor(p, equipArmor);

    // "Log back in" at the last outpost: respawns the party (including
    // a hired Thom) and fully restores everyone, GW1-style.
    World_RestoreToOutpost((ZoneId)ClampInt(outpost, 0, ZONE_COUNT - 1));

    TraceLog(LOG_INFO, "SAVE: loaded %s (level %d, %d gold)", g_savePath, p->level, g_gold);
    return true;
}
