# Guild Wars 1 — Design Research

Research notes on the systems that make the original Guild Wars (2005,
ArenaNet) tick, gathered as the foundation for a 2D "demake." Sources are
the Guild Wars Wiki (wiki.guildwars.com) and community references, cross
checked against direct knowledge of the game. Cited where a specific claim
was verified against a search result; otherwise this reflects established
game knowledge.

## 1. What Guild Wars 1 actually is

Guild Wars shipped as a "Competitive Online RPG" — no subscription, an
instanced world (every explorable area is a private instance for your
party, only towns/outposts are shared), and a hard level cap of 20 reached
within the first fraction of the campaign. The studio's stated design goal
was to sell *content and skill unlocks*, not power — once you're level 20
with full armor, a day-one character and a 2000-hour character have
access to the same numerical ceiling. Everything past that point is
**build mastery**, not stat inflation.

This one decision is the root of almost everything else in this document:
if power can't grow indefinitely, the game has to generate depth some
other way. GW1's answer was an enormous, combinatorial *skill system*.

Four campaigns/expansions (Prophecies, Factions, Nightfall, Eye of the
North) each added professions, skills, and regions, but never raised the
level cap or reset the power baseline.

## 2. Professions

Ten professions total, six in the base game and four added later
[[wiki]](https://wiki.guildwars.com/wiki/Profession):

- **Prophecies (launch):** Warrior, Ranger, Monk, Necromancer, Mesmer,
  Elementalist
- **Factions:** Assassin, Ritualist
- **Nightfall:** Paragon, Dervish

Each profession has one **primary attribute** that is only usable at full
strength if that profession is your *primary* profession (see §4). Primary
attributes are the profession's identity stat and usually do something
beyond just empowering skills:

| Profession | Primary attribute | What it does |
|---|---|---|
| Warrior | Strength | Armor penetration on attack skills |
| Ranger | Expertise | Reduces energy cost of non-spell skills |
| Monk | Divine Favor | Bonus healing, flat armor bonus while enchanted |
| Necromancer | Soul Reaping | Energy gain on nearby deaths |
| Mesmer | Fast Casting | Reduces spell cast time |
| Elementalist | Energy Storage | Directly raises max energy |
| Assassin | Critical Strikes | Increases critical hit chance, energy on crit |
| Ritualist | Spawning Power | Improves spirit/binding ritual health & duration |
| Paragon | Leadership | Extends shout/chant duration, energy on ally skill use |
| Dervish | Mysticism | Bonus effects on enchantment end |

Each profession also ships with ~70–90 profession skills (about 15 of
which are **elite skills**, one-per-bar exclusives), so the total skill
pool across a full install is in the 1,000+ range.

## 3. The 8-skill bar

The entire moment-to-moment game is built around one UI object: an 8-slot
skill bar, at most one of those slots an elite skill. A **build** is
defined as "8 skills + an attribute distribution + equipment"
[[wiki: Build]](https://wiki.guildwars.com/wiki/Build). There is no
mandatory slot — you are not required to bring a heal, an attack chain, or
any specific archetype skill. That's a deliberate looseness: the
constraint is the *number* of choices (8), not their *category*, and it's
what generates the game's famous build diversity — hybrid and off-meta
comps ("permasins," "600 monks," spirit-spam builds) exist because nothing
stops you from putting six enchantments and two attack skills on a
Warrior bar.

Respeccing is free-ish and instant outside combat (gold cost to unlock a
new template, otherwise just re-open the panel), so build experimentation
carries almost no sunk cost — a sharp contrast to games that gate
respeccing behind time or currency walls.

## 4. Attributes: primary/secondary profession split

A character has **one primary and one secondary profession**, chosen at
creation (primary) and unlocked in-game via a quest (secondary), giving
10 × 9 = 90 possible profession pairings before a single skill is chosen
[[wiki: Secondary profession]](https://wiki.guildwars.com/wiki/Secondary_profession).

- You get **all secondary attributes** of *both* your primary and
  secondary profession (e.g., a Warrior secondary can put points in
  Illusion Magic without being a Mesmer).
- You do **not** get your secondary profession's *primary* attribute at
  meaningful rank — it exists but is severely handicapped, which is the
  mechanism that stops "secondary Warrior" from being strictly better
  than "primary Warrior" for a Strength-scaling build.
- Attribute points are earned leveling to 20, plus two quests worth 15
  points each, for a **hard cap of 200 total points**
  [[wiki: Attribute point]](https://wiki.guildwars.com/wiki/Attribute_point).
- Unmodified attribute rank is capped **0–12**; runes (armor inscriptions)
  can push a primary-profession attribute higher, at the cost of a
  max-health penalty for the stronger runes
  [[wiki: Attribute]](https://wiki.guildwars.com/wiki/Attribute).
- Runes only work on armor matching your *primary* profession — you
  cannot rune your secondary profession's attributes at all. Attribute
  points are the *only* permanent way to raise a secondary-profession
  attribute.

The interaction of "200 points, 12 cap per line, primary attribute
gated to your primary profession" is the whole knob the designers use to
make dual-classing meaningfully constrained rather than "pick the two
best skills from ten classes."

## 5. Skill types and the counterplay web

Every skill has a **type**, and the type — not just the numbers — is what
other skills interact with
[[wiki: Skill type]](https://wiki.guildwars.com/wiki/Skill_type). This is
the single most important system to replicate faithfully in a demake,
because GW1's PvP metagame is fundamentally a game of countering *types*,
not countering numbers.

Core hierarchy (abbreviated):

- **Spell** (base type for most magic)
  - **Enchantment Spell** — buff on the caster/ally, removable by
    *enchantment removal*
  - **Hex Spell** — debuff on a foe, removable by *hex removal*
  - **Ward / Glyph / Well / Binding Ritual / Item Spell / Form** —
    specialized spell subtypes with their own removal/interaction rules
- **Skill** (base type for non-spell actions)
  - **Attack Skill** — modifies your next weapon attack, usually costs
    adrenaline
  - **Shout / Chant** — instant, party-wide, unblockable, uninterruptible
  - **Stance** — self buff, mutually exclusive with other stances
  - **Signet** — free (no energy cost), pays for that in cast/recharge time
  - **Ritual** — Ritualist spirit-summon
  - **Preparation** — Ranger buff applied to arrows, replaced by the next
    preparation used
  - **Trap** — placed, triggers on enemy proximity
  - **Touch skill** — melee range, uninterruptible unlike most spells

Because *removal skills* target a type ("Remove Hex," "Shatter
Enchantment," "Rip Enchantment") rather than a specific skill, the whole
metagame becomes rock-paper-scissors at the type level: hex-heavy builds
are answered by hex removal, enchant-heavy builds by enchant removal,
interruptible-cast builds by interrupt rangers/mesmers, and so on. This is
the "skill:counter" design DNA that makes team building in GW1 feel like
deckbuilding.

## 6. Resource economies

Different professions spend different currencies, which is itself a
design lever:

- **Energy** — the default resource (mana equivalent). Regenerates in
  discrete "pips," modified by max-energy items, Energy Storage, Ranger
  Expertise (cost reduction rather than regen), and various skills that
  grant or drain energy on hit/cast.
- **Overcast** — Elementalist-specific: some high-power spells cost you
  future *max* energy instead of current energy, a self-imposed debt
  mechanic unique to the profession's glass-cannon identity
  [[wiki: Overcast]](https://wiki.guildwars.com/wiki/Overcast).
- **Adrenaline** — Warrior/Paragon/some hybrid skills. Builds passively
  from landing/taking attacks rather than regenerating on a timer, so
  adrenaline skills reward staying in the fight rather than sitting back;
  it is *not* spent like a mana pool, it's a threshold you cross per
  skill.
- **Health sacrifice** — several Necromancer skills cost a % of max
  health instead of energy, trading survivability for burst — a resource
  that is intentionally *not* fungible with energy.

Four independent resource types (energy, overcast, adrenaline, health
sacrifice) mean different professions "feel" mechanically distinct even
before their skills' effects are considered.

## 7. Combat mechanics

- **Click-to-move**, not WASD: left-click ground to move, left-click a
  foe to auto-attack/target, right-click-drag for camera. This matters
  for a demake because it changes how "positioning" skills read at 60fps.
- **Cast bar / interruptibility**: most spells have a cast time during
  which the caster is vulnerable to *interrupt* skills that punish
  specifically the act of being mid-cast (extra damage, longer recharge,
  outright cancel). Touch skills, shouts, stances, and signets are
  deliberately exempt from this, which is why they occupy a different
  tactical niche than spells with 1–3s cast times.
- **Aggro bubble**: NPCs pull from a radius around the party's "aggro
  bubble" center; pulling with a bow/spirit from outside melee range,
  splitting a mob, or body-blocking in a chokepoint are core PvE tactics
  that exist *because* of this specific aggro model, not because of raw
  numbers.
- **Conditions** (Bleeding, Poison, Disease, Blind, Weakness, Deep Wound,
  Crippled, Dazed) are debuffs from a shared category, separate from hexes,
  with their own removal skills ("condition removal" vs "hex removal" vs
  "enchant removal" are three different counters).
- **Knockdown** and **interrupt** are the two hard-CC-adjacent tools,
  deliberately kept short and skill-triggered rather than long stuns, so
  fights stay skill-check heavy rather than "who CCs first wins."

## 8. Skill acquisition and elite capture

- Regular skills are bought from **skill trainers** in outposts, using
  gold + skill points (a level-up currency, separate from attribute
  points).
- **Elite skills cannot be bought.** They're captured by defeating a
  named boss NPC who has that elite skill equipped and using a **Signet
  of Capture** on the corpse — a skill-slot item itself, consumable, that
  you can carry up to 3 of at once
  [[wiki: Elite skill]](https://wiki.guildwars.com/wiki/Elite_skill),
  [[wiki: Signet of Capture]](https://wiki.guildwars.com/wiki/Signet_of_Capture).
  You may only capture elites belonging to your current primary or
  secondary profession.
- This turns "get the elite you want" into an actual PvE objective (kill
  a specific boss, in a specific mission/area) rather than a currency
  purchase, and it's the basis of the **Skill Hunter** title track
  (capture all elites in a campaign — 290 skills for the top tier).
- A handful of **PvE-only skills** (added in later campaigns, unlocked via
  quest reward) exist purely to smooth solo/PvE play and are explicitly
  excluded from competitive PvP skill balance — an explicit acknowledgment
  that PvE and PvP have different design needs.

## 9. PvE vs PvP as separate design surfaces

GW1 treats PvE and competitive PvP as different games sharing a skill
system:

- **PvP characters** can be created directly at level 20 with every
  skill/rune/weapon *unlocked account-wide* (once unlocked via any PvE
  character), no grind required to compete.
- Skill balance was patched **separately** for PvE and PvP contexts in
  later years — a skill could be strong in PvE and nerfed in PvP
  simultaneously, because the design problems (AI-heavy attrition fights
  vs. human-optimized team play) are genuinely different problems.
- PvP formats: Random Arena/Team Arena (pickup), Guild vs. Guild (ladder,
  the "real" competitive mode with Guild Halls), Hero Battles,
  Alliance Battles (large-scale faction PvP). Each format further
  constrains the same 8-skill/build system into a different metagame.

## 10. Party composition: Henchmen and Heroes

- **Henchmen**: free, always-available NPC allies with fixed skill bars
  and no player control beyond basic stances (aggressive/defensive/etc in
  some versions). They exist so a full 8-person party is achievable solo,
  but they cannot be optimized.
- **Heroes** (introduced in Nightfall, later universal): full AI-controlled
  party members whose **skill bar, attributes, and equipment you set
  yourself**, exactly like a player character, plus battlefield commands
  (flag to a location, target focus, behavior toggles like
  guard/avoid-combat/attack). This is arguably GW1's single most
  influential design contribution — it turns "party composition," the
  deepest strategic layer of the genre, into something a solo player can
  fully engage with, by making the AI companions build-customizable
  instead of scripted NPCs. Heroes need to individually unlock elite
  skills via their own effective "hero skill points," which recreates the
  elite-capture progression loop *per hero*, not just per player.

## 11. Progression past level 20 (horizontal progression)

Once a character hits the level cap (usually within 10–20 hours) and has
"normal" max-armor gear, there is close to zero further *numerical*
growth available. What replaces the traditional XP/gear treadmill:

- **Title tracks** (~40 by end of life): Skill Hunter, Survivor,
  Vanquisher, Cartographer, Protector, Sunspear/Lightbringer/Kurzick/
  Luxon/Asura/Norn/Deldrimor reputation, Treasure Hunter, Party Animal,
  etc. Some are grindy busywork, but several (Vanquisher — kill every
  monster in a zone; Cartographer — explore 100% of the map) are
  legitimate mastery/completionist goals, and a few (Survivor — reach
  level 20 without ever dying) are genuine skill challenges.
- **Hard Mode**: unlocked per-character after beating the campaign;
  re-runs the same content with tougher monster stat lines, smarter AI
  skill usage, and better loot/title progress, effectively acting as
  "New Game+" without changing the character's own numbers.
- **Vanquishing**: clear 100% of foes in an explorable zone in one
  continuous visit — a pure execution/build-check challenge, no numerical
  reward beyond the title.
- **Skin collecting**: rare weapon/armor *skins* (visually distinct, same
  underlying stats as common gear) are the closest thing to "loot chase"
  the game has, and critically, they're **cosmetic only** — a green
  weapon skin and a max-stat white weapon of the same type perform
  identically in combat. This keeps the loot economy from becoming a
  second gear treadmill.

The unifying idea: once power is capped, "progression" becomes about
*breadth of mastery* (title tracks, map completion, skill collection,
build skill) rather than *height of power*. This is what people mean when
they call GW1's progression "horizontal."

## 12. Itemization philosophy

- Armor value is a small, mostly-fixed table per profession/campaign tier
  (heavier armor for melee, lighter for casters) — buying the "best"
  armor early is cheap and not meaningfully different from the "best"
  armor bought at end-game, other than cosmetics and minor rune slots.
- Weapon damage is a fixed range determined by weapon *type* (e.g.,
  swords always deal roughly the same range), not by rarity — a
  common-drop max-damage sword performs identically to a rare gold skin
  of the same type. Rarity buys **looks**, occasionally a minor
  attribute-boosting inscription slot, not power.
- **Dye** is a pure cosmetic economy layered on top (dyes are combined to
  produce colors, traded on the open market) — self-contained, doesn't
  touch combat balance at all.
- **Customization**: a weapon can be "customized" to one character
  (bonus damage vs. foes, but no longer resellable) — a clean sink for
  "do I keep this generic or commit to it" decisions.

## 13. Why this design holds up

The throughline across every system above: **once numeric power is
capped early and kept flat, every remaining system has to generate depth
through combinatorics and choice, not through bigger numbers.** 10
professions × freely-mixed secondary × ~80 skills each × an 8-slot bar ×
a 200-point attribute budget produces more *meaningfully distinct*
characters than almost any stat-treadmill MMO, without ever requiring the
player to regrind gear. It's also why GW1 remains legible to a 2D
demake: its depth lives in the skill/attribute/build layer, which is
resolution- and dimension-independent, not in its 3D positioning, camera
work, or graphical fidelity (those matter for feel, but not for the core
loop).

## 14. The exact numbers

Everything above is design intent. This section is the arithmetic, kept
separate because it's what the code has to match rather than what the
design has to argue for. The demake states all of it once, in
`src/gwmath.h` / `src/gwmath.c`, so a formula can be checked against
this table instead of hunted for across call sites.

### Armor

Damage is scaled by `2^((60 - AL) / 40)`
[[wiki: Armor rating]](https://wiki.guildwars.com/wiki/Armor_rating).

- AL 60 is the baseline: multiplier 1.0.
- Every **+40 AL halves** incoming damage; every **-40 AL doubles** it.
- So a Warrior in AL 80 takes ~71% of what an AL 60 caster takes, and an
  AL 100 target takes 50%.

**Armor penetration** removes a percentage of the target's armor
*before* the curve, which is why the same 20% penetration is worth far
more against a heavily armored target than a lightly armored one.

Profession armor ceilings differ, and that difference is the entire
reason "who stands in front" is a build decision:

| Profession | Max AL |
|---|---|
| Warrior | 80 |
| Ranger | 70 |
| Monk, Necromancer, Mesmer, Elementalist | 60 |

### Energy

- **Every character has 20 base energy**, regardless of profession
  [[wiki: Energy]](https://wiki.guildwars.com/wiki/Energy). A caster does
  *not* get a deeper pool for being a caster.
- **Energy Storage** (Elementalist primary) adds **+3 maximum energy per
  rank** — the only attribute that raises the pool itself, which is
  precisely what makes it the Elementalist's identity.
- **Regeneration is measured in pips.** One pip = **1 energy per 3
  seconds**. Characters have **3 pips** naturally, so baseline
  regeneration is **1 energy per second**, in real time.
- A **health** pip is a different rate: **2 health per second**.

### Level

- **100 base health at level 1, +20 per level** thereafter.
- Attribute points accumulate on a curve reaching **200 at level 20**.

### Attribute costs

Cumulative points to reach each rank, 0 through 12
[[wiki: Attribute point]](https://wiki.guildwars.com/wiki/Attribute_point):

`0, 1, 3, 6, 10, 15, 21, 28, 37, 48, 61, 77, 97`

Rank 12 costs 97 of a level-20 character's 200 points — which is why
maxing two lines is roughly the whole budget, and why "spread thin or
commit" is a genuine decision rather than an obvious one.

### Primary attribute effects

| Primary | Effect per rank | Applies to |
|---|---|---|
| Strength | 1% armor penetration | attack skills only |
| Expertise | -4% energy cost | attack skills (and other non-spells) |
| Divine Favor | +3.2 healing | Monk spells cast on an ally |
| Soul Reaping | +1 energy per rank on a nearby death | any creature, capped at 3 triggers per 15s |
| Fast Casting | cast time × `2^(-rank/15)` | spells only — rank 15 halves it |
| Energy Storage | +3 maximum energy | always |

## 15. Prophecies pacing: the second profession

Prophecies does two separate things with your secondary profession, and
the distance between them is deliberate:

1. You **acquire** a secondary early, shortly after leaving the tutorial
   region, once you have actually played the primary long enough to have
   an opinion about what it lacks.
2. You cannot **change** it until much later in the campaign
   [[wiki: Secondary profession]](https://wiki.guildwars.com/wiki/Secondary_profession).

That gap is what makes the choice a commitment. A build you can rewrite
on a whim isn't a build, it's a menu — the cost of being wrong is what
gives being right any weight.

## Sources

- [Armor rating – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Armor_rating)
- [Attribute – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Attribute)
- [Attribute point – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Attribute_point)
- [Profession – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Profession)
- [Secondary profession – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Secondary_profession)
- [Energy – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Energy)
- [Skill – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Skill)
- [Skill type – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Skill_type)
- [Build – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Build)
- [Overcast – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Overcast)
- [Elite skill – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Elite_skill)
- [Signet of Capture – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Signet_of_Capture)
- [Skill Hunter – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Skill_Hunter)
- [Guide to Hero Basics and Optimization – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Guide_to_Hero_Basics_and_Optimization)
- [Hero behavior – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Hero_behavior)


## Audit corrections (combat)

Three numbers the implementation had wrong, found by auditing the code
against the wiki rather than against this document:

- **Degeneration was ~3x too weak.** The pip is the unit, not the health:
  one pip is **2 health per second**, so Bleeding (-3) is **6/s** and
  Burning (-7) is **14/s**. The code returned the pip NUMBER as health
  per second, so Bleeding ticked 2 and Burning 5. It also ticked each
  condition separately, which made GW1's **-10 pip cap** (20 health a
  second, no matter how many sources) impossible to express. Sources are
  now summed into one capped total.
  ([Health degeneration](https://wiki.guildwars.com/wiki/Health_degeneration))

- **Weakness was 25%, not 66%.** GW1's Weakness takes **66% off the
  weapon's base damage** - not off bonus damage from attack skills - and
  additionally drops **every non-zero attribute by 1**. The attribute
  half was missing entirely.
  ([Weakness](https://wiki.guildwars.com/wiki/Weakness))

- **Adrenaline was one shared pool.** GW1 gives **every adrenal skill its
  own pool**, counts in **strikes worth 25 points**, and - the part that
  matters - **drains 25 from every other adrenal skill when one is
  spent**. A single shared pool silently removed the constraint the whole
  Warrior bar is built around: you could charge once and fire everything.
  ([Adrenaline](https://wiki.guildwars.com/wiki/Adrenaline))

Still simplified, deliberately: no critical hits, no per-body-part armour
hit location, no blocking/blind miss chance, and attributes are not
reduced for the derived maximums (Energy Storage's pool) - only for
use-time reads, so Weakness can't make maximum energy flicker.
