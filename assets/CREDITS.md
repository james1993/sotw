# Asset credits

Everything bundled in `assets/` and who made it. Generated in part
by `tools/build_icon_atlas.py` - edit that, or the manifest, rather
than the icon section below.

## Skill icons - `assets/icons/`

Icons from [game-icons.net](https://game-icons.net), used under the
[Creative Commons Attribution 3.0](https://creativecommons.org/licenses/by/3.0/)
licence. CC BY credits the individual artist, so:

- Icons made by **delapouite**, available on https://game-icons.net (6 used)
- Icons made by **lorc**, available on https://game-icons.net (25 used)

The same credit is shown in-game on the title screen, which is how
CC BY asks a video game to carry it. The exact icon used for each
skill is listed in
`skills.manifest`; the unmodified source SVGs are kept in
`icons/svg/` so the atlas can be rebuilt from scratch.
Modification: each icon's background square was removed and the
silhouette rasterised to a shared atlas, which the game tints.

## Fonts - `assets/fonts/`

- **Cinzel** by Natanael Gama / The Cinzel Project Authors -
  [SIL Open Font License 1.1](fonts/OFL-Cinzel.txt). Display face:
  titles, panel headers, zone names.
- **Alegreya Sans** by Juan Pablo del Peral / Huerta Tipografica -
  [SIL Open Font License 1.1](fonts/OFL-AlegreyaSans.txt). Body face:
  everything dense and small.
- **DejaVu Sans** - [DejaVu Fonts License](fonts/LICENSE-DejaVu.txt).
  Kept as the fallback when a bundled face is missing.

## Sound - `assets/audio/`

- Sound effects from **Kenney** ([kenney.nl](https://kenney.nl)) - RPG
  Audio, Impact Sounds, Interface Sounds and Music Jingles - released
  under [CC0 1.0](audio/LICENSE-Kenney.txt). Used unmodified.
- The five casting and bow sounds are **generated**, not sourced:
  `tools/synth_sfx.py` writes them from scratch. No CC0 pack covers
  magic or archery, and synthesising five clips beat taking on
  per-asset licence checking across several sites for them.

## Ground textures - `assets/ground/`

- Patterns from **Kenney** ([kenney.nl](https://kenney.nl)), released under
  [CC0 1.0](ground/LICENSE-Kenney.txt) - public domain, no attribution
  required. Credited anyway, because it's free to do and he earned it.
