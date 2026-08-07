#include "builds.h"
#include "character.h"
#include "skill.h"
#include "skillbook.h"
#include <stdio.h>
#include <string.h>

static BuildTemplate g_builds[BUILD_SLOT_COUNT];

BuildTemplate *Builds_Slot(int slot) {
    if (slot < 0 || slot >= BUILD_SLOT_COUNT) return NULL;
    return &g_builds[slot];
}

// The whole attribute budget the character has to spend, whatever it is
// currently spent on. Recovering it this way (rather than trusting a
// stored total) means a template can be fitted against the truth.
static int TotalAttributeBudget(const Entity *player) {
    int total = player->attributePoints;
    for (int a = 0; a < ATTR_COUNT; a++) {
        int rank = player->attributeRank[a];
        if (rank > 0 && rank <= ATTRIBUTE_RANK_CAP) total += g_attrCumulativeCost[rank];
    }
    return total;
}

void Builds_SaveFrom(int slot, const Entity *player) {
    BuildTemplate *b = Builds_Slot(slot);
    if (!b || !player) return;

    for (int i = 0; i < SKILL_BAR_SIZE; i++) b->skillBar[i] = player->skillBar[i];
    for (int a = 0; a < ATTR_COUNT; a++) {
        int rank = player->attributeRank[a];
        b->attributeRank[a] = (unsigned char)(rank < 0 ? 0 : rank);
    }

    if (b->name[0] == '\0') {
        // Name it after what it IS: the professions and whichever line it
        // leans on hardest. A slot labelled "Build 2" tells you nothing
        // when you come back to it.
        int best = -1, bestRank = 0;
        for (int a = 0; a < ATTR_COUNT; a++) {
            if (b->attributeRank[a] > bestRank) { bestRank = b->attributeRank[a]; best = a; }
        }
        // A character with no second profession carries the primary
        // twice internally (see World_Init) - writing that out as "Mo/Mo"
        // would advertise a secondary they don't have.
        char prof[12];
        if (player->secondaryProfession == player->primaryProfession) {
            snprintf(prof, sizeof(prof), "%s", Character_ProfessionAbbrev(player->primaryProfession));
        } else {
            snprintf(prof, sizeof(prof), "%s/%s",
                     Character_ProfessionAbbrev(player->primaryProfession),
                     Character_ProfessionAbbrev(player->secondaryProfession));
        }
        if (best >= 0) snprintf(b->name, sizeof(b->name), "%s %.14s", prof, g_attributeNames[best]);
        else           snprintf(b->name, sizeof(b->name), "%s", prof);
    }
    b->used = true;
}

bool Builds_LoadInto(int slot, Entity *player) {
    BuildTemplate *b = Builds_Slot(slot);
    if (!b || !b->used || !player) return false;

    // --- Attributes: fit the spread to the budget we actually have ---
    // Ranks are shaved from the shallowest line first, so a template
    // loaded on a lower-level character keeps the shape it was saved
    // with - the deep line stays deep - instead of collapsing evenly.
    int budget = TotalAttributeBudget(player);
    int want[ATTR_COUNT];
    for (int a = 0; a < ATTR_COUNT; a++) {
        int rank = b->attributeRank[a];
        if (rank > ATTRIBUTE_RANK_CAP) rank = ATTRIBUTE_RANK_CAP;
        if (rank < 0) rank = 0;
        // A rank in a line these professions can't reach is meaningless.
        if (!Attribute_Accessible((AttributeKind)a, player->primaryProfession,
                                  player->secondaryProfession)) rank = 0;
        want[a] = rank;
    }

    for (;;) {
        int spent = 0;
        for (int a = 0; a < ATTR_COUNT; a++) spent += g_attrCumulativeCost[want[a]];
        if (spent <= budget) break;

        // Shave the shallowest non-zero line.
        int victim = -1, lowest = ATTRIBUTE_RANK_CAP + 1;
        for (int a = 0; a < ATTR_COUNT; a++) {
            if (want[a] > 0 && want[a] < lowest) { lowest = want[a]; victim = a; }
        }
        if (victim < 0) break; // everything already zero; nothing left to shave
        want[victim]--;
    }

    int spent = 0;
    for (int a = 0; a < ATTR_COUNT; a++) {
        player->attributeRank[a] = want[a];
        spent += g_attrCumulativeCost[want[a]];
    }
    player->attributePoints = budget - spent;
    if (player->attributePoints < 0) player->attributePoints = 0;
    Entity_RecomputeAttributeStats(player);

    // --- Skills: only what this character actually knows ---
    for (int i = 0; i < SKILL_BAR_SIZE; i++) {
        int id = b->skillBar[i];
        bool ok = id >= 0 && id < g_skillCount && Skillbook_IsUnlocked(id) &&
                  Skillbook_IsUsableBy(id, player->primaryProfession, player->secondaryProfession);
        player->skillBar[i] = ok ? id : -1;
        player->skillRecharge[i] = 0.0f;
    }
    return true;
}

void Builds_Clear(int slot) {
    BuildTemplate *b = Builds_Slot(slot);
    if (b) memset(b, 0, sizeof(*b));
}

void Builds_Reset(void) {
    memset(g_builds, 0, sizeof(g_builds));
}
