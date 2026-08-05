#include "character.h"
#include <stdio.h>
#include <string.h>

CharacterDef g_character;

// Palettes are deliberately short. A wall of near-identical swatches
// makes a creator feel bigger without giving the player a meaningfully
// different character; these are spaced far enough apart that every
// choice reads at sprite size.
const Color g_skinTones[SKIN_TONE_COUNT] = {
    { 246, 216, 186, 255 },
    { 224, 188, 148, 255 },
    { 196, 148, 106, 255 },
    { 148, 104,  70, 255 },
    {  98,  66,  46, 255 },
};

const Color g_hairColors[HAIR_COLOR_COUNT] = {
    {  46,  36,  30, 255 }, // black
    { 106,  70,  44, 255 }, // brown
    { 196, 158,  86, 255 }, // blond
    { 158,  62,  38, 255 }, // auburn
    { 206, 206, 210, 255 }, // silver
    {  92, 116, 150, 255 }, // slate
};

const char *Character_ProfessionName(int profession) {
    switch (profession) {
        case PROF_WARRIOR:       return "Warrior";
        case PROF_ELEMENTALIST:  return "Elementalist";
        case PROF_MONK:          return "Monk";
        default:                 return "None";
    }
}

const char *Character_ProfessionAbbrev(int profession) {
    switch (profession) {
        case PROF_WARRIOR:       return "W";
        case PROF_ELEMENTALIST:  return "E";
        case PROF_MONK:          return "Mo";
        default:                 return "";
    }
}

const char *Character_ProfessionBlurb(int profession) {
    switch (profession) {
        case PROF_WARRIOR:
            return "Heavy armour and adrenaline. Stands in front and stays there.";
        case PROF_ELEMENTALIST:
            return "A deep energy pool and ranged fire. Fragile, and hits hardest.";
        case PROF_MONK:
            return "Keeps the party standing, and punishes with smiting prayers.";
        default:
            return "";
    }
}

void Character_FormatTitle(const CharacterDef *def, char *out, int outSize) {
    if (!def || !out || outSize <= 0) return;
    if (def->secondary == PROF_NONE) {
        snprintf(out, outSize, "%s (%s)", def->name,
                 Character_ProfessionAbbrev(def->primary));
    } else {
        snprintf(out, outSize, "%s (%s/%s)", def->name,
                 Character_ProfessionAbbrev(def->primary),
                 Character_ProfessionAbbrev(def->secondary));
    }
}

CharacterDef Character_Default(void) {
    CharacterDef d;
    memset(&d, 0, sizeof(d));
    snprintf(d.name, sizeof(d.name), "%s", "Ascalonian");
    d.primary = PROF_MONK;
    d.secondary = PROF_NONE;
    d.sex = 1;
    d.skinTone = 1;
    d.hairColor = 1;
    d.hairStyle = 0;
    return d;
}
