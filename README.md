# OG Synth: source code (MPL-2.0 part)

OG Synth is firmware for the M-VAVE FM-1 by Mike Johnson Beats. Site, installer and manual: https://ogsynth.cc

This repository holds the source of the parts of OG Synth covered by the Mozilla Public License 2.0, for each released
version (git tags match the firmware versions on ogsynth.cc).

## What is here
- `app/`: the parts of the application based on the fm1-base template: `app.c` (the template's functions as OG Synth
  changed them), `app.h`, `fm1.toml`, `words.tsv`, the manual, the emulator and backup pages; `LICENSE`: MPL-2.0
- `patches/`: changes to [fm1-base](https://github.com/charlesvestal/fm1-base) and
  [felucca-bsp](https://github.com/hugelton/felucca-bsp) (MPL-2.0), applied with `git apply` before the build
- `app/ogs_fm1_bank.h`, `app/web/fm1/`: the FM-1 factory bank by M-VAVE, Apache-2.0 (`app/LICENSE-FM1-BANK.txt`)
- Drum models in the 88 and 99 engines are based on Plaits by Emilie Gillet (Mutable Instruments), MIT (`app/LICENSE-PLAITS.txt`)

## What is not here
The sound engines, effects, sequencer and the rest of the application (the `ogs_app_*.c` files `app.c` includes:
pages, MULTI, MIDI, saving, the Manager) are separate files under a proprietary licence and are not published.
They are combined with the files here as a Larger Work (MPL-2.0 section 3.3), so this repository alone does not build.

M-VAVE and FM-1 are trademarks of their respective owners. OG Synth is not affiliated with M-VAVE.
