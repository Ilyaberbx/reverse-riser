# Design system: soft neumorphism, one hero, light from the music

A calm instrument panel. Extruded, softly lit surfaces carry the controls. One big knob, **Mix**, is the obvious place to start. A recessed screen shows the Phýllon visualizer, and the music is the only thing that adds colour. The same world comes in a light and a dark palette, and the user picks one in the ☰ menu.

Code: tokens and components live in `source/Theme.h/.cpp`, the layout in `source/PluginEditor.cpp`, and the visualizer in `source/PhyllonAnalyser.*` and `source/PhyllonView.*`.

## Layout

The base size is **520×720**, portrait. From top to bottom:

1. **Preset bar:** ‹ [preset name ▾] › centred, with ☰ on the right (Light theme / Dark theme / Reset window size).
2. **Screen:** a recessed panel holding the visualizer.
   - The wordmark RE**VERB** sits top-left.
   - The **Freeze** power toggle sits bottom-left.
   - Light from the trail heads spills softly around the screen's edge.
3. **Hero, Mix:** the largest control at every size. It has a tick-dot ring, a recessed well, a raised disc and a pointer line, with the caption **MIX** 35 % below.
4. **Tabs:** Space · Tone · Character, as a pressed track with a raised pill on the selected tab. Under them sit four small knobs (three on Character, centred).
5. **Output:** a small knob with its caption, at the bottom.

Dot grips mark the four corners, and the bottom-right one is the resize handle.

## Responsive rules

- **Resizing:** from 380×520 to 1800×1800, at any aspect ratio. The size is saved with the DAW session.
- **One unit:** `u = clamp(min(w/520, h/720), 0.75, 2.2)` scales every font, gap, radius, shadow and knob.
- **Portrait / square (w ≤ 1.15·h):** the stack above. The hero takes up to 68% of the flexible height and up to 80% of the width; the screen gets the rest (64u to 340u), so tall windows never leave dead space.
- **Wide (w > 1.15·h):** the left column holds the preset bar, screen, tabs and knobs. The right column is the hero, as large as it fits, with Output under it.
- **Knob grid:** 4 across when each cell gets at least 84u, otherwise 2×2.
- **Minimum sizes:** small knobs are never under 56 px, and the Output knob and other targets never under 44 px.
- **Pixel snapping:** edges snap to whole pixels, so neighbours never overlap.
- **Test:** `tests/EditorLayoutTests.cpp` enforces all of this at seven odd sizes.

## Tokens

### Palettes (literal ARGB only; never build tokens from `juce::Colours::*`, which may not be initialised yet)

| Token | Light | Dark | Use |
|---|---|---|---|
| `base` | `#EEF1F6` | `#1F2127` | Panel surface |
| `faceTop` → `faceBottom` | `#FBFCFE` → `#E3E7EE` | `#2B2E36` → `#1B1D22` | Raised faces, lit from the top-left |
| `screen` | `#E2E6ED` | `#14161A` | Recessed surfaces: screen, field, tab track |
| `shadowDark` | `#A3B1C6` @ 50% | `#000000` @ 55% | Lower-right shadow |
| `shadowLight` | `#FFFFFF` | `#FFFFFF` @ 8% | Upper-left highlight |
| `textPrimary` | `#2B3040` | `#E7E9EE` | Values, selected items, names |
| `textMuted` | `#626B80` (5:1) | `#9198A8` (6:1) | Labels, inactive tabs, units |
| `textFaint` | `#C3CAD6` | `#3B3F49` | Unlit tick dots only, never text |

**Glow** (Phýllon bands, low → high): `#5EE7DF` · `#7FB8FF` · `#8C7CFF` · `#C77DFF` · `#FF7EB3` · `#FF9A8B` · `#FFC371` · `#FFE29A`. **Accent** for "on" states: `#8C7CFF`.

### Depth (all distances × u)
| Element | Recipe |
|---|---|
| Hero disc | Raised, d 9, blur 24 |
| Hero well | Pressed, d 4, blur 10 |
| Small-knob disc | Raised, d 4, blur 10 |
| Small-knob groove | Pressed, d 1.5, blur 3 |
| Selected tab pill / icon button | Raised, d 3 / 2.5, blur 8 / 7 |
| Screen / field / tab track | Pressed, d 3 / 2.5 / 2, blur 9 / 6 / 5 |

Raised = `shadowDark` at (+d, +d) and `shadowLight` at (−d, −d). Pressed = the same pair as inner shadows.

### Type: Manrope (OFL, bundled in `assets/fonts/`)
| Role | Weight | Size (×u) | Tracking | Notes |
|---|---|---|---|---|
| Wordmark | 300 + 800 | 15 | 0.22 | "RE" muted, "VERB" primary |
| Hero name / value | 800 / 400 | 15 | 0.16 / 0.06 | Value uses tabular figures |
| Knob label | 700 | 10.5 | 0.12 | UPPERCASE, muted |
| Knob value | 600 | 12.5 | 0.02 | Tabular figures, primary |
| Tab / toggle | 700 | 11.5 | 0.10 | UPPERCASE |
| Preset / menu | 600 / 500 | 13 | 0 | |

## Visualizer (Phýllon, refined)

Restraint over spectacle: structure first, motion second.

- **Seed field:** all 420 phyllotaxis seeds (golden angle 137.5°, radius ∝ √n) as small dots. Inner rings are bass and outer rings treble, each ring coloured by its band's glow hue and swelling with that band.
- **Comets:** 8 fine trails, one per band, glide along the spiral **arms**. Each step is 21 seeds (a Fibonacci number, so the next seed on the same arm), eased in polar coordinates so the path curves. They turn back at the edge, taper and fade within about ¾ s, and quiet bands nearly vanish.
- **Light:** a small halo at each head and a faint wash in the low-res layer, plus a soft spill outside the screen. The field turns slowly with overall loudness.

## Behaviour

- **Double-click** any knob to return it to the current preset's value.
- **Shift-drag** for fine control. The mouse wheel also works.
- **Click a value** to type an exact number.
- **Hover** shows a plain-language tooltip on every control and gives a knob a faint accent rim.
- **‹ ›** step through presets and wrap around; the field opens the full list.
- **Freeze** lights its icon and text in the accent colour when on.

## Rules

- **Do:** keep colour for the glow: the visualizer, the hero's lit ticks, the small knobs' value arcs and the Freeze accent. Everything else is surface and text.
- **Do:** keep Mix the biggest control, whatever you add.
- **Do:** show state through depth. Raised means available; pressed means selected, active or a place where things sit.
- **Don't:** use gradient text, coloured borders or saturated fills, and don't add a second accent.
- **Don't:** use glass or blur as decoration. The only light effect is the screen's spill, which comes from the music.
- **Don't:** use a fixed pixel size. Everything goes through `u`, and new controls get added to the layout test.
