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

Attacks use a shared anticipation curve rather than a symmetric ease —
the body winds *back*, snaps forward far faster than it withdrew, then
recovers. Every species is driven by it, so a blow has a readable
telegraph and a visible impact frame: Charr crouch and flare their
mane, thrust the head, gape the jaw and rake a clawed arc with a motion
streak; Devourers rear up, drive the shell forward, whip the stinger
and clash their pincers shut.
The UI shares one GW1-flavored chrome — slate panels with gold trim
and corner ticks, beveled resource bars, and per-skill vector icons in
the bar with hover tooltips (`ui_theme.c`), a gilt compass bezel, and a
mounted rail behind the skill bar.

Everything that floats over an entity — nameplates, health and cast
bars, quest markers, loot labels, the interact prompt — is drawn in a
separate screen-space pass (`ui_world.c`) rather than inside the camera
transform, so text stays crisp and plates stay the same size at any
zoom. That pass also decides *what* deserves a label: idle monsters stay
anonymous until they're targeted, hovered, awake, or hurt, which keeps a
quiet field from becoming a wall of floating text. Plates are laid out
before anything is drawn and de-collided — nearest-to-camera keeps its
place and the ones behind step up above it, capped so a crowded melee
lifts a plate rather than flinging it off-screen — so four characters
piled on the same spot still read as four separate nameplates.

The interface is built from shared design tokens rather than per-panel
magic numbers: a 4px spacing scale, a five-step type scale, and
semantic colors (`UI_TEXT_PRIMARY`, `UI_POSITIVE`, `UI_SURFACE`…), plus
common `UI_Row` / `UI_Button` / `UI_Tabs` widgets. One implementation of
"a list row" means the merchant, the trainer, the bags and the skill
list all *behave* identically, which is most of what makes an interface
feel finished.

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
  GW1's classic keys. **Escape** clears your target. Target cycling only
  ever walks *hostile* foes: friendly NPCs are never in the rotation.
  Whatever is targeted gets a ground reticle with rotating ticks, a gold
  nameplate, and a gold outline on its floating health bar.
- **Left-click a party member** (in the world or their bar in the party
  window) to select them, so ally spells like Orison of Healing land on
  them; with no ally selected, ally spells fall back to casting on
  yourself, like GW1.
- **Walk up to an NPC and press F** to talk: accept a quest, hire Little
  Thom, or trade. Standing in range lights that NPC with a green ground
  reticle and a floating **Talk to \<name\>** prompt showing the button,
  so there's never a question about who you're about to speak to — and
  the prompt marks the exact NPC the key acts on. **Left-clicking** an
  NPC works too (and walks you over if you're out of range).
- **1–8** activate whatever you've slotted on your skill bar. Vekk, the
  hero companion, fights automatically the same way a GW1 hero does.
- **L** opens the **skills panel** — your eight-slot bar on top, every
  skill you've learned underneath. Click a slot to arm it, then click a
  skill to place it (click the armed slot again to empty it). Like GW1,
  the bar can only be rearranged **in an outpost**: you commit to a
  build before you head out and live with it in the field.
- **I** toggles the inventory (walk over drops to pick them up; click an
  item to equip it). **E** toggles the equipment screen - both slots
  with their numbers, plus a character summary; click a slot to cycle
  through compatible gear. **K** toggles the attributes panel for
  spending earned attribute points at GW1's real rank costs.
- **Scroll wheel** zooms. **M** opens the full-region map (the whole
  zone, its boundary, portals, quest markers, and every contact).
- **P** (or **Start**) opens the in-game menu, which is the hub for
  everything: Resume, Skills & Build, Equipment, Inventory, Attributes,
  Region Map, then Quit to Main Menu / Quit Game behind a divider. Each
  row shows its own keyboard shortcut, and picking a screen closes the
  menu and opens it. On a controller the panels have scattered bindings
  (Y, R3, Select) that nobody should have to memorise — Start leads
  everywhere. The world freezes while the menu is up.
- **Controller**: left stick moves (auto-attacks the nearest enemy in
  range), L2 + A/B/X/Y = skills 1–4, R2 + A/B/X/Y = skills 5–8.
  **L1/R1** cycle backward/forward through visible enemies (nearest
  first, hostiles only). **D-pad up/down** selects party members and
  **left/right** also cycles enemies, matching GW1's official gamepad
  scheme; bare **B** clears the target. Bare **X** (Xbox) / **Square**
  (PlayStation) talks to the NPC you're standing next to — the one the
  floating prompt is pointing at — and advances the conversation
  (accept quest, hire, browse) once a dialog is up. While a dialog or the merchant window is open, a
  GW1-style **menu cursor** appears: the **left stick** steers it, **A**
  clicks whatever it hovers (individual shop rows included), and **B**
  backs out of the conversation; touching the mouse hands the pointer
  back instantly. Bare **Y** opens the bags - inventory and equipment
  together (cursor + A equips or cycles a slot, B closes), **Select/Back** opens the region map, and **Start** opens
  the pause menu. The main menu and pause menu are fully pad-navigable:
  **D-pad** or **left stick** moves the highlight, **A** confirms
  (Up/Down + Enter on keyboard).
- You don't have to memorise any of this: a **control legend** sits
  along the bottom of the screen listing every binding, and it swaps to
  pad glyphs the moment a controller is detected. Each panel also carries
  its own hotkey badge in its title bar and a close box.

### Zones

The world is a four-zone chain. The game starts in **Ashford Camp**, a
GW1-style outpost: no combat, service NPCs (quest giver, merchant,
armorer, skill trainer, hireable henchman), and a portal to **Ashford Plains** — a
combat instance that reloads fresh on every entry, exactly like GW1's
per-party explorable areas. Past the plains' eastern ridge lie the
**Charr Foothills**, a harder explorable where warbands field **Charr
Shamans** (ranged Fire Magic casters worth killing first) and the
gullies crawl with **Devourers** — a second species with its own look
and no hides to salvage. The road ends at **Piken Watch**, a forward
outpost where **Warmaster Grast** offers his own quest book (kill,
collect, and item-reward quests) and a trader keeps you stocked.
Returning to any outpost fully restores the party, and party members
wait near the gate while you wander the outpost instead of trailing
you around town - they only fall in behind you out in the field.

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

### Conditions and hexes

Two separate families of affliction, kept separate on purpose — that
separation is what forces a bar to carry answers for both instead of
one catch-all cleanse.

**Conditions** are physical and every one of them changes what you can
do, not just your health bar:

| Condition | Effect |
|---|---|
| Bleeding | 2 health per second |
| Burning | 5 health per second |
| Crippled | Movement speed halved — this is what takes kiting away |
| Weakness | Your attacks deal 25% less damage |

**Hexes** are magical, and punish what the target *does* rather than
grinding it down. GW1 puts most hexes on Necromancer and Mesmer, which
this demake doesn't have, so Smiting Prayers carries them:

- **Shroud of Doubt** — the target attacks 50% slower, plus light
  degeneration.
- **Price of Faith** — the target loses health every time it *attacks*,
  charged on the swing itself so it costs them even on a miss.

Reapplying an affliction refreshes its duration rather than stacking a
second copy, exactly as GW1 does.

Removal is split to match: **Mend Ailment** (Healing Prayers) strips one
condition and heals; **Smite Hex** (Smiting Prayers) strips one hex and
heals. Neither can touch the other category. Both take the
longest-remaining affliction first, so a cleanse removes the one you'd
otherwise be stuck with.

Monsters apply them too, which is what makes carrying removal worth a
slot: Charr use **Rending Claws** (Bleeding + Weakness), Devourers use
**Hobbling Strike** (Crippled — the reason you can't stroll out of their
gully), and Charr Shamans hex as well as burn.

Everything is visible on **foes as well as allies**: coloured pips ride
under every nameplate (round for conditions, diamond for hexes, so the
category reads without relying on colour), and the focused target's
panel lists each affliction by name with a live countdown —
*"Bleeding 5s"*, *"Weakness 3s"* — which is the information a cleanse
decision actually needs.

### Skills and build-crafting

A new character knows **one skill** and has seven empty slots. Filling
them is the game — in GW1 the eight-slot bar is the *output* of a
collection loop, not something handed to you at creation, so the demake
starts you nearly empty and makes every skill something you went and
got:

- **Quests** are the main early source. Each one names its skill reward
  up front (*"Accept: Charr at the Gate (250 XP, 100g, Smite)"*), and
  learning it throws a banner across the screen.
- **Skill trainers** (Master Ilsa in Ashford Camp, Adept Kerra at Piken
  Watch) sell skills for **1 skill point plus gold**, with the gold
  price climbing on every purchase. You earn a skill point per level.
  Their stock is filtered to what your two professions can actually
  use — a Mo/E is offered Monk and Fire Magic skills, but never Energy
  Storage, which is the Elementalist *primary* attribute.
- **Elites are never sold.** The only way one reaches your bar is
  killing the boss that uses it: **Kruul the Emberfang** in Ashford
  Plains teaches Meteor, **Vharn the Bonesmith** in the foothills
  teaches Healing Light. Bosses are bigger, hit harder, and are worth
  hunting specifically — GW1's Signet of Capture, condensed.

Everything you own is saved with the character, and a skill can't sit on
your bar unless the book backs it.

Quests are per-giver, GW1-style: Captain Osric's chain in Ashford
(finish *Charr at the Gate* and he offers *Scout the Eastern Ridge*,
whose marker points at the foothills gate), and Warmaster Grast's book
at Piken Watch — *Silence the Shamans* (Warmaster's Hammer plus
Banish), *Clear the Gullies* (an Identification Kit plus Fire Bolt), and the
collect quest *Hides for the Watch*, which consumes 4 Charr Hides on
turn-in and tracks your bag live. Item rewards need a free bag slot,
and each giver's green "!" only lights for their own work.

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
