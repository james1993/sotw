#include "save.h"
#include "entity.h"
#include "items.h"
#include "quests.h"
#include "titles.h"
#include "builds.h"
#include "world.h"
#include "skill.h"
#include "character.h"
#include "skillbook.h"
#include "attributes.h"
#include "progression.h"
#include "audio.h"
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
// -1 until the selection screen picks one. Saving with no slot chosen
// would have to invent a file, and inventing a character's home is
// exactly the kind of guess that loses someone's save.
static int g_slot = -1;

static void SlotPath(int slot, char *out, size_t outSize) {
    if (slot < 0) snprintf(out, outSize, "%s/char0.txt", g_saveDir);
    else          snprintf(out, outSize, "%s/char%d.txt", g_saveDir, slot);
}

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
    SlotPath(g_slot, g_savePath, sizeof(g_savePath));

    // One-time migration: a save written before slots existed becomes
    // the first character rather than being orphaned.
    char legacy[512];
    snprintf(legacy, sizeof(legacy), "%s/save.txt", g_saveDir);
    if (FileExists(legacy)) {
        char first[512];
        snprintf(first, sizeof(first), "%s/char0.txt", g_saveDir);
        if (!FileExists(first)) {
            rename(legacy, first);
            TraceLog(LOG_INFO, "SAVE: migrated save.txt into slot 1");
        }
    }
}

void Save_SelectSlot(int slot) {
    g_slot = (slot >= 0 && slot < SAVE_SLOT_COUNT) ? slot : -1;
    // Force the next ResolvePaths to rebuild the path for the new slot.
    g_savePath[0] = '\0';
    ResolvePaths();
}

int Save_SelectedSlot(void) {
    return g_slot;
}

bool Save_DeleteSlot(int slot) {
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return false;
    ResolvePaths();
    char path[512];
    SlotPath(slot, path, sizeof(path));
    if (!FileExists(path)) return false;
    return remove(path) == 0;
}

// Reads only the handful of keys the roster needs. Deliberately its own
// tiny parser rather than a partial Save_LoadAndApply: loading a whole
// character to draw one row would mean six characters loaded to draw
// the screen, each one stomping the live world.
bool Save_ReadSlotInfo(int slot, SaveSlotInfo *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (slot < 0 || slot >= SAVE_SLOT_COUNT) return false;

    ResolvePaths();
    char path[512];
    SlotPath(slot, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return false;

    out->level = 1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n");
        if (nl) *nl = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;
        if (strcmp(key, "charName") == 0)        snprintf(out->name, sizeof(out->name), "%s", val);
        else if (strcmp(key, "primary") == 0)    out->primary = atoi(val);
        else if (strcmp(key, "secondary") == 0)  out->secondary = atoi(val);
        else if (strcmp(key, "level") == 0)      out->level = atoi(val);
        else if (strcmp(key, "reforged") == 0)   out->reforged = atoi(val) != 0;
        else if (strcmp(key, "searing") == 0)    out->searingSurvived = atoi(val) != 0;
    }
    fclose(f);
    out->used = true;
    if (!out->name[0]) snprintf(out->name, sizeof(out->name), "Ascalonian");
    return true;
}

const char *Save_GetPath(void) {
    ResolvePaths();
    return g_savePath;
}

bool Save_Exists(void) {
    if (g_slot < 0) return false;
    ResolvePaths();
    return FileExists(g_savePath);
}

void Save_Disable(void) {
    g_enabled = false;
}

void Save_Enable(void) {
    g_enabled = true;
}

bool Save_Write(void) {
    if (!g_enabled) return false;
    if (g_slot < 0) return false; // no character chosen - nothing to write to
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
    fprintf(f, "reforged=%d\n", g_character.reforged ? 1 : 0);
    // The campaign's end state. The roster reads it, so a character who
    // has been through the Searing can say so on the selection screen.
    fprintf(f, "searing=%d\n", World_SearingHappened() ? 1 : 0);
    fprintf(f, "titleWorn=%d\n", Titles_Displayed());

    // Skill templates: name, bar, spread. Written one line per slot so a
    // save from before templates existed simply has none.
    for (int i = 0; i < BUILD_SLOT_COUNT; i++) {
        const BuildTemplate *b = Builds_Slot(i);
        if (!b || !b->used) continue;
        fprintf(f, "build%d=%s|", i, b->name);
        for (int k = 0; k < SKILL_BAR_SIZE; k++) fprintf(f, "%d%s", b->skillBar[k], k + 1 < SKILL_BAR_SIZE ? "," : "");
        fprintf(f, "|");
        for (int a = 0; a < ATTR_COUNT; a++) fprintf(f, "%d%s", b->attributeRank[a], a + 1 < ATTR_COUNT ? "," : "");
        fprintf(f, "\n");
    }
    fprintf(f, "charName=%s\n", g_character.name);
    fprintf(f, "outpost=%d\n", (int)World_GetLastOutpostId());
    fprintf(f, "level=%d\n", p->level);
    fprintf(f, "xp=%d\n", p->xp);
    fprintf(f, "attrPoints=%d\n", p->attributePoints);
    // Armour dye: packed RGB, and a flag so "no dye" survives distinctly
    // from "dyed black".
    fprintf(f, "dye=%d,%d\n", p->dyed ? 1 : 0,
            ((int)p->dyeColor.r << 16) | ((int)p->dyeColor.g << 8) | (int)p->dyeColor.b);
    for (int i = 0; i < ATTR_COUNT; i++) {
        fprintf(f, "attr%d=%d\n", i, p->attributeRank[i]);
    }
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        fprintf(f, "skill%d=%d\n", i, p->skillBar[i]);
    }
    fprintf(f, "gold=%d\n", g_gold);
    // A setting rather than character state, but there is one save
    // file and one player, and a volume that resets every launch is
    // worse than a slightly impure schema.
    fprintf(f, "volume=%d\n", Audio_GetVolume());
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
    fprintf(f, "petCharmed=%d\n", World_IsPetCharmed() ? 1 : 0);
    fprintf(f, "petProgress=%d,%d\n", World_GetPetLevel(), World_GetPetXp());
    fprintf(f, "visitedOutposts=%u\n", World_VisitedMask());
    for (int i = 0; i < QUEST_COUNT; i++) {
        fprintf(f, "quest%d=%d,%d\n", i, (int)g_quests[i].state, g_quests[i].kills);
    }
    // One line for all seven worn slots plus the bag - a save written
    // before the split simply has none, and the loader falls back to the
    // old two keys.
    fprintf(f, "equipped=");
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        fprintf(f, "%d%s", g_equipped[i], i + 1 < EQUIP_SLOT_COUNT ? "," : "");
    }
    fprintf(f, "\n");
    fprintf(f, "itemCount=%d\n", g_inventoryCount);
    for (int i = 0; i < g_inventoryCount; i++) {
        const Item *it = &g_inventory[i];
        int dyePacked = ((int)it->dyeColor.r << 16) | ((int)it->dyeColor.g << 8) |
                        (int)it->dyeColor.b;
        // Name goes last so it can contain spaces (never '|'). The upgrade
        // fields (mods, rune, insignia, dye) trail the older columns so an
        // old loader parses the first eight and ignores the rest.
        fprintf(f, "item%d=%d|%d|%d|%.3f|%.3f|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s\n", i,
                (int)it->kind, it->dmgMin, it->dmgMax,
                it->range, it->attackInterval, it->armor,
                (it->count > 0 ? it->count : 1),
                it->unidentified ? 1 : 0,
                it->modHealth, it->modArmorPen, it->modLifesteal,
                it->runeAttr, it->runeAttrBonus, it->runeHealth, it->insigniaArmor,
                dyePacked, it->rarity, it->modEnergyGain, it->modArmor, it->modEnchantPct,
                it->name);
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
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) g_equipped[i] = -1;
    g_gold = 0;

    int outpost = (int)ZONE_ASCALON_CITY;
    bool thomHired = false;
    bool petCharmed = false;
    int petLevel = 0, petXp = 0;
    unsigned visitedOutposts = 0;
    int titleWorn = -1;
    bool searingHappened = false;
    int equipWeapon = -1, equipArmor = -1; // legacy single-slot keys
    int equipped[EQUIP_SLOT_COUNT];
    bool sawEquipped = false;
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) equipped[i] = -1;
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
        } else if (strcmp(key, "searing") == 0) {
            searingHappened = atoi(val) != 0;
        } else if (strcmp(key, "reforged") == 0) {
            g_character.reforged = (atoi(val) != 0);
        } else if (strcmp(key, "charName") == 0) {
            snprintf(g_character.name, sizeof(g_character.name), "%s", val);
        } else if (strcmp(key, "outpost") == 0) {
            outpost = atoi(val);
        } else if (strcmp(key, "level") == 0) {
            p->level = ClampInt(atoi(val), 1, MAX_LEVEL);
        } else if (strcmp(key, "xp") == 0) {
            p->xp = ClampInt(atoi(val), 0, 1000000);
        } else if (strcmp(key, "dye") == 0) {
            int dd = 0, dc = 0;
            if (sscanf(val, "%d,%d", &dd, &dc) == 2) {
                p->dyed = (dd != 0);
                p->dyeColor = (Color){ (unsigned char)((dc >> 16) & 0xFF),
                                       (unsigned char)((dc >> 8) & 0xFF),
                                       (unsigned char)(dc & 0xFF), 255 };
            }
        } else if (strcmp(key, "attrPoints") == 0) {
            p->attributePoints = ClampInt(atoi(val), 0, 300);
        } else if (sscanf(key, "attr%d", &idx) == 1 && idx >= 0 && idx < ATTR_COUNT) {
            p->attributeRank[idx] = ClampInt(atoi(val), 0, ATTRIBUTE_RANK_CAP);
        } else if (sscanf(key, "skill%d", &idx) == 1 && idx >= 0 && idx < SKILL_BAR_SIZE) {
            int s = atoi(val);
            p->skillBar[idx] = (s >= -1 && s < g_skillCount) ? s : -1;
        } else if (strcmp(key, "volume") == 0) {
            Audio_SetVolume(ClampInt(atoi(val), 0, 100));
        } else if (strcmp(key, "gold") == 0) {
            g_gold = ClampInt(atoi(val), 0, 1000000000);
        } else if (strcmp(key, "skillsKnown") == 0) {
            Skillbook_ReadMask(val);
            sawSkillbook = true;
        } else if (strcmp(key, "skillPoints") == 0) {
            g_skillPoints = ClampInt(atoi(val), 0, 999);
        } else if (strcmp(key, "skillsBought") == 0) {
            g_skillsPurchased = ClampInt(atoi(val), 0, 999);
        } else if (sscanf(key, "build%d", &idx) == 1 && idx >= 0 && idx < BUILD_SLOT_COUNT) {
            BuildTemplate *b = Builds_Slot(idx);
            char *bars = strchr(val, '|');
            char *ranks = bars ? strchr(bars + 1, '|') : NULL;
            if (b && bars && ranks) {
                *bars = '\0';
                *ranks = '\0';
                snprintf(b->name, sizeof(b->name), "%s", val);
                const char *p2 = bars + 1;
                for (int k = 0; k < SKILL_BAR_SIZE; k++) {
                    b->skillBar[k] = (int)strtol(p2, (char **)&p2, 10);
                    if (*p2 == ',') p2++;
                }
                p2 = ranks + 1;
                for (int a = 0; a < ATTR_COUNT; a++) {
                    long r = strtol(p2, (char **)&p2, 10);
                    if (r < 0) r = 0;
                    if (r > ATTRIBUTE_RANK_CAP) r = ATTRIBUTE_RANK_CAP;
                    b->attributeRank[a] = (unsigned char)r;
                    if (*p2 == ',') p2++;
                }
                b->used = true;
            }
        } else if (strcmp(key, "titleWorn") == 0) {
            // Applied after the player's level is restored, below - the
            // setter refuses a title that isn't earned yet, and at this
            // point in the parse the level may still be the default.
            titleWorn = atoi(val);
        } else if (strcmp(key, "thomHired") == 0) {
            thomHired = atoi(val) != 0;
        } else if (strcmp(key, "petCharmed") == 0) {
            petCharmed = atoi(val) != 0;
        } else if (strcmp(key, "petProgress") == 0) {
            sscanf(val, "%d,%d", &petLevel, &petXp);
        } else if (strcmp(key, "visitedOutposts") == 0) {
            visitedOutposts = (unsigned)strtoul(val, NULL, 10);
        } else if (sscanf(key, "quest%d", &idx) == 1 && idx >= 0 && idx < QUEST_COUNT) {
            int state = 0, kills = 0;
            sscanf(val, "%d,%d", &state, &kills);
            g_quests[idx].state = (QuestState)ClampInt(state, 0, (int)QUEST_DONE);
            g_quests[idx].kills = ClampInt(kills, 0, 999);
        } else if (strcmp(key, "equipped") == 0) {
            const char *p2 = val;
            for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
                equipped[i] = (int)strtol(p2, (char **)&p2, 10);
                if (*p2 == ',') p2++;
            }
            sawEquipped = true;
        } else if (strcmp(key, "equipWeapon") == 0) {
            equipWeapon = atoi(val);
        } else if (strcmp(key, "equipArmor") == 0) {
            equipArmor = atoi(val);
        } else if (sscanf(key, "item%d", &idx) == 1) {
            Item it;
            memset(&it, 0, sizeof(it));
            int kind = 0, unid = 0, consumed = 0;
            int mh = 0, map = 0, mls = 0, ra = 0, rab = 0, rh = 0, ia = 0, dye = 0;
            int rar = 0, meg = 0, marm = 0, mench = 0;
            // Formats, newest first: full upgrade columns WITH rarity; the
            // earlier upgrade columns; count+unidentified; count only;
            // neither. Older saves parse fine on the shorter branches.
            if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%n",
                       &kind, &it.dmgMin, &it.dmgMax, &it.range, &it.attackInterval,
                       &it.armor, &it.count, &unid, &mh, &map, &mls, &ra, &rab, &rh, &ia,
                       &dye, &rar, &meg, &marm, &mench, &consumed) >= 20 && consumed > 0) {
                it.unidentified = (unid != 0);
                it.modHealth = mh; it.modArmorPen = map; it.modLifesteal = mls;
                it.runeAttr = ra; it.runeAttrBonus = rab; it.runeHealth = rh;
                it.insigniaArmor = ia;
                it.rarity = rar; it.modEnergyGain = meg; it.modArmor = marm;
                it.modEnchantPct = mench;
                it.dyeColor = (Color){ (unsigned char)((dye >> 16) & 0xFF),
                                       (unsigned char)((dye >> 8) & 0xFF),
                                       (unsigned char)(dye & 0xFF), 255 };
            } else if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%n",
                       &kind, &it.dmgMin, &it.dmgMax, &it.range, &it.attackInterval,
                       &it.armor, &it.count, &unid, &mh, &map, &mls, &ra, &rab, &rh, &ia,
                       &dye, &consumed) >= 16 && consumed > 0) {
                it.unidentified = (unid != 0);
                it.modHealth = mh; it.modArmorPen = map; it.modLifesteal = mls;
                it.runeAttr = ra; it.runeAttrBonus = rab; it.runeHealth = rh;
                it.insigniaArmor = ia;
                it.dyeColor = (Color){ (unsigned char)((dye >> 16) & 0xFF),
                                       (unsigned char)((dye >> 8) & 0xFF),
                                       (unsigned char)(dye & 0xFF), 255 };
            } else if (sscanf(val, "%d|%d|%d|%f|%f|%d|%d|%d|%n", &kind, &it.dmgMin, &it.dmgMax,
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
            it.kind = (ItemKind)ClampInt(kind, 0, (int)ITEM_DYE);
            it.count = ClampInt(it.count, 1, 9999);
            strncpy(it.name, val + consumed, sizeof(it.name) - 1);
            Items_AddToInventory(it);
        } else if (strcmp(key, "version") != 0 && strcmp(key, "itemCount") != 0) {
            // The writer and the reader are two hand-maintained lists of
            // the same keys, and nothing makes them agree. When they
            // drift, the symptom is silent: a field saves fine and comes
            // back as its default. (That is exactly how "name=" was
            // written for a while while the loader only ever read
            // "charName=".) Naming the orphan turns a silent data loss
            // into a line in the log.
            TraceLog(LOG_WARNING, "SAVE: no loader for key '%s' - writer and reader have drifted", key);
        }
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
    World_SetPetProgress(petLevel, petXp);
    World_SetVisitedMask(visitedOutposts);
    World_SetPetCharmed(petCharmed);
    World_SetSearingHappened(searingHappened);
    // Only now, with the level restored, can the setter judge whether
    // the title was actually earned.
    Titles_Reset();
    Titles_SetDisplayed(titleWorn);
    if (sawEquipped) {
        for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
            g_equipped[i] = (equipped[i] >= 0 && equipped[i] < g_inventoryCount) ? equipped[i] : -1;
        }
    } else {
        // A save from before armour was split into pieces: its single
        // armour item goes on the chest, which is the piece that carries
        // most of GW1's armour anyway.
        if (equipWeapon >= 0 && equipWeapon < g_inventoryCount) g_equipped[EQUIP_WEAPON] = equipWeapon;
        if (equipArmor >= 0 && equipArmor < g_inventoryCount) g_equipped[EQUIP_CHEST] = equipArmor;
    }
    Items_RecomputeEquipped(p);

    // "Log back in" at the last outpost: respawns the party (including
    // a hired Thom) and fully restores everyone, GW1-style.
    World_RestoreToOutpost((ZoneId)ClampInt(outpost, 0, ZONE_COUNT - 1));

    TraceLog(LOG_INFO, "SAVE: loaded %s (level %d, %d gold)", g_savePath, p->level, g_gold);
    return true;
}
