# Manual (draft)

OG Synth by Mike Johnson Beats is a custom firmware for the M-VAVE FM-1, built for sound design: 16 sound engines, an
effects chain, a sequencer with arpeggiator and MULTI mode (4 tracks at once). Every engine has full parameter pages,
factory presets, 128 USER slots and on-screen graphs.

> This is a draft. Page layouts of newer features are proposals and may still change.

---

## Contents

1. Installation and backup
2. Buttons and screen
3. Engines
4. Modulation (LFO / ENV gesture, MOD page)
5. Presets and saving
6. Effects and PERFORM
7. Sequencer, arpeggiator, RACK
8. MULTI: 4 tracks
9. GLO settings
10. Troubleshooting

---

## 1. Installation and backup

- **Back up first**: the *Backup* page saves the FM-1's presets and settings to your computer (and restores them).
- **Install**: open the *Install* page in Chrome or Edge with the FM-1 connected over USB. The page reloads itself when
  the FM-1 switches to update mode. Keep it open until it shows the end.
- **Back to the M-VAVE firmware**: same page, section *Back to the stock firmware* (a .fwsc file from disk).
- **No FM-1**: the browser emulator runs the same code, with sound.

## 2. Buttons and screen

| Button / knob | What it does |
|---|---|
| **HOME** | main screen (oscilloscope, preset name); **HOME on HOME = ENGINE window** (choose an engine); **HOME held = switch SOLO / MULTI** |
| **EDIT / ENV / LFO** | engine page sections; short press = last page of the section, held = first page; the same button on its own section = **MORE** (rarer parameters, "+" next to the page name). **Held + K1-K4**: EDIT = the engine's quick page (usually FILTER), ENV = AMP ENV, LFO = LFO 1; back after 2 s |
| **SELECT** | pages within a section; in windows and lists: choose |
| **K1-K4** | the 4 parameters of a page; from HOME they jump straight to the engine's quick page (usually FILTER) and return after 2 s; in MULTI, on HOME: the track mixer (chapter 8) |
| **SEL** | short = the page's switch (described in the header), confirms in windows; **held + black key** = page shortcuts (OP1-OP6, PITCH, GLOBAL = VOICE, MONO / POLY) |
| **ALGO** | the parameter assigned in GLO > ALGO KNOB (by default the engine's most important one); in the ENGINE window it jumps between engine groups |
| **PRESETS** | the engine's presets; **HOME held + PRESETS = ALL PRESETS** (all engines) |
| **SAVE** | lit when the sound has changed; saves as a new USER preset or overwrites |
| **FX** | effects chain (right on press); in MULTI, FX held = PERFORM (chapter 6) |
| **GLO** | global settings; a second GLO returns to where you came from |
| **OCT- / OCT+** | octave -3..+3 |

- Hints appear at the bottom of the screen (GLO > SYSTEM > HINTS). Holding SEL shows what the keys do.
- On-screen scale: 0-100 (0 = off), counts start at 1.
- SAVE glows softly when the sound differs from the saved one; settings save themselves during silence.

## 3. Engines

ENGINE window (HOME on HOME): SELECT steps through, ALGO jumps by group, PRESETS = sounds of that engine, SEL confirms,
HOME goes back.

**VA (analog)**
- **MINI**: Minimoog / Poly D: 4 oscillators, mixer with FEEDBACK, 24 dB ladder, DRIVE, chorus, MONO / PARA.
- **SVF**: Oberheim SEM: SVF filter LP > NOTCH > HP or BP, POLY MOD as in VCO.
- **VCO**: Prophet-5: POLY MOD, SYNC, REV 2 / REV 3 filter.
- **DCO**: Juno-60 / 106: SUB, HPF, built-in chorus.
- **ALPHA**: Alpha Juno: saws with PWM, 4-segment envelope, HOOVER.
- **LPG**: West Coast: complex oscillator (FM / AM), wavefolder, low pass gate, ENV 1 / ENV 2, LOOP.

**DIGI (digital)**
- **FM6**: 6 operators like the DX7 (32 algorithms, .syx bank import), UNISON, FILTER (switch, off by default),
  GLIDE MODE ALWAYS / LEGATO / FOLLOW. FILTER shortcut: SEL + GLOBAL (VOICE > UNISON > FILTER > CONTROL > VINTAGE).
- **PHASE**: Casio CZ: phase modulation, 8-segment envelopes, CZ sound import.
- **WAVE**: Serum-style wavetable: 2 oscillators, warps, unison, 2 filters, 4 envelopes with curves and HOLD, 4 LFOs,
  DRAW, 16-slot matrix.
- **8BIT**: glitch / 8-bit: BITS, GLITCH.
- **PHYS**: physical modelling: PLUCK / STRIKE / BOW excitation, STRING resonator or BAR / BELL / DRUM / TUBE (modes);
  POSITION = where the string is plucked, DECAY / DAMP / STIFF = string or material.
- **FORMANT**: voice and choir: VOICE / SAW / PULSE source + BREATH, 5 formants on every note, singers BASS / TENOR / ALTO / SOPRANO,
  VOWEL A > E > I > O > U (also on ALGO), SHIFT, CHOIR page (SPREAD, JITTER, VIBRATO), ENV 2 > VOWEL for talking basses.

**DRUM**: white keys = the sounds of the kit
- **MORPH**: like Microtonic / DRUM-X: each sound has A / B and MORPH.
- **88**, **99**: TR-808 and TR-909.
- **DUAL**: like Volca Drum: 8 parts with 2 layers each, resonator.

Every engine has a **VINTAGE** page (deviations like an old unit) and a **CHORD** page (a chord on one key, SEL = LEARN).

## 4. Modulation

- **ENV held + K1-K4** = quick access to AMP ENV (A, D, S, R), **LFO held + K1-K4** = LFO 1; returns after 2 s.
- **LFO / ENV held + OP n key** = the **MOD page** of that source (OP n = the engine's n-th LFO / envelope; in FM6
  ENV + OP n = envelope of operator n): SELECT = target pages, K1-K4 = amount -100..100, LFO / ENV or HOME closes it.
- A blue bar next to a parameter means it is being modulated.
- Engines have a modulation matrix (4 or 16 slots); amount 0 = no connection.

## 5. Presets and saving

- Order: INIT, USER presets, factory, FM-1 sounds (in FM6, DX7 banks 1-4), imported DX7 / CZ banks.
- **SAVE**: on INIT and on a bank sound always a new USER preset; otherwise an OVERWRITE / NEW window, then the name
  (12 characters: SELECT = letter, OCT- / OCT+ = cursor, SEL saves, HOME cancels).
- **ALL PRESETS** (HOME held + PRESETS): all engines, sections USER / FACTORY / IMPORTED.
- **OG SYNTH MANAGER** (emulator / site): DX7 banks and Casio CZ sounds (.syx) to the FM-1, deleting DX7 banks, USER WAVE
  tables and the table editor.

## 6. Effects and PERFORM

- **FX**: 8-slot chain (empty by default): SEL short = ON / OFF, SEL held = effects list, ALGO = quick choice, FX on FX =
  MORE. Effects include DISTORTION, CHORUS, REVERB (VintageVerb style), DELAY (Valhalla style), COMPRESSOR, EQ, PHASER,
  FLANGER, MULTIBAND / OTT, AUTO WAH, SUB, VOWEL, COMB, STUTTER.
- A red effect name = not enough memory in the chain (a message appears on screen).
- **PERFORM, MULTI only** (FX held + key): live effects on the whole 4-track mix (REPEAT, TAPE, FILTER ..., LATCH). In SOLO,
  FX works right on press (FX page, second press = MORE).

## 7. Sequencer, arpeggiator, RACK

- **SEQ**: NOTES (piano roll), PATTERN, FEEL; 16 patterns, any length from 1 to 256 steps (odd meters too): PATTERN K2 = STEPS, SEL + K2 = by bar, SEL + ALGO = STEPS; SWING, SLIDE, RATCHET, probability.
- Drums: **RACK**, like the Channel Rack in FL Studio (white keys = steps). A step with a note glows on its key; during PLAY the current step lights up too (brightest on a note) and the page follows PLAY.
- Drums in NOTES: a note shows how long the sound really rings. Shorten it with K3 and that hit is cut short (using the sound's FADE); each note has its own length. Lengthen it back and it follows the sound again. Keys and MIDI always play the whole sound.
- **ARP**: modes, LATCH (SEL), ARP + CHORD.
- MULTI: SEQ plays 4 tracks; SEQ held + OP1-OP4 = track T1-T4; notes of the other tracks appear as ghost notes. SEQ held alone = scene list (chapter 8).

## 8. MULTI: 4 tracks

- To enter: **hold HOME** (hold it again to go back to SOLO). The new mode appears on screen for a moment.
- Tracks: **T1 DRUMS**, T2, T3, T4. Each engine once; not together: WAVE with FM6, WAVE with MINI. An engine that can't play
  next to the others appears in red with the reason, and the track keeps its current engine. MIDI: channels 1-4 = T1-T4.
- **Notes**: drums have no limit. T2-T4 share up to 12 held notes, split any way you like. Heavier engines use the shared
  budget faster (next to MINI: FM6 about 8 notes, PHASE about 4), and the oldest note is released when it runs out.
- **HOME is the main screen, it works like the FX page**: SELECT = track (like a slot), **ALGO = the track's engine** (only engines that can play), **K1 VOL, K2 PAN,
  K3 REVERB, K4 DELAY, SEL = MUTE**. REVERB and DELAY start from the MIX of the effect in the track's preset and don't change the preset; a new preset starts from its
  MIX again.
- **SEL held = engine list** for the track: SELECT = engine, ALGO = group, SEL = OK, HOME = back without a change; red = can't play now, with the reason. On an empty track a short
  SEL opens the list too.
- **EDIT held + K1-K4 = the engine's quick page** (usually FILTER), like K1-K4 on the SOLO main screen; back after 2 s.
- **HOME on HOME = the same ENGINE window as in SOLO**, for the selected track: SELECT = engine (plays at once), past the last engine EMPTY, ALGO = groups, PRESETS = sound, SEL = OK,
  HOME = back.
- **Scenes: SEQ held = scene list**: SELECT = scene, SEL = load, **SAVE = store** (engines, presets, VOL, PAN, MUTE, sends, MASTER FX), HOME or SEQ = back. A scene remembers
  saved presets, so save a changed sound first (message "Tn NOT SAVED").
- **MASTER FX**: on the FX page, slots 1-2 are always DELAY and REVERB, shared by all tracks (M1 / M2, marked MASTER FX). SEL = ON / OFF, ALGO or SEL held =
  swap their order (the first feeds the second), K4 = RETURN; they can't be removed. A preset's own REVERB / DELAY knobs play only in SOLO.
- **Effects per track**: FX opens the selected track's chain after the two master slots. DISTORTION, FILTER, COMPRESSOR and EQ play on that track, in slot
  order. A red effect doesn't play: SOLO ONLY (other effects) or NO CPU (about 6 effects fit in the whole mix; effects also leave
  room for fewer notes). At the end of the mix: GLO > AUDIO > MULTI MASTER (OFF / LIMIT / COMP).

## 9. GLO settings

PLAY (tuning, octave, velocity, pressure), MIDI (channel, clock, MIDI OUT, LOCAL, PROG CHANGE), AUDIO (SPEAKER EQ, second
core, MULTI MASTER), DISPLAY (visuals, color, LEDs), ALGO KNOB (the parameter under ALGO for each engine), SYSTEM (INFO,
FACTORY RESET, BACKUP, CALIBRATION, HINTS).

## 10. Troubleshooting

- **The installer doesn't see the FM-1**: unplug and replug the FM-1, reload the page; the page retries by itself when the
  mode changes.
- **The FM-1 stayed in update mode** ("Felucca Update"): running *Install* again finishes the write.
- **The sound breaks up**: too many notes with a heavy effects chain. The FM-1 fades out the oldest voice by itself; use
  fewer effects or a lighter engine.

---

Code: MPL-2.0 (application); engines, effects and sequencer are closed (Mike Johnson Beats). Based on fm1-base and
felucca-bsp. Drum models in 88 / 99 from Plaits (Mutable Instruments, MIT). M-VAVE and FM-1 are trademarks of their owners.
