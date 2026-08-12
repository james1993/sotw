#include "ui_world.h"
#include "entity.h"
#include "input.h"
#include "items.h"
#include "quests.h"
#include "titles.h"
#include "world.h"
#include "ui_font.h"
#include "ui_theme.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define PLAYER_INDEX 0

// Nameplate colors by allegiance, the GW1 language: your party reads
// cool, hostiles read hot, service NPCs read green (talk to me).
static const Color COLOR_FOE   = { 244, 132, 122, 255 };
static const Color COLOR_ALLY  = { 158, 206, 255, 255 };
static const Color COLOR_NPC   = { 138, 226, 148, 255 };
static const Color COLOR_SELF  = { 246, 240, 220, 255 };

static const char *NpcRoleLabel(NpcRole role) {
    switch (role) {
        // Giving a quest isn't a profession - Rurik is a prince who has
        // work, not a "Quests" NPC. The green "!" already says he has
        // something, so his plate carries only his name.
        case NPC_QUEST_GIVER: return NULL;
        case NPC_MERCHANT:    return "Merchant";
        case NPC_HENCHMAN:    return "Henchman";
        case NPC_CRAFTER:     return "Armorer";
        case NPC_SKILL_TRAINER: return "Skill Trainer";
        case NPC_COLLECTOR:   return "Collector";
        case NPC_PROFESSION_CHANGER: return "Profession";
        default:              return NULL;
    }
}

// A compact floating bar: dark well, colored fill, hairline border.
// Deliberately not UI_ThemeBar - that one's bevel reads as a chunky
// panel widget, too heavy at nameplate size.
static void FloatBar(float x, float y, float w, float h, float pct, Color fill) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    DrawRectangle((int)x - 1, (int)y - 1, (int)w + 2, (int)h + 2, (Color){ 0, 0, 0, 170 });
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){ 30, 30, 36, 255 });
    if (pct > 0.0f) {
        DrawRectangle((int)x, (int)y, (int)(w * pct), (int)h, fill);
        // A lighter top edge gives the fill a little roundness.
        DrawRectangle((int)x, (int)y, (int)(w * pct), (int)(h * 0.4f),
                      (Color){ 255, 255, 255, 45 });
    }
}

// The "you are about to talk to THIS one" callout: a bobbing chevron
// over the NPC plus a chip naming the action and the button that does
// it. Drawn only for the entity input.c actually resolved, so the
// highlight and the behavior can never point at different NPCs.
static void DrawInteractPrompt(Vector2 head, const Entity *npc, float scale, double now) {
    bool pad = IsGamepadAvailable(0);
    const char *key = pad ? "X" : "F";

    char label[80];
    snprintf(label, sizeof(label), "Talk to %s", npc->name);

    int font = (int)(13 * scale);
    int badgeW = UI_KeyBadge(key, 0, 0, font, false);
    int gap = (int)(6 * scale);
    int textW = UITextWidth(label, font);
    int padX = (int)(9 * scale);
    int boxW = padX * 2 + badgeW + gap + textW;
    int boxH = (int)(font * 1.5f) + (int)(10 * scale);

    // Bob gently so it reads as an invitation rather than furniture.
    float bob = sinf((float)now * 3.2f) * 3.0f * scale;
    float chevronY = head.y - (int)(16 * scale) + bob;

    // Downward chevron pointing at the NPC's head.
    float cw = 9.0f * scale;
    DrawTriangle((Vector2){ head.x - cw, chevronY - cw },
                 (Vector2){ head.x, chevronY },
                 (Vector2){ head.x + cw, chevronY - cw },
                 (Color){ 255, 226, 140, 235 });

    int boxX = (int)(head.x - boxW / 2.0f);
    int boxY = (int)(chevronY - cw - boxH - 4 * scale);
    Rectangle box = { (float)boxX, (float)boxY, (float)boxW, (float)boxH };
    DrawRectangleRounded(box, 0.35f, 8, (Color){ 16, 17, 22, 232 });
    DrawRectangleRoundedLines(box, 0.35f, 8, UI_GOLD);

    int inner = boxY + (boxH - font) / 2;
    UI_KeyBadge(key, boxX + padX, boxY + (boxH - (font + font / 2)) / 2, font, true);
    UI_TextShadow(label, boxX + padX + badgeW + gap, inner, font, (Color){ 240, 233, 210, 255 });
}

// Ground-loot labels. Crisp screen-space text on a dark chip, so a pile
// of drops after a fight is readable instead of a smear of tiny letters
// baked into the world at whatever the camera zoom happens to be.
static void DrawDropLabels(Camera2D camera, int screenWidth, int screenHeight, float scale) {
    int font = (int)(11 * scale);
    for (int i = 0; i < MAX_DROPS; i++) {
        const GroundDrop *d = &g_drops[i];
        if (!d->active) continue;

        char label[64];
        Color c;
        if (d->gold > 0) {
            snprintf(label, sizeof(label), "%d gold", d->gold);
            c = (Color){ 255, 214, 118, 255 };
        } else {
            snprintf(label, sizeof(label), "%s", Items_DisplayName(&d->item));
            c = (d->item.kind == ITEM_WEAPON)
                ? (d->item.unidentified ? (Color){ 205, 168, 235, 255 } : (Color){ 150, 205, 255, 255 })
                : (d->item.kind == ITEM_MATERIAL) ? (Color){ 214, 188, 146, 255 }
                                                  : (Color){ 140, 226, 150, 255 };
        }

        Vector2 p = GetWorldToScreen2D((Vector2){ d->pos.x, d->pos.y }, camera);
        if (p.x < -100 || p.x > screenWidth + 100 ||
            p.y < -100 || p.y > screenHeight + 100) continue;

        int tw = UITextWidth(label, font);
        int padX = (int)(5 * scale);
        int y = (int)(p.y + 12 * scale);
        DrawRectangleRounded((Rectangle){ p.x - tw / 2.0f - padX, (float)y - 2,
                                          (float)(tw + padX * 2), (float)(font + 5) },
                             0.4f, 6, (Color){ 12, 12, 16, 190 });
        UI_TextShadowCentered(label, (int)p.x, y, font, c);
    }
}

// Plate layout for one entity, resolved before anything is drawn.
typedef struct {
    int entity;
    Vector2 head;   // screen anchor
    float top;      // y of the topmost element in the stack
    float height;   // total stack height
    float halfWidth;
    bool showPlate, showBar, showCast, showQuest;
    const char *role;
} PlateLayout;

void UIWorld_Draw(Camera2D camera, int screenWidth, int screenHeight) {
    float scale = UI_Scale(screenHeight);
    double now = GetTime();

    DrawDropLabels(camera, screenWidth, screenHeight, scale);

    Entity *player = Entity_Get(PLAYER_INDEX);
    int targetIdx = player ? Entity_RefIndex(player->targetRef) : -1;
    int interactIdx = Input_InteractNpcIndex();
    int hoverIdx = Input_HoverEntityIndex();

    int nameFont = (int)(12 * scale);
    int smallFont = (int)(10 * scale);
    float barW = 44.0f * scale;
    float barH = 5.0f * scale;

    // --- Pass 1: decide what each entity shows and how tall its stack
    // is, without drawing. Overlapping plates were the last thing making
    // a melee unreadable - three names and three bars stacked on the
    // same pixels - so layout has to be resolved for everybody before
    // any of it is committed to the screen.
    PlateLayout plates[MAX_ENTITIES];
    int plateCount = 0;

    for (int i = 0; i < g_entityCount; i++) {
        Entity *e = &g_entities[i];
        if (!e->alive) continue;

        bool isTarget = (i == targetIdx);
        bool isHover = (i == hoverIdx);
        bool isFoe = (e->kind == ENT_MONSTER);
        bool isParty = (e->kind == ENT_PLAYER || e->kind == ENT_HERO);
        bool hurt = (e->maxHp > 0 && e->hp < e->maxHp);

        Vector2 head = GetWorldToScreen2D(
            (Vector2){ e->pos.x, e->pos.y - e->radius }, camera);

        float margin = 140.0f * scale;
        if (head.x < -margin || head.x > screenWidth + margin ||
            head.y < -margin || head.y > screenHeight + margin) continue;

        // A quiet field shouldn't be a wall of floating text, so a
        // sleeping monster stays anonymous until it matters. Everything
        // you can act on - party, NPCs, whatever you've targeted or are
        // pointing at - always reads.
        PlateLayout pl = { 0 };
        pl.entity = i;
        pl.head = head;
        pl.showPlate = isParty || e->kind == ENT_NPC || isTarget || isHover ||
                       (isFoe && (e->aggroed || hurt));
        pl.showBar = isParty || isTarget || isHover ||
                     (isFoe && (e->aggroed || hurt));
        if (e->kind == ENT_NPC) pl.showBar = false; // service NPCs never fight
        pl.showCast = Entity_IsCasting(e);
        pl.showQuest = (e->kind == ENT_NPC && e->npcRole == NPC_QUEST_GIVER &&
                        Quests_GiverHasAttentionFor(e->name));
        pl.role = (e->kind == ENT_NPC) ? NpcRoleLabel(e->npcRole) : NULL;

        float h = 10.0f * scale;
        if (Entity_CountEffects(e, EFFECT_CONDITION) + Entity_CountEffects(e, EFFECT_HEX) > 0) {
            h += 6.0f * scale + 3.0f * scale; // pip row
        }
        if (pl.showCast) h += barH + 3.0f * scale;
        if (pl.showBar) h += barH + 3.0f * scale;
        if (pl.showPlate) {
            h += nameFont + 3.0f * scale;
            if (pl.role) h += smallFont + 2.0f * scale;
        }
        if (pl.showQuest) h += 20.0f * scale;

        pl.height = h;
        pl.top = head.y - h;
        float nameW = pl.showPlate ? (float)UITextWidth(e->name, nameFont) : 0.0f;
        // Foe plates carry a trailing "lv N", so reserve for it too or
        // the level tag of one monster lands on its neighbour's name.
        if (pl.showPlate && isFoe && e->level > 0) nameW += 34.0f * scale;
        pl.halfWidth = (nameW > barW ? nameW : barW) * 0.5f + 4.0f * scale;
        plates[plateCount++] = pl;
    }

    // --- Pass 2: stack colliding plates instead of overlapping them.
    //
    // Placement order is nearest-to-camera first (largest screen y), so
    // the character in front keeps its natural position and the ones
    // behind step up above it - the same depth order the sprites draw
    // in. Each plate is nudged only as far as it takes to clear what's
    // already placed, and never past a cap, so a crowded fight lifts a
    // plate a little rather than flinging it off the top of the screen.
    int order[MAX_ENTITIES];
    for (int i = 0; i < plateCount; i++) order[i] = i;
    for (int i = 1; i < plateCount; i++) {
        int v = order[i];
        float vy = plates[v].head.y;
        int j = i - 1;
        while (j >= 0 && plates[order[j]].head.y < vy) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }

    float maxLift = 90.0f * scale;
    for (int oi = 1; oi < plateCount; oi++) {
        PlateLayout *me = &plates[order[oi]];
        float lifted = 0.0f;
        // Re-check against everything already placed after each nudge:
        // clearing one neighbour can slide us into another.
        for (int pass = 0; pass < plateCount && lifted < maxLift; pass++) {
            bool moved = false;
            for (int oj = 0; oj < oi; oj++) {
                PlateLayout *other = &plates[order[oj]];
                float dx = me->head.x - other->head.x;
                if (dx < 0) dx = -dx;
                if (dx > me->halfWidth + other->halfWidth) continue;
                float gap = me->head.y - other->top; // >0 means we overlap them
                if (gap <= 0.0f) continue;
                float lift = gap + 2.0f * scale;
                if (lifted + lift > maxLift) lift = maxLift - lifted;
                me->head.y -= lift;
                me->top -= lift;
                lifted += lift;
                moved = true;
                break;
            }
            if (!moved) break;
        }
    }

    // --- Pass 3: draw ---
    for (int pi = 0; pi < plateCount; pi++) {
        PlateLayout *pl = &plates[pi];
        int i = pl->entity;
        Entity *e = &g_entities[i];

        bool isTarget = (i == targetIdx);
        bool isFoe = (e->kind == ENT_MONSTER);
        bool showPlate = pl->showPlate;
        bool showBar = pl->showBar;
        Vector2 head = pl->head;

        float stackY = head.y - (int)(10 * scale);

        // --- Cast bar sits highest: it's the thing you react to ---
        if (Entity_IsCasting(e)) {
            float pct = (e->castTimeTotal > 0.0f)
                ? 1.0f - (e->castTimeRemaining / e->castTimeTotal) : 0.0f;
            stackY -= barH + 3.0f * scale;
            FloatBar(head.x - barW / 2, stackY, barW, barH, pct,
                     (Color){ 120, 190, 255, 255 });
        }

        // --- Affliction pips, tucked between the character and its bar ---
        // Conditions and hexes have to be visible on FOES, not just on
        // your own party: "is that Charr still bleeding?" and "did my
        // hex land?" are questions you answer mid-fight, at a glance.
        {
            int conds = Entity_CountEffects(e, EFFECT_CONDITION);
            int hexes = Entity_CountEffects(e, EFFECT_HEX);
            if (conds + hexes > 0) {
                float pipR = 3.0f * scale;
                float gap = 2.5f * scale;
                float totalW = (conds + hexes) * (pipR * 2 + gap) - gap;
                float px = head.x - totalW / 2 + pipR;
                stackY -= pipR * 2 + 3.0f * scale;
                for (int s = 0; s < MAX_ACTIVE_EFFECTS; s++) {
                    const ActiveEffect *fx = &e->effects[s];
                    if (!fx->active) continue;
                    Color c = Entity_EffectColor(fx);
                    DrawCircleV((Vector2){ px, stackY + pipR }, pipR + 1.0f, (Color){ 0, 0, 0, 170 });
                    if (fx->category == EFFECT_HEX) {
                        // Hexes are diamonds, conditions are dots, so the
                        // two categories are distinguishable without color.
                        DrawPoly((Vector2){ px, stackY + pipR }, 4, pipR + 0.5f, 45.0f, c);
                    } else {
                        DrawCircleV((Vector2){ px, stackY + pipR }, pipR, c);
                    }
                    px += pipR * 2 + gap;
                }
            }
        }

        if (showBar) {
            float pct = (e->maxHp > 0) ? (float)e->hp / (float)e->maxHp : 0.0f;
            Color fill = isFoe ? (Color){ 198, 58, 52, 255 } : (Color){ 78, 174, 96, 255 };
            stackY -= barH + 3.0f * scale;
            FloatBar(head.x - barW / 2, stackY, barW, barH, pct, fill);
            // Targeted units get a gold hairline around their bar, so
            // the selection is legible even in a crowded melee.
            if (isTarget) {
                DrawRectangleLines((int)(head.x - barW / 2) - 2, (int)stackY - 2,
                                   (int)barW + 4, (int)barH + 4, UI_GOLD);
            }
        }

        if (showPlate) {
            Color c;
            if (i == PLAYER_INDEX) c = COLOR_SELF;
            else if (e->kind == ENT_NPC) c = COLOR_NPC;
            else if (isFoe) c = COLOR_FOE;
            else c = COLOR_ALLY;
            if (isTarget) c = (Color){ 255, 224, 140, 255 };

            stackY -= nameFont + 3.0f * scale;
            UI_TextShadowCentered(e->name, (int)head.x, (int)stackY, nameFont, c);

            // Foes carry their level, like GW1's nameplate - it's the
            // fastest read on whether a pull is a bad idea.
            if (isFoe && e->level > 0) {
                char lvl[16];
                snprintf(lvl, sizeof(lvl), "lv %d", e->level);
                int nameW = UITextWidth(e->name, nameFont);
                UI_TextShadow(lvl, (int)(head.x + nameW / 2 + 5 * scale),
                              (int)(stackY + 1), smallFont, (Color){ 214, 176, 132, 255 });
            }

            // A displayed title sits above the name, GW1's placement -
            // the whole point of Legendary Defender of Ascalon is that
            // other people can see you wearing it.
            if (i == PLAYER_INDEX) {
                const char *title = Titles_DisplayedText(e);
                if (title) {
                    stackY -= smallFont + 2.0f * scale;
                    UI_TextShadowCentered(title, (int)head.x, (int)stackY, smallFont,
                                          (Color){ 224, 194, 112, 255 });
                }
            }

            // NPCs advertise what they're for, so you know which one to
            // walk to without opening every conversation in the camp.
            const char *role = (e->kind == ENT_NPC) ? NpcRoleLabel(e->npcRole) : NULL;
            if (role) {
                stackY -= smallFont + 2.0f * scale;
                UI_TextShadowCentered(role, (int)head.x, (int)stackY, smallFont,
                                      (Color){ 150, 180, 152, 255 });
            }
        }

        // --- Quest marker: a proper gold diamond badge, not bare text ---
        if (e->kind == ENT_NPC && e->npcRole == NPC_QUEST_GIVER &&
            Quests_GiverHasAttentionFor(e->name)) {
            float bob = sinf((float)now * 2.6f + i) * 2.5f * scale;
            stackY -= (int)(20 * scale);
            Vector2 c = { head.x, stackY + bob };
            float r = 11.0f * scale;
            DrawPoly(c, 4, r + 2.0f, 45.0f, (Color){ 0, 0, 0, 150 });
            DrawPoly(c, 4, r, 45.0f, (Color){ 96, 214, 96, 255 });
            DrawPolyLines(c, 4, r, 45.0f, (Color){ 220, 255, 210, 255 });
            UI_TextShadowCentered("!", (int)c.x, (int)(c.y - nameFont / 2),
                                  nameFont, (Color){ 20, 46, 20, 255 });
        }

        // --- Combat callouts, above everything else on the stack ---
        if (e->interruptFlashTimer > 0.0f) {
            UI_TextShadowCentered("INTERRUPTED", (int)head.x,
                                  (int)(stackY - nameFont - 6 * scale), smallFont, UI_GOLD);
        } else if (e->blockFlashTimer > 0.0f) {
            UI_TextShadowCentered("BLOCKED", (int)head.x,
                                  (int)(stackY - nameFont - 6 * scale), smallFont,
                                  (Color){ 150, 200, 240, 255 });
        } else if (e->blindMissFlashTimer > 0.0f) {
            UI_TextShadowCentered("MISS", (int)head.x,
                                  (int)(stackY - nameFont - 6 * scale), smallFont,
                                  (Color){ 205, 210, 225, 255 });
        } else if (e->dodgeFlashTimer > 0.0f) {
            UI_TextShadowCentered("DODGED", (int)head.x,
                                  (int)(stackY - nameFont - 6 * scale), smallFont,
                                  (Color){ 205, 210, 225, 255 });
        }

        // --- The interact prompt, last so it sits on top of its plate ---
        if (i == interactIdx) {
            DrawInteractPrompt((Vector2){ head.x, stackY }, e, scale, now);
        }
    }
}
