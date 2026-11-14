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
| **ALGO** | the parameters assigned in GLO > ALGO KNOB (up to 8; by default the engine's most important one); in the ENGINE window it jumps between engine groups |
| **PRESETS** | the engine's presets, each with its own effects; **SEL held + PRESETS** = the next sound with the current effects; **HOME held + PRESETS = ALL PRESETS** (all engines) |
| **SAVE** | lit when the sound has changed; saves as a new USER preset or overwrites; **on SEQ pages: the pattern** (chapter 7) |
| **FX** | effects chain (right on press); in MULTI, FX held = PERFORM (chapter 6) |
| **GLO** | global settings; a second GLO returns to where you came from |
| **OCT- / OCT+** | octave -3..+3 |

- Hints appear at the bottom of the screen (GLO > SYSTEM > HINTS). Holding SEL shows what the keys do.
- On-screen scale: 0-100 (0 = off), counts start at 1.
- SAVE glows softly when the sound differs from the saved one; settings save themselves during silence.
- **Autosave on every working page**: also FX, SEQ, ARP, PERFORM, MOD, the note card and TOOLS (up to 1.7.1 powering off there lost
  the changes); not in browsing windows (ALL PRESETS, ENGINE, SAVE, the name window, lists). GLO and ALGO KNOB save themselves while
  you stay in GLO.

## 3. Engines

ENGINE window (HOME on HOME): SELECT steps through, ALGO jumps by group, PRESETS = sounds of that engine, SEL confirms
(when released), HOME goes back. Each engine comes back with its own sound and its own effects. **SEL held + SELECT / ALGO /
PRESETS** = the next engine or sound with the current effects (chapter 5).

**VA (analog)**
- **MINI**: Minimoog / Poly D: 4 oscillators, mixer with FEEDBACK, 24 dB ladder, DRIVE, chorus, MONO / PARA. DRIVE: DISTORT 0-10 fades
  the drive in smoothly, from 10 up as before. MOD page: LFO RATE in Hz; press LFO on the MOD page (MORE), then K2 = **LFO MODE**
  FREE (default) / RETRIG (the wave restarts with a new phrase, like the envelopes) / SYNC (tempo).
- **SVF**: Oberheim SEM: SVF filter LP > NOTCH > HP or BP, POLY MOD as in VCO.
- **VCO**: Prophet-5: POLY MOD, SYNC, REV 2 / REV 3 filter.
- **DCO**: Juno-60 / 106: SUB, HPF, built-in chorus.
- **ALPHA**: Alpha Juno: saws with PWM, 4-segment envelope, HOOVER.
- **LPG**: West Coast: complex oscillator (FM / AM), wavefolder, low pass gate, ENV 1 / ENV 2, LOOP.

**DIGI (digital)**
- **FM6**: 6 operators like the DX7 (32 algorithms, .syx bank import), UNISON, FILTER (switch, off by default), LFO 2 page SEL =
  **LFO MODE** FREE / RETRIG (the old KEY SYNC OFF / ON, same sound) / SYNC (tempo); LFO RATE in Hz,
  GLIDE MODE ALWAYS / LEGATO / FOLLOW. FILTER shortcut: SEL + GLOBAL (VOICE > UNISON > FILTER > CONTROL > VINTAGE).
  **BRIGHT** (ALGO page K4, the level of all modulators) -100..+100: -50..+50 sounds as before, above +50 much brighter.
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
- **POGO** (all four, per sound): like POGO in the FL Studio sampler, from the hit the pitch slides down (left) or up (right) at a steady
  speed, up to 4 octaves; 0 = off. MORPH TONE, DUAL PART, 88 / 99 SOUND: EDIT on the page = MORE, then K4.

Every engine has a **VINTAGE** page (deviations like an old unit) and a **CHORD** page (a chord on one key, SEL = LEARN).

## 4. Modulation

- **ENV held + K1-K4** = quick access to AMP ENV (A, D, S, R), **LFO held + K1-K4** = LFO 1; returns after 2 s.
- **LFO / ENV held + OP n key** = the **MOD page** of that source (OP n = the engine's n-th LFO / envelope; in FM6
  ENV + OP n = envelope of operator n): SELECT = target pages, K1-K4 = amount -100..100, LFO / ENV or HOME closes it.
  The targets of one oscillator stay together (A FREQ, A FINE, A PW, A LEVEL on one page, B on the next); a group that does not
  fit starts on the next page, so a knob can stay empty.
- A blue bar next to a parameter means it is being modulated.
- **LFO RATE** in every engine goes up to about 100 Hz (old sounds keep their rate: the knob simply turns further). LFO MODE SYNC
  follows the tempo; while the sequencer plays (also on external MIDI clock) the wave stays locked to the bar, so the same bar
  always sounds the same.
- Every note starts click-free: no DC offset before the VCA and a 2 ms soft start on every new note, retrigger and stolen voice.
  Drums keep their sharp hit; a drum layer with ATTACK > 0 starts (and restarts) without a click.
- Engines have a modulation matrix (4 or 16 slots); amount 0 = no connection.

## 5. Presets and saving

- Order: INIT, USER presets, factory, FM-1 sounds (in FM6, DX7 banks 1-4), imported DX7 / CZ banks.
- **SAVE**: on INIT and on a bank sound always a new USER preset; otherwise an OVERWRITE / NEW window, then the name
  (12 characters: **SELECT = cursor**, **K1 = letter**, OCT- / OCT+ = cursor too, SEL saves, HOME cancels; turning the first letter
  of a suggested USER nn clears the rest of it, other letters keep the name).
- **Browsing** (PRESETS, ALL PRESETS, ENGINE window, MULTI tracks): a sound comes with its own effects (the chain saved with
  it, the built-in chain of a factory preset that has one, otherwise an empty chain). **Hold SEL while you turn** to keep the
  current effects and hear the next sound through them: the preset itself doesn't change, SAVE lights.
- **ALL PRESETS** (HOME held + PRESETS): all engines, sections USER / FACTORY / IMPORTED. SELECT = section, ALGO = engine,
  SEL = OK (when released); SEL held without turning on a USER preset = DELETE? (SEL = yes, HOME = no).
- **OG SYNTH MANAGER** (emulator / site): DX7 banks and Casio CZ sounds (.syx) to the FM-1, deleting DX7 banks, USER WAVE
  tables and the table editor.

## 6. Effects and PERFORM

- **FX**: 8-slot chain (empty by default): SEL short = ON / OFF, SEL held = effects list, ALGO = quick choice, FX on FX =
  MORE. Effects include DISTORTION, CHORUS, REVERB (VintageVerb style), DELAY (Valhalla style), COMPRESSOR, EQ, PHASER,
  FLANGER, MULTIBAND / OTT, AUTO WAH, SUB, VOWEL, COMB, STUTTER.
- **Effect settings come back**: an edited effect you swap for another (ALGO, the list, EMPTY) returns with its settings when
  you pick it again in the same sound. The last 4 edited effects are kept; a preset change or switching off forgets them.
- SATURATION TYPE **OG** = the output stage used up to 1.7.0, as an effect: DRIVE 0.0DB + TONE OFF + MIX 127 + LEVEL 0 sounds
  exactly like it (put it last in the chain; switching TYPE to OG sets an untouched DRIVE / TONE to these values). LO-FI MORE: SATURATE, TONE, LEVEL (a new LO-FI starts as loud as the dry sound).
  EQ: GAIN -24..+24 dB, LOW CUT / HIGH CUT across 20 Hz - 20 kHz (an EQ from older presets switches to these ranges the first time
  you turn a GAIN or CUT knob, without a jump). Effect LFO RATE goes up to about 100 Hz; FREQ SHIFT has SYNC on MORE K4.
  CHORUS FEEDBACK no longer runs away by itself (same sound up to 75 %, above that it rings but fades).
- **OCTAVER TYPE** (MORE K1): **PEDAL** (as before: follows one note, like a pedal) or **VOICE**: the engine plays the two lowest
  notes again an octave (OCT -1) and two octaves (OCT -2) lower, with the same filters and LFO, no delay. Only free voices are
  used: your notes always come first (6 notes in all at most; none on a MONO sound, fewer with low VOICES or with
  UNISON). Not on MINI and drum engines.
- A red effect name = not enough memory in the chain (a message appears on screen).
- **PERFORM, MULTI only** (FX held + key): live effects on the whole 4-track mix (REPEAT, TAPE, FILTER ..., LATCH). In SOLO,
  FX works right on press (FX page, second press = MORE).
- **PERFORM stays open**: hold FX and release it without touching a key or knob - the keys are now effects without FX (held,
  or on / off with LATCH = SEL). A single short FX click or HOME goes back to the page you came from; latched effects keep
  playing until you switch them off in PERFORM or turn LATCH off. The old way still works: FX held + key (or knob), release FX
  and PERFORM closes. Hold an effect key + knob = its settings (SYNC, START, LENGTH; SCRATCH: SYNC, RATE 1/32-1/2). A red
  REWIND LENGTH = not enough memory for full speed (it rewinds slower).

## 7. Sequencer, arpeggiator, RACK

- **SEQ**: NOTES (piano roll), PATTERN, FEEL; 16 patterns, any length from 1 to 256 steps (odd meters too): PATTERN K2 = STEPS, SEL + K2 = by bar, SEL + ALGO = STEPS; SWING, GLIDE, RATCHET, probability.
- **NOTES**: K1 = which note of the step (left = up), also long notes still sounding from earlier steps (the step still counts as
  empty: K2 adds a note, keys enter notes). SEL + SELECT = range (notes that start in it); **K2 on a range** moves that slice of time
  up / down with its glides, and a note that goes on past the range gets a glide back to its old pitch at the range end. Note card
  (SEL): DUPLICATE knob = **HERE** (an identical copy in the same place; K1 switches between the copies) / NEXT STEP. Shortening a note
  that cuts its glides says GLIDES CUT (SEL + K4 = undo). Notes of the same pitch that overlap play side by side when one of them has
  glides (without glides the later one cuts the earlier, as before; MINI always cuts).
- **MOVE** (note card, first row; the card opens on it): SEL = MOVE mode (header MOVE C4 / MOVE RANGE): SELECT moves the note or the
  range by GRID, SEL + SELECT by one tick, glides go with their notes. A range moves the notes it had when you entered (it does not
  pick up notes it passes); no wrap at the pattern ends (PATTERN START / PATTERN END). HOME = back to the notes, SEL = the card again.
- **GRID** = ALGO in NOTES and AUTO: 1/1, 1/2, 1/4 ... 1/32 (right = finer). Moving the cursor to a step highlights the note nearest
  in pitch to the one you had (equal distance: the lower one). During PLAY the view follows the playhead only 2 s after you last moved
  the cursor or turned a knob (in glide edit and MOVE it stays) and keeps the note nearest in pitch.
- A note that runs past the pattern end stops there (what you see is what you hear; up to 1.7.1 it rang on into the next pass); a
  drum hit without its own length rings out.
- **Glide edit** (note card > GLIDE, SEL; on drums this row is FLAM ON / OFF): the note bends to other pitches. SELECT = step along the
  note, **K1 = the note sounding at the cursor** (left = up), K2 = target pitch of the glide at the cursor (no glide there: a new one;
  on the note's **first step: the start pitch only**), **SEL + K2 on the first step = the whole note with its glides** (elsewhere ZOOM H),
  K3 = glide time (stops at the next glide; SEL + K3 by tick), K4 = Q (curve), SEL + K4 = undo. SEL = glide card: **MOVE** (SELECT moves
  the glide by GRID, SEL + SELECT by one tick, HOME back), DELETE, CLEAR GLIDES. HOME = back to the notes. On a MONO sound only one of
  overlapping notes plays (message MONO SOUND). Patterns from 1.7.1 with the old GLIDE ON load as glides.
  While PLAY runs, glide changes are heard at once (also on a note that is already sounding).
- **AUTO** (SEQ page after NOTES): two automation lanes per pattern, **AUTO 1 / AUTO 2**, each moves one parameter of a track's
  sound while the pattern plays. It works like glide edit: SELECT = step, **K1 = lane**, **K2 = value** at the cursor (no point there:
  a new point that jumps to the value; left = higher, like NOTES), **K3 = TIME** (how long the move to this value takes, by GRID;
  SEL + K3 = FINE TIME by step; TIME 0 = jump), **K4 = Q** (curve of the move), SEL + K1 / K2 = zoom, SEL + K4 = undo / redo.
  SEL = AUTO card: **PARAMETER** (which parameter the lane moves), **MOVE** (SELECT moves the point by GRID, SEL + SELECT by step,
  HOME back), **DELETE**, **CLEAR LANE** (points go, the parameter stays). Up to 24 points per lane. Lanes from 1.7.1 play exactly as
  before (STEP = jumps, CURVE = moves to the next point). The pattern's notes show in a pale tint of the display color.
- **LOOP**: mark a range (SEL + SELECT), SEL = range card, row **LOOP**, SEL = loop ON (again on the same range = OFF). The loop is
  saved with the pattern and plays on all MULTI tracks; it shows in NOTES, RACK, AUTO and PATTERN. **PATTERN > TOOLS > LOOP** = ON / OFF.
  PLAY starts where you start it; at the loop end it jumps back to the loop start (started after the loop: to the pattern end, then
  the loop). Notes stop at the loop end, like at the pattern end.
- **PLAY from the cursor, a pattern change and the loop start** play the notes that are already held at that point for the rest of
  their length (a glide note at the pitch drawn there; MIDI OUT gets them too). Drum hits start only on their own step.
- **Zoom**: SEL + K2 up to **8 and 16 bars**, SEL + K1 up to **36 and 45 rows**.
- **STRETCH** (SEQ > OPTIONS, SEL = **SEL + K3: FINE LENGTH / STRETCH**, shown in the title): with STRETCH, SEL + K3 on a range
  stretches it like Alt in FL Studio: right = **twice as long**, left = **half as long**, around the range start (positions and
  lengths, glides with their notes; parts of notes past the range end are cut first). One undo step per turn. Default = FINE LENGTH.
- ARP + REC records only the arpeggio's notes (before 1.8.0 the pattern's own notes were copied in again on every pass).
- **SONG** (SEQ settings, the page right after PATTERN) chains patterns into a whole song: 16 positions, each with a **pattern** (K1, P01-P16;
  left of P01 = END, the end of the song) and how many times it plays (**K2**, x1-x16). In MULTI a position can also load the
  sounds of a **scene** (**K3**: - = keep the sounds, S1-S8; the pattern stays the position's). SELECT = position (left of the
  first: PATTERN, right past the last: FEEL), **SEL = SONG ON / OFF** (while playing: from the first position after this pass). With SONG ON, PLAY starts at the first
  position and after the last one the song starts again; the header shows S2/5. The next pattern starts exactly at the end of the
  pass, without a gap (a pattern changed while it played is saved first, with a short gap; a position with a scene fades the
  sound for a moment while its sounds load). While SONG
  plays, a LOOP waits (every pass is the whole pattern). Changing the pattern by hand while playing turns SONG off.
- **SWITCH** (SONG page **K4**): **NOW** (default, as before) or **AT END**: a pattern chosen on PATTERN K1 while playing
  waits for the end of the pass (header NEXT P03); choosing the playing pattern again cancels it. Stopped: it changes at once.
- Drums: **RACK**, like the Channel Rack in FL Studio (white keys = steps). A step with a note glows on its key; during PLAY the current step lights up too (brightest on a note) and the page follows PLAY.
- Drums in NOTES: a note shows how long the sound really rings. Shorten it with K3 and that hit is cut short (using the sound's FADE); each note has its own length. Lengthen it back and it follows the sound again. Keys and MIDI always play the whole sound.
- **ARP**: modes, LATCH (SEL), ARP + CHORD.
- MULTI: SEQ plays 4 tracks; SEQ held + OP1-OP4 = track T1-T4; notes of the other tracks appear as ghost notes. SEQ held alone = sequencer settings (scenes: HOME on HOME, chapter 8).
- **SAVE on SEQ pages** (NOTES, PATTERN, FEEL, RACK, AUTO, the SEQ settings, TOOLS, the note card) saves the **pattern** at once,
  without a window (while SEQ plays: SAVES AT STOP, it is saved at STOP without a gap in the sound); there the SAVE light shows the
  pattern's state. To save the sound from there: HOME, then SAVE. A red **NOT SAVED** in the SEQ header = the pattern's notes are
  not in memory yet (they save themselves in silence: right after STOP / REC OFF, otherwise about 10 s after your last touch).

## 8. MULTI: 4 tracks

- To enter: **hold HOME** (hold it again to go back to SOLO). The new mode appears on screen for a moment.
- Tracks: **T1 DRUMS**, T2, T3, T4. Each engine once; not together: WAVE with FM6, WAVE with MINI. An engine that can't play
  next to the others appears in red with the reason, and the track keeps its current engine. MIDI: channels 1-4 = T1-T4.
- **Notes**: drums have no limit. T2-T4 share up to 12 held notes, split any way you like. Heavier engines use the shared
  budget faster (next to MINI: FM6 about 8 notes, PHASE about 4), and the oldest note is released when it runs out.
- **HOME is the main screen, it works like the FX page**: SELECT = track (like a slot), **SEL + ALGO = the track's engine** (only engines that can play; ALGO alone = ALGO KNOB as in SOLO, on an empty track the engine), **K1 VOL, K2 PAN,
  K3 REVERB, K4 DELAY, SEL = MUTE**. REVERB and DELAY start from the MIX of the effect in the track's preset and don't change the preset; a new preset starts from its
  MIX again. A muted track shows a red **MUTE** in the track bar at the top and in VOL; the selected muted track gets a colored
  frame instead of the full color (like an effect that is off on the FX page).
- **SEL held + K1-K4 = VOL of T1, T2, T3, T4** (a mixer for all four tracks at once, for playing live and recording): while SEL
  is held, the four knob boxes show T1-T4 with their levels (red MUTE on a muted track). It works on the PERFORM screen too. After
  a turn, letting go of SEL does nothing (no MUTE).
- **SEL held longer (without a knob) = engine list** for the track: SELECT = engine, ALGO = group, SEL = OK, HOME = back without a change; red = can't play now, with the reason. On an empty track a short
  SEL opens the list too.
- **4 sounds at once**: the 4 tracks play together from the sequencer and from MIDI (channels 1-4). The FM1 keys play the
  **selected** track, plus every track with **KEYS LAYER** on: **hold ARP + OP1-OP4 = KEYS LAYER T1-T4 ON / OFF** (a **+** in
  the track bar; while ARP is held the OP keys of layered tracks light up). Layered synth tracks get the same notes as the
  selected one (octave, TRANSPOSE, chord, ARP, LATCH, pedal); a layered drum track plays the sound under the key. A layer is
  saved with MULTI and in scenes. Recording with the keys goes to the selected track only. When the note limit is reached,
  layered tracks give way first.
- **EDIT held + K1-K4 = the engine's quick page** (usually FILTER), like K1-K4 on the SOLO main screen; back after 2 s.
- **PRESETS** = the selected track's sounds, each with its own effects; **SEL held + PRESETS** keeps the current effects (chapter 5).
- **Scenes: HOME on HOME = scene list** (MULTI has no ENGINE window: the engine is SEL + ALGO or the list): SELECT = scene, SEL = load, **SAVE = store** (engines, presets, VOL, PAN, MUTE, sends, MASTER FX),
  SEL held on a scene = DELETE?, HOME = back. A scene remembers saved presets, so save a changed sound first (message "Tn NOT SAVED").
  A scene also keeps its SEQ pattern: with SEQ > OPTIONS > SCENE = **+ PATTERN** (default) loading it switches to that pattern too
  (SOUND = sounds only). This now survives a restart (up to 1.7.1 scenes forgot their pattern after power-off).
- **MASTER FX**: on the FX page, slots 1-2 are always DELAY and REVERB, shared by all tracks (M1 / M2, marked MASTER FX). SEL = ON / OFF, ALGO or SEL held =
  swap their order (the first feeds the second), K4 = RETURN; they can't be removed. A preset's own REVERB / DELAY knobs play only in SOLO.
- **Effects per track**: FX opens the selected track's chain after the two master slots. DISTORTION, FILTER, COMPRESSOR and EQ play on that track, in slot
  order. A red effect doesn't play: SOLO ONLY (other effects) or NO CPU (about 6 effects fit in the whole mix; effects also leave
  room for fewer notes). At the end of the mix: GLO > AUDIO > MULTI MASTER (OFF / LIMIT / COMP).

## 9. GLO settings

PLAY (tuning, octave, velocity, pressure), MIDI (channel, clock, MIDI OUT, LOCAL, PROG CHANGE), AUDIO (SPEAKER EQ, second
core, MULTI MASTER), DISPLAY (visuals, color ORANGE / GREEN / PURPLE / CYAN / RED / MONO / TEAL, LEDs), ALGO KNOB, SYSTEM
(INFO, FACTORY RESET, BACKUP, CALIBRATION, HINTS).

**ALGO KNOB**: up to 8 parameters on ALGO for each engine (ALGO = engine, K1-K4 = KNOB 1-4, SELECT = KNOB 5-8). Each column
shows where the parameter comes from (page, grey), its name (orange) and its current value as a bar; a parameter can sit in
one slot only. Kept after power-off and firmware updates; FACTORY RESET clears them.

**MIDI > GLIDE OUT** (how SEQ glides go out on MIDI OUT): **NOTES** (default) = a new note at the end of each glide; **BEND** = pitch
bend on the track's channel, exactly the drawn curve, while that note plays alone on the channel (otherwise that glide as NOTES; set
the receiving synth's pitch bend range to 48 semitones - PLAY sends RPN 0 = 48); **MPE** = in SOLO every SEQ note on its own channel
2-16 (MPE lower zone, master channel 1) with its own pitch bend, so whole chords glide; in MULTI it works like BEND (tracks keep their
channels).

## 10. Troubleshooting

- **The installer doesn't see the FM-1**: unplug and replug the FM-1, reload the page; the page retries by itself when the
  mode changes.
- **The FM-1 stayed in update mode** ("Felucca Update"): running *Install* again finishes the write.
- **The sound breaks up**: too many notes with a heavy effects chain. The FM-1 fades out the oldest voice by itself; use
  fewer effects or a lighter engine.

---

Code: MPL-2.0 (application); engines, effects and sequencer are closed (Mike Johnson Beats). Based on fm1-base and
felucca-bsp. Drum models in 88 / 99 from Plaits (Mutable Instruments, MIT). M-VAVE and FM-1 are trademarks of their owners.
