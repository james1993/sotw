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
a player character of any of the six Prophecies professions, one
AI-controlled Hero companion (Elementalist), and one monster, fighting
in a top-down arena with a real 8-slot skill bar, energy/adrenaline
resource pools, cast bars, interrupts-by-type, and skill recharge —
built directly on top of the `Skill`/`EffectStep`/`Entity` data model
described in the architecture doc, not a simplified stand-in for it.

It has been **built and run in this session** (headless, via Xvfb) to
confirm the architecture actually compiles and works: skill activation,
resource costs, recharge timers, cast bars, and AI-driven combat were
all exercised and visually verified.

Characters are procedural, not sprite sheets: layered shapes with
movement-driven walk cycles, swing and cast animations, and in-hand
weapons matching what's equipped (robe color tracks profession and
armor tier); combat plays slash arcs, school-colored bursts, heal
sparkles, AoE rings, and knockdown stars (`sprite.c`, `fx.c`). That
stays procedural on purpose — it's what lets equipment and appearance
drive the figure instead of a fixed frame doing it.

Where a *fixed* image is the right answer, the game uses real
open-source art rather than hand-drawn vector approximations: skill
icons, two display/body typefaces, and tiled ground textures. See
[Assets](#assets) below, and `assets/CREDITS.md` for who made what.

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

### Assets

All bundled art is open-source and credited in `assets/CREDITS.md`, with
the licence text alongside each pack. The title screen carries a short
credit too, because that's what CC BY asks a game for.

**Skill icons** — [game-icons.net](https://game-icons.net), CC BY 3.0.
Thirty-one hand-authored vector glyphs in a `switch` used to draw the
skill bar; that neither scaled to new skills (nine of them had no glyph
at all) nor looked like a painted GW1 icon. Now `tools/build_icon_atlas.py`
reads `assets/icons/skills.manifest`, rasterises each source SVG to a
white silhouette and packs them into one 1024×512 atlas, which the game
**tints** per skill — so one greyscale sheet serves every school colour
instead of shipping the same picture eight times.

The atlas is indexed by `SkillId`, and the script parses the enum out of
`src/skill.h` and refuses to run if the two disagree, naming the slot
that drifted. The generated atlas is committed, so building the game
needs no Python, no network and no SVG rasteriser — same deal as the
bundled font. Regenerate with:

```bash
python3 tools/build_icon_atlas.py          # from the committed SVGs
python3 tools/build_icon_atlas.py --fetch  # download any that are missing
```

**Fonts** — [Cinzel](https://fonts.google.com/specimen/Cinzel) and
[Alegreya Sans](https://fonts.google.com/specimen/Alegreya+Sans), both
SIL OFL. Cinzel is a Roman capital serif and carries titles, window
headers and zone names — the places GW1 sets its own display type. It is
deliberately *not* available to body text: what makes it good at 40px is
what makes it unreadable at 11px. Alegreya Sans **Medium** does the dense
work; Regular was tried first and measurably lost against DejaVu in the
HUD, which is the kind of thing you only find by looking at a screenshot
of the actual hint bar. DejaVu stays as the fallback.

**Controller.** Two different idioms, chosen per screen rather than one
applied everywhere. The character creator is *spatial* — three columns
and a grid of colour swatches — so it gets the virtual cursor: the left
stick steers a pointer and X/A clicks. NPC conversations are *lists*, so
they get a focus index instead (`ui_focus.c`): D-pad or stick steps
through the replies, X (PlayStation) / A (Xbox) takes the highlighted
one, and B/Circle backs out one layer at a time — the shop window first,
then the conversation. Steering a pointer onto a reply is work the
player shouldn't have to do.

The same focus runs off the arrow keys and Enter, which is deliberate:
keyboard players get list navigation for free, and the whole path can be
exercised on a machine with no controller attached.

**Sound** — [Kenney](https://kenney.nl) audio packs, CC0, plus five clips
generated by `tools/synth_sfx.py`. Sounds are keyed by *what a thing is*,
not by which skill did it: attack skills split on range, and spells are
chosen from their own effect steps, so a skill that heals sounds like
healing whichever profession owns it and every skill added later gets
audio for free rather than silently getting none.

No CC0 pack anywhere covers magic or archery, which is the one gap; the
casting and bow sounds are synthesised instead of taking on per-asset
licence checking across several sites for five clips.

Three things that matter more than the clips themselves. A raylib `Sound`
cannot overlap itself, so each one keeps a ring of four aliases — four
party members landing hits on the same frame is normal. A 45ms
same-sound retrigger guard stops an AoE hitting five foes from firing
five identical impacts a millisecond apart, which reads as one clipped
blast rather than five hits. And impacts are gated on there being an
attacker, so condition ticks stay silent instead of machine-gunning one
impact per second per affliction per character.

Sound covers navigation as well as clicks: a quiet tick as focus steps,
a click on confirm, and a distinct refusal on anything you can't afford
or aren't allowed to do yet — including a press on a disabled button,
because "nothing happened" is the case that most needs feedback.

Volume lives in the pause menu (`Sound: 70%`, stepping to off) and
persists in the save. With no sound device at all — a headless box, a
container — `Audio_Init` says so once and every call after it is a
no-op, the same policy the font and icon loaders follow.

**Ground** — [Kenney](https://kenney.nl) Pattern Pack, CC0. The world was
a flat fill plus a lattice of grid lines, which read as a level editor.
Two seamless patterns now tile the surface — laid stone inside a
settlement, broken ground outside it — stored as alpha masks and tinted
with the zone's own ground colour, so the zone table stays the single
source of what a place looks like. The grid survived at about half its
old weight: it still gives distance a scale you can count, but with the
surface carrying the detail, the lattice is the one that should yield.

Deliberately *not* imported: character sprite sheets. LPC would fit the
layered-equipment model, but it's CC BY-SA (share-alike on derived art)
and fixed frames would replace the procedural animation and the
appearance system the character creator previews live. That's trading a
working system for art.

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
- **1–8** activate whatever you've slotted on your skill bar. Cynn, the
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

### Setting: Pre-Searing Ascalon

The prototype is set in **pre-Searing Ascalon** — GW1 Prophecies' opening
region, before the Charr burn it. Nine zones, laid out as GW1 lays them
out (research and the liberties taken are written up in
`docs/research/pre-searing.md`):

| Zone | Kind | What's there |
| --- | --- | --- |
| **Ashford Abbey** | outpost | Where you start. Brother Mhenlo (Monk trainer), Abbot Ciglo, Merchant Niles, Armorer Dunda, Little Thom |
| **Lakeside County** | explorable | The gentle first field: River Skale and Moa Birds |
| **Ascalon City** | outpost | The capital and the hub. Sir Tydus, Prince Rurik, Halbrik (skills), Merchant Vassar, Armorer Gali |
| **Green Hills County** | explorable | Warrior country and the theatre. Grawl warbands, Warmaster Grast (Warrior), Lady Althea (Mesmer) |
| **Regent Valley** | explorable | Ranger country. Bandits and giant spiders, Master Ranger Nente (Ranger), Duke Barradin |
| **Wizard's Folly** | explorable | Elementalist country. Aloes and skale, Elementalist Aziure |
| **The Catacombs** | explorable | The undead below the abbey. Necromancer Munne |
| **The Northlands** | explorable | The Charr frontier — the only place the war is real yet |
| **Piken Square** | outpost | Reforged Mode only. Warmaster Riga, Trader Hurm, Adept Kerra, Armorer Sten |

The **six Prophecies professions each have one trainer**, scattered so
that finding a second profession means actually leaving the city:
Monk at the abbey, Warrior and Mesmer in Green Hills, Ranger in Regent
Valley, Elementalist in Wizard's Folly, Necromancer in the Catacombs.
Sir Tydus' quest **A Second Profession** is what sends you looking.

Eight quests run off canon givers — Abbot Ciglo's tithe, Sir Tydus'
second profession, Prince Rurik's two Charr chains, Duke Barradin's
bandits and aloes, and Warmaster Riga's pair in Piken Square.

Enemies are drawn from the pre-Searing bestiary and each species has its
own silhouette: **Skale** (hunched amphibians, dorsal crest), **Moa
Birds** (long legs, sweeping neck, harmless unless provoked),
**Grawl** (bowed legs, heavy shoulders, club), **Devourers** and the
giant spiders that share their build, **Undead** (gappy ribcage, rusted
blade), **Aloes** (rooted — they never move, they only sway), and the
**Charr** themselves.

### Reforged Mode

**Reforged Mode** is a per-character toggle offered at creation, in the
appearance column. It is not a difficulty slider you can flip later —
you choose it when you make the character, and the save remembers it.
Turning it on changes four things:

- **Piken Square opens.** The Northlands grows a second portal to a
  forward outpost with its own trader, armorer, skill trainer and quest
  giver. Without Reforged the portal is not merely locked, it is never
  drawn — a character who can't go there is never shown a door.
- **More Charr in the north.** Four extra spawns, including the boss
  **Bonfaaz Burntfur**, so reaching Piken Square is a fight rather than
  a walk.
- **Foes hit softer.** Every monster loses 15% of its health and 5
  armor, verified in play: a Charr Grunt drops from 200 HP / AL 50 to
  170 HP / AL 45.
- **+5% XP and gold** on quest turn-ins and minted drops.

### Zones

Every zone is closed in by a **mountain ridge** rather than a drawn
line. The ridge is generated around the zone's perimeter as a chain of
overlapping rock masses, each set back from the edge by a different
amount, so the walkable region is an irregular blob and what stops you
is terrain you can see and walk up to. Gaps are carved around every
portal and resurrection shrine, so the way out is never buried. The
compass plots the ridge as stone, and the region map draws its real
shape. Outposts have no combat and full service NPCs; explorables
reload fresh on every entry, exactly like GW1's per-party instances. Returning to any outpost fully restores the
party, and party members wait near the gate while you wander the outpost
instead of trailing you around town - they only fall in behind you out
in the field.

**Armor is crafted, never looted** - GW1's rule. Charr drop **Charr
Hides** and skale drop **Skale Fins** (both stack), and the armorers
turn hides plus gold into raiment. The merchant deals in weapons and
**kits**: dropped weapons come up **unidentified** (masked name, hidden
stats, can't be equipped, nearly worthless to sell) until an
**Identification Kit** reveals them, and a **Salvage Kit** breaks
unwanted gear into materials for crafting - GW1's full kit economy.
Click a kit in the inventory to arm it, then click the target item;
kits carry 25 uses and vanish when spent.

Monsters camp out in small **groups** (never more than four) that share
aggro like GW1 mobs: pull any member — by proximity, a landed hit, or a
spell — and the whole camp answers. Patrols hunt alone; their threat is
walking into your fight at the wrong moment.

### Character creation

**New Game** opens a GW1-style creator rather than dropping you into a
preset character. Three columns: profession on the left, a **live
preview** in the middle, appearance on the right, name underneath.

The preview is the real thing — it builds an actual `Entity` and runs it
through the same `sprite.c` the world uses, so it can't drift out of sync
with what you'll actually see. Change profession and the robe colour,
weapon and title update immediately.

**All six Prophecies professions** are playable — Warrior, Ranger, Monk,
Necromancer, Mesmer, Elementalist. Factions and Nightfall professions
are out of scope, so they're absent rather than half-built.

Profession is a real decision, not a label. Each one issues a different
starting kit, and armour comes from the armour piece rather than a
number written twice:

| | Energy | Armour | Weapon | Starting skill |
|---|---|---|---|---|
| Warrior | 20 | AL 40 | Ascalon Sword | Gash (adrenaline) |
| Ranger | 20 | AL 35 | Ascalon Longbow | Power Shot |
| Monk | 20 | AL 30 | Smiting Rod | Orison of Healing |
| Necromancer | 20 | AL 30 | Bone Idol | Vampiric Gaze |
| Mesmer | 20 | AL 30 | Jeweled Wand | Ether Feast |
| Elementalist | 29 | AL 30 | Kindling Staff | Fire Bolt |

Everyone starting on 20 energy is not an oversight — it's GW1's rule.
The Elementalist's 29 is 20 plus three ranks of Energy Storage, which is
exactly why that attribute is their primary and why a *secondary*
Elementalist never gets it.

The panel shows those numbers, the accessible attribute lines (with the
primary marked), a one-line pitch, and — in gold — what the primary
attribute actually *does*, since that's the half of the choice you can
never take back.

Appearance is procedural like everything else: sex, hair style, and
palettes for skin and hair colour, all read straight into the shapes the
sprite renderer draws. Name is typed, capped at 20 characters, and a
nameless character can't be created.

**Secondary profession is deliberately absent from creation.** GW1 makes
you earn it in-game once you've actually played the primary, so a
freshly created character is single-profession — the nameplate reads
*"Sera (Mo)"* — and the creator says as much. See *Prophecies pacing*
below for when you get one.

### Prophecies pacing: the second profession

Prophecies does two separate things with your secondary, a campaign
apart, and the distance between them is the design. The demake keeps
both halves:

- **Six profession trainers**, one per profession, scattered across the
  county will grant you a second profession — but only once Sir Tydus'
  *A Second Profession* is done. Before that they tell you so, by name:
  "Requires: A Second Profession." Each trainer teaches only their own
  calling, so which one you walk to *is* the choice.
- **Nicholas the Restless** is out in the Northlands and is the only one
  who will *change* it, and only at **level 10 or above**. Under that,
  the button reads "Requires: level 10 (you are 6)."

A build you can rewrite on a whim isn't a build, it's a menu. Changing
your secondary refunds every attribute point sunk into the old one's
lines (GW1 refunds them too — it has to, or the change costs you a chunk
of character with no way to earn it back) and clears any bar slot holding
a skill you can no longer use.

### GW1's actual numbers

Armour, energy, regeneration and the primary-attribute effects are GW1's
real formulas, not approximations of them. They live in one file,
`src/gwmath.h` / `gwmath.c`, so each one can be checked against
`docs/research/gw1-mechanics.md` §14 rather than hunted for across call
sites.

**Armour** scales incoming damage by `2^((60 - AL) / 40)`: every +40 AL
halves the damage, every -40 doubles it, AL 60 is neutral. Armour
penetration removes a fraction of the target's AL *before* the curve, so
the same penetration is worth much more against a Warrior than a caster.

**Energy** is 20 base for everyone, +3 per rank of Energy Storage.
**Regeneration is in pips**: one pip is 1 energy per 3 seconds, everyone
has 3, so the baseline is **1 energy per second in real time** — the same
clock every energy cost in GW1 is balanced against. The pip count is a
per-character field, and the arrows drawn in the energy bar read it, so
anything that ever moves regen moves the readout with it.

**Health** is 100 at level 1, +20 per level.

Every primary attribute carries its real mechanic:

| Primary | Per rank | Applies to |
|---|---|---|
| Strength | 1% armour penetration | attack skills only |
| Expertise | -4% energy cost | attack skills |
| Divine Favor | +3.2 healing | Monk spells cast on an ally |
| Soul Reaping | +1 energy on a nearby death | any death, max 3 per 15s |
| Fast Casting | cast time × `2^(-rank/15)` | spells only |
| Energy Storage | +3 maximum energy | always |

Spending a point into Energy Storage moves your maximum energy on the
spot, in the attributes panel — the feedback is what makes a primary
attribute legible.

Monster skills sit on an attribute (`ATTR_MONSTROUS`) that belongs to no
profession at all, so Claw Swipe and friends fall out of every trainer
list and attribute panel automatically. The check is ownership, so
there's nothing to remember to filter.

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
grinding it down. There are two hex *mechanics*, named for what they do
rather than for a skill, because GW1 has dozens of hexes that all mean
"your attacks come slower" and several professions need to reach them:

- **Faltering** — the target attacks 50% slower, plus light
  degeneration. Applied by *Shroud of Doubt* (Smiting Prayers) and
  *Faintheartedness* (Curses).
- **Backlash** — the target loses health every time it *attacks*,
  charged on the swing itself so it costs them even on a miss. Applied
  by *Price of Faith* (Smiting Prayers) and *Empathy* (Domination).

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
  learning it throws a banner across the screen. The skill is resolved
  against *your* professions rather than hard-named, preferring your
  primary's lines — with six professions in play, a fixed reward would
  be dead loot to four of them.
- **Skill trainers** (Halbrik in Ascalon City, Adept Kerra in Piken
  Square) sell skills for **1 skill point plus gold**, with the gold
  price climbing on every purchase. You earn a skill point per level.
  Their stock is filtered to what your two professions can actually
  use — a Mo/E is offered Monk and Fire Magic skills, but never Energy
  Storage, which is the Elementalist *primary* attribute. Until you've
  earned a secondary, that's your primary's skills and nothing else.
- **Elites are never sold.** The only way one reaches your bar is
  killing the boss that uses it: **Ulrick Grawl Chief** in Green Hills
  teaches Death Blow, and **Bonfaaz Burntfur** in the Northlands
  teaches Meteor. Bosses are bigger, hit harder, and are worth
  hunting specifically — GW1's Signet of Capture, condensed.

Everything you own is saved with the character, and a skill can't sit on
your bar unless the book backs it.

Quests are per-giver, GW1-style: Prince Rurik's chain in Ascalon City
(finish *A Second Profession* and he offers *Charr at the Gate*, whose
marker points at the Northlands), Duke Barradin's pair in Regent Valley
(*Bandit Raid*, then *Unnatural Growths*), and Warmaster Riga's book in
Piken Square — *Hold the Square*, and the collect quest *Hides for the
Watch*, which consumes 4 Charr Hides on turn-in and tracks your bag
live. Abbot Ciglo's *Tithe for Ashford Abbey* does the same with 3
Skale Fins. Item rewards need a free bag slot, and each giver's green
"!" only lights for their own work.

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

The file carries a version, and **a save from before the six-profession
work is declined rather than mis-read**. Adding the remaining
professions renumbered the `Profession`, `AttributeKind` and `SkillId`
enums, all three of which the save stores *by number* — loading an old
file would silently turn a Monk into a Ranger with points in the wrong
lines. Refusing it and starting fresh is the honest failure.

### What's deliberately not built yet

All six Prophecies professions exist with their real mechanics, but only
a handful of skills each (see `src/skill.c`) — GW1 ships ~80 per
profession. No tilemap/LoS, and no JSON skill loading (skills are inline
C data for now — see `docs/design/raylib-architecture.md` #3 for the
planned JSON format). These are content/scope gaps, not architecture
gaps: the effect VM, entity model, and AI loop the doc describes are
what's actually running, so extending to more skills is additive work on
top of a validated foundation, not a rewrite.

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
