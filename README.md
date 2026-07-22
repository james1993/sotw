# Guild Wars 1 — 2D Demake (research + prototype)

A research project and working prototype exploring what a 2D top-down
"demake" of the original *Guild Wars* (2005, ArenaNet) would look like,
built in raylib/C.

## Start here

1. [`docs/research/gw1-mechanics.md`](docs/research/gw1-mechanics.md) —
   deep dive into GW1's actual design: professions, the 8-skill-bar
   system, the primary/secondary attribute rules, skill types and the
   counterplay web they create, resource economies, elite skill capture,
   heroes/henchmen, and why its "horizontal progression" model still
   holds up.
2. [`docs/design/demake-design.md`](docs/design/demake-design.md) —
   what survives a 2D top-down demake unchanged, what needs adaptation
   (camera, verticality), and what's explicitly out of scope for a first
   playable slice.
3. [`docs/design/raylib-architecture.md`](docs/design/raylib-architecture.md) —
   the technical architecture: a flat entity array (no OOP hierarchy), a
   small **data-driven skill/effect VM** (skills are JSON-shaped data
   composed from a fixed set of effect primitives, not one-off C
   functions per skill — this is the part that has to be right for the
   project to scale past a handful of skills), and how player, hero, and
   monster AI all funnel through the same activation code path.

## The prototype

`src/` is a working vertical slice validating the architecture above:
one player character (Warrior primary / Elementalist secondary), one
AI-controlled Hero companion (Elementalist), and one monster, fighting
in a top-down arena with a real 8-slot skill bar, energy/adrenaline
resource pools, cast bars, interrupts-by-type, and skill recharge —
built directly on top of the `Skill`/`EffectStep`/`Entity` data model
described in the architecture doc, not a simplified stand-in for it.

It has been **built and run in this session** (headless, via Xvfb) to
confirm the architecture actually compiles and works: skill activation,
resource costs, recharge timers, cast bars, and AI-driven combat were
all exercised and visually verified.

Presentation is procedural, no image assets: characters are layered-
shape sprites with movement-driven walk cycles, swing and cast
animations, and in-hand weapons matching what's equipped (robe color
tracks armor tier); combat plays slash arcs, school-colored bursts,
heal sparkles, AoE rings, and knockdown stars (`sprite.c`, `fx.c`).
The UI shares one GW1-flavored chrome — slate panels with gold trim
and corner ticks, beveled resource bars, and per-skill vector icons in
the bar with hover tooltips (`ui_theme.c`).

### Build

Requires a C compiler and CMake 3.15+. raylib is fetched automatically
via CMake `FetchContent` — no manual install step.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/sotw_demake
```

On Linux you'll also need the usual raylib X11/OpenGL dev packages if
they aren't already installed (`libgl1-mesa-dev libx11-dev libxrandr-dev
libxi-dev libxcursor-dev libxinerama-dev` on Debian/Ubuntu).

### Controls

- **Left-click** empty ground to move there.
- **Left-click** an enemy to target it (auto-walks into range and
  auto-attacks). **C** targets the nearest foe, **Tab** cycles foes —
  GW1's classic keys. **Escape** clears your target.
- **Left-click a party member** (in the world or their bar in the party
  window) to select them, so ally spells like Orison of Healing land on
  them; with no ally selected, ally spells fall back to casting on
  yourself, like GW1.
- **Left-click an NPC** in the outpost to talk: accept Captain Osric's
  quest, hire Little Thom, or trade with the Merchant.
- **1–5** activate your equipped skills (Monk bar: Orison of Healing,
  Banish, Smite, Bane Signet, plus Fire Bolt from the Elementalist
  secondary). Vekk, the hero companion, fights automatically the same
  way a GW1 hero does.
- **I** toggles the inventory (walk over drops to pick them up; click an
  item to equip it). **E** toggles the equipment screen - both slots
  with their numbers, plus a character summary; click a slot to cycle
  through compatible gear. **K** toggles the attributes panel for
  spending earned attribute points at GW1's real rank costs.
- **Scroll wheel** zooms. **M** opens the full-region map (the whole
  zone, its boundary, portals, quest markers, and every contact);
  **P** pauses with an in-game menu (Resume / Quit to Main Menu / Quit
  Game). The world freezes while paused.
- **Controller**: left stick moves (auto-attacks the nearest enemy in
  range), L2 + A/B/X/Y = skills 1–4, R2 + A/B/X/Y = skills 5–8.
  **L1/R1** cycle backward/forward through visible enemies (nearest
  first). **D-pad up/down** selects party members and **left/right**
  also cycles enemies, matching GW1's official gamepad scheme; bare
  **B** clears the target. Bare **X** (Xbox) / **Square** (PlayStation)
  talks: it walks you to the nearest NPC, opens their dialog in reach,
  and advances the conversation (accept quest, hire, browse) once a
  dialog is up. While a dialog or the merchant window is open, a
  GW1-style **menu cursor** appears: the **left stick** steers it, **A**
  clicks whatever it hovers (individual shop rows included), and **B**
  backs out of the conversation; touching the mouse hands the pointer
  back instantly. Bare **Y** opens the bags - inventory and equipment
  together (cursor + A equips or cycles a slot, B closes), **Select/Back** opens the region map, and **Start** opens
  the pause menu. The main menu and pause menu are fully pad-navigable:
  **D-pad** or **left stick** moves the highlight, **A** confirms
  (Up/Down + Enter on keyboard).

### Zones

The game starts in **Ashford Camp**, a GW1-style outpost: no combat,
service NPCs (quest giver, merchant, hireable henchman), and a portal to
**Ashford Plains** — a combat instance that reloads fresh on every
entry, exactly like GW1's per-party explorable areas. Returning to the
outpost fully restores the party, and party members wait near the gate
while you wander the outpost instead of trailing you around town -
they only fall in behind you out in the field.

Every zone has a hard **boundary** - drawn as a wall line in the
world, dashed on the compass, and framing the region map - so the
instance's edges are never a mystery.

**Armor is crafted, never looted** - GW1's rule. Charr drop **Charr
Hides** (they stack), and Armorer Dunda in the camp turns hides plus
gold into raiment. The merchant deals in weapons and **kits**: dropped
weapons come up **unidentified** (masked name, hidden stats, can't be
equipped, nearly worthless to sell) until an **Identification Kit**
reveals them, and a **Salvage Kit** breaks unwanted gear into hides for
crafting - GW1's full kit economy. Click a kit in the inventory to arm
it, then click the target item; kits carry 25 uses and vanish when
spent.

Charr camp out in small **groups** (never more than four) that share
aggro like GW1 mobs: pull any member — by proximity, a landed hit, or a
spell — and the whole camp answers. Patrols hunt alone; their threat is
walking into your fight at the wrong moment.

Captain Osric's quests chain GW1-style: finish *Charr at the Gate*
(kill quest) and he offers *Scout the Eastern Ridge* (reach the green
flag marker deep in the plains).

### Death

Dying costs a party member 15% of max health and energy (GW1's death
penalty), stacking to -60% and shown in the party window; it clears
when you rezone. If part of the party falls but the fight is won, the
survivors revive them shortly after combat ends. If the whole party
wipes, everyone respawns at the zone's resurrection shrine, penalty
intact. Hired henchmen can be dismissed from the party window — in the
outpost only, GW1's rule for party changes.

### Saving

The game saves like GW1: the **character** persists, the world doesn't.
Level, XP, attributes, skill bar, gold, inventory, equipment, quest log,
and party composition are autosaved on zone transitions, quest changes,
hiring/dismissing, and quit - there is no save button. Loading (the
menu's *Continue*) logs you back in at the last outpost you visited,
party restored; explorable instances are never saved, so re-entering
one always spawns it fresh. One slot, plain text, at
`$XDG_DATA_HOME/sotw-demake/save.txt` (Linux, default
`~/.local/share/...`) or `%APPDATA%\sotw-demake\save.txt` (Windows).

### What's deliberately not built yet

Only two professions' worth of sample skills (see `src/skill.c`), one
arena, no outpost/skill-trainer/attribute-panel UI, no tilemap/LoS, no
JSON skill loading (skills are inline C data for now — see
`docs/design/raylib-architecture.md` #3 for the planned JSON format).
These are content/scope gaps, not architecture gaps: the effect VM,
entity model, and AI loop the doc describes are what's actually running,
so extending to more skills/professions is additive work on top of a
validated foundation, not a rewrite.

### Known limitations

- **macOS Retina renders at logical (non-Retina) resolution.** raylib's
  `FLAG_WINDOW_HIGHDPI` is deliberately off: with OS display scaling it
  made `GetScreenWidth()` and `GetMousePosition()` disagree, sending
  click-to-move wildly off target. Coordinates staying consistent beats
  crispness, so Retina displays get an OS-upscaled (slightly soft)
  image until the flag can be re-enabled together with mouse-coordinate
  scaling and verified on real hardware.
- **Gamepad support is desk-checked, not hardware-tested.** The mapping
  (left stick move, L2/R2 + face buttons for skills, D-pad targeting)
  follows raylib's XInput-style layout; DirectInput pads that report
  triggers or the D-pad differently may need remapping.
