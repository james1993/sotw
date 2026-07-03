# Guild Wars 1 → 2D Demake: Design Decisions

This document takes the systems in [`gw1-mechanics.md`](../research/gw1-mechanics.md)
and decides, system by system, what survives a 2D top-down demake
unchanged, what needs adaptation, and what should be deliberately cut for
a first playable slice.

## Guiding principle

GW1's depth is almost entirely in the **skill/attribute/build layer**,
which is pure data and math — dimension-independent. Its *feel* comes
from **positioning under a fixed top-down-ish camera** (aggro bubbles, AoE
footprints, kiting, LoS pulls), which a 2D top-down view reproduces
almost natively — arguably better, since the original game's 3D camera
never really used verticality for tactics anyway (missions are functionally
flat combat spaces with occasional elevation used for LoS blocking, not
platforming). So: **a 2D top-down demake is not really a compromise for
GW1 specifically** — closer to restoring the game to the genre (Diablo-era
ARPG) it was always mechanically adjacent to.

## 1. Camera & movement — keep click-to-move, top-down

**Decision:** Fixed top-down (or slight isometric-feeling but
orthogonally-projected) camera, following the player. Movement is
**click-to-move**: left-click empty ground moves you there, left-click a
foe targets + auto-attacks (walks into range first), matching the
original exactly.

Why not WASD: GW1's kiting game depends on the fact that turning to
retarget/reposition doesn't cost "aim" the way twin-stick WASD+aim does —
kiting is a positioning puzzle against known projectile speeds and skill
ranges, not a reflex-aim game. Click-to-move preserves that. It also
keeps melee/ranged range rings, aggro circles, and AoE footprints legible
as literal circles drawn on the ground, which is cheap and readable in
raylib's immediate-mode 2D drawing.

WASD can be added later as an accessibility/alternate control scheme
without touching combat math, since range/targeting logic doesn't care
how the player's position got there.

## 2. Positioning mechanics — translate ~1:1

- **Aggro bubble** → a radius around the party centroid; unchanged.
- **AoE skills** (nova, ward, well) → literal 2D circles/rectangles on the
  ground, drawn with alpha-blended `DrawCircleV`/`DrawRectangleRec`, using
  the *same radii* as the original (GW1 publishes AoE radii in fixed
  in-game distance units — we adopt the same unit so tuning data ports
  directly).
- **Projectile travel time** (arrows, spells) → real projectile entities
  with a travel speed, meaning a target that moves after the shot is
  fired can dodge it — a mechanic 3D GW1 has but many players don't
  consciously notice. In 2D top-down this becomes *very* visible and is a
  genuine gameplay upgrade opportunity (skill expression via
  projectile-dodging), not just a faithful port.
- **Melee/ranged range rings** → concentric circles around each unit,
  toggleable overlay, same idea as GW1's "aggro circle" addon that became
  semi-standard in the community.
- **Line of sight** → GW1 uses LoS heavily (pulling a caster around a
  corner breaks their spell). In 2D this is a straightforward tile/wall
  raycast (Bresenham line against a collision grid) — computationally
  trivial versus the original's 3D navmesh raycasts, and gives us LoS
  "for free" from the tilemap we need for movement collision anyway.
- **Verticality/elevation** — genuinely lost. GW1 uses cliffs/ramps
  occasionally to block LoS or funnel movement. We approximate with
  "elevated" tiles that block LoS and movement like a wall, without a
  true Z-axis. This is an acceptable, low-cost simplification: elevation
  in GW1 is used as a *wall/LoS-blocker with a fancier skin*, not as
  jump-and-fall platforming.

## 3. Skill system — preserve type hierarchy and resource math exactly

This is the one place where faithfulness matters more than anywhere
else, because the *type system* (§5 of the research doc) is the whole
metagame. Concretely:

- Every skill keeps its **type** (Spell/Enchantment/Hex/Stance/Signet/
  Shout/Attack Skill/Preparation/Trap/Ritual/...) as first-class data,
  because removal skills, interrupts, and counters all key off type, not
  effect.
- Attribute scaling formulas (usually linear or near-linear in rank, per
  the wiki's per-skill tables) are ported as data, not re-balanced,
  for the first pass — the goal is "recognizably GW1," not "a new game
  with GW1 flavor."
- The four resource types (Energy, Overcast, Adrenaline, Health
  Sacrifice) are kept **separate and non-fungible**, exactly as in the
  original, because that separation is what makes professions feel
  mechanically distinct at a resource level, independent of their skill
  effects.
- Cast time + interruptibility is preserved: a visible cast bar over the
  caster's head, interrupt skills that key off "is this unit currently
  casting a Spell," touch/shout/stance/signet skills exempted, same as
  original.

**Adaptation, not preservation:** authoring ~1000 skills in hand-written C
is unmanageable and not the point of a first playable. See
[`raylib-architecture.md`](raylib-architecture.md) for the data-driven
effect system that lets skills be authored as data (JSON) instead of code,
which is the only way this scales past a handful of professions.

## 4. Professions — build the pattern with 2, generalize to 10

**Decision for v1 slice:** implement two professions all the way through
(recommend **Warrior** — melee/adrenaline/no cast bar — and
**Elementalist** — ranged spell/energy/overcast — because they exercise
maximally different resource and combat-range mechanics) plus the
secondary-profession attribute rules, rather than spreading thin across
all ten. Every profession after the first two is "more data," not "more
engine" if the effect system is built right — that's the point of getting
the architecture right early.

## 5. Attribute/build system — keep the 200-point / 12-cap math intact

Direct port: 200 attribute points, 0–12 unmodified rank cap, primary
attribute locked to primary profession, secondary profession contributes
secondary attributes only. This is pure UI + arithmetic and has no 3D
dependency at all — the "Skills and Attributes Panel" becomes a 2D menu
screen, functionally identical to the original's.

## 6. Heroes and Henchmen — a strong fit for a solo-scoped demake

Since a first playable is very likely single-player (no netcode budget —
see §9), **Heroes are not optional polish, they're the mechanism that
makes solo play work**, exactly as they were built to do in Nightfall.
Recommendation: skip Henchmen (fixed, low-value AI) for v1 and go straight
to a Hero companion with a player-editable skill bar/attributes — it's
more valuable per unit of engineering effort, since the AI decision layer
(target selection, skill usage, flag/stay commands) is needed either way
and a fixed henchman doesn't teach us anything the hero AI doesn't.

## 7. PvE progression — keep the shape, shrink the scale

Faithful for v1, scaled down:

- Skill trainers in the outpost (buy skills with gold + skill points).
- Elite capture via boss kill + a capture item, at a boss placed in the
  one explorable zone — proves the loop with one skill before scaling to
  hundreds.
- One title track (recommend **Vanquisher**: clear every monster in the
  zone) as proof-of-concept for the "horizontal progression" pillar,
  since it requires zero new UI beyond a counter and is a pure
  content-complete check.

Deferred: the other ~39 title tracks, Hard Mode, dye economy, skin
rarity — all pure content/data additions once the core loop and effect
system are proven, not architecture problems.

## 8. Itemization — implement the fixed-power model directly

Because GW1's itemization deliberately avoids a power treadmill, this is
actually the *cheapest* system to demake correctly: weapon damage by
*type* (not rarity), armor value by *profession tier* (not rarity), dye
as a **pure tint** operation. This last one is a great raylib fit —
`DrawTextureEx`/sprite tinting via a `Color` multiply is literally what
raylib's tint parameter does, so "dye" costs approximately zero extra
code beyond a color picker UI and a per-armor-piece tint field.

## 9. Multiplayer / PvP — explicitly out of scope for v1

The original's GvG/HA/AB metagame is a huge part of why the design is
celebrated, but raylib has no built-in networking, and building
authoritative netcode is a separate, large project from "prove the GW1
build system works in 2D." Recommendation: scope v1 as **single-player +
AI heroes**, architect the simulation layer (see architecture doc) so
combat state updates are deterministic and side-effect-free enough that a
later networked mode (e.g. via ENet, lockstep or client-authoritative with
server reconciliation) isn't precluded, but don't build it now.

## 10. Explicit non-goals for a first playable

- All 10 professions / ~1000 skills (start with 2 professions, ~20-30
  skills, expand via data once the pattern is proven).
- Dye economy, skin rarity, trade/auction UI.
- Multiplayer/PvP netcode.
- Cinematics, voice, full campaign narrative structure — one outpost + one
  explorable zone + one boss is enough to validate the loop.
- Elevation/verticality beyond LoS-blocking "tall" tiles.

## Summary: what makes this demake worth doing

GW1 is unusually well-suited to a 2D demake compared to most "de-make a
3D game" projects, because its actual design innovation — the skill
type system, the dual-profession attribute budget, the AI-hero party
system, and horizontal (not vertical) progression — was never really
about 3D space to begin with. The risk in this project isn't "can 2D
represent GW1's combat" (yes, cleanly), it's "can the *content authoring
pipeline* scale to GW1's actual skill count without becoming an
unmaintainable pile of special-cased C code" — which is why the next
document is entirely about the data-driven skill/effect system.
