#include "sprite.h"
#include "character.h"
#include "skill.h"
#include "fx.h"
#include "raylib.h"
#include <math.h>
#include <string.h>

#define PLAYER_INDEX 0

WeaponVisual Sprite_WeaponVisualOf(const Item *item) {
    if (!item || item->kind != ITEM_WEAPON) return WPNVIS_NONE;
    if (strstr(item->name, "Sword") || strstr(item->name, "Blade")) return WPNVIS_SWORD;
    if (strstr(item->name, "Hammer")) return WPNVIS_HAMMER;
    if (strstr(item->name, "Staff")) return WPNVIS_STAFF;
    if (strstr(item->name, "Rod") || strstr(item->name, "Wand") ||
        strstr(item->name, "Idol")) return WPNVIS_ROD;
    if (strstr(item->name, "Bow")) return WPNVIS_BOW;
    return WPNVIS_SWORD;
}

// What this entity is holding: the player's actual equipped weapon,
// profession-flavored gear for heroes, nothing for NPCs and beasts.
static WeaponVisual WeaponFor(const Entity *e) {
    if (e == &g_entities[PLAYER_INDEX]) {
        int wep = g_equipped[EQUIP_WEAPON];
        if (wep >= 0 && wep < g_inventoryCount) {
            return Sprite_WeaponVisualOf(&g_inventory[wep]);
        }
        return WPNVIS_NONE;
    }
    if (e->kind == ENT_HERO) {
        // Profession-appropriate gear for anyone without an inventory:
        // heroes, henchmen, and the character-creation preview.
        switch (e->primaryProfession) {
            case PROF_RANGER:       return WPNVIS_BOW;
            case PROF_ELEMENTALIST: return WPNVIS_STAFF;
            case PROF_NECROMANCER:  return WPNVIS_STAFF;
            case PROF_MONK:         return WPNVIS_ROD;
            case PROF_MESMER:       return WPNVIS_ROD;
            default:                return WPNVIS_SWORD;
        }
    }
    return WPNVIS_NONE;
}

// School color for cast glows, matching the FX system's language.
static Color AttrColor(AttributeKind a) {
    // The cast glow reads the same school colours the FX system uses -
    // one palette, so a Fire spell looks like Fire wherever it's drawn.
    return Fx_AttrColor(a);
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

// The colour of the focus on a staff or rod. A caster's weapon should
// look like the magic they cast, not like everyone else's - a
// Necromancer holding a blue Elementalist orb reads as the wrong
// profession before a single word is on screen.
static Color FocusColor(Profession p) {
    switch (p) {
        case PROF_NECROMANCER:  return (Color){ 150, 220, 120, 255 }; // bone green
        case PROF_MESMER:       return (Color){ 235, 140, 220, 255 };
        case PROF_MONK:         return (Color){ 255, 215, 120, 255 };
        case PROF_ELEMENTALIST:
        default:                return (Color){ 120, 190, 255, 255 };
    }
}

// The weapon in a hand at `hand`, pointing along `angle` (radians,
// 0 = right). Length/shape by type; sx flips with facing. `focus` tints
// the head of a staff or rod.
static void DrawWeapon(WeaponVisual w, Vector2 hand, float angle, float r, Color focus) {
    if (w == WPNVIS_NONE) return;
    float len = (w == WPNVIS_STAFF) ? r * 2.6f
              : (w == WPNVIS_HAMMER) ? r * 1.7f
              : (w == WPNVIS_ROD) ? r * 1.2f
              : (w == WPNVIS_BOW) ? r * 1.6f : r * 1.9f;
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
            DrawCircleV(tip, r * 0.32f, focus);
            DrawCircleLines((int)tip.x, (int)tip.y, r * 0.32f,
                            (Color){ (unsigned char)(focus.r / 2), (unsigned char)(focus.g / 2),
                                     (unsigned char)(focus.b / 2), 255 });
            break;
        }
        case WPNVIS_ROD: {
            DrawLineEx(hand, tip, r * 0.18f, (Color){ 150, 120, 80, 255 });
            DrawCircleV(tip, r * 0.24f, focus);
            break;
        }
        case WPNVIS_BOW: {
            // A bow reads by its silhouette: the limbs bow AWAY from the
            // string, so it's drawn as two arcs off the grip with the
            // string as the straight chord between the tips.
            Vector2 perp = { -sinf(angle), cosf(angle) };
            Vector2 grip = { hand.x + cosf(angle) * r * 0.35f, hand.y + sinf(angle) * r * 0.35f };
            Vector2 t1 = { grip.x + perp.x * len * 0.5f, grip.y + perp.y * len * 0.5f };
            Vector2 t2 = { grip.x - perp.x * len * 0.5f, grip.y - perp.y * len * 0.5f };
            Vector2 belly = { grip.x + cosf(angle) * r * 0.42f, grip.y + sinf(angle) * r * 0.42f };
            Color wood = { 138, 100, 62, 255 };
            DrawLineEx(t1, belly, r * 0.16f, wood);
            DrawLineEx(belly, t2, r * 0.16f, wood);
            DrawLineEx(t1, t2, r * 0.06f, (Color){ 220, 214, 196, 255 });
            break;
        }
        default: break;
    }
}

// ----- Humanoid: robe, head, arms, weapon, walk + swing + cast -----
// Attack timing curve, shared by every species.
//
// A plain sin(t*PI) hump eases in and out symmetrically, which reads as
// a wobble rather than a blow. Real strikes anticipate: the body pulls
// BACK, snaps forward much faster than it withdrew, then settles. This
// returns roughly -0.35 (wound up) through +1.0 (full extension) over
// the swing, so a limb driven by it has a readable telegraph and a
// visible impact frame.
static float AttackSwing(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.32f) {
        // Wind-up: ease back.
        float u = t / 0.32f;
        return -0.35f * sinf(u * 1.5708f);
    }
    if (t < 0.52f) {
        // Strike: snap through, overshooting slightly past full reach.
        float u = (t - 0.32f) / 0.20f;
        return -0.35f + 1.42f * (u * u * (3.0f - 2.0f * u));
    }
    // Recovery: drift back to rest.
    float u = (t - 0.52f) / 0.48f;
    return 1.07f * (1.0f - u) * (1.0f - u);
}

// Normalized progress through the current swing, or -1 when idle.
static float SwingPhase(const Entity *e) {
    if (e->attackAnimTimer <= 0.0f) return -1.0f;
    return 1.0f - e->attackAnimTimer / ATTACK_ANIM_DURATION;
}

// The player's robe colour: it tracks the equipped armour tier, and dye
// overrides the tier entirely. Public because the roster portrait draws
// the same figure off a save file and must land on the same colour the
// world would - one rule, two callers.
Color Sprite_PlayerRobeColor(int armor, bool dyed, Color dyeColor) {
    if (dyed) return dyeColor; // legs and trim derive from it too
    return (armor >= 60) ? (Color){ 205, 175, 95, 255 }   // gilded raiment
         : (armor >= 45) ? (Color){ 120, 140, 190, 255 }  // woad blue
                         : (Color){ 190, 165, 120, 255 }; // plain cloth
}

// A humanoid figure decoupled from the Entity: pure look plus a pose.
// The world sprite fills one of these from a live entity, the character
// roster fills one from a save file, and both draw through DrawFigure -
// so a character can never look one way in the world and another on the
// selection screen.
typedef struct {
    Color robe, skin, hair;
    int sex, hairStyle;
    WeaponVisual weapon;
    Color weaponFocus;
    float walk;   // -1..1 sway; 0 stands still
    float swing;  // attack extension; 0 idle
    float run;    // 0..1 locomotion intensity - stride, knee lift, arm pump
    float lean;   // signed forward lean (facing.x * run); 0 = upright
    float swingPhase; // -1 idle, else 0..1 through the strike (lunge + trail)
    Vector2 aim;  // facing unit vector - lean/lunge/spark direction
    bool casting;
    bool restArms; // portrait pose: weapon lowered to the side, not extended
    bool showGlow;
    Color glowColor;
    float glowProg;
} FigurePose;

// The forward push of an attack: nothing during the wind-up, a hard peak
// at the impact frame, then a quick settle. Shared by the lunge and the
// blade trail so the smear and the step land together.
static float LungeAmount(float phase) {
    if (phase < 0.0f) return 0.0f;
    if (phase < 0.34f) return 0.0f;                 // winding up, no drive yet
    if (phase < 0.5f)  return (phase - 0.34f) / 0.16f; // snap forward
    return fmaxf(0.0f, 1.0f - (phase - 0.5f) / 0.5f);  // recover
}

static void DrawFigure(Vector2 p, float r, float sx, const FigurePose *fp, double now) {
    float walk = fp->walk;
    float run = fp->run;
    float bob = fabsf(walk) * r * (0.12f + 0.10f * run); // stride bounce grows at speed

    // Idle breathing: a slow rise of the chest when standing still, so a
    // stopped figure is alive rather than frozen.
    float idle = 1.0f - fminf(1.0f, run + fabsf(fp->swing) + (fp->casting ? 1.0f : 0.0f));
    float breathe = sinf((float)now * 2.1f) * r * 0.025f * idle;

    // Attack lunge: the upper body drives forward through the strike, in
    // the direction the figure is facing.
    float lunge = LungeAmount(fp->swingPhase);
    float lungeX = fp->aim.x * lunge * r * 0.30f;

    // Forward lean while running (upper body only; the feet stay planted),
    // plus the lunge, gives every displacement a clear x offset.
    float ux = p.x + fp->lean * r * 0.5f + lungeX;

    // Shadow (tracks the planted feet, not the leaning body)
    DrawEllipse((int)p.x, (int)(p.y + r * 0.95f), r * 0.85f, r * 0.32f, (Color){ 0, 0, 0, 70 });

    // Legs: hip -> knee -> foot, so a running stride lifts the knee and
    // plants the heel instead of just scissoring two straight sticks.
    Color legC = Darken(fp->robe, 0.45f);
    float stride = r * (0.26f + 0.34f * run);
    float lift   = r * (0.05f + 0.28f * run);
    for (int i = 0; i < 2; i++) {
        float side = (i == 0) ? -1.0f : 1.0f;     // left / right leg
        float ph = side * walk;                   // this leg's forward amount
        Vector2 hip = { p.x + side * r * 0.22f, p.y + r * 0.28f - bob * 0.4f };
        float footLift = fmaxf(0.0f, ph) * lift;  // raised while it swings forward
        Vector2 foot = { hip.x + ph * stride * sx, p.y + r * 0.95f - footLift };
        Vector2 knee = { (hip.x + foot.x) * 0.5f + sx * run * r * 0.10f,
                         (hip.y + foot.y) * 0.5f - run * r * 0.12f - footLift * 0.3f };
        DrawLineEx(hip, knee, r * 0.22f, legC);
        DrawLineEx(knee, foot, r * 0.19f, legC);
        DrawCircleV(foot, r * 0.12f, Darken(legC, 0.85f)); // heel
    }

    // Robe: a tapered body (triangle skirt + chest circle)
    float top = p.y - r * 0.55f - bob + breathe;
    DrawTriangle((Vector2){ ux, top },
                 (Vector2){ p.x - r * 0.78f, p.y + r * 0.55f },
                 (Vector2){ p.x + r * 0.78f, p.y + r * 0.55f }, fp->robe);
    DrawCircleV((Vector2){ ux, top + r * 0.18f },
                r * (fp->sex == 0 ? 0.45f : 0.5f), fp->robe);
    // Trim line - brighter on higher armor.
    DrawLineEx((Vector2){ p.x - r * 0.6f, p.y + r * 0.42f },
               (Vector2){ p.x + r * 0.6f, p.y + r * 0.42f }, r * 0.12f, Lighten(fp->robe, 0.35f));

    // Weapon arm. Anchored on the leaning torso; pumps with the stride.
    float pump = walk * (0.15f + 0.35f * run);
    Vector2 shoulder = { ux + sx * r * 0.42f, top + r * 0.30f - bob * 0.5f };
    float armAngle;
    if (fp->casting) {
        armAngle = -1.9f + sinf((float)now * 8.0f) * 0.15f; // raised, trembling with power
    } else if (fp->restArms) {
        armAngle = sx > 0 ? 1.15f : 1.99f; // weapon lowered, held at the side
    } else {
        armAngle = (sx > 0 ? -0.5f : 3.64f) + (sx > 0 ? fp->swing : -fp->swing);
        armAngle += pump;
    }
    Vector2 hand = { shoulder.x + cosf(armAngle) * r * 0.75f,
                     shoulder.y + sinf(armAngle) * r * 0.75f };
    // Blade trail: faint smears at earlier swing angles as it snaps through.
    if (fp->swingPhase >= 0.30f && fp->swingPhase <= 0.62f && !fp->casting) {
        float tr = 1.0f - fabsf(fp->swingPhase - 0.46f) / 0.16f;
        for (int g = 1; g <= 2 && tr > 0.0f; g++) {
            float ga = armAngle - (sx > 0 ? 1.0f : -1.0f) * 0.42f * g;
            Vector2 gh = { shoulder.x + cosf(ga) * r * 0.78f, shoulder.y + sinf(ga) * r * 0.78f };
            DrawLineEx(shoulder, gh, r * (0.16f - 0.03f * g),
                       Fade((Color){ 230, 236, 245, 255 }, 0.22f * tr / g));
        }
    }
    DrawLineEx(shoulder, hand, r * 0.2f, Darken(fp->robe, 0.7f));
    DrawCircleV(hand, r * 0.16f, fp->skin);
    DrawWeapon(fp->weapon, hand, fp->casting ? -1.57f : armAngle, r, fp->weaponFocus);
    // Impact spark at the weapon tip on the strike frame.
    if (fp->swingPhase >= 0.42f && fp->swingPhase <= 0.56f && !fp->casting) {
        float sp = 1.0f - fabsf(fp->swingPhase - 0.49f) / 0.07f;
        Vector2 tip = { hand.x + cosf(armAngle) * r * 0.55f, hand.y + sinf(armAngle) * r * 0.55f };
        DrawCircleV(tip, r * 0.14f * sp, Fade((Color){ 255, 250, 220, 255 }, 0.8f * sp));
        for (int k = 0; k < 4; k++) {
            float a = armAngle - 0.9f + k * 0.6f;
            DrawLineEx(tip, (Vector2){ tip.x + cosf(a) * r * 0.3f * sp, tip.y + sinf(a) * r * 0.3f * sp },
                       r * 0.05f, Fade((Color){ 255, 244, 200, 255 }, 0.7f * sp));
        }
    }

    // Off arm
    Vector2 shoulder2 = { ux - sx * r * 0.42f, top + r * 0.30f - bob * 0.5f };
    float armAngle2 = fp->casting ? -1.25f : (sx > 0 ? 3.6f : -0.45f) - pump;
    Vector2 hand2 = { shoulder2.x + cosf(armAngle2) * r * 0.7f,
                      shoulder2.y + sinf(armAngle2) * r * 0.7f };
    DrawLineEx(shoulder2, hand2, r * 0.2f, Darken(fp->robe, 0.7f));
    DrawCircleV(hand2, r * 0.16f, fp->skin);

    // Head + hair. The style changes the silhouette, which is what
    // actually makes two characters distinguishable at this size.
    Vector2 headC = { ux, top - r * 0.42f };
    DrawCircleV(headC, r * 0.42f, fp->skin);
    switch (fp->hairStyle) {
        case 1: // long: a cap plus a fall down the back
            DrawCircleSector(headC, r * 0.46f, 180.0f, 360.0f, 12, fp->hair);
            DrawEllipse((int)(headC.x - sx * r * 0.16f), (int)(headC.y + r * 0.28f),
                        r * 0.26f, r * 0.42f, fp->hair);
            break;
        case 2: // cropped: a low band, leaving the crown bare
            DrawCircleSector(headC, r * 0.45f, 200.0f, 340.0f, 12, fp->hair);
            break;
        default: // the original cap
            DrawCircleSector(headC, r * 0.44f, 180.0f, 360.0f, 12, fp->hair);
            break;
    }
    // Eyes face the walk direction
    DrawCircleV((Vector2){ headC.x + sx * r * 0.15f, headC.y + r * 0.05f }, r * 0.05f, BLACK);

    // Cast glow: school-colored halo swelling with cast progress, with
    // motes spiralling inward - energy being gathered before release.
    if (fp->showGlow) {
        Vector2 gc = { ux, top - r * 1.15f };
        Color glow = fp->glowColor;
        float gr = r * (0.35f + 0.45f * fp->glowProg) + sinf((float)now * 10.0f) * r * 0.06f;
        glow.a = 170;
        DrawCircleV(gc, gr, Fade(glow, 0.35f));
        DrawCircleLines((int)gc.x, (int)gc.y, gr, glow);
        for (int m = 0; m < 5; m++) {
            float a = (float)now * 3.2f + m * 1.2566f;       // 2pi/5 apart
            float dist = r * (1.0f - fp->glowProg) * (0.9f + 0.3f * sinf(a * 2.0f));
            Vector2 mote = { gc.x + cosf(a) * dist, gc.y + sinf(a) * dist };
            DrawCircleV(mote, r * (0.06f + 0.05f * fp->glowProg), Fade(glow, 0.85f));
        }
    }
}

// An interactive NPC wears gilded shoulder armour and a collar gem so
// the people worth walking up to read as special at a glance, not just
// from the nameplate. Quest-givers carry a green gem to match their "!"
// badge; other service NPCs a ruby one. Drawn over the finished figure,
// so it sits on the shoulders like real pauldrons.
static void DrawNpcAdornment(Vector2 p, float r, float top, NpcRole role) {
    Color gold   = { 214, 182, 96, 255 };
    Color goldHi = { 245, 225, 150, 255 };
    Color goldDk = { 150, 120, 48, 255 };
    float sy = top + r * 0.26f;
    float sx = r * 0.5f;
    // Collar bar across the chest, then a pauldron on each shoulder.
    DrawLineEx((Vector2){ p.x - sx, sy }, (Vector2){ p.x + sx, sy }, r * 0.12f, goldDk);
    DrawLineEx((Vector2){ p.x - sx * 0.8f, sy - r * 0.02f },
               (Vector2){ p.x + sx * 0.8f, sy - r * 0.02f }, r * 0.05f, gold);
    DrawCircleV((Vector2){ p.x - sx, sy }, r * 0.22f, gold);
    DrawCircleV((Vector2){ p.x + sx, sy }, r * 0.22f, gold);
    DrawCircleV((Vector2){ p.x - sx, sy }, r * 0.10f, goldHi);
    DrawCircleV((Vector2){ p.x + sx, sy }, r * 0.10f, goldHi);
    // Collar gem, coloured by whether this NPC has quests to give.
    Color gem = (role == NPC_QUEST_GIVER) ? (Color){ 90, 200, 110, 255 }
                                          : (Color){ 200, 90, 90, 255 };
    DrawCircleV((Vector2){ p.x, sy + r * 0.03f }, r * 0.09f, gem);
    DrawCircleV((Vector2){ p.x, sy + r * 0.03f }, r * 0.045f, goldHi);
}

static void DrawHumanoid(const Entity *e, double now) {
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f; // horizontal flip

    // Robe color: the player's tracks the equipped armor tier; everyone
    // else keeps their identity color.
    Color robe = e->color;
    if (e == &g_entities[PLAYER_INDEX]) {
        robe = Sprite_PlayerRobeColor(e->armor, e->dyed, e->dyeColor);
    }

    // Appearance from character creation. Everyone who isn't the player
    // keeps index 0 defaults, which is why NPCs and heroes still look
    // the way they always did. Hair is gated on the DATA, not on "is
    // this g_entities[0]": the creation preview is deliberately not the
    // player slot, and anyone who never picked hair carries -1 and keeps
    // their identity colour.
    Color skin = g_skinTones[(e->skinTone >= 0 && e->skinTone < SKIN_TONE_COUNT)
                             ? e->skinTone : 1];
    Color hair = (e->hairColor >= 0 && e->hairColor < HAIR_COLOR_COUNT)
                 ? g_hairColors[e->hairColor] : Darken(e->color, 0.5f);

    // Casting state feeds the arms and the glow; the swing uses the same
    // anticipation curve the beasts do, so a party member's blow and a
    // Charr's read with the same weight.
    bool casting = Entity_IsCasting(e);
    float phase = SwingPhase(e);
    float swing = (phase >= 0.0f) ? AttackSwing(phase) * 1.55f : 0.0f;
    float walk = sinf(e->animTime * 6.0f) * e->moveBlend;
    // Locomotion intensity: the eased move blend, pushed toward a full run
    // as actual pace climbs. Lean is signed by facing and fades on vertical
    // travel (facing.x -> 0), where there's no clear forward to tip into.
    float run = e->moveBlend * fminf(1.0f, 0.4f + e->gaitSpeed / 130.0f);

    FigurePose pose = {
        .robe = robe, .skin = skin, .hair = hair,
        .sex = e->sex, .hairStyle = e->hairStyle,
        .weapon = WeaponFor(e), .weaponFocus = FocusColor(e->primaryProfession),
        .walk = walk, .run = run, .lean = e->facing.x * run,
        .swingPhase = phase, .aim = e->facing,
        .swing = swing, .casting = casting, .showGlow = casting,
    };
    if (casting) {
        int skillIdx = e->skillBar[e->castingSlot];
        pose.glowColor = (skillIdx >= 0 && skillIdx < g_skillCount)
                         ? AttrColor(g_skillDB[skillIdx].attribute)
                         : (Color){ 200, 200, 210, 255 };
        pose.glowProg = (e->castTimeTotal > 0.0f)
                        ? 1.0f - e->castTimeRemaining / e->castTimeTotal : 0.5f;
    }
    DrawFigure(p, r, sx, &pose, now);

    // Interactive NPCs wear special armour so they stand out in a crowd.
    if (e->kind == ENT_NPC && e->npcRole != NPC_NONE) {
        float bob = fabsf(walk) * r * 0.12f;
        DrawNpcAdornment(p, r, p.y - r * 0.55f - bob, e->npcRole);
    }
}

void Sprite_DrawPortrait(Vector2 center, float radius, const SpritePortrait *look) {
    Color robe = Sprite_PlayerRobeColor(look->armor, look->dyed, look->dyeColor);
    Color skin = g_skinTones[(look->skinTone >= 0 && look->skinTone < SKIN_TONE_COUNT)
                             ? look->skinTone : 1];
    Color hair = (look->hairColor >= 0 && look->hairColor < HAIR_COLOR_COUNT)
                 ? g_hairColors[look->hairColor] : Darken(robe, 0.5f);
    FigurePose pose = {
        .robe = robe, .skin = skin, .hair = hair,
        .sex = look->sex, .hairStyle = look->hairStyle,
        .weapon = look->weapon,
        .weaponFocus = FocusColor((Profession)look->profession),
        .walk = 0.0f, .swing = 0.0f, .casting = false,
        .restArms = true, .showGlow = false,
    };
    DrawFigure(center, radius, 1.0f, &pose, 0.0);
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
    float phase = SwingPhase(e);
    float swing = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;
    float lunge = swing * r * 0.62f;
    // The whole beast rocks into the blow, not just the arm - weight is
    // what makes a hit look like it landed.
    float crouch = (swing < 0.0f) ? -swing * r * 0.16f : 0.0f;

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

    // Hunched body, leaning into the strike and crouching on the wind-up
    DrawEllipse((int)(p.x + lunge * sx * 0.35f), (int)(p.y - r * 0.1f + crouch),
                r * 0.95f, r * 0.72f, fur);
    // Mane spikes along the back - they flare on the wind-up
    float flare = (swing < 0.0f) ? -swing * 0.5f : 0.0f;
    for (int i = 0; i < 4; i++) {
        float mx = p.x - sx * r * (0.55f - i * 0.32f) + lunge * sx * 0.25f;
        float my = p.y - r * (0.55f + 0.08f * sinf((float)i * 2.1f)) + crouch;
        DrawTriangle((Vector2){ mx, my - r * (0.38f + flare * 0.3f) },
                     (Vector2){ mx - r * 0.16f, my },
                     (Vector2){ mx + r * 0.16f, my }, Darken(fur, 0.75f));
    }

    // Head with muzzle + horns, thrust forward when attacking
    Vector2 head = { p.x + sx * (r * 0.75f + lunge), p.y - r * 0.35f + crouch * 0.6f };
    DrawCircleV(head, r * 0.45f, fur);
    // Jaw drops open through the strike - a snarl you can actually see.
    float gape = (swing > 0.2f) ? swing * r * 0.16f : 0.0f;
    DrawEllipse((int)(head.x + sx * r * 0.35f), (int)(head.y + r * 0.1f + gape),
                r * 0.32f, r * 0.2f, Lighten(fur, 0.15f));
    if (gape > 0.02f) {
        DrawEllipse((int)(head.x + sx * r * 0.38f), (int)(head.y + r * 0.06f),
                    r * 0.2f, gape, (Color){ 60, 24, 24, 255 });
    }
    // Horns
    DrawTriangle((Vector2){ head.x - sx * r * 0.1f, head.y - r * 0.35f },
                 (Vector2){ head.x - sx * r * 0.45f, head.y - r * 0.85f },
                 (Vector2){ head.x - sx * r * 0.3f, head.y - r * 0.25f },
                 (Color){ 220, 210, 190, 255 });
    // Eye
    DrawCircleV((Vector2){ head.x + sx * r * 0.12f, head.y - r * 0.08f }, r * 0.08f,
                e->aggroed ? (Color){ 255, 90, 60, 255 } : (Color){ 240, 200, 90, 255 });

    // Claw arm raking forward on the swing. The paw travels on an arc -
    // up and back to rear, then down and across on the strike - so the
    // rake reads as a swipe rather than a piston.
    Vector2 sh = { p.x + sx * r * 0.5f + lunge * sx * 0.3f, p.y + r * 0.05f + crouch };
    float armAngle = -0.9f + swing * 1.7f; // radians, negative = raised
    float reach = r * (0.62f + swing * 0.32f);
    Vector2 paw = { sh.x + sx * cosf(armAngle) * reach, sh.y + sinf(armAngle) * reach + r * 0.3f };
    DrawLineEx(sh, paw, r * 0.22f, dark);
    for (int c = 0; c < 3; c++) {
        Vector2 tip = { paw.x + sx * r * 0.3f, paw.y - r * 0.12f + c * r * 0.12f };
        DrawLineEx(paw, tip, r * 0.07f, (Color){ 235, 230, 220, 255 });
    }

    // Motion streak trailing the claws through the fast part of the
    // strike, so the frame the blow lands is unmistakable.
    if (swing > 0.45f) {
        float a = (swing - 0.45f) / 0.55f;
        Color streak = { 255, 240, 225, (unsigned char)(150 * a) };
        DrawRing(sh, reach * 0.86f, reach * 1.04f,
                 sx > 0 ? -70.0f : 110.0f, sx > 0 ? 40.0f : 220.0f, 10, streak);
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
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;
    // Rears back on its hind legs, then drives the whole shell forward.
    float lunge = snap * r * 0.45f;
    float rear = (snap < 0.0f) ? -snap * r * 0.3f : 0.0f;

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

    // Segmented body, driven forward and lifted on the rear-back
    float bx = p.x + lunge * sx;
    float by = p.y - rear;
    DrawEllipse((int)(bx - sx * r * 0.45f), (int)by, r * 0.75f, r * 0.5f, Darken(shell, 0.8f));
    DrawEllipse((int)(bx + sx * r * 0.25f), (int)(by - r * 0.05f), r * 0.85f, r * 0.55f, shell);
    DrawEllipse((int)(bx + sx * r * 0.25f), (int)(by - r * 0.25f), r * 0.6f, r * 0.25f, Lighten(shell, 0.15f));

    // Stinger tail curling over the back - whips forward with the strike
    float whip = snap * r * 0.35f;
    Vector2 t0 = { bx - sx * r * 1.0f, by - r * 0.05f };
    Vector2 t1 = { bx - sx * (r * 1.3f - whip * 0.5f), by - r * 0.75f };
    Vector2 t2 = { bx - sx * (r * 0.9f - whip), by - r * 1.15f };
    DrawLineEx(t0, t1, r * 0.16f, dark);
    DrawLineEx(t1, t2, r * 0.13f, dark);
    DrawTriangle(t2,
                 (Vector2){ t2.x + sx * r * 0.3f, t2.y + r * 0.05f },
                 (Vector2){ t2.x + sx * r * 0.05f, t2.y + r * 0.3f },
                 (Color){ 230, 190, 90, 255 });

    // Pincers: gape wide during the wind-up, then clash shut on impact.
    for (int s = -1; s <= 1; s += 2) {
        Vector2 base = { bx + sx * r * 0.95f, by + s * r * 0.28f };
        // Negative swing (winding up) spreads them; the strike slams
        // them together, which is the whole read on a pincer attack.
        float open = (0.45f - snap * 0.42f) * s;
        Vector2 tip = { base.x + sx * r * (0.55f + snap * 0.3f), base.y + open * r };
        DrawLineEx(base, tip, r * 0.17f, dark);
        DrawLineEx(tip, (Vector2){ tip.x + sx * r * 0.2f, tip.y - s * r * 0.18f }, r * 0.12f, dark);
    }
    // A spark where the pincers meet at full closure.
    if (snap > 0.75f) {
        Vector2 clash = { bx + sx * r * 1.5f, by };
        DrawCircleV(clash, r * 0.16f * (snap - 0.75f) * 4.0f, (Color){ 255, 230, 190, 170 });
    }

    // Eyes
    DrawCircleV((Vector2){ bx + sx * r * 0.75f, by - r * 0.25f }, r * 0.07f,
                (Color){ 255, 120, 70, 255 });
    DrawCircleV((Vector2){ bx + sx * r * 0.6f, by - r * 0.33f }, r * 0.06f,
                (Color){ 255, 120, 70, 255 });
}


// ----- Skale: a hunched amphibian. Long snout, fin crest, webbed feet.
// The first thing most Prophecies characters ever killed. -----
static void DrawSkale(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color hide = e->color;
    Color dark = Darken(hide, 0.6f);
    Color belly = Lighten(hide, 0.35f);

    float bob = sinf(e->animTime * 7.0f) * e->moveBlend * r * 0.1f;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;
    float lunge = snap * r * 0.5f;

    DrawEllipse((int)p.x, (int)(p.y + r * 0.72f), r * 0.95f, r * 0.26f, (Color){ 0, 0, 0, 70 });

    float bx = p.x + sx * lunge;
    float by = p.y + bob;

    // Splayed legs, low and wide - it squats rather than stands.
    for (int i = -1; i <= 1; i += 2) {
        float k = sinf(e->animTime * 7.0f + (i > 0 ? 0.0f : 3.14f)) * e->moveBlend;
        DrawLineEx((Vector2){ bx + i * r * 0.34f, by + r * 0.18f },
                   (Vector2){ bx + i * r * 0.62f, by + r * 0.66f + k * r * 0.08f },
                   r * 0.15f, dark);
    }

    // Body: a low egg leaning forward.
    DrawEllipse((int)bx, (int)(by + r * 0.1f), r * 0.62f, r * 0.5f, hide);
    DrawEllipse((int)bx, (int)(by + r * 0.26f), r * 0.44f, r * 0.28f, belly);

    // Dorsal fin crest - the silhouette cue that says "not a mammal".
    for (int i = 0; i < 3; i++) {
        float fx = bx - sx * (r * 0.1f + i * r * 0.22f);
        float h = r * (0.34f - i * 0.07f);
        DrawTriangle((Vector2){ fx - r * 0.1f, by - r * 0.1f },
                     (Vector2){ fx + r * 0.1f, by - r * 0.1f },
                     (Vector2){ fx, by - r * 0.1f - h }, dark);
    }

    // Head on a short neck, snout thrust forward.
    float hx = bx + sx * r * 0.5f, hy = by - r * 0.22f;
    DrawCircleV((Vector2){ hx, hy }, r * 0.3f, hide);
    DrawEllipse((int)(hx + sx * r * 0.26f), (int)(hy + r * 0.06f), r * 0.26f, r * 0.14f, hide);
    // Jaw gapes on the strike.
    float gape = (snap > 0.0f) ? snap * r * 0.13f : 0.0f;
    DrawEllipse((int)(hx + sx * r * 0.28f), (int)(hy + r * 0.16f + gape), r * 0.2f, r * 0.07f, dark);
    DrawCircleV((Vector2){ hx + sx * r * 0.1f, hy - r * 0.12f }, r * 0.07f,
                (Color){ 240, 240, 200, 255 });
    DrawCircleV((Vector2){ hx + sx * r * 0.11f, hy - r * 0.12f }, r * 0.035f, BLACK);
}

// ----- Grawl: hunched ape-men. Heavy shoulders, long arms, a club. -----
static void DrawGrawl(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color fur = e->color;
    Color dark = Darken(fur, 0.6f);
    Color skin = Lighten(fur, 0.45f);

    float step = sinf(e->animTime * 8.0f) * e->moveBlend;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;

    DrawEllipse((int)p.x, (int)(p.y + r * 0.78f), r * 0.9f, r * 0.26f, (Color){ 0, 0, 0, 70 });

    float bx = p.x + sx * snap * r * 0.3f;
    float by = p.y;

    // Short bowed legs.
    for (int i = -1; i <= 1; i += 2) {
        float k = (i > 0 ? step : -step) * r * 0.14f;
        DrawLineEx((Vector2){ bx + i * r * 0.2f, by + r * 0.2f },
                   (Vector2){ bx + i * r * 0.3f, by + r * 0.72f + k },
                   r * 0.19f, dark);
    }

    // Barrel torso, hunched forward.
    DrawEllipse((int)bx, (int)(by - r * 0.05f), r * 0.5f, r * 0.46f, fur);
    // Heavy shoulders: the read is "top-heavy".
    DrawCircleV((Vector2){ bx - sx * r * 0.3f, by - r * 0.34f }, r * 0.26f, fur);
    DrawCircleV((Vector2){ bx + sx * r * 0.3f, by - r * 0.34f }, r * 0.26f, fur);

    // Long forward arm carrying the club; it winds back and comes down.
    float armA = -0.5f + snap * 1.5f;
    Vector2 hand = { bx + sx * (r * 0.42f + cosf(armA) * r * 0.5f),
                     by - r * 0.2f + sinf(armA) * r * 0.5f };
    DrawLineEx((Vector2){ bx + sx * r * 0.32f, by - r * 0.3f }, hand, r * 0.16f, fur);
    DrawLineEx(hand, (Vector2){ hand.x + sx * r * 0.42f, hand.y + r * 0.24f },
               r * 0.17f, (Color){ 122, 92, 60, 255 });

    // Head: low brow, jutting jaw.
    float hx = bx + sx * r * 0.16f, hy = by - r * 0.62f;
    DrawCircleV((Vector2){ hx, hy }, r * 0.27f, fur);
    DrawEllipse((int)(hx + sx * r * 0.16f), (int)(hy + r * 0.1f), r * 0.19f, r * 0.13f, skin);
    DrawCircleV((Vector2){ hx + sx * r * 0.08f, hy - r * 0.06f }, r * 0.055f, BLACK);
}

// ----- Moa: a tall flightless bird. Harmless unless you start it. -----
static void DrawMoa(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color plume = e->color;
    Color dark = Darken(plume, 0.6f);

    float step = sinf(e->animTime * 9.0f) * e->moveBlend;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;

    DrawEllipse((int)p.x, (int)(p.y + r * 0.8f), r * 0.7f, r * 0.22f, (Color){ 0, 0, 0, 70 });

    float bx = p.x, by = p.y + sinf(e->animTime * 9.0f) * e->moveBlend * r * 0.06f;

    // Two long legs - most of the bird's height is leg.
    for (int i = -1; i <= 1; i += 2) {
        float k = (i > 0 ? step : -step) * r * 0.3f;
        DrawLineEx((Vector2){ bx + i * r * 0.12f, by + r * 0.15f },
                   (Vector2){ bx + i * r * 0.16f + k, by + r * 0.78f },
                   r * 0.09f, (Color){ 180, 150, 90, 255 });
    }

    // Round body, tail plume behind.
    DrawEllipse((int)bx, (int)(by - r * 0.05f), r * 0.46f, r * 0.4f, plume);
    DrawEllipse((int)(bx - sx * r * 0.42f), (int)(by - r * 0.12f), r * 0.22f, r * 0.3f, dark);

    // Long neck sweeping up, head at the top. It stoops when it pecks.
    float stoop = snap * r * 0.45f;
    Vector2 headPos = { bx + sx * (r * 0.2f + stoop * 0.6f), by - r * 0.72f + stoop };
    DrawLineEx((Vector2){ bx + sx * r * 0.1f, by - r * 0.2f }, headPos, r * 0.13f, plume);
    DrawCircleV(headPos, r * 0.17f, plume);
    // Beak.
    DrawTriangle((Vector2){ headPos.x + sx * r * 0.1f, headPos.y - r * 0.05f },
                 (Vector2){ headPos.x + sx * r * 0.1f, headPos.y + r * 0.07f },
                 (Vector2){ headPos.x + sx * r * 0.38f, headPos.y + r * 0.02f },
                 (Color){ 210, 170, 80, 255 });
    DrawCircleV((Vector2){ headPos.x + sx * r * 0.05f, headPos.y - r * 0.05f }, r * 0.04f, BLACK);
}

// ----- Undead: the Catacombs. A humanoid frame stripped to bone. -----
static void DrawUndead(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    Color bone = (Color){ 214, 208, 186, 255 };
    Color shadow = (Color){ 96, 92, 80, 255 };

    float sway = sinf(e->animTime * 5.0f) * e->moveBlend;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;

    DrawEllipse((int)p.x, (int)(p.y + r * 0.76f), r * 0.8f, r * 0.24f, (Color){ 0, 0, 0, 80 });

    float bx = p.x + sx * snap * r * 0.28f;
    float by = p.y + sway * r * 0.05f;

    // Legs: bare bone, no mass.
    for (int i = -1; i <= 1; i += 2) {
        float k = (i > 0 ? sway : -sway) * r * 0.2f;
        DrawLineEx((Vector2){ bx + i * r * 0.16f, by + r * 0.18f },
                   (Vector2){ bx + i * r * 0.22f + k, by + r * 0.72f },
                   r * 0.1f, bone);
    }

    // Ribcage: a spine with ribs, deliberately gappy so it reads hollow.
    DrawLineEx((Vector2){ bx, by - r * 0.42f }, (Vector2){ bx, by + r * 0.2f }, r * 0.11f, bone);
    for (int i = 0; i < 3; i++) {
        float ry = by - r * 0.3f + i * r * 0.19f;
        DrawEllipseLines((int)bx, (int)ry, r * (0.34f - i * 0.04f), r * 0.1f, bone);
    }

    // Arm with a rusted blade, winding back and chopping down.
    float armA = -0.7f + snap * 1.7f;
    Vector2 hand = { bx + sx * (r * 0.3f + cosf(armA) * r * 0.46f),
                     by - r * 0.28f + sinf(armA) * r * 0.46f };
    DrawLineEx((Vector2){ bx + sx * r * 0.22f, by - r * 0.36f }, hand, r * 0.09f, bone);
    DrawLineEx(hand, (Vector2){ hand.x + sx * r * 0.5f, hand.y + r * 0.16f },
               r * 0.09f, (Color){ 150, 140, 130, 255 });

    // Skull, with the sockets lit - the only colour on the whole figure.
    float hx = bx, hy = by - r * 0.62f;
    DrawCircleV((Vector2){ hx, hy }, r * 0.26f, bone);
    DrawEllipse((int)hx, (int)(hy + r * 0.18f), r * 0.16f, r * 0.1f, bone);
    DrawCircleV((Vector2){ hx + sx * r * 0.1f, hy - r * 0.03f }, r * 0.075f, shadow);
    DrawCircleV((Vector2){ hx - sx * r * 0.06f, hy - r * 0.03f }, r * 0.07f, shadow);
    DrawCircleV((Vector2){ hx + sx * r * 0.1f, hy - r * 0.03f }, r * 0.035f,
                (Color){ 150, 230, 160, 255 });
    DrawCircleV((Vector2){ hx - sx * r * 0.06f, hy - r * 0.03f }, r * 0.032f,
                (Color){ 150, 230, 160, 255 });
}

// ----- Aloe: a rooted plant. It never moves, so it never walks. -----
static void DrawAloe(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    Color leaf = e->color;
    Color dark = Darken(leaf, 0.6f);

    // Sways with the wind rather than with movement: it has none.
    float breeze = sinf((float)e->animTime * 1.6f + p.x * 0.01f) * 0.12f;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;

    DrawEllipse((int)p.x, (int)(p.y + r * 0.7f), r * 0.85f, r * 0.25f, (Color){ 0, 0, 0, 70 });

    // A rosette of blades, splayed from the root.
    for (int i = 0; i < 7; i++) {
        float a = -3.14159f / 2.0f + (i - 3) * 0.42f + breeze + snap * 0.25f;
        float len = r * (1.0f - fabsf((float)(i - 3)) * 0.09f);
        Vector2 tip = { p.x + cosf(a) * len, p.y + r * 0.3f + sinf(a) * len };
        Vector2 mid = { p.x + cosf(a) * len * 0.5f - sinf(a) * r * 0.12f,
                        p.y + r * 0.3f + sinf(a) * len * 0.5f + cosf(a) * r * 0.12f };
        DrawTriangle((Vector2){ p.x - r * 0.16f, p.y + r * 0.34f },
                     (Vector2){ p.x + r * 0.16f, p.y + r * 0.34f }, tip,
                     (i % 2 == 0) ? leaf : dark);
        DrawLineEx((Vector2){ p.x, p.y + r * 0.3f }, mid, r * 0.05f, dark);
    }
    // The bulb at the centre, which is the part that actually bites.
    DrawCircleV((Vector2){ p.x, p.y + r * 0.26f }, r * 0.26f, Lighten(leaf, 0.3f));
    DrawCircleV((Vector2){ p.x, p.y + r * 0.26f }, r * 0.12f, dark);
}

// ----- Fire Imp: a small fiend wreathed in flame (Wizard's Folly) -----
static void DrawImp(const Entity *e, double now) {
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    float hop = sinf(e->animTime * 8.0f) * e->moveBlend * r * 0.08f;
    float flick = sinf((float)now * 9.0f + p.x * 0.05f);
    Color body = e->color;                  // ember red
    Color glow = Lighten(body, 0.4f);
    Color flame = (Color){ 255, 170, 60, 255 };

    DrawEllipse((int)p.x, (int)(p.y + r * 0.7f), r * 0.6f, r * 0.2f, (Color){ 0, 0, 0, 70 });
    float by = p.y - hop;

    // A halo of flame licking upward behind the body.
    for (int i = -1; i <= 1; i++) {
        float fx = p.x + i * r * 0.28f;
        float h = r * (0.6f + 0.2f * flick * (i == 0 ? 1.0f : -1.0f));
        DrawTriangle((Vector2){ fx - r * 0.16f, by - r * 0.1f },
                     (Vector2){ fx + r * 0.16f, by - r * 0.1f },
                     (Vector2){ fx, by - h }, (i % 2 == 0) ? flame : (Color){ 255, 210, 90, 220 });
    }

    // Squat body + head.
    DrawCircleV((Vector2){ p.x, by }, r * 0.42f, body);
    DrawCircleV((Vector2){ p.x, by - r * 0.5f }, r * 0.3f, glow);
    // Horns.
    DrawTriangle((Vector2){ p.x - r * 0.28f, by - r * 0.62f },
                 (Vector2){ p.x - r * 0.1f,  by - r * 0.6f },
                 (Vector2){ p.x - r * 0.3f,  by - r * 0.95f }, body);
    DrawTriangle((Vector2){ p.x + r * 0.1f,  by - r * 0.6f },
                 (Vector2){ p.x + r * 0.28f, by - r * 0.62f },
                 (Vector2){ p.x + r * 0.3f,  by - r * 0.95f }, body);
    // Molten eyes.
    DrawCircleV((Vector2){ p.x + sx * r * 0.1f, by - r * 0.5f }, r * 0.07f, (Color){ 255, 240, 180, 255 });
}

// ----- Worm: a segmented burrower rising out of the ground -----
static void DrawWorm(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;
    Color skin = e->color;
    Color dark = Darken(skin, 0.65f);

    DrawEllipse((int)p.x, (int)(p.y + r * 0.5f), r * 0.7f, r * 0.22f, (Color){ 0, 0, 0, 70 });

    // A stack of segments curving up out of the earth; the head rears back
    // then strikes on the swing.
    int segs = 5;
    for (int i = 0; i < segs; i++) {
        float t = (float)i / (float)(segs - 1);
        float wob = sinf(e->animTime * 4.0f + i * 0.9f) * e->moveBlend * r * 0.12f;
        float rear = (1.0f - snap) * t * r * 0.2f;
        float x = p.x + sx * (wob + t * r * 0.15f) - sx * rear;
        float y = p.y + r * 0.4f - t * r * 0.95f;
        float rad = r * (0.34f - t * 0.12f);
        DrawCircleV((Vector2){ x, y }, rad, (i % 2 == 0) ? skin : dark);
    }
    // Head at the top, a round maw.
    float hx = p.x + sx * (snap * r * 0.5f);
    float hy = p.y - r * 0.55f - snap * r * 0.15f;
    DrawCircleV((Vector2){ hx, hy }, r * 0.26f, skin);
    DrawCircleV((Vector2){ hx + sx * r * 0.06f, hy }, r * 0.12f, (Color){ 60, 20, 24, 255 }); // maw
}

// ----- Melandru's Stalker: a lean charmable big cat -----
static void DrawStalker(const Entity *e, double now) {
    (void)now;
    float r = e->radius;
    Vector2 p = e->pos;
    float sx = (e->facing.x < -0.05f) ? -1.0f : 1.0f;
    float step = sinf(e->animTime * 9.0f) * e->moveBlend;
    float phase = SwingPhase(e);
    float snap = (phase >= 0.0f) ? AttackSwing(phase) : 0.0f;
    Color coat = e->color;
    Color dark = Darken(coat, 0.6f);

    DrawEllipse((int)p.x, (int)(p.y + r * 0.65f), r * 0.85f, r * 0.24f, (Color){ 0, 0, 0, 75 });
    float by = p.y;

    // Four legs, front pair reaching on a pounce.
    for (int i = -1; i <= 1; i += 2) {
        float k = (i > 0 ? step : -step) * r * 0.25f;
        DrawLineEx((Vector2){ p.x - sx * r * 0.35f, by + r * 0.15f },
                   (Vector2){ p.x - sx * r * 0.35f + k, by + r * 0.6f }, r * 0.1f, dark); // hind
        DrawLineEx((Vector2){ p.x + sx * (r * 0.35f + snap * r * 0.2f), by + r * 0.1f },
                   (Vector2){ p.x + sx * (r * 0.4f) - k, by + r * 0.6f }, r * 0.1f, dark); // fore
    }
    // Long low body.
    DrawEllipse((int)p.x, (int)(by + r * 0.05f), r * 0.62f, r * 0.32f, coat);
    // Tail flicking behind.
    DrawLineEx((Vector2){ p.x - sx * r * 0.55f, by },
               (Vector2){ p.x - sx * (r * 0.95f), by - r * 0.3f - step * r * 0.15f }, r * 0.08f, coat);
    // Head forward, low.
    Vector2 head = { p.x + sx * (r * 0.6f + snap * r * 0.2f), by - r * 0.12f };
    DrawCircleV(head, r * 0.28f, coat);
    DrawTriangle((Vector2){ head.x - sx * r * 0.02f, head.y - r * 0.24f },
                 (Vector2){ head.x - sx * r * 0.18f, head.y - r * 0.24f },
                 (Vector2){ head.x - sx * r * 0.1f,  head.y - r * 0.42f }, coat); // ear
    DrawCircleV((Vector2){ head.x + sx * r * 0.12f, head.y - r * 0.04f }, r * 0.05f,
                (Color){ 240, 220, 120, 255 }); // eye
}

void Sprite_DrawEntity(const Entity *e, double now) {
    switch (e->species) {
        case SPECIES_CHARR:    DrawCharr(e, now); break;
        case SPECIES_DEVOURER: DrawDevourer(e, now); break;
        case SPECIES_SKALE:    DrawSkale(e, now); break;
        case SPECIES_GRAWL:    DrawGrawl(e, now); break;
        case SPECIES_MOA:      DrawMoa(e, now); break;
        case SPECIES_UNDEAD:   DrawUndead(e, now); break;
        case SPECIES_ALOE:     DrawAloe(e, now); break;
        case SPECIES_IMP:      DrawImp(e, now); break;
        case SPECIES_WORM:     DrawWorm(e, now); break;
        case SPECIES_STALKER:  DrawStalker(e, now); break;
        default:               DrawHumanoid(e, now); break;
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
                // A dropped weapon has no owner, so it keeps the neutral focus.
                DrawWeapon(w, (Vector2){ pos.x - r * 0.5f, pos.y + r * 0.5f }, -0.9f, r,
                           FocusColor(PROF_ELEMENTALIST));
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
