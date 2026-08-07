# Pre-Searing Ascalon — setting research

The prototype's setting. Everything below is from the Guild Wars wikis
and the Reforged announcements; sources at the bottom. Where the demake
takes a liberty it says so, in the "Liberties" section — that list is
the honest part of this document and should stay short.

## 1. Why pre-Searing is the right slice

Pre-Searing Ascalon is Prophecies' tutorial region: a green, peaceful
countryside with a level cap of 20 you will never reach, low-stakes
enemies, and a dense cluster of quests whose real job is to teach the
game. It is also the only part of Tyria that is *pretty* — everything
after the Searing is ash. For a small demake it's ideal: a compact,
fully-enclosed region with a clear beginning (Ashford), a hub (Ascalon
City), six themed explorable areas, and a hard ending (the Searing).

## 2. Geography

Six explorable areas, each deliberately designed around one profession:

| Area | Profession theme |
|---|---|
| Lakeside County | Mesmer and Monk |
| Green Hills County | Warrior |
| Regent Valley | Ranger |
| Wizard's Folly | Elementalist |
| The Catacombs | Necromancer |
| The Northlands | — (the northern frontier) |

Outposts: **Ascalon City** (the capital and hub) and **Ashford Abbey**,
both reached from Lakeside County.

The Northlands historically needed a second player to hold a gate lever
open; a 2018 update extended the gate timer so one player can get in
alone.

## 3. Second profession

Sir Tydus in Ascalon City gives **A Second Profession**, which sends you
to find a trainer — and each profession's trainer stands in a different
place, so choosing a secondary means *travelling* to it:

| Profession | Trainer | Where |
|---|---|---|
| Warrior | Warmaster Grast | Green Hills County |
| Ranger | Master Ranger Nente | Regent Valley |
| Monk | Brother Mhenlo | Ashford Abbey |
| Elementalist | Elementalist Aziure | Wizard's Folly |
| Necromancer | Necromancer Munne | The Catacombs |
| Mesmer | Lady Althea | the theatre, north-west of the city |

This is a better structure than a single profession-changer NPC and it's
what the demake now uses: the trainer you can *reach* is the secondary
you can take.

## 4. Bestiary

Pre-Searing's threats are Skale, Grawl, bandits, Moa birds, devourers,
spiders, and — only at the very end, and only in the north — the Charr.

- **Lakeside County** — River Skale, Moa birds. Deliberately gentle.
- **Green Hills County** — Grawl, Moa.
- **Regent Valley** — bandits, spiders, skale, grawl.
- **Wizard's Folly** — skale, grawl, aloes.
- **The Catacombs** — undead, and Diseased Devourers.
- **The Northlands** — Charr and grawl. The dangerous one.

## 5. Reforged Mode

Guild Wars Reforged is a series of updates announced 18 November 2025,
first live 3 December 2025, co-developed by ArenaNet and 2weeks. It is
mostly a modernisation — controller support, HD skill icons, larger
text, lighting and bloom — plus a $19.99 compilation of all three
campaigns and a free ad-supported mobile client in summer 2026.

**Reforged Mode** is the part with actual gameplay content. It is a
**per-character toggle chosen at character creation**, and for
pre-Searing specifically:

- **Piken Square** becomes an unlockable outpost — *"but you will need
  to get there first"*. Reforged characters only.
- **The Northlands has additional spawns.**
- **Enemies have reduced health and armor** in pre-Searing.
- **+5% experience and gold** across Prophecies.
- **Party size raised to 4** (this one is for everybody, not just
  Reforged).
- Henchmen retuned, with additional skills and higher levels in the
  early areas.
- There is **no hard mode** in pre-Searing.

In a multiplayer party every member must be in Reforged Mode to see the
new content.

## 6. Liberties the demake takes

- **Henchmen in pre-Searing.** There are none in the real game. The
  demake keeps Little Thom and Cynn — both genuine Prophecies henchmen,
  just post-Searing ones — because a solo party has nobody to heal it
  and the party window is a system worth exercising.
- **Compressed geography.** Each area is one screen-sized instance
  rather than a sprawling map, and the connections are simplified into a
  clean graph. Foible's Fair (the theatre) is folded into Green Hills
  County rather than being its own outpost.
- **The Searing itself is absent.** Pre-Searing is the whole game here,
  so there is no ending that burns it all down.
- **Quest givers.** Where a quest's canonical giver wasn't confirmable,
  it has been placed with the most plausible NPC and is marked in
  `quests.c`.

## Sources

- [Guild Wars Reforged – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Guild_Wars_Reforged)
- [Announcing Guild Wars Reforged – GuildWars2.com](https://www.guildwars2.com/en/news/announcing-guild-wars-reforged/)
- [Reforged Mode – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Reforged_Mode)
- [Reforged gets new modes … and a new outpost in pre-searing – Guild Wars Legacy](https://guildwarslegacy.com/article/15-guild-wars-reforged-gets-new-modes-old-login-character-screens-return-and-a-new/)
- [Ascalon (pre-Searing) – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Ascalon_(pre-Searing))
- [Guide to Ascalon (pre-Searing) – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Guide_to_Ascalon_(pre-Searing))
- [A Second Profession – Guild Wars Wiki](https://wiki.guildwars.com/wiki/A_Second_Profession)
- [Charr at the Gate – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Charr_at_the_Gate)
- [Ashford Abbey – Guild Wars Wiki](https://wiki.guildwars.com/wiki/Ashford_Abbey)
- [Regent Valley (Pre-Searing) – GuildWars Wiki (Fandom)](https://guildwars.fandom.com/wiki/Regent_Valley_(Pre-Searing))
- [The Catacombs – GuildWars Wiki (Fandom)](https://guildwars.fandom.com/wiki/The_Catacombs)
