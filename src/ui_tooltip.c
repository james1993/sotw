#include "ui_tooltip.h"
#include "attributes.h"
#include "entity.h"
#include "skill.h"
#include "gwmath.h"
#include "ui_font.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

#define TIP_MAX_LINES 8
#define TIP_LINE_LEN  96

// ---------------------------------------------------------------------
// Describing a skill from its steps
//
// Each EffectStep gets one sentence. The value is resolved the same way
// effect.c resolves it - base plus per-rank times the viewer's rank in
// the skill's attribute - so what the tooltip promises is arithmetically
// the thing that will happen.

static int StepValue(const EffectStep *step, const Entity *viewer, AttributeKind attr) {
    int rank = 0;
    if (viewer && attr >= 0 && attr < ATTR_COUNT) rank = viewer->attributeRank[attr];
    float v = step->baseValue + step->perAttributeRank * (float)rank;
    return (int)(v + 0.5f);
}

static const char *ConditionName(int kind) {
    switch ((ConditionKind)kind) {
        case COND_BLEEDING: return "Bleeding";
        case COND_BURNING:  return "Burning";
        case COND_CRIPPLED: return "Crippled";
        case COND_WEAKNESS: return "Weakness";
        default:            return "a condition";
    }
}

static const char *HexName(int kind) {
    switch ((HexKind)kind) {
        case HEX_FALTERING: return "Faltering";
        case HEX_BACKLASH:  return "Backlash";
        default:            return "a hex";
    }
}

// "target foe" / "target ally" / ... - the noun each sentence acts on,
// so a step that hits the caster reads differently from one that hits
// whoever you clicked.
static const char *SubjectFor(const Skill *s, const EffectStep *step) {
    if (step->selfTarget) return "you";
    switch (s->targeting) {
        case TARGET_SELF:        return "you";
        case TARGET_SINGLE_ALLY: return "target ally";
        case TARGET_AOE_FOES:    return "each foe in the area";
        default:                 return "target foe";
    }
}

// Capitalises the first letter of a finished sentence in place.
static void Capitalise(char *s) {
    if (s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
}

static void DescribeStep(const Skill *s, const EffectStep *step, const Entity *viewer,
                         char *out, size_t outSize) {
    int v = StepValue(step, viewer, s->attribute);
    if (v < 0) v = -v;
    const char *who = SubjectFor(s, step);

    switch (step->kind) {
        case FX_DAMAGE:
            snprintf(out, outSize, "%s takes %d damage.", who, v);
            break;
        case FX_HEAL:
            snprintf(out, outSize, "%s is healed for %d Health.", who, v);
            break;
        case FX_APPLY_CONDITION:
            snprintf(out, outSize, "%s suffers %s for %.0f seconds.",
                     who, ConditionName(step->conditionKind), step->duration);
            break;
        case FX_APPLY_HEX:
            snprintf(out, outSize, "%s is hexed with %s for %.0f seconds.",
                     who, HexName(step->conditionKind), step->duration);
            break;
        case FX_REMOVE_CONDITION:
            if (v <= 1) snprintf(out, outSize, "Removes one condition from %s.", who);
            else        snprintf(out, outSize, "Removes %d conditions from %s.", v, who);
            break;
        case FX_REMOVE_HEX:
            if (v <= 1) snprintf(out, outSize, "Removes one hex from %s.", who);
            else        snprintf(out, outSize, "Removes %d hexes from %s.", v, who);
            break;
        case FX_ENERGY_DELTA:
            if (step->baseValue >= 0.0f) snprintf(out, outSize, "%s gains %d Energy.", who, v);
            else                         snprintf(out, outSize, "%s loses %d Energy.", who, v);
            break;
        case FX_ADRENALINE_DELTA:
            snprintf(out, outSize, "%s gains %d adrenaline.", who, v);
            break;
        case FX_KNOCKDOWN:
            snprintf(out, outSize, "%s is knocked down for %.0f seconds.", who, step->duration);
            break;
        case FX_INTERRUPT:
            snprintf(out, outSize, "Interrupts what %s is doing.", who);
            break;
        default:
            out[0] = '\0';
            return;
    }
    Capitalise(out);
}

// Greedy word wrap into the line buffer. Returns the number of lines
// written, never more than `maxLines`.
static int WrapInto(const char *text, int font, int maxWidth,
                    char lines[][TIP_LINE_LEN], int startLine, int maxLines) {
    int line = startLine;
    const char *word = text;
    char cur[TIP_LINE_LEN];
    cur[0] = '\0';

    while (*word && line < maxLines) {
        const char *end = word;
        while (*end && *end != ' ') end++;
        size_t wlen = (size_t)(end - word);
        if (wlen >= TIP_LINE_LEN) wlen = TIP_LINE_LEN - 1;

        char candidate[TIP_LINE_LEN];
        if (cur[0]) snprintf(candidate, sizeof(candidate), "%s %.*s", cur, (int)wlen, word);
        else        snprintf(candidate, sizeof(candidate), "%.*s", (int)wlen, word);

        if (cur[0] && UITextWidth(candidate, font) > maxWidth) {
            snprintf(lines[line++], TIP_LINE_LEN, "%s", cur);
            snprintf(cur, sizeof(cur), "%.*s", (int)wlen, word);
        } else {
            snprintf(cur, sizeof(cur), "%s", candidate);
        }
        word = (*end == ' ') ? end + 1 : end;
    }
    if (cur[0] && line < maxLines) snprintf(lines[line++], TIP_LINE_LEN, "%s", cur);
    return line;
}

static const char *TypeName(const Skill *s) {
    switch (s->type) {
        case SKILLTYPE_ATTACK_SKILL: return "Attack Skill";
        case SKILLTYPE_STANCE:       return "Stance";
        case SKILLTYPE_SHOUT:        return "Shout";
        case SKILLTYPE_SIGNET:       return "Signet";
        default:                     return "Spell";
    }
}

void UITooltip_Skill(int skillIndex, const Entity *viewer,
                     Rectangle anchor, int screenWidth, int screenHeight) {
    if (skillIndex < 0 || skillIndex >= g_skillCount) return;
    const Skill *s = &g_skillDB[skillIndex];

    float scale = UI_Scale(screenHeight);
    int pad     = UI_SP(scale, 3);
    int titleF  = UI_FontSize(scale, UI_TEXT_LG);
    int bodyF   = UI_FontSize(scale, UI_TEXT_MD);
    int smallF  = UI_FontSize(scale, UI_TEXT_SM);
    int w       = (int)(260 * scale);
    int textW   = w - pad * 2;

    // --- Body: one line per effect step, wrapped ---
    char lines[TIP_MAX_LINES][TIP_LINE_LEN];
    int lineCount = 0;
    for (int i = 0; i < s->stepCount && lineCount < TIP_MAX_LINES; i++) {
        char sentence[TIP_LINE_LEN];
        DescribeStep(s, &s->steps[i], viewer, sentence, sizeof(sentence));
        if (sentence[0]) lineCount = WrapInto(sentence, bodyF, textW, lines, lineCount, TIP_MAX_LINES);
    }
    if (lineCount == 0) snprintf(lines[lineCount++], TIP_LINE_LEN, "No effect.");

    // --- Subtitle: what kind of skill, and what drives it ---
    char subtitle[80];
    const char *attrName = (s->attribute >= 0 && s->attribute < ATTR_COUNT)
                           ? g_attributeNames[s->attribute] : "";
    if (s->isElite) snprintf(subtitle, sizeof(subtitle), "Elite %s  -  %s", TypeName(s), attrName);
    else            snprintf(subtitle, sizeof(subtitle), "%s  -  %s", TypeName(s), attrName);

    // --- Footer: GW1's energy / activation / recharge row ---
    char footer[80];
    if (s->adrenalineCost > 0) {
        // GW1 prices adrenal skills in STRIKES, not points - "5 adrenaline"
        // on a GW1 skill means five landed hits.
        int strikes = (s->adrenalineCost + GW_ADRENALINE_PER_STRIKE - 1) / GW_ADRENALINE_PER_STRIKE;
        snprintf(footer, sizeof(footer), "%d adrenaline    %.0fs cast    %.0fs recharge",
                 strikes, s->castTime, s->recharge);
    } else {
        snprintf(footer, sizeof(footer), "%d energy    %.1fs cast    %.0fs recharge",
                 s->energyCost, s->castTime, s->recharge);
    }

    // A rank note only means something once the viewer actually has one,
    // and only for skills that scale - a signet at rank 0 shouldn't
    // advertise an attribute the player can't yet use.
    char rankNote[64];
    rankNote[0] = '\0';
    if (viewer && s->attribute >= 0 && s->attribute < ATTR_COUNT) {
        bool scales = false;
        for (int i = 0; i < s->stepCount; i++) {
            if (s->steps[i].perAttributeRank != 0.0f) scales = true;
        }
        if (scales) {
            snprintf(rankNote, sizeof(rankNote), "Shown at %s rank %d",
                     attrName, viewer->attributeRank[s->attribute]);
        }
    }

    // --- Layout ---
    int lineH = bodyF + UI_SP(scale, 1);
    int h = pad + titleF + UI_SP(scale, 1) + smallF + UI_SP(scale, 2)
          + lineCount * lineH + UI_SP(scale, 2) + smallF + pad;
    if (rankNote[0]) h += smallF + UI_SP(scale, 1);

    // Two placements, because the two things being hovered want
    // different ones. A row in a list reads best beside it. The HUD's
    // skill bar sits on the bottom edge, and anything beside it lands on
    // top of the health and energy bars - so anchors down there get the
    // tooltip stacked ABOVE, which is where GW1 puts it too.
    int gap = UI_SP(scale, 2);
    float x, y;
    if (anchor.y > (float)screenHeight * 0.62f) {
        x = anchor.x + anchor.width / 2.0f - (float)w / 2.0f;
        y = anchor.y - (float)h - (float)gap;
    } else {
        x = anchor.x + anchor.width + (float)gap;
        if (x + (float)w > (float)screenWidth - (float)gap) {
            x = anchor.x - (float)w - (float)gap;
        }
        y = anchor.y;
    }

    // Clamp last, so neither placement can leave the screen.
    if (x + (float)w > (float)screenWidth - (float)gap) x = (float)(screenWidth - w - gap);
    if (x < (float)gap) x = (float)gap;
    if (y + (float)h > (float)screenHeight - (float)gap) y = (float)(screenHeight - h - gap);
    if (y < (float)gap) y = (float)gap;

    Rectangle r = { x, y, (float)w, (float)h };
    UI_ThemePanel(r, scale, 0);

    int tx = (int)r.x + pad;
    int ty = (int)r.y + pad;

    UITextDisplay(s->name, tx, ty, titleF, s->isElite ? (Color){ 224, 186, 96, 255 } : UI_GOLD);
    ty += titleF + UI_SP(scale, 1);
    UIText(subtitle, tx, ty, smallF, UI_TEXT_SECOND);
    ty += smallF + UI_SP(scale, 2);

    for (int i = 0; i < lineCount; i++) {
        UIText(lines[i], tx, ty, bodyF, UI_TEXT_PRIMARY);
        ty += lineH;
    }

    ty += UI_SP(scale, 1);
    DrawLineEx((Vector2){ r.x + (float)pad, (float)ty },
               (Vector2){ r.x + r.width - (float)pad, (float)ty }, 1.0f, UI_GOLD_DIM);
    ty += UI_SP(scale, 1);
    UIText(footer, tx, ty, smallF, UI_TEXT_SECOND);
    if (rankNote[0]) {
        ty += smallF + UI_SP(scale, 1);
        UIText(rankNote, tx, ty, smallF, UI_TEXT_MUTED);
    }
}

// --- Deferred request -------------------------------------------------

static int g_pendingSkill = -1;
static Rectangle g_pendingAnchor;

void UITooltip_Request(int skillIndex, Rectangle anchor) {
    g_pendingSkill = skillIndex;
    g_pendingAnchor = anchor;
}

void UITooltip_Flush(const Entity *viewer, int screenWidth, int screenHeight) {
    if (g_pendingSkill < 0) return;
    UITooltip_Skill(g_pendingSkill, viewer, g_pendingAnchor, screenWidth, screenHeight);
    g_pendingSkill = -1;
}
