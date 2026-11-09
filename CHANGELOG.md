# Changelog

## v1.6.2 — 2026-11-09
Źródła MPL OG Synth 1.6.2 „Clock” (ARP za zegarem MIDI z TRS / USB, paczki F8, ARP SWING — kod w pliku zamkniętym
ogs_seq.c; app.c: mikro-test CPU SysEx 0x12 tylko przy OGS_BENCH=1, sq_zoom minsize; app.h: wersja).

## v1.6.1 — 2026-11-09
Źródła MPL OG Synth 1.6.1 „Amp env” razem z niewydaną 1.6.0 „Auto” (AUTO — automatyka w SEQ, kod silnika w pliku zamkniętym
ogs_auto.c; MULTI: SEQ + OP1-4 = MUTE ścieżek, ustawienia SEQ po 0,7 s; TOOLS: COPY PATTERN / TRACK FROM; FM6: SAVE > SEND AS DX7
i AMP ENV; gest_matrix: n < 0 = czwarte pole nie jest VIA; usunięte przenoszenie zapisów sprzed 1.0).

## v1.5.1 — 2026-11-09
Źródła MPL OG Synth 1.5.1 „Controls out” (GLO > MIDI > CONTROLS OUT: gałki i przyciski FM1 wysyłają MIDI do DAW — nauczone w MIDI
LEARN swoim CC / kanałem / zakresem, inne CC 21-28 i 102-115, echo z DAW pomijane; scena MULTI pamięta wzór SEQ, SEQ > OPTIONS > SCENE;
pole GLO 73 po CC IN; minsize na funkcjach obsługi).

## v1.5.0 — 2026-11-08
Źródła MPL OG Synth 1.5.0 „Latch” (LATCH cały akord i bez ARP, pedał sustain CC 64, tryby ARP, ALGO = makro do 8 parametrów — obiekt
OBJ_MAC, FX SEND KEEP, MULTI SEL + ALGO; app/ogs_lists.h).

## v1.4.2 — 2026-11-07
Źródła MPL OG Synth 1.4.2 „Drum keys” (perkusja: F#5 jak OP1-OP6 — SEL + F#5, w RACK od razu; dwa FM6 w MULTI: nuta po 10 punktów,
razem najwyżej 9; SysEx pomiaru MULTI 0x1C bez zapisu GLO).

## v1.4.1 — 2026-11-07
Źródła MPL OG Synth 1.4.1 „Together” (MULTI: ten sam silnik na dwóch ścieżkach — bliźniak mx_t, zapis OBJ_MTX, pole GLO G_MTTW;
DCO + ALPHA i 88 + 99 razem; app/app.h: OBJ_MTX w mapie zapisu i kopii zapasowej). Wersja 1.4.0 nie była wydana.

## v1.3.1 — 2026-11-06
Źródła MPL OG Synth 1.3.1 „Env key” (F#5 = obwiednia głośności, RACK: klawisze z nadrukiem na strony perkusji,
CHORD LEARN w perkusji, engine_switch bez strony spoza silnika; app/ogs_engine.h: K_AENV).

## v1.3.0 — 2026-11-06
Źródła MPL OG Synth 1.3.0 „MIDI learn” (GLO > MIDI > SEL: gałki i przyciski FM1 z kontrolera MIDI; patches/fm1-base.patch:
plat_master_midi — MASTER z MIDI do ruszenia gałki).

## v1.1.2 — 2026-11-05
Źródła MPL OG Synth 1.1.2 „Clean scenes” (lista scen MULTI: SEL trzymany usuwa scenę, pusta scena czyści ścieżki;
HOME: PRESET n / m fabrycznych presetów z efektami, issue #2).

## v1.1.1 — 2026-11-05
Źródła MPL OG Synth 1.1.1 „Fine tune” (TUNE A4 427,0-453,0 Hz co 0,1 Hz, DETUNE unisona do +-100 centów).

## v1.1.0 — 2026-11-05
Źródła MPL OG Synth 1.1.0 „Key & chords” (tonacja wzoru, akordy KEY 3 / 7 i VOICING, nazwa akordu, RECORD z BPM i TAP TEMPO,
nowe pola GLO, UPDATE MODE; app/ogs_lists.h: listy wartości parametrów jako numery).

## v1.0.2b — 2026-11-04
OG Synth 1.0.2 nadpisana (ta sama wersja): powrót do stock ze strony najpierw kasuje dane OG Synth (SysEx WIPE).

## v1.0.2 — 2026-11-04
Źródła MPL OG Synth 1.0.2 (zapis i aktualizacje na FM1 z flashem UC25HQ80, usuwanie presetów USER, komunikat instalatora).

## v1.0.1 — 2026-11-04
Źródła MPL OG Synth 1.0.1 (GLO > MIDI > T1-T4 CHANNEL).

## v1.0.0 — 2026-11-04
Źródła MPL OG Synth 1.0.0 (pierwsza oficjalna wersja; wcześniej 0.x).

## v0.91.0 — 2026-11-03
Źródła MPL OG Synth 0.91.0 (pierwsza publiczna wersja).
