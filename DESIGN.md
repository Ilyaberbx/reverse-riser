# Design system: soft neumorphism, one hero, the riser on screen

A calm instrument panel for making reverse-reverb risers. Extruded, softly lit surfaces carry the controls. One big knob, **REVERSE** (the riser's length), is the obvious place to start. A recessed screen shows the riser itself. The same world comes in a light and a dark palette, and the user picks one in the ☰ menu.

Code: tokens and components live in `source/Theme.h/.cpp`, the layout and the riser display in `source/PluginEditor.cpp`.

## Layout

The base size is **520×720**, portrait. From top to bottom:

1. **Preset bar:** ‹ [preset name ▾] › and a save button, centred, with ☰ on the right (Light theme / Dark theme / Show user presets / Reset window size).
2. **Screen:** a recessed panel.
   - The wordmark REVERSE **RISER** sits top-left, RESET top-right, and next to it a small strip of the captured audio with red markers around the syllable.
   - The riser: accent bars on a dB scale, fading in from the left, with the pitch curve on top when PITCH is on and FX is selected.
   - LISTEN and LOOP bottom-left, the RAW | FX switch bottom-right.
   - Hovering shows a dashed "Drag & Drop into DAW" zone; dragging exports the WAV.
3. **Hero, REVERSE:** the largest control at every size, with five detents (1/4 to 4 bars) and the caption **REVERSE** 1 BAR below.
4. **FX row:** REVERB · CHORUS · TREMOLO · PITCH power toggles. The icon switches an effect; the name opens its settings in a call-out (the reverb dropdown, or two or three sliders).

Dot grips mark the four corners, and the bottom-right one is the resize handle.

## Responsive rules

- **Resizing:** from 380×520 to 1800×1800, at any aspect ratio. The size is saved with the DAW session.
- **One unit:** `u = clamp(min(w/520, h/720), 0.75, 2.2)` scales every font, gap, radius, shadow and knob.
- **One column:** wide windows centre it (at most 0.9 × height wide) instead of stretching it. The hero takes up to 60% of the flexible height; the screen gets the rest (150u to 320u).
- **Pixel snapping:** edges snap to whole pixels, so neighbours never overlap.
- **Test:** `tests/EditorLayoutTests.cpp` enforces this at seven odd sizes.

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

**Glow** (hero tick dots, low → high): `#5EE7DF` · `#7FB8FF` · `#8C7CFF` · `#C77DFF` · `#FF7EB3` · `#FF9A8B` · `#FFC371` · `#FFE29A`. **Accent** for "on" states: `#8C7CFF`.

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
| Wordmark | 300 + 800 | 15 | 0.22 | "REVERSE" muted, "RISER" primary |
| Hero name / value | 800 / 400 | 15 | 0.16 / 0.06 | Value uses tabular figures |
| Knob label | 700 | 10.5 | 0.12 | UPPERCASE, muted |
| Knob value | 600 | 12.5 | 0.02 | Tabular figures, primary |
| Tab / toggle | 700 | 11.5 | 0.10 | UPPERCASE |
| Preset / menu | 600 / 500 | 13 | 0 | |

## Behaviour

- **Capture:** the first sound after 100 ms of silence is captured once; RESET waits for the next phrase.
- **Any change** to a setting re-renders the riser within a frame.
- **LISTEN** plays the selected render through the output and turns itself off at the end unless LOOP is on.
- **‹ ›** step through factory and user presets and wrap around; the field opens the full list.
- **Hover** shows a plain-language tooltip on every control.

## Rules

- **Do:** keep colour for the riser, the hero's lit ticks and the accent on "on" states. The syllable markers are the only red. Everything else is surface and text.
- **Do:** keep REVERSE the biggest control, whatever you add.
- **Do:** show state through depth. Raised means available; pressed means selected, active or a place where things sit.
- **Don't:** use gradient text, coloured borders or saturated fills, and don't add a second accent.
- **Don't:** use glass or blur as decoration. The one exception is the frosted surface texture under the panel, drawn once at low alpha (see Imagery).
- **Don't:** use a fixed pixel size. Everything goes through `u`, and new controls get added to the layout test.

## Imagery

Five generated bitmaps live in `assets/images/` and are embedded as `BinaryData` (every file in `assets/` is). `Theme::images()` loads them once; a missing or broken image draws nothing rather than crashing.

| File | Used by | Role |
|---|---|---|
| `hero_vortex.png` | `drawHeroKnob` | The face of the REVERSE disc: a blue plasma vortex, clipped to the disc and rotated with the value. |
| `knob_cap.png` | `drawSmallKnob` | Brushed-metal cap on small rotary knobs (alpha cut-out). |
| `logo_mark.png` | `RiserView::paint` | The vortex mark left of the wordmark, drawn as an alpha mask in `textPrimary`; also the app icon. |
| `glass_light.png`, `glass_dark.png` | `PluginEditor::paint` | Frosted surface under the whole panel at ~20% alpha, one per palette. |

Rules for images:

- **Surfaces only.** No UI chrome, text, icons, pointers, rims or shadows inside a bitmap; those stay vector so they follow `u`, the palette and the value.
- **Square for anything drawn into a circle**, so the clip never shows a corner.
- **At least 1024 px** on the short side, since `u` reaches 2.2.
- **Colour comes from the image only where the rules already allow it:** the vortex is the hero's colour, like its lit ticks. The glass textures are near-monochrome.
