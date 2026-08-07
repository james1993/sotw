#include "titles.h"
#include "entity.h"
#include <stddef.h>

static int g_displayed = -1;

const char *Titles_Name(TitleId id) {
    switch (id) {
        case TITLE_DEFENDER_OF_ASCALON: return "Legendary Defender of Ascalon";
        default: return "";
    }
}

bool Titles_IsEarned(TitleId id, const Entity *player) {
    if (!player) return false;
    switch (id) {
        case TITLE_DEFENDER_OF_ASCALON: return player->level >= TITLE_LDOA_LEVEL;
        default: return false;
    }
}

bool Titles_IsRevealed(TitleId id, const Entity *player) {
    if (!player) return false;
    switch (id) {
        case TITLE_DEFENDER_OF_ASCALON: return player->level >= TITLE_LDOA_REVEAL;
        default: return false;
    }
}

float Titles_Progress(TitleId id, const Entity *player) {
    if (!player) return 0.0f;
    switch (id) {
        case TITLE_DEFENDER_OF_ASCALON: {
            // Levels, not experience: the level number is what the player
            // is watching, and it's what the requirement is written in.
            float p = (float)(player->level - 1) / (float)(TITLE_LDOA_LEVEL - 1);
            if (p < 0.0f) p = 0.0f;
            if (p > 1.0f) p = 1.0f;
            return p;
        }
        default: return 0.0f;
    }
}

int Titles_Displayed(void) {
    return g_displayed;
}

void Titles_SetDisplayed(int titleId) {
    if (titleId < 0) { g_displayed = -1; return; }
    if (titleId >= TITLE_COUNT) return;
    // Wearing a title you haven't earned is the one thing a title track
    // must never allow, so the check lives here rather than in the UI.
    if (!Titles_IsEarned((TitleId)titleId, Entity_Get(0))) return;
    g_displayed = titleId;
}

const char *Titles_DisplayedText(const Entity *player) {
    if (g_displayed < 0 || g_displayed >= TITLE_COUNT) return NULL;
    if (!Titles_IsEarned((TitleId)g_displayed, player)) return NULL;
    return Titles_Name((TitleId)g_displayed);
}

void Titles_Reset(void) {
    g_displayed = -1;
}
