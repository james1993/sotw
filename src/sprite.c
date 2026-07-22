#include "sprite.h"
#include "skill.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0

WeaponVisual Sprite_WeaponVisualOf(const Item *item) {
    if (!item || item->kind != ITEM_WEAPON) return WPNVIS_NONE;
    if (strstr(item->name, "Sword") || strstr(item->name, "Blade")) return WPNVIS_SWORD;
    if (strstr(item->name, "Hammer")) return WPNVIS_HAMMER;
    if (strstr(item->name, "Staff")) return WPNVIS_STAFF;
    if (strstr(item->name, "Rod") || strstr(item->name, "Wand")) return WPNVIS_ROD;
    return WPNVIS_SWORD;
}

// What this entity is holding: the player's actual equipped weapon,
// profession-flavored gear for heroes, nothing for NPCs and beasts.
static WeaponVisual WeaponFor(const Entity *e) {
    if (e == &g_entities[PLAYER_INDEX]) {
        if (g_equippedWeapon >= 0 && g_equippedWeapon < g_inventoryCount) {
            return Sprite_WeaponVisualOf(&g_inventory[g_equippedWeapon]);
        }
        return WPNVIS_NONE;
    }
    if (e->kind == ENT_HERO) {
        return (e->primaryProfession == PROF_ELEMENTALIST) ? WPNVIS_STAFF : WPNVIS_SWORD;
    }
    return WPNVIS_NONE;
}

// School color for cast glows, matching the FX system's language.
static Color AttrColor(AttributeKind a) {
    switch (a) {
        case ATTR_FIRE_MAGIC:      return (Color){ 255, 140, 50, 255 };
        case ATTR_ENERGY_STORAGE:  return (Color){ 150, 120, 255, 255 };
        case ATTR_HEALING_PRAYERS: return (Color){ 120, 240, 140, 255 };
        case ATTR_SMITING_PRAYERS: return (Color){ 255, 220, 110, 255 };
        case ATTR_DIVINE_FAVOR:    return (Color){ 220, 240, 255, 255 };
        default:                   return (Color){ 200, 200, 210, 255 };
    }
}

static Color Darken(Color c, float f) {
    return (Color){ (unsigned char)(c.r * f), (unsigned char)(c.g * f),
                    (unsigned char)(c.b * f), c.a };
}

static Color Lighten(Color c, float f) {
    int r = (int)(c.r + (255 - c.r) * f);
    int g = (int)(c.g + (255 - c.g) * f);
    int b = (int)(c.b + (255 - c.b) * f);
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

// The weapon in a hand at `hand`, pointing along `angle` (radians,
// 0 = right). Length/shape by type; sx flips with facing.
static void DrawWeapon(WeaponVisual w, Vector2 hand, float angle, float r) {
    if (w == WPNVIS_NONE) return;
    float len = (w == WPNVIS_STAFF) ? r * 2.6f
              : (w == WPNVIS_HAMMER) ? r * 1.7f
              : (w == WPNVIS_ROD) ? r * 1.2f : r * 1.9f;
    Vector2 tip = { hand.x + cosf(angle) * len, hand.y + sinf(angle) * len };

    switch (w) {
        case WPNVIS_SWORD: {
            DrawLineEx(hand, tip, r * 0.22f, (Color){ 200, 205, 215, 255 });
            // crossguard
            Vector2 g1 = { hand.x + cosf(angle) * r * 0.45f, hand.y + sinf(angle) * r * 0.45f };
            Vector2 perp = { -sinf(angle), cosf(angle) };
            DrawLineEx((Vector2){ g1.x - perp.x * r * 0.4f, g1.y - perp.y * r * 0.4f },
                       (Vector2){ g1.x + perp.x * r * 0.4f, g1.y + perp.y * r * 0.4f },
                       r * 0.16f, (Color){ 150, 130, 70, 255 });
            break;
        }
        case WPNVIS_HAMMER: {
            DrawLineEx(hand, tip, r * 0.2f, (Color){ 120, 90, 60, 255 });
            Vector2 perp = { -sinf(angle), cosf(angle) };
            Rectangle head = { 0 };
            (void)head;
            Vector2 h1 = { tip.x - perp.x * r * 0.5f, tip.y - perp.y * r * 0.5f };
            Vector2 h2 = { tip.x + perp.x * r * 0.5f, tip.y + perp.y * r * 0.5f };
            DrawLineEx(h1, h2, r * 0.55f, (Color){ 140, 140, 150, 255 });
            break;
        }
        case WPNVIS_STAFF: {
            Vector2 base = { hand.x - cosf(angle) * r * 0.8f, hand.y - sinf(angle) * r * 0.8f };
            DrawLineEx(base, tip, r * 0.18f, (Color){ 130, 95, 60, 255 });
            DrawCircleV(tip, r * 0.32f, (Color){ 120, 190, 255, 255 });
            DrawCircleLines((int)tip.x, (int)tip.y, r * 0.32f, (Color){ 60, 90, 140, 255 });
            break;
        }
        case WPNVIS_ROD: {
            DrawLineEx(hand, tip, r * 0.18f, (Color){ 150, 120, 80, 255 });
            DrawCircleV(tip, r * 0.24f, (Color){ 255, 215, 120, 255 });
            break;
        }
        default: break;
    }
}

// ----- Humanoid: robe, head, arms, weapon, walk + swing + cast -----
static void DrawHumanoid(const Entity *e, double now) {
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f; // horizontal flip

    // Robe color: the player's tracks the equipped armor tier; everyone
    // else keeps their identity color.
    Color robe = e->color;
    if (e == &g_entities[PLAYER_INDEX]) {
        int al = e->armor;
        robe = (al >= 60) ? (Color){ 205, 175, 95, 255 }   // gilded raiment
             : (al >= 45) ? (Color){ 120, 140, 190, 255 }  // woad blue
                          : (Color){ 190, 165, 120, 255 }; // plain cloth
    }

    float walk = sinf(e->animTime * 6.0f) * e->moveBlend;
    float bob = fabsf(walk) * r * 0.12f;

    // Shadow
    DrawEllipse((int)p.x, (int)(p.y + r * 0.95f), r * 0.85f, r * 0.32f, (Color){ 0, 0, 0, 70 });

    // Legs (two short strokes scissoring while walking)
    Color legC = Darken(robe, 0.45f);
    float legSwing = walk * r * 0.45f;
    DrawLineEx((Vector2){ p.x - r * 0.25f, p.y + r * 0.25f },
               (Vector2){ p.x - r * 0.25f + legSwing * sx, p.y + r * 0.95f }, r * 0.22f, legC);
    DrawLineEx((Vector2){ p.x + r * 0.25f, p.y + r * 0.25f },
               (Vector2){ p.x + r * 0.25f - legSwing * sx, p.y + r * 0.95f }, r * 0.22f, legC);

    // Robe: a tapered body (triangle skirt + chest circle)
    float top = p.y - r * 0.55f - bob;
    DrawTriangle((Vector2){ p.x, top },
                 (Vector2){ p.x - r * 0.78f, p.y + r * 0.55f },
                 (Vector2){ p.x + r * 0.78f, p.y + r * 0.55f }, robe);
    DrawCircleV((Vector2){ p.x, top + r * 0.18f }, r * 0.5f, robe);
    // Trim line - brighter on higher armor.
    DrawLineEx((Vector2){ p.x - r * 0.6f, p.y + r * 0.42f },
               (Vector2){ p.x + r * 0.6f, p.y + r * 0.42f }, r * 0.12f, Lighten(robe, 0.35f));

    // Casting state feeds the arms and the glow.
    bool casting = Entity_IsCasting(e);
    float swing = 0.0f;
    if (e->attackAnimTimer > 0.0f) {
        float t = 1.0f - e->attackAnimTimer / 0.3f; // 0..1 through the swing
        swing = sinf(t * 3.14159f) * 1.6f;          // wind up and through
    }

    // Weapon arm
    Vector2 shoulder = { p.x + sx * r * 0.42f, top + r * 0.30f - bob * 0.5f };
    float armAngle;
    if (casting) {
        armAngle = -1.9f + sinf((float)now * 8.0f) * 0.15f; // raised, trembling with power
    } else {
        armAngle = (sx > 0 ? -0.5f : 3.64f) + (sx > 0 ? swing : -swing);
        armAngle += walk * 0.15f;
    }
    Vector2 hand = { shoulder.x + cosf(armAngle) * r * 0.75f,
                     shoulder.y + sinf(armAngle) * r * 0.75f };
    Color skin = (Color){ 224, 188, 148, 255 };
    DrawLineEx(shoulder, hand, r * 0.2f, Darken(robe, 0.7f));
    DrawCircleV(hand, r * 0.16f, skin);
    DrawWeapon(WeaponFor(e), hand, casting ? -1.57f : armAngle, r);

    // Off arm
    Vector2 shoulder2 = { p.x - sx * r * 0.42f, top + r * 0.30f - bob * 0.5f };
    float armAngle2 = casting ? -1.25f : (sx > 0 ? 3.6f : -0.45f) - walk * 0.15f;
    Vector2 hand2 = { shoulder2.x + cosf(armAngle2) * r * 0.7f,
                      shoulder2.y + sinf(armAngle2) * r * 0.7f };
    DrawLineEx(shoulder2, hand2, r * 0.2f, Darken(robe, 0.7f));
    DrawCircleV(hand2, r * 0.16f, skin);

    // Head + hair
    Vector2 headC = { p.x, top - r * 0.42f };
    DrawCircleV(headC, r * 0.42f, skin);
    DrawCircleSector(headC, r * 0.44f, 180.0f, 360.0f, 12, Darken(e->color, 0.5f));
    // Eyes face the walk direction
    DrawCircleV((Vector2){ headC.x + sx * r * 0.15f, headC.y + r * 0.05f }, r * 0.05f, BLACK);

    // Cast glow: school-colored halo swelling with cast progress.
    if (casting) {
        int skillIdx = e->skillBar[e->castingSlot];
        Color glow = (skillIdx >= 0 && skillIdx < g_skillCount)
                     ? AttrColor(g_skillDB[skillIdx].attribute)
                     : (Color){ 200, 200, 210, 255 };
        float prog = (e->castTimeTotal > 0.0f)
                     ? 1.0f - e->castTimeRemaining / e->castTimeTotal : 0.5f;
        float gr = r * (0.35f + 0.45f * prog) + sinf((float)now * 10.0f) * r * 0.06f;
        glow.a = 170;
        DrawCircleV((Vector2){ p.x, top - r * 1.15f }, gr, Fade(glow, 0.35f));
        DrawCircleLines((int)p.x, (int)(top - r * 1.15f), gr, glow);
    }
}

// ----- Charr: hunched horned beast with a mane and tail -----
static void DrawCharr(const Entity *e, double now) {
    (void)now;
    float r = e->radius * 1.1f;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color fur = e->color;
    Color dark = Darken(fur, 0.6f);

    float walk = sinf(e->animTime * 6.0f) * e->moveBlend;
    float lunge = 0.0f;
    if (e->attackAnimTimer > 0.0f) {
        float t = 1.0f - e->attackAnimTimer / 0.3f;
        lunge = sinf(t * 3.14159f) * r * 0.5f;
    }

    DrawEllipse((int)p.x, (int)(p.y + r * 0.85f), r * 1.05f, r * 0.34f, (Color){ 0, 0, 0, 70 });

    // Tail sweeping behind
    Vector2 tailBase = { p.x - sx * r * 0.85f, p.y + r * 0.1f };
    Vector2 tailTip = { tailBase.x - sx * r * 0.8f, tailBase.y - r * 0.35f + walk * r * 0.2f };
    DrawLineEx(tailBase, tailTip, r * 0.18f, dark);

    // Legs
    float step = walk * r * 0.4f;
    DrawLineEx((Vector2){ p.x - r * 0.45f, p.y + r * 0.2f },
               (Vector2){ p.x - r * 0.45f + step * sx, p.y + r * 0.9f }, r * 0.24f, dark);
    DrawLineEx((Vector2){ p.x + r * 0.35f, p.y + r * 0.2f },
               (Vector2){ p.x + r * 0.35f - step * sx, p.y + r * 0.9f }, r * 0.24f, dark);

    // Hunched body
    DrawEllipse((int)(p.x + lunge * sx * 0.3f), (int)(p.y - r * 0.1f), r * 0.95f, r * 0.72f, fur);
    // Mane spikes along the back
    for (int i = 0; i < 4; i++) {
        float mx = p.x - sx * r * (0.55f - i * 0.32f);
        float my = p.y - r * (0.55f + 0.08f * sinf((float)i * 2.1f));
        DrawTriangle((Vector2){ mx, my - r * 0.38f },
                     (Vector2){ mx - r * 0.16f, my },
                     (Vector2){ mx + r * 0.16f, my }, Darken(fur, 0.75f));
    }

    // Head with muzzle + horns, thrust forward when attacking
    Vector2 head = { p.x + sx * (r * 0.75f + lunge), p.y - r * 0.35f };
    DrawCircleV(head, r * 0.45f, fur);
    DrawEllipse((int)(head.x + sx * r * 0.35f), (int)(head.y + r * 0.1f), r * 0.32f, r * 0.2f, Lighten(fur, 0.15f));
    // Horns
    DrawTriangle((Vector2){ head.x - sx * r * 0.1f, head.y - r * 0.35f },
                 (Vector2){ head.x - sx * r * 0.45f, head.y - r * 0.85f },
                 (Vector2){ head.x - sx * r * 0.3f, head.y - r * 0.25f },
                 (Color){ 220, 210, 190, 255 });
    // Eye
    DrawCircleV((Vector2){ head.x + sx * r * 0.12f, head.y - r * 0.08f }, r * 0.08f,
                e->aggroed ? (Color){ 255, 90, 60, 255 } : (Color){ 240, 200, 90, 255 });

    // Claw arm raking forward on the swing
    Vector2 sh = { p.x + sx * r * 0.5f, p.y + r * 0.05f };
    Vector2 paw = { sh.x + sx * (r * 0.55f + lunge), sh.y + r * 0.3f };
    DrawLineEx(sh, paw, r * 0.22f, dark);
    for (int c = 0; c < 3; c++) {
        Vector2 tip = { paw.x + sx * r * 0.3f, paw.y - r * 0.12f + c * r * 0.12f };
        DrawLineEx(paw, tip, r * 0.07f, (Color){ 235, 230, 220, 255 });
    }
}

// ----- Devourer: low chitinous crawler with pincers and a stinger -----
static void DrawDevourer(const Entity *e, double now) {
    (void)now;
    float r = e->radius * 1.05f;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color shell = e->color;
    Color dark = Darken(shell, 0.55f);

    float skitter = sinf(e->animTime * 10.0f) * e->moveBlend;
    float snap = 0.0f;
    if (e->attackAnimTimer > 0.0f) {
        float t = 1.0f - e->attackAnimTimer / 0.3f;
        snap = sinf(t * 3.14159f);
    }

    DrawEllipse((int)p.x, (int)(p.y + r * 0.7f), r * 1.15f, r * 0.3f, (Color){ 0, 0, 0, 70 });

    // Six legs, alternating
    for (int i = 0; i < 3; i++) {
        float off = (i - 1) * r * 0.45f;
        float lift = ((i % 2 == 0) ? skitter : -skitter) * r * 0.18f;
        DrawLineEx((Vector2){ p.x + off, p.y + r * 0.15f },
                   (Vector2){ p.x + off - r * 0.4f, p.y + r * 0.75f - lift }, r * 0.1f, dark);
        DrawLineEx((Vector2){ p.x + off, p.y + r * 0.15f },
                   (Vector2){ p.x + off + r * 0.4f, p.y + r * 0.75f + lift }, r * 0.1f, dark);
    }

    // Segmented body
    DrawEllipse((int)(p.x - sx * r * 0.45f), (int)p.y, r * 0.75f, r * 0.5f, Darken(shell, 0.8f));
    DrawEllipse((int)(p.x + sx * r * 0.25f), (int)(p.y - r * 0.05f), r * 0.85f, r * 0.55f, shell);
    DrawEllipse((int)(p.x + sx * r * 0.25f), (int)(p.y - r * 0.25f), r * 0.6f, r * 0.25f, Lighten(shell, 0.15f));

    // Stinger tail curling over the back
    Vector2 t0 = { p.x - sx * r * 1.0f, p.y - r * 0.05f };
    Vector2 t1 = { p.x - sx * r * 1.3f, p.y - r * 0.75f };
    Vector2 t2 = { p.x - sx * r * 0.9f, p.y - r * 1.15f };
    DrawLineEx(t0, t1, r * 0.16f, dark);
    DrawLineEx(t1, t2, r * 0.13f, dark);
    DrawTriangle(t2,
                 (Vector2){ t2.x + sx * r * 0.3f, t2.y + r * 0.05f },
                 (Vector2){ t2.x + sx * r * 0.05f, t2.y + r * 0.3f },
                 (Color){ 230, 190, 90, 255 });

    // Pincers snapping forward on attack
    for (int s = -1; s <= 1; s += 2) {
        Vector2 base = { p.x + sx * r * 0.95f, p.y + s * r * 0.28f };
        float open = (0.45f - snap * 0.35f) * s;
        Vector2 tip = { base.x + sx * r * (0.55f + snap * 0.25f), base.y + open * r };
        DrawLineEx(base, tip, r * 0.17f, dark);
        DrawLineEx(tip, (Vector2){ tip.x + sx * r * 0.2f, tip.y - s * r * 0.18f }, r * 0.12f, dark);
    }

    // Eyes
    DrawCircleV((Vector2){ p.x + sx * r * 0.75f, p.y - r * 0.25f }, r * 0.07f,
                (Color){ 255, 120, 70, 255 });
    DrawCircleV((Vector2){ p.x + sx * r * 0.6f, p.y - r * 0.33f }, r * 0.06f,
                (Color){ 255, 120, 70, 255 });
}

void Sprite_DrawEntity(const Entity *e, double now) {
    switch (e->species) {
        case SPECIES_CHARR: DrawCharr(e, now); break;
        case SPECIES_DEVOURER: DrawDevourer(e, now); break;
        default: DrawHumanoid(e, now); break;
    }
}

void Sprite_DrawItemDrop(const Item *item, Vector2 pos) {
    float r = 7.0f;
    switch (item->kind) {
        case ITEM_WEAPON: {
            WeaponVisual w = Sprite_WeaponVisualOf(item);
            if (item->unidentified) {
                // A wrapped bundle: you don't know what it is yet.
                DrawRectangle((int)(pos.x - r * 0.7f), (int)(pos.y - r * 0.9f),
                              (int)(r * 1.4f), (int)(r * 1.8f), (Color){ 150, 120, 170, 255 });
                DrawLineEx((Vector2){ pos.x - r * 0.7f, pos.y }, (Vector2){ pos.x + r * 0.7f, pos.y },
                           1.5f, (Color){ 90, 70, 110, 255 });
                DrawLineEx((Vector2){ pos.x, pos.y - r * 0.9f }, (Vector2){ pos.x, pos.y + r * 0.9f },
                           1.5f, (Color){ 90, 70, 110, 255 });
            } else {
                DrawWeapon(w, (Vector2){ pos.x - r * 0.5f, pos.y + r * 0.5f }, -0.9f, r);
            }
            break;
        }
        case ITEM_ARMOR:
            // A little vest
            DrawRectangle((int)(pos.x - r * 0.7f), (int)(pos.y - r * 0.6f),
                          (int)(r * 1.4f), (int)(r * 1.3f), (Color){ 120, 220, 130, 255 });
            DrawRectangle((int)(pos.x - r * 0.95f), (int)(pos.y - r * 0.6f),
                          (int)(r * 0.35f), (int)(r * 0.55f), (Color){ 120, 220, 130, 255 });
            DrawRectangle((int)(pos.x + r * 0.6f), (int)(pos.y - r * 0.6f),
                          (int)(r * 0.35f), (int)(r * 0.55f), (Color){ 120, 220, 130, 255 });
            DrawTriangle((Vector2){ pos.x - r * 0.25f, pos.y - r * 0.6f },
                         (Vector2){ pos.x + r * 0.25f, pos.y - r * 0.6f },
                         (Vector2){ pos.x, pos.y - r * 0.2f }, (Color){ 60, 120, 70, 255 });
            break;
        case ITEM_MATERIAL:
            // A folded hide
            DrawEllipse((int)pos.x, (int)pos.y, r, r * 0.65f, (Color){ 160, 115, 70, 255 });
            DrawEllipse((int)pos.x, (int)pos.y, r * 0.6f, r * 0.35f, (Color){ 190, 145, 95, 255 });
            break;
        default:
            DrawPoly(pos, 4, r, 45.0f, SKYBLUE);
            DrawPolyLines(pos, 4, r, 45.0f, BLACK);
            break;
    }
}
