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
        case PROF_RANGER:        return "Ranger";
        case PROF_MONK:          return "Monk";
        case PROF_NECROMANCER:   return "Necromancer";
        case PROF_MESMER:        return "Mesmer";
        case PROF_ELEMENTALIST:  return "Elementalist";
        default:                 return "None";
    }
}

// GW1's own two-letter codes, which is what every nameplate and every
// build ever written uses ("Me/E", not "Mesmer/Elementalist").
const char *Character_ProfessionAbbrev(int profession) {
    switch (profession) {
        case PROF_WARRIOR:       return "W";
        case PROF_RANGER:        return "R";
        case PROF_MONK:          return "Mo";
        case PROF_NECROMANCER:   return "N";
        case PROF_MESMER:        return "Me";
        case PROF_ELEMENTALIST:  return "E";
        default:                 return "";
    }
}

const char *Character_ProfessionBlurb(int profession) {
    switch (profession) {
        case PROF_WARRIOR:
            return "Heavy armour and adrenaline. Stands in front and stays there.";
        case PROF_RANGER:
            return "Bow range, cheap attack skills, and the best armour of any caster.";
        case PROF_MONK:
            return "Keeps the party standing, and punishes with smiting prayers.";
        case PROF_NECROMANCER:
            return "Trades health for power, and refuels off everything that dies.";
        case PROF_MESMER:
            return "Casts faster than anyone and turns a foe's own skills against them.";
        case PROF_ELEMENTALIST:
            return "The deepest energy pool in the game, spent on ranged devastation.";
        default:
            return "";
    }
}

// The colour of a profession's starting armour. Shared by the creator's
// preview and World_Init so the figure you picked is the figure you get.
Color Character_ProfessionColor(int profession) {
    switch (profession) {
        case PROF_WARRIOR:      return (Color){ 170,  96,  72, 255 }; // rust plate
        case PROF_RANGER:       return (Color){  96, 134,  78, 255 }; // forest leather
        case PROF_MONK:         return (Color){ 205, 190, 150, 255 }; // undyed linen
        case PROF_NECROMANCER:  return (Color){  86,  74, 104, 255 }; // bruised violet
        case PROF_MESMER:       return (Color){ 168,  92, 148, 255 }; // court silk
        case PROF_ELEMENTALIST: return (Color){  80, 120, 200, 255 }; // storm blue
        default:                return (Color){ 160, 160, 160, 255 };
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

bool Character_IsReforged(void) {
    // Reforged Mode is always on in this demake - there is no toggle. Every
    // character gets the Reforged content (Piken Square, the extra Northlands
    // Charr, reduced enemy health/armor, the +5% XP and gold). The stored
    // `reforged` flag is kept only for save-file compatibility.
    return true;
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
    // Reforged Mode is always on (see Character_IsReforged); the flag is set
    // for tidiness and save compatibility, not as a choice.
    d.reforged = true;
    return d;
}
