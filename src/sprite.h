#ifndef SPRITE_H
#define SPRITE_H

#include "entity.h"
#include "items.h"

// Procedural character sprites: layered shapes drawn fresh each frame,
// so everything that matters is parametric - the player's hand holds
// whatever weapon is actually equipped, the robe reflects the armor
// tier, walk cycles ease in with movement, attacks swing, casts glow in
// the casting skill's school color. No image assets; the whole look is
// code, which keeps it crisp at every zoom.

// What the drawn figure holds, derived from an item's name.
typedef enum {
    WPNVIS_NONE = 0,
    WPNVIS_SWORD,
    WPNVIS_HAMMER,
    WPNVIS_STAFF,
    WPNVIS_ROD,
    WPNVIS_BOW
} WeaponVisual;

WeaponVisual Sprite_WeaponVisualOf(const Item *item);

// Draws the entity's full figure (shadow, body, weapon, cast glow) in
// world space. Replaces the old circle. `now` = GetTime().
void Sprite_DrawEntity(const Entity *e, double now);

// Small ground-drop glyphs: a real little sword/hammer/staff/vest/hide
// instead of an anonymous diamond.
void Sprite_DrawItemDrop(const Item *item, Vector2 pos);

#endif
