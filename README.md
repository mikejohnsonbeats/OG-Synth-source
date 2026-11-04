# OG Synth: source code (MPL-2.0 part)

OG Synth is firmware for the M-VAVE FM-1 by Mike Johnson Beats. Site, installer and manual: https://ogsynth.cc

This repository holds the source of the parts of OG Synth covered by the Mozilla Public License 2.0, for each released
version (git tags match the firmware versions on ogsynth.cc).

## Downloads and recovery
Every version is also in [Releases](https://github.com/mikejohnsonbeats/OG-Synth-source/releases), in case ogsynth.cc is
ever down: the firmware package, an offline copy of the install page (runs on your own computer with Python 3) and SHA-256 sums.
- Back to stock: the install page has "Back to the stock firmware" (M-VAVE's FM-1.fwsc from https://www.m-vave.com/download),
  or use M-VAVE's own updater, M-UPGRADE.
- FM-1 won't start: with the m-bootloader option installed, hold PLAY while switching on for its recovery menu (REC: update
  mode). Without it, a Mac can put the stock firmware back with the
  [felucca-bsp tools](https://github.com/hugelton/felucca-bsp/tree/main/tools).

## What is here
- `app/`: the application (panel, screen, saving, MIDI, Manager and backup pages), `LICENSE`: MPL-2.0
- `patches/`: changes to [fm1-base](https://github.com/charlesvestal/fm1-base) and
  [felucca-bsp](https://github.com/hugelton/felucca-bsp) (MPL-2.0), applied with `git apply` before the build
- `app/ogs_fm1_bank.h`, `app/web/fm1/`: the FM-1 factory bank by M-VAVE, Apache-2.0 (`app/LICENSE-FM1-BANK.txt`)
- Drum models in the 88 and 99 engines are based on Plaits by Emilie Gillet (Mutable Instruments), MIT (`app/LICENSE-PLAITS.txt`)

## What is not here
The sound engines, effects and sequencer are separate files under a proprietary licence and are not published.
They are combined with the files here as a Larger Work (MPL-2.0 section 3.3), so this repository alone does not build.

M-VAVE and FM-1 are trademarks of their respective owners. OG Synth is not affiliated with M-VAVE.
