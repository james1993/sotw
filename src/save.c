#include "save.h"
#include "entity.h"
#include "items.h"
#include "quests.h"
#include "world.h"
#include "skill.h"
#include "character.h"
#include "skillbook.h"
#include "attributes.h"
#include "progression.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYER_INDEX 0
// Version 2 added the remaining three Prophecies professions. Doing so
// renumbered the Profession, AttributeKind and SkillId enums, and this
// file stores all three BY NUMBER (primary=, attr%d=, skill%d=). A
// version-1 file read as version 2 would silently turn a Monk into a
// Ranger with points in the wrong lines, which is worse than refusing
// it, so old saves are declined and the character is remade.
#define SAVE_VERSION 2

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
    // The created character. Name goes last on its line so it may
    // contain spaces.
    fprintf(f, "primary=%d\n", (int)g_character.primary);
    fprintf(f, "secondary=%d\n", g_character.secondary);
    fprintf(f, "look=%d,%d,%d,%d\n", g_character.sex, g_character.skinTone,
            g_character.hairColor, g_character.hairStyle);
    fprintf(f, "charName=%s\n", g_character.name);
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
    // Which skills the character owns - the collection is the character,
    // as much as their level or their gear.
    {
        char mask[MAX_SKILLS + 1];
        Skillbook_WriteMask(mask, sizeof(mask));
        fprintf(f, "skillsKnown=%s\n", mask);
    }
    fprintf(f, "skillPoints=%d\n", g_skillPoints);
    fprintf(f, "skillsBought=%d\n", g_skillsPurchased);
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
        fprintf(f, "item%d=%d|%d|%d|%.3f|%.3f|%d|%d|%d|%s\n", i,
                (int)it->kind, it->dmgMin, it->dmgMax,
                it->range, it->attackInterval, it->armor,
                (it->count > 0 ? it->count : 1),
                it->unidentified ? 1 : 0, it->name);
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

    // Check the version before touching anything. The parse loop below
    // overwrites live state as it goes, so a file we're going to reject
    // has to be rejected while the world is still intact.
    {
        char probe[512];
        int fileVersion = 0;
        while (fgets(probe, sizeof(probe), f)) {
            if (sscanf(probe, "version=%d", &fileVersion) == 1) break;
        }
        if (fileVersion < SAVE_VERSION) {
            fclose(f);
            TraceLog(LOG_WARNING,
                     "SAVE: %s is version %d, this build needs %d - starting fresh",
                     g_savePath, fileVersion, SAVE_VERSION);
            return false;
        }
        rewind(f);
    }

    // The save's inventory replaces the starting kit World_Init handed out.
    g_inventoryCount = 0;
    g_equippedWeapon = -1;
    g_equippedArmor = -1;
    g_gold = 0;

    int outpost = (int)ZONE_ASHFORD_CAMP;
    bool thomHired = false;
    int equipWeapon = -1, equipArmor = -1;
    bool sawSkillbook = false;

    // Start from a clean book; the file fills it in below.
    Skillbook_Reset();

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line;
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';

        int idx;
        if (strcmp(key, "primary") == 0) {
            int v = atoi(val);
            g_character.primary = (Profession)ClampInt(v, 0, PROF_COUNT - 1);
        } else if (strcmp(key, "secondary") == 0) {
            int v = atoi(val);
            g_character.secondary = (v < 0) ? PROF_NONE : ClampInt(v, 0, PROF_COUNT - 1);
        } else if (strcmp(key, "look") == 0) {
            int sex = 1, skin = 1, hair = 1, style = 0;
            sscanf(val, "%d,%d,%d,%d", &sex, &skin, &hair, &style);
            g_character.sex = ClampInt(sex, 0, 1);
            g_character.skinTone = ClampInt(skin, 0, SKIN_TONE_COUNT - 1);
            g_character.hairColor = ClampInt(hair, 0, HAIR_COLOR_COUNT - 1);
            g_character.hairStyle = ClampInt(style, 0, HAIR_STYLE_COUNT - 1);
        } else if (strcmp(key, "charName") == 0) {
            snprintf(g_character.name, sizeof(g_character.name), "%s", val);
        } else if (strcmp(key, "outpost") == 0) {
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
        } else if (strcmp(key, "skillsKnown") == 0) {
            Skillbook_ReadMask(val);
            sawSkillbook = true;
        } else if (strcmp(key, "skillPoints") == 0) {
            g_skillPoints = ClampInt(atoi(val), 0, 999);
        } else if (strcmp(key, "skillsBought") == 0) {
            g_skillsPurchased = ClampInt(atoi(val), 0, 999);
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
            int kind = 0, unid = 0, consumed = 0;
            // Formats, newest first: count+unidentified before the
            // name; count only; neither. Older saves parse fine.
            if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
                       &it.range, &it.attackInterval, &it.armor, &it.count, &unid,
                       &consumed) >= 8 && consumed > 0) {
                it.unidentified = (unid != 0);
            } else if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
                       &it.range, &it.attackInterval, &it.armor, &it.count,
                       &consumed) >= 7 && consumed > 0) {
                // count-only format
            } else if (sscanf(val, "%d|%d|%d|%f|%f|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
                       &it.range, &it.attackInterval, &it.armor, &consumed) >= 6
                && consumed > 0) {
                it.count = 1;
            } else {
                continue;
            }
            it.kind = (ItemKind)ClampInt(kind, 0, (int)ITEM_KIT_ID);
            it.count = ClampInt(it.count, 1, 9999);
            strncpy(it.name, val + consumed, sizeof(it.name) - 1);
            Items_AddToInventory(it);
        }
        // "version" / "itemCount" are informational; unknown keys are
        // skipped so older builds tolerate newer saves.
    }
    fclose(f);

    // World_Init built the player from g_character BEFORE this file was
    // parsed, so the entity is currently wearing the default character.
    // Re-apply what the save actually says now that we know it.
    Character_FormatTitle(&g_character, p->name, sizeof(p->name));
    p->primaryProfession = g_character.primary;
    p->secondaryProfession = (g_character.secondary == PROF_NONE)
                             ? g_character.primary
                             : (Profession)g_character.secondary;
    p->sex = g_character.sex;
    p->skinTone = g_character.skinTone;
    p->hairColor = g_character.hairColor;
    p->hairStyle = g_character.hairStyle;

    // Rebuild what's derived rather than stored - health from level,
    // energy from Energy Storage - through the same call World_Init
    // uses, so a loaded character and a new one can't disagree.
    Entity_RecomputeAttributeStats(p);
    p->hp = p->maxHp;
    p->energy = p->maxEnergy;

    // Saves written before skills had to be earned have no skillsKnown
    // line. Rather than stripping those characters back to one skill,
    // grant whatever their bar was already carrying - the bar IS the
    // evidence of what they owned.
    if (!sawSkillbook) {
        for (int i = 0; i < SKILL_BAR_SIZE; i++) {
            if (p->skillBar[i] >= 0) Skillbook_Unlock(p->skillBar[i]);
        }
    } else {
        // Never leave a skill on the bar the book doesn't back - a hand
        // edited or truncated mask would otherwise hand out free skills.
        for (int i = 0; i < SKILL_BAR_SIZE; i++) {
            if (p->skillBar[i] >= 0 && !Skillbook_IsUnlocked(p->skillBar[i])) {
                p->skillBar[i] = -1;
            }
        }
    }

    World_SetThomHired(thomHired);
    if (equipWeapon >= 0) Items_EquipWeapon(p, equipWeapon);
    if (equipArmor >= 0) Items_EquipArmor(p, equipArmor);

    // "Log back in" at the last outpost: respawns the party (including
    // a hired Thom) and fully restores everyone, GW1-style.
    World_RestoreToOutpost((ZoneId)ClampInt(outpost, 0, ZONE_COUNT - 1));

    TraceLog(LOG_INFO, "SAVE: loaded %s (level %d, %d gold)", g_savePath, p->level, g_gold);
    return true;
}
