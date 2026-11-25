# OG Synth: source code (MPL-2.0 part)

OG Synth is firmware for the M-VAVE FM-1 by Mike Johnson Beats. Site, installer and manual: https://ogsynth.cc

This repository holds the source of the parts of OG Synth covered by the Mozilla Public License 2.0, for each released
version (git tags match the firmware versions on ogsynth.cc).

## Newer FM-1 stuck on 1.0.0 or 1.0.1?
FM-1s sold since autumn 2026 have a different memory chip. On OG Synth 1.0.0 or 1.0.1 they show SAVE FAILED and can't update
from ogsynth.cc. From 1.0.2 on, saving and updates from the site work on them. To get there, put the stock firmware back with
M-Flasher, then install the latest OG Synth:
1. Download [M-Flasher](https://github.com/jvitkauskas/m-flasher/releases) (free, independent, by Julius Vitkauskas) and the
   stock `FM-1.fwsc` from https://www.m-vave.com/download.
2. Connect the FM-1 straight to the computer (USB data cable, no hub) with OG Synth running. M-Flasher finds it: press
   **DOWNLOAD MODE**. If it doesn't find it: keep the cable plugged in, switch the FM-1 off, hold **REC** and switch it on.
3. When M-Flasher shows the FM-1 (the FM-1's screen says M-FLASHER CONNECTED, READY): **FLASH FIRMWARE**, choose
   `FM-1.fwsc`, leave "Also erase settings and Bluetooth pairings" ticked, then **REVIEW & FLASH** and **WRITE**.
   M-Flasher backs up the whole flash first.
4. Unplug, switch the FM-1 off and on (it starts the stock firmware), then install the latest OG Synth from https://ogsynth.cc.

All the steps, with tips for Mac and flaky cables: https://ogsynth.cc/manual#stuck

## Downloads and recovery
Every version is also in [Releases](https://github.com/mikejohnsonbeats/OG-Synth-source/releases), in case ogsynth.cc is
ever down: the firmware package, an offline copy of the install page (runs on your own computer with Python 3) and SHA-256 sums.
- Back to stock: the install page has "Back to the stock firmware" (M-VAVE's FM-1.fwsc from https://www.m-vave.com/download),
  or use M-VAVE's own updater, M-UPGRADE, or M-Flasher (steps above).
- FM-1 won't start: with the m-bootloader option installed, hold PLAY while switching on for its recovery menu (REC: update
  mode). Without it, a Mac can put the stock firmware back with the
  [felucca-bsp tools](https://github.com/hugelton/felucca-bsp/tree/main/tools).

## Switching firmware
- OG Synth keeps its sounds and settings in its own part of the FM-1's flash. It doesn't read projects or sounds saved by
  other firmwares, and another firmware may overwrite OG Synth's.
- Back up first: https://ogsynth.cc/backup/. "Back to the stock firmware" on the install page erases OG Synth's data before
  it writes the stock firmware, so the stock firmware starts clean. The backup keeps everything: presets, settings, DX7 and CZ
  banks, SEQ patterns, WAVE tables and samples.
- Updating from an older OG Synth keeps your sounds and settings. 2.1 moves your USER presets to a new layout the first time
  it starts; an older OG Synth put back after that doesn't see them, so keep the backup you made before updating.
- Identity (what the FM-1 answers to installers): `FM-1_1313` and the version, 2.1.0 = `FM-1_1313020100`
  (`FM-1_11` and the version up to 1.6.2).

## What is here
- `app/`: the parts of the application based on the fm1-base template: `app.c` (the template's functions as OG Synth
  changed them), `app.h`, `fm1.toml`, `words.tsv`, the manual, the emulator and backup pages; `LICENSE`: MPL-2.0
- `patches/`: changes to [fm1-base](https://github.com/charlesvestal/fm1-base) and
  [felucca-bsp](https://github.com/hugelton/felucca-bsp) (MPL-2.0), applied with `git apply` before the build
- `app/ogs_fm1_bank.h`, `app/web/fm1/`: the FM-1 factory bank by M-VAVE, Apache-2.0 (`app/LICENSE-FM1-BANK.txt`)
- The kick and snare models in 88 and the tambourine in 99 are based on Plaits by Emilie Gillet (Mutable Instruments), MIT
  (`app/LICENSE-PLAITS.txt`)
- `app/assets/fonts/`: Knewave (Tyler Finck) and Sedgwick Ave Display (The Sedgwick Ave Project Authors), the name fonts on the
  FM-1's screen, SIL Open Font License 1.1 (their `-OFL.txt` files)

## What is not here
The sound engines, effects, sequencer and the rest of the application (the `ogs_app_*.c` files `app.c` includes:
pages, MULTI, MIDI, saving, the Manager) are separate files under a proprietary licence and are not published.
They are combined with the files here as a Larger Work (MPL-2.0 section 3.3), so this repository alone does not build.

M-VAVE and FM-1 are trademarks of their respective owners. OG Synth is not affiliated with M-VAVE.
