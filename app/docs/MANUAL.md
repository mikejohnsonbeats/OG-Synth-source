# Manual (draft)

OG Synth by Mike Johnson Beats is a custom firmware for the M-VAVE FM-1, built for sound design: 18 sound engines, an
effects chain, a sequencer with arpeggiator and MULTI mode (4 tracks at once, with a MASTER chain over the whole mix). Every
engine has full parameter pages, its own INIT sound, factory presets (all but SAMPLE), 256 USER slots and on-screen graphs.

> This is the short version, for OG Synth 2.1. The full manual: https://ogsynth.cc/manual

---

## Contents

1. Installation and backup
2. Buttons and screen
3. Engines
4. Modulation (LFO / ENV gesture, MOD page)
5. Presets and saving
6. Effects and PERFORM
7. Sequencer, arpeggiator, RACK
8. MULTI: 4 tracks and MASTER
9. GLO settings
10. Troubleshooting

---

## 1. Installation and backup

- **Back up first**: the *Backup* page saves everything on the FM-1 to your computer and restores it: presets and settings, and
  (from 2.0.2) the DX7 and CZ banks, SEQ patterns with their AUTO lanes, WAVE tables and samples, in one .ogsbak file.
- **Install**: open the *Install* page in Chrome, Edge or Opera with the FM-1 connected over USB. The page reloads itself
  when the FM-1 switches to update mode. Keep it open until it shows the end. Updating from an older OG Synth keeps your
  sounds and settings.
- **Back to the M-VAVE firmware**: same page, section *Back to the stock firmware* (a .fwsc file from disk).
- **No FM-1**: the browser emulator runs the same code, with sound.

## 2. Buttons and screen

| Button / knob | What it does |
|---|---|
| **HOME** | main screen (oscilloscope, preset name); **HOME on HOME = ENGINE window** (choose an engine; in MULTI: the scenes); **HOME held = switch SOLO / MULTI** |
| **EDIT / ENV / LFO** | engine page sections; short press = last page of the section, held = first page; the same button on its own section = **MORE** (rarer parameters, "+" next to the page name). **Held + K1-K4**: EDIT = the engine's quick page (usually FILTER), ENV = AMP ENV, LFO = LFO 1; back after 2 s |
| **SELECT** | pages within a section; in windows and lists: choose |
| **K1-K4** | the 4 parameters of a page; from HOME they jump straight to the engine's quick page (usually FILTER) and return after 2 s; in MULTI, on HOME: the track mixer (chapter 8) |
| **SEL** | short = the page's switch (described in the header), confirms in windows; **held + black key** = page shortcuts (OP1-OP6, PITCH, GLOBAL = VOICE, MONO / POLY; on drum engines OP4 = RES ENV / RES LFO) |
| **ALGO** | the parameters assigned in GLO > ALGO KNOB (up to 8; by default the engine's most important one); in the ENGINE window it jumps between engine groups |
| **PRESETS** | the engine's presets, each with its own effects; **SEL held + PRESETS** = the next sound with the current effects; **HOME held + PRESETS = ALL PRESETS** (all engines) |
| **SAVE** | lit when the sound has changed; saves as a new USER preset or overwrites; **on SEQ pages: the pattern** (chapter 7) |
| **FX** | effects chain (right on press); in MULTI it starts with MASTER and the reverb / delay sends, and FX held = PERFORM (chapter 6) |
| **GLO** | global settings; a second GLO returns to where you came from |
| **OCT- / OCT+** | octave -3..+3; on a drum engine it is **DRUM TUNE** in semitones (the keys stay where they are), kept apart from the keyboard octave: OCT on a synth never retunes the drums |

- Hints appear at the bottom of the screen (GLO > SYSTEM > HINTS). With HINTS off the pages use that space (in MULTI the
  mixer row moves down into it, on the ARP page the steps sit in the middle). Holding SEL shows what the keys do.
- Engine names are written in Knewave (brush), preset names in Sedgwick (tag); Barlow, the look before 2.0, is in
  GLO > DISPLAY (chapter 9).
- On-screen scale: 0-100 (0 = off), counts start at 1.
- SAVE glows softly when the sound differs from the saved one; settings save themselves during silence.
- **Autosave on every working page**: also FX, SEQ, ARP, PERFORM, MOD, the note card and TOOLS (up to 1.7.1 powering off there lost
  the changes); not in browsing windows (ALL PRESETS, ENGINE, SAVE, the name window, lists). GLO and ALGO KNOB save themselves while
  you stay in GLO.

## 3. Engines

ENGINE window (HOME on HOME): SELECT steps through, ALGO jumps by group (VA, DIGI, DRUM), PRESETS = sounds of that engine, SEL
confirms (when released), HOME goes back. Each engine comes back with its own sound and its own effects. **SEL held + SELECT /
ALGO / PRESETS** = the next engine or sound with the current effects (chapter 5).

Every engine with GLIDE has the same **VOICE** page: K1 MODE, K4 GLIDE, LEGATO under SEL; in its MORE K1 BEND RANGE, K2 WIDTH
(where the engine has it), K4 **GLIDE MODE**: ALWAYS (glide from the last note every time) or LEGATO (fingered glide, as on a DX7:
only a key pressed while another is still held glides); FM6 also FOLLOW. VEL > AMP is in the MORE of the AMP ENV page (on WAVE:
VOICE MORE K3).

**VA (analog)**
- **MINI**: 4 oscillators, mixer with FEEDBACK, 24 dB ladder filter (LADDER 24), DRIVE, chorus, MONO / PARA (in PARA all four
  oscillators play: turn up OSC 4's level too). DRIVE: DISTORT 0-10 fades the drive in smoothly. TUNE page at the end of EDIT
  (DETUNE, tuning of oscillators 2-4), RETRIG in the MORE of VOICE (EVERY KEY / NEW NOTE). MOD page: LFO RATE in Hz; press LFO on
  the MOD page (MORE), then K2 = **LFO MODE** FREE (default) / RETRIG (the wave restarts with a new phrase, like the envelopes) /
  SYNC (tempo).
- **SVF**: a soft 12 dB filter that sweeps LP > NOTCH > HP (or BP), POLY MOD as in VCO.
- **VCO**: 2 oscillators, POLY MOD, SYNC, a sharp 24 dB filter (REV 2 / REV 3).
- **DCO**: one steady oscillator with SUB and noise, HPF, two models with their own feel, chorus built in.
- **ALPHA**: saws with PWM, 4-segment envelope, pitch envelope, HOOVER.
- **LPG**: West Coast style: complex oscillator (FM / AM), wavefolder, low pass gate, ENV 1 / ENV 2, LOOP.

**DIGI (digital)**
- **FM6**: 6 operators (32 algorithms, classic six-operator .syx banks such as the DX7's), UNISON, FILTER (switch, off by
  default), LFO 2 page SEL = **LFO MODE** FREE / RETRIG (the old KEY SYNC OFF / ON, same sound) / SYNC (tempo); LFO RATE in Hz,
  GLIDE MODE ALWAYS / LEGATO / FOLLOW (VOICE MORE K4). FILTER shortcut: SEL + GLOBAL (VOICE > UNISON > FILTER > CONTROL > VINTAGE).
  **BRIGHT** (ALGO page K4, the level of all modulators) -100..+100: -50..+50 sounds as before, above +50 much brighter.
  **AMP ENV** (first ENV page): one envelope for the whole sound, -100..100, moves all carrier envelopes at once.
- **PHASE**: phase distortion, 8-segment envelopes, CZ sound import (.syx).
- **WAVE**: modern wavetable: 2 oscillators, each with a WARP (30 modes) and a WARP 2 (21 modes): SYNC WIN, ASYM +, ASYM -, FLIP,
  QUANTIZE and REMAP in both, FM / RING / AM from the SUB or the NOISE in WARP. Sub, noise, unison, 2 filters, 4 envelopes with
  curves and HOLD, 4 LFOs, DRAW (LFO 2-4 can each have their own: OWN DRAW), 16-slot matrix (chapter 4). When the processor is
  really full, a big unison sound with heavy POSITION or WARP modulation thins its unison a little instead of crackling.
- **8BIT**: chip waves, bit tricks and bytebeat: BITS, GLITCH.
- **PHYS**: physical modelling: PLUCK / STRIKE / BOW excitation, STRING resonator or BAR / BELL / DRUM / TUBE (modes);
  POSITION = where the string is plucked, DECAY / DAMP / STIFF = string or material.
- **FORMANT**: voice and choir: VOICE / SAW / PULSE source + BREATH, 5 formants on every note, singers BASS / TENOR / ALTO / SOPRANO,
  VOWEL A > E > I > O > U (also on ALGO), SHIFT, CHOIR page (SPREAD, JITTER, VIBRATO), ENV 2 > VOWEL for talking basses.
- **SAMPLE** (2.0): a sound as an oscillator, played chromatically from its root note. K1 on the SAMPLE page = the source:
  **USER 1-3** (a .wav sent with the Manager, chapter 5) or **REC** (what the FM-1 itself just played). No factory presets: the
  sound is yours, and so are its presets.
  - Pages: SAMPLE (K2 START, K3 TUNE, K4 FINE; KEYBOARD under SEL: OFF plays every key at the sample's own pitch), LOOP (OFF /
    FORWARD / PING-PONG, LOOP START, LOOP END, XFADE; REVERSE under SEL), VOICE (MODE, UNISON, DETUNE, GLIDE; LEGATO under SEL),
    FILTER (LP / BP / HP under SEL), FILTER ENV, AMP ENV, LFO, LFO > (PITCH, FILTER, AMP, PAN), VINTAGE, REC, STORE.
  - **Recording**: REC page (SEL + OP3), SEL = **ARMED: PLAY** (an older recording plays until the new one starts). Then play: in
    SOLO switch to any other engine (its effects are recorded too), in MULTI play the whole mix. The recording starts with the
    first sound and stops when it's full, or with SEL on the REC page; it's taken after the output limiter, without the metronome.
    RATE (K1 on REC) = 44.1 / 22 / 11 kHz: about half a second, one or two seconds. A new recording switches SAMPLE to REC; C4
    plays it as it was.
  - The recording stays until power-off. **STORE** (after REC): TO SAMPLE = USER 1-3, SEL saves it there for good (named REC);
    CLEAR throws it away. From arming until CLEAR, the recording borrows memory from the effects.

**DRUM**: white keys = the sounds of the kit
- **88**: the early-eighties analog drum machine; its claps, toms, congas and cowbell are modelled after the original circuits.
- **99**: its punchier sibling, rebuilt in 2.0 after the original circuits: kick, snare, clap, rim, closed hat and toms, and an
  open hat, crash and ride that ring like real cymbals.
- **LN** (2.0): the classic early-eighties digital drum machine, synthesised, no samples inside: kick, snare, claps, rim, hats,
  cowbell, cabasa, toms, congas, tambourine; kits TIGHT, DEEP, BRIGHT. It plays like 88 and 99 (sixteen sounds on the white keys,
  the same pages, kits and saves).
- **MORPH**: each sound moves between two settings, A / B, with MORPH.
- **DUAL**: 8 parts with 2 layers each, through a resonator.
- **UPPER** (MORPH and DUAL, K4 on the KIT page): the white keys F3-F4 play the 8 sounds, the next eight white keys (G4-G5) play the
  same 8 sounds with a twist: **B** (MORPH only: the sound moved toward its B setting), **ACCENT** (louder; in MORPH also brighter),
  **PITCH** (higher, up to an octave) or **FLAM** (a quiet first hit, the main one 5-30 ms later). **AMOUNT** (KIT MORE, K1) sets how
  much: B / ACCENT 0-100, PITCH in semitones, FLAM in milliseconds.
- **CHOKE, PAN and LEVEL** of a sound sit on K2, K3, K4 in every drum engine: on MIX and in the MORE of the sound's page.
- **POGO** (per sound): from the hit the pitch slides down (left) or up (right) at a steady speed, up to 4 octaves; 0 = off. In the
  MORE of the sound's page (EDIT there): K1 on SOUND in 88, 99 and LN, on PART in DUAL, K4 on TONE in MORPH.
- **Resonators** (2.0): every drum engine can send its sounds into METAL, WOOD, NATURAL, FABRIC, GLASS, VOLTAGE, PLASTIC or LASER.
  K1 on MIX = the send of the sound you're on, MATERIAL = K3 in the MORE of MIX, MAT DECAY / MAT TONE = K2 / K3 in the MORE of KIT
  (MORPH keeps its SPRING as the first choice, DUAL has the materials in WG TYPE after TUBE and STRING). A kit without a send sounds
  as before. **SEL + OP4** = **RES ENV** (an envelope on the resonator: K1-K4 ATTACK, DECAY, SUSTAIN, RELEASE; a filter on the send
  under SEL: OFF / LP / BP / HP, its CUTOFF, RESO and ENV AMT in MORE), again = **RES LFO** (RATE, DEPTH, DEST: TONE / CUTOFF /
  LEVEL of the resonance, SHAPE).
- The drum pages show the sound you hit on the FM-1's keys (or pick with SEL + key). Sequencer notes, and by default MIDI notes,
  only play it, so the pages stay put while a pattern or a DAW plays the drums (GLO > MIDI > DRUM SELECT, chapter 9).

Every engine has a **VINTAGE** page (the last page of EDIT: deviations like an old unit). Chords on one key live in the
arpeggiator's settings (chapter 7).

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
- Engines have a modulation matrix (4 or 16 slots); amount 0 = no connection. The drum engines have no LFOs or envelopes to route
  (their resonance has its own: RES ENV / RES LFO, chapter 3).
- **WAVE** has 16 connections (MOD 1-16: SOURCE, DEST, AMOUNT, VIA). In the MORE of a MOD page K2 = the **CURVE** of that
  connection (-7..7: plus starts slowly, minus jumps in early, 0 = straight) and K1 = CHAOS RATE. Sources include **CHAOS 1 / 2**
  (X and Y of two chaotic attractors); destinations include AMP TIME and ENV 2-4 TIME, UNI WIDTH, UNI BLEND, DRIVE MIX, DRV TONE,
  F ENV AMT and F2 PARAL. **OWN DRAW** (SHAPE of LFO 2-4): that LFO draws its own 8 points (pages L2-L4 DRAW after DRAW 5-8,
  SEL = STEP / CURVE for that drawing).

## 5. Presets and saving

- Order: INIT, USER presets, factory, FM-1 sounds (in FM6, DX7 banks 1-4), imported DX7 / CZ banks. SAMPLE has only INIT and
  your own presets. On HOME a sound from the DX7 or CZ banks or the FM-1 sounds shows its place among all the loaded sounds of
  that section and how many there are (2.1.0; deleted sounds don't count).
- **USER presets: 256 slots** (from 2.1.0; 128 before), shared by all engines. The SAVE window shows how many are used and how
  much preset memory is free (USER 40 / 256 USED   FREE 71%; also GLO > SYSTEM > INFO); if the memory fills up first,
  SAVE says MEMORY FULL and nothing is overwritten. Presets from older versions move to the new layout by themselves at the first
  start (same numbers, names and sound); an older OG Synth (2.0.x) after that no longer sees USER presets, so keep the backup you
  make before updating if you may go back.
- **SAVE**: on INIT and on a bank sound always a new USER preset; otherwise an OVERWRITE / NEW window, then the name
  (12 characters: **SELECT = cursor**, **K1 = letter**, OCT- / OCT+ = cursor too, SEL saves, HOME cancels; turning the first letter
  of a suggested USER nn clears the rest of it, other letters keep the name). On FM6 the window also has **SEND AS DX7** (the sound
  goes out over USB as a single DX7 voice).
- **Browsing** (PRESETS, ALL PRESETS, ENGINE window, MULTI tracks): a sound comes with its own effects (the chain saved with
  it, the built-in chain of a factory preset that has one, otherwise an empty chain). **Hold SEL while you turn** to keep the
  current effects and hear the next sound through them: the preset itself doesn't change, SAVE lights.
- **ALL PRESETS** (HOME held + PRESETS): all engines, sections USER / FACTORY / IMPORTED. SELECT = section, ALGO = engine,
  SEL = OK (when released); SEL held without turning on a USER preset = DELETE? (SEL = yes, HOME = no); on a sound of a DX7 or CZ
  bank under IMPORTED the same DELETE? takes just that one sound out of its bank (when its last sound goes, the bank's slot is free;
  factory presets and the FM-1's original sounds stay; a whole DX7 bank: OG SYNTH MANAGER).
- **OG SYNTH MANAGER** (the Studio page of ogsynth.cc, Chrome, Edge or Opera, over USB): DX7 sounds and banks (.syx) for FM6,
  CZ sounds (.syx) for PHASE, deleting DX7 banks, WAVE tables (.wav, up to 63 frames) into USER 1-3 and the table editor, **samples
  (.wav) for SAMPLE** into the same USER 1-3 (a longer sample takes the next slots too: from USER 1 about 1.1 s at 44.1 kHz, down to
  11 kHz for about 4.4 s; made mono, trimmed and normalised, root note and loop from the file or found for you), **classic .fxp synth
  presets translated into WAVE** (one or a whole folder, with a list of what was approximated), the **preset editor** (every
  parameter of every engine: play it in the browser or live on the FM-1, save it as a USER preset or as a file), the FM-1's
  original 128 sounds back into DX7 banks 1-4, and the preset pack (128 sounds). From 2.1.0 the pack and the editor keep effect
  copies with their own settings (a pack with copies needs OG Synth 2.1.0; one without them loads on any OG Synth from 1.8.0), the
  editor's .ogsp file holds the whole sound with its effects chain (older .ogsp files still open), and a backup tab gives the same
  full backup as the *Backup* page.

## 6. Effects and PERFORM

- **FX**: 8-slot chain (empty by default): SELECT = slot, ALGO = quick choice, SEL short = ON / OFF (on an empty slot: the
  list), SEL held + SELECT = move it, SEL held longer = effects list, FX on FX = MORE.
- **Several of the same effect (2.1.0)**: the 14 light effects marked **+** in the list (DISTORTION, SATURATION, BITCRUSHER, EQ,
  SIDECHAIN, TRANSIENT, FILTER, AUTO WAH, VOWEL, PHASER, TREMOLO, RING MOD, OCTAVER, SUB) can sit in the chain up to 8 times, e.g.
  a DISTORTION and a FUZZ after it, or an EQ before and after the drive. Each copy is a separate effect with all its own settings
  (knobs, MORE pages, SYNC). The heavier effects (DELAY, REVERB, CHORUS, SHIMMER...) stay once per chain. On the right of the list:
  **HERE** = in the slot you edit, **SLOT 2** / **SLOT 1,4** = where it already is; under the cursor **ADD** = a new copy here,
  **MOVE** = it comes here (empty slot), **SWAP** = it trades places with the effect in this slot. ALGO can pick a light effect
  that is already in the chain too (a new copy). Moving or swapping effects takes each one's settings with it. MULTI tracks play
  the copies of their presets too, and MASTER can hold copies as well (chapter 8); each copy counts in the CPU budget. 28 effects in seven groups: drive, dynamics, filter, mod, pitch, time and space (DISTORTION,
  SATURATION, CHORUS, REVERB, DELAY, COMPRESSOR, EQ, PHASER, FLANGER, MULTIBAND / OTT, AUTO WAH, SUB, VOWEL, COMB, STUTTER, GRAIN,
  SHIMMER, OCTAVER, FREQ SHIFT, LO-FI and more). DELAY and REVERB at MIX 100 play only the effect.
- **Effect settings come back**: an edited effect you swap for another (ALGO, the list, EMPTY) returns with its settings when
  you pick it again in the same sound. The last 4 edited effects are kept; a preset change or switching off forgets them.
- **Memory**: the effects share one memory. When it runs out, an effect plays in 16 bits: its name, its slot and a DELAY TIME or
  STUTTER DIV past full quality turn **pale orange** (FX MEMORY FULL: ORANGE FX = 16-BIT). **Red** = the effect doesn't play,
  with the reason: NO MEMORY, NO CPU or SOLO ONLY (MULTI, chapter 8).
- SATURATION TYPE **OG** = the output stage used up to 1.7.0, as an effect: DRIVE 0.0DB + TONE OFF + MIX 127 + LEVEL 0 sounds
  exactly like it (put it last in the chain; switching TYPE to OG sets an untouched DRIVE / TONE to these values). LO-FI MORE: SATURATE, TONE, LEVEL (a new LO-FI starts as loud as the dry sound).
  EQ: 4 bands, GAIN -24..+24 dB, LOW CUT / HIGH CUT across 20 Hz - 20 kHz (an EQ from older presets switches to these ranges the
  first time you turn a GAIN or CUT knob, without a jump), SEL held + knob = that band's Q. Effect LFO RATE goes up to about
  100 Hz; FREQ SHIFT has SYNC on MORE K4. CHORUS FEEDBACK no longer runs away by itself (same sound up to 95, above that it rings
  but fades).
- **OCTAVER TYPE** (MORE K1): **PEDAL** (as before: follows one note, like a pedal) or **VOICE**: the engine plays the two lowest
  notes again an octave (OCT -1) and two octaves (OCT -2) lower, with the same filters and LFO, no delay. Only free voices are
  used: your notes always come first (6 notes in all at most; none on a MONO sound, fewer with low VOICES or with
  UNISON). Not on MINI, the drum engines or a MULTI track.
- **PERFORM, MULTI only** (FX held + key): live effects on the whole 4-track mix (repeats, gates, tape stops, rewinds, filter
  sweeps, LATCH). Without a key K1-K4 = FILTER, CRUSH, THROW, DEPTH; an effect key held + knob = its settings (K1 SYNC, K2 START,
  K3 LENGTH; SCRATCH: SYNC, RATE 1/32-1/2). OCT+ / OCT- = CURVES (GATE, PUMP, REPEAT, HALF on F3-B3). In SOLO, FX works right on
  press (FX page, second press = MORE).
- **PERFORM stays open**: hold FX and release it without touching a key or knob - the keys are now effects without FX (held,
  or on / off with LATCH = SEL). A single short FX click or HOME goes back to the page you came from; latched effects keep
  playing until you switch them off in PERFORM or turn LATCH off. The old way still works: FX held + key (or knob), release FX
  and PERFORM closes. A red REWIND LENGTH = not enough memory for full speed (it rewinds slower).

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
  first: PATTERN, right past the last: FEEL), **SEL = SONG ON / OFF** (while playing: from the first position after this pass; the header says NEXT S1 until then). With SONG ON,
  PLAY starts at the first position and after the last one the song starts again; the header shows S2/5. Started from a DAW (MIDI
  clock, CLOCK IN), Start also plays from the first position and Continue goes on from the position where it stopped; when the first
  position has another pattern than the one it stopped on, its very first note can come up to ~20 ms late. The next pattern starts exactly at the end of the
  pass, without a gap (a pattern changed while it played is saved first, with a short gap; a position with a scene fades the
  sound for a moment while its sounds load). While SONG
  plays, a LOOP waits (every pass is the whole pattern). Changing the pattern by hand while playing turns SONG off.
- **SWITCH** (SONG page **K4**): **NOW** (default, as before) or **AT END**: a pattern chosen on PATTERN K1 while playing
  waits for the end of the pass (header NEXT P03); choosing the playing pattern again cancels it. Stopped: it changes at once.
- Drums: **RACK**, like the Channel Rack in FL Studio (white keys = steps). A step with a note glows on its key; during PLAY the current step lights up too (brightest on a note) and the page follows PLAY.
- Drums in NOTES: a note shows how long the sound really rings. Shorten it with K3 and that hit is cut short (using the sound's FADE); each note has its own length. Lengthen it back and it follows the sound again. Keys and MIDI always play the whole sound.
- **ARP** (ARP = on / off; held = settings, in MULTI 0.7 s): five pages, ARP, MORE, CHORD, KEY and STEPS. 19 modes (UP, DOWN,
  UP-DOWN, AS PLAYED, RANDOM, CHORD, TURING, UP+8, CONVERGE, DIVERGE, PINKY, THUMB, WALK, DOWN-UP, UP&DOWN, DOWN&UP, CON-DIV,
  PINKY U-D, THUMB U-D); STEPS = REPEATS (each step 1-8 times), OFFSET, RETRIG; LATCH (SEL on MORE); CHORD = a chord on every key
  (SEL = learn; in MULTI every track keeps its own chord and VOICING, selected or not; SOLO has its own), KEY = scale
  (ALL / SNAP / LOCK, LIGHTS).
- MULTI: SEQ plays 4 tracks; in the SEQ pages SEL + OP1-OP4 picks the track you edit; notes of the other tracks appear as ghost
  notes. **SEQ held + OP1-OP4 = mute / unmute T1-T4** (from any screen). SEQ held alone = sequencer settings (scenes: HOME on HOME,
  chapter 8).
- **SAVE on SEQ pages** (NOTES, PATTERN, FEEL, RACK, AUTO, the SEQ settings, TOOLS, the note card) saves the **pattern** at once,
  without a window (while SEQ plays: SAVES AT STOP, it is saved at STOP without a gap in the sound); there the SAVE light shows the
  pattern's state. To save the sound from there: HOME, then SAVE. A red **NOT SAVED** in the SEQ header = the pattern's notes are
  not in memory yet (they save themselves in silence: right after STOP / REC OFF, otherwise about 10 s after your last touch).

## 8. MULTI: 4 tracks and MASTER

- To enter: **hold HOME** (hold it again to go back to SOLO). The new mode appears on screen for a moment.
- Tracks: **T1** plays a drum engine (88, 99, LN, MORPH or DUAL) or nothing; T2-T4 play any engine, drums too. An engine plays on
  two tracks at most, each with its own sound, effects and preset (the second one starts from INIT); WAVE and MINI play on one
  track only. WAVE doesn't play next to FM6 or MINI. An engine that can't play next to the others appears in red with the reason,
  and the track keeps its current engine. MIDI: channels 1-4 = T1-T4.
- **Notes**: drum hits are never cut. T2-T4 share up to 12 held notes, fewer with heavy engines (with nothing else playing: FM6 11,
  two FM6 9, PHASE 7); MINI, a drum engine on T2-T4 and every playing track effect take a fixed share. Past the limit the oldest
  note is released, on layered tracks first.
- **HOME is the main screen, it works like the FX page**: SELECT = track (like a slot), **SEL + ALGO = the track's engine** (only
  engines that can play; ALGO alone = ALGO KNOB as in SOLO, on an empty track the engine), **K1 VOL, K2 PAN, K3 REVERB send,
  K4 DELAY send, SEL = MUTE**. The sends stay when you change the track's preset or engine (GLO > AUDIO > FX SEND: KEEP; PRESET
  takes them from each preset). A muted track shows a red **MUTE** in the track bar at the top and in VOL; the selected muted track
  gets a colored frame instead of the full color (like an effect that is off on the FX page).
- **SEL held + K1-K4 = VOL of T1, T2, T3, T4** (a mixer for all four tracks at once, for playing live and recording): while SEL
  is held, the four knob boxes show T1-T4 with their levels (red MUTE on a muted track). It works on the PERFORM screen too. After
  a turn, letting go of SEL does nothing (no MUTE).
- **SEL held longer (without a knob) = engine list** for the track: SELECT = engine, ALGO = group, SEL = OK, HOME = back without a change; red = can't play now, with the reason. On an empty track a short
  SEL opens the list too.
- **4 sounds at once**: the 4 tracks play together from the sequencer and from MIDI (channels 1-4). The FM1 keys play the
  **selected** track, plus every track with **KEYS LAYER** on: **hold ARP + OP1-OP4 = KEYS LAYER T1-T4 ON / OFF** (a **+** in
  the track bar; while ARP is held the OP keys of layered tracks light up). Layered synth tracks get the same notes as the
  selected one (octave, TRANSPOSE, chord, ARP, LATCH, pedal); a layered drum track plays the sound under the key. A layer is
  saved with MULTI and in scenes. Recording with the keys goes to the selected track only. Switching the selected track doesn't
  cut a long note coming from a DAW.
- **OCT** on a synth track moves the keys of the synth tracks; on a drum track it is the drums' DRUM TUNE. The two are kept apart
  (2.0.1), so changing the octave on T2-T4 never retunes the drums.
- **EDIT held + K1-K4 = the engine's quick page** (usually FILTER), like K1-K4 on the SOLO main screen; back after 2 s.
- **PRESETS** = the selected track's sounds, each with its own effects; **SEL held + PRESETS** keeps the current effects (chapter 5).
- **Scenes: HOME on HOME = scene list** (8 scenes; MULTI has no ENGINE window: the engine is SEL + ALGO or the list): SELECT = scene, SEL = load, **SAVE = store** (engines, presets, VOL, PAN, MUTE, sends, MASTER),
  SEL held on a scene = DELETE?, HOME = back. A scene remembers saved presets, so save a changed sound first (message "Tn NOT SAVED").
  A scene also keeps its SEQ pattern: with SEQ > OPTIONS > SCENE = **+ PATTERN** (default) loading it switches to that pattern too
  (SOUND = sounds only).
- **FX page in MULTI**: first **MASTER**, then the shared **REVERB and DELAY sends** (S1 / S2, in the order they play; swap them to
  change it), then the selected track's chain.
- **Effects per track**: every effect except REVERB and DELAY plays on a track, with its own sound. OCTAVER, SUB, COMB and AUTO WAH
  follow the notes of their own track (SUB stays silent on T1 and drum tracks). A few are lighter on a track (GRAIN grains up to
  about 90 ms, STUTTER's long DIVs turn pale orange sooner, COMB down to A#0, SHIMMER's octave a touch quieter, no OCTAVER VOICE).
  Track effects share one processor budget and the memory that's left: one that doesn't fit shows **NO CPU** or **NO MEMORY**, and
  a muted track frees its share. If the processor still runs out in a dense moment, track effects give way one by one, faded, and
  come back when it's calm; MASTER keeps playing.
- **REVERB and DELAY**: one of each for the whole mix, fed by the sends (K3 / K4 on HOME). A preset's own REVERB or DELAY only sets
  where its send starts (in its chain it says SOLO ONLY). When track effects need memory, a playing DELAY fades out and comes back
  with a shorter line.
- **MASTER**: up to four effects over the whole mix (M1-M4, any effect but REVERB and DELAY, any order), before the output limiter;
  they get processor and memory before the tracks' effects. From 2.1.0 the light effects marked **+** can sit in MASTER more than
  once (ADD in the list, as in a chain), each copy with its own knobs and MORE pages; WET, SYNC and NO DRUMS belong to the slot. On the MASTER position K1-K4 set each one's **WET**; SEL opens the slots
  (they work like a track's chain, FX = the effect's MORE). After the effect's pages FX shows **MASTER ROUTING**: K1 REVERB, K2 DELAY,
  K3 PERFORM = AFTER (MASTER works on their sound too) or BEFORE MASTER; K4 LIMIT = the output limiter's threshold (= GLO > AUDIO >
  LIMIT). FX once more: **MASTER DRUMS**: K1-K4 set each slot to ALL or **NO DRUMS** (that slot works on the mix without the drum
  tracks, and the drums join right after it; NO DRUMS slots always play first). So a SIDECHAIN can duck everything but the drums
  while a COMPRESSOR after it still glues the whole mix. Scenes keep MASTER. The master compressor from GLO > AUDIO (before 2.0)
  is now an effect in MASTER, in the first slot, with the same settings.

## 9. GLO settings

PLAY (tuning, transpose, velocity, FIXED VEL, TUNE KNOB), MIDI (chapter below), AUDIO, DISPLAY, ALGO KNOB, SYSTEM (INFO, FACTORY
RESET, BACKUP, CALIBRATION, HINTS, UPDATE MODE).

**AUDIO**: SPEAKER EQ, 2ND CORE, MULTI MASTER (OFF, or LIMIT: one more limiter on the MULTI mix), FX SEND (KEEP / PRESET, chapter 8)
and **LIMIT** (-12..0 dB, -2 dB by default): the output stays clean, a limiter keeps the loudest moments under this threshold.

**DISPLAY**: visuals, lights, color (ORANGE / GREEN / PURPLE / CYAN / RED / MONO / TEAL / **PINK**: neon pink in SOLO, fuchsia across
MULTI), **ENGINE FONT** and **PRESET FONT** (KNEWAVE for engines and SEDGWICK for presets by default, or BARLOW, the look before
2.0, each on its own), where the color goes (2.0.2): **BACKGROUND** and **BARS** OFF / LOW (checkerboard) / HIGH in the theme
color, **TEXT**, **TILES**, **HINTS** and **GRID** (the NOTES / AUTO grid) each OFF (gray), THEME or a color of their own (MONO =
white), SCREEN OFF (1-30 minutes untouched: the screen goes black, the backlight stays on, any key or knob wakes it).

**ALGO KNOB**: up to 8 parameters on ALGO for each engine (ALGO = engine, K1-K4 = KNOB 1-4, SELECT = KNOB 5-8). Each column
shows where the parameter comes from (page, grey), its name (orange) and its current value as a bar; a parameter can sit in
one slot only; on a drum engine they can come from different drum sounds. Kept after power-off and firmware updates; FACTORY
RESET clears them.

**MIDI**: channel (ALL, 1-16 or **MPE**), MIDI OUT, LOCAL, PROG CHANGE, CLOCK IN (AUTO / INTERNAL), CLOCK OUT, GLIDE OUT, T1-T4
CHANNEL, OTHER TRACKS, KNOB PICKUP (JUMP / CATCH), **DRUM SELECT**, **UNASSIGNED** and CONTROLS OUT.
- **MPE** (2.1.0, MIDI CH set to MPE): for MPE controllers (ROLI Seaboard / Lumi, LinnStrument, Osmose, Push 3) and for DAWs that send
  MPE (Ableton Live 11+, Bitwig). Every note comes on its own MIDI channel, so each note has its own pitch bend: bend one finger and only
  that note moves. MPE lower zone: channel 1 works as before for all notes (pitch bend x the preset's BEND RANGE, mod wheel, sustain,
  pressure), channels 2-16 carry the notes, each with its own pitch bend of +-48 semitones (the MPE standard; set your controller to 48).
  Works in every synth engine (a chord from CHORD bends as one note; with GLIDE on, the bend glides too; the arpeggiator plays its own
  notes, without per-note bend). In SOLO; in MULTI it works like ALL. Per-note pressure and slide (CC 74) are not in 2.1.0: pressure
  works for all notes as before.
- **PROG CHANGE** (OFF by default): ON = Program Change n loads the n-th preset of the engine (0 = INIT). From 2.1.0 a **Bank Select**
  (CC 0) in front of it picks the engine too, in the order of the ENGINE window: 0 MINI, 1 SVF, 2 VCO, 3 DCO, 4 ALPHA, 5 LPG, 6 FM6,
  7 PHASE, 8 WAVE, 9 8BIT, 10 PHYS, 11 FORMANT, 12 SAMPLE, 13 88, 14 99, 15 LN, 16 MORPH, 17 DUAL (a DAW that counts from 1, like
  Ableton, shows MINI as bank 1). So one bank + program pair from a scene or a marker switches the whole sound. Each MIDI channel
  keeps its last bank; without a bank it works as before (presets of the engine you're on). In MULTI a Program Change on a track's
  channel goes to that track, also one that isn't selected (it becomes the selected one); a bank with another engine changes the
  track's engine like SEL + ALGO, and if that engine can't play there, the screen says why and the track stays as it is.
- **UNASSIGNED** (2.1.0, MULTI): MIDI from a channel that no track listens to (T1-T4 CHANNEL). **SELECTED TRACK** (default, as before):
  its notes play the selected track. **IGNORE**: nothing from such a channel reaches OG Synth, handy on a MIDI bus shared with other
  instruments.
- **DRUM SELECT** (2.0): which notes pick the sound shown on the drum pages. **KEYS** (default): the FM-1's keys and SEL + key;
  MIDI notes only play, so the pages don't jump while a DAW plays the drums. **KEYS+MIDI**: a MIDI note picks its sound too, as
  before 2.0.
- **GLIDE OUT** (how SEQ glides go out on MIDI OUT): **NOTES** (default) = a new note at the end of each glide; **BEND** = pitch
  bend on the track's channel, exactly the drawn curve, while that note plays alone on the channel (otherwise that glide as NOTES; set
  the receiving synth's pitch bend range to 48 semitones - PLAY sends RPN 0 = 48); **MPE** = in SOLO every SEQ note on its own channel
  2-16 (MPE lower zone, master channel 1) with its own pitch bend, so whole chords glide; in MULTI it works like BEND (tracks keep their
  channels).

## 10. Troubleshooting

- **The installer doesn't see the FM-1**: unplug and replug the FM-1, reload the page; the page retries by itself when the
  mode changes.
- **The FM-1 stayed in update mode** ("Felucca Update"): running *Install* again finishes the write.
- **The sound breaks up**: too many notes with a heavy effects chain. When the processor is full (2.1.0), WAVE and PHASE first thin
  out their UNISON (it comes back when there's room), then the oldest voice fades out within a few milliseconds; in MULTI, notes
  held on the other tracks stay as long as WAVE or PHASE can still give way, and track effects give way one by one until it's calm.
  If it still breaks up, use fewer effects or a lighter engine.
- **An effect is red**: it doesn't play, the reason is in the corner (NO MEMORY, NO CPU, SOLO ONLY). **Pale orange**: it plays
  in 16 bits because the effect memory is full.

---

Code: MPL-2.0 (application); engines, effects and sequencer are closed (Mike Johnson Beats). Based on fm1-base and
felucca-bsp. Kick and snare models in 88 and the tambourine in 99 from Plaits (Mutable Instruments, MIT). Fonts on the screen:
Barlow, Knewave and Sedgwick Ave Display, SIL OFL 1.1. M-VAVE and FM-1 are trademarks of their owners.
