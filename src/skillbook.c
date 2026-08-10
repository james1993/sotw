#include "skillbook.h"
#include "skill.h"
#include "items.h"
#include <stddef.h>
#include <string.h>

static bool g_unlocked[MAX_SKILLS];
int g_skillPoints = 0;
int g_skillsPurchased = 0;

void Skillbook_Reset(void) {
    // Empty. The one skill a new character starts with is their
    // profession's signature, so World_Init unlocks it after it has
    // decided what they are - keeping a starting-skill table here as
    // well would just be a second place for that to drift.
    memset(g_unlocked, 0, sizeof(g_unlocked));
    g_skillPoints = 0;
    g_skillsPurchased = 0;
}

bool Skillbook_IsUnlocked(int skillId) {
    if (skillId < 0 || skillId >= MAX_SKILLS) return false;
    return g_unlocked[skillId];
}

bool Skillbook_Unlock(int skillId) {
    if (skillId < 0 || skillId >= g_skillCount) return false;
    if (g_unlocked[skillId]) return false;
    g_unlocked[skillId] = true;
    return true;
}

int Skillbook_UnlockedCount(void) {
    int n = 0;
    for (int i = 0; i < g_skillCount; i++) {
        if (g_unlocked[i]) n++;
    }
    return n;
}

bool Skillbook_IsUsableBy(int skillId, Profession primary, Profession secondary) {
    if (skillId < 0 || skillId >= g_skillCount) return false;
    return Attribute_Accessible(g_skillDB[skillId].attribute, primary, secondary);
}

int Skillbook_TrainerGoldCost(void) {
    // Cheap for your first few, steep once you're filling out a build -
    // the same shape as GW1's trainer pricing.
    return 100 + 75 * g_skillsPurchased;
}

bool Skillbook_CanBuy(int skillId, Profession primary, Profession secondary) {
    if (!Skillbook_IsUsableBy(skillId, primary, secondary)) return false;
    if (Skillbook_IsUnlocked(skillId)) return false;
    if (g_skillDB[skillId].isElite) return false;      // elites are captured, never sold
    if (!g_skillDB[skillId].preSearing) return false;  // trainer only teaches pre-Searing skills
    return g_skillPoints >= 1 && g_gold >= Skillbook_TrainerGoldCost();
}

bool Skillbook_Buy(int skillId, Profession primary, Profession secondary) {
    if (!Skillbook_CanBuy(skillId, primary, secondary)) return false;
    g_gold -= Skillbook_TrainerGoldCost();
    g_skillPoints--;
    g_skillsPurchased++;
    g_unlocked[skillId] = true;
    return true;
}

int Skillbook_PickReward(Profession primary, Profession secondary) {
    // Two passes: anything on the primary's own attribute lines first,
    // then the secondary's. A quest reward should push the character
    // further into what they chose to be before it broadens them.
    for (int pass = 0; pass < 2; pass++) {
        Profession want = (pass == 0) ? primary : secondary;
        for (int i = 0; i < g_skillCount; i++) {
            if (g_unlocked[i] || g_skillDB[i].isElite) continue;
            if (!g_skillDB[i].preSearing) continue; // quests grant only pre-Searing skills
            if (!Skillbook_IsUsableBy(i, primary, secondary)) continue;
            if (g_attributeProfession[g_skillDB[i].attribute] != want) continue;
            return i;
        }
    }
    return -1;
}

void Skillbook_WriteMask(char *out, int outSize) {
    if (!out || outSize <= 0) return;
    int n = g_skillCount;
    if (n > outSize - 1) n = outSize - 1;
    for (int i = 0; i < n; i++) out[i] = g_unlocked[i] ? '1' : '0';
    out[n] = '\0';
}

void Skillbook_ReadMask(const char *mask) {
    if (!mask) return;
    memset(g_unlocked, 0, sizeof(g_unlocked));
    for (int i = 0; i < g_skillCount && mask[i]; i++) {
        g_unlocked[i] = (mask[i] == '1');
    }
}
