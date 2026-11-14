# Changelog

## v1.8.0 — 2026-11-14
Źródła MPL OG Synth 1.8.0 „Song”: SONG i kolejka wzoru (strona SONG, zmiana dokładnie na końcu obiegu — UI w app.c, przygotowanie
i zamiana w plikach zamkniętych ogs_auto.c / ogs_seq.c), MULTI SEL + K1-K4 = VOL T1-T4 i KEYS LAYER, OCTAVER VOICE (oktawy na
wolnych głosach, limit MONO / VOICES / UNISON), strony MOD i GLO grupami, ALGO KNOB z paskami, presety z własnymi efektami, okno
nazwy, SysEx 24 / 26 / 27 (paczka presetów: cały preset w kawałkach z potwierdzeniem) — app.c; ogs_engine.h, ogs_lists.h; Manager
(app/web/manager.html + nowy paczka_sysex.js: wgrywanie paczki). Dane paczki (app/web/paczka.js, brzmienia) nie wchodzą do źródeł.

## v1.7.1 — 2026-11-11
Źródła MPL OG Synth 1.7.1 „Clean” (z niewydaną 1.7.0: glide i zapis wzorów — v1.7.0 wyżej). Czyste wyjście wszystkich silników: limiter
-2 dB z wyprzedzeniem (kod w pliku zamkniętym ogs_fx.c) i wyjście liniowe zamiast tanh — app.c; FM6: ALGORITHM w trakcie nuty przez bramkę
bez trzasków — app.c; sinus FM6 z interpolacją w plikach zamkniętych; app.h: wersja.

## v1.7.0 — 2026-11-10
Źródła MPL OG Synth 1.7.0 „Glide” (zaawansowany glide w NOTES: rysowanie skoków, karta nuty > GLIDE, TOOLS > GLIDE: SIMPLE /
ADVANCED, skoki idą za nutą — app.c; edycja skoków i dźwięk w plikach zamkniętych ogs_glide.c, ogs_seq.c i silnikach;
ogs_engine.h: `note_bend` i `ctl.gid`; pewniejszy zapis wzoru — zapis zaraz po STOP / REC OFF, SAVE sceny zapisuje nuty,
„NOT SAVED” w nagłówkach SEQ; logo startowe kodem Huffmana — ogs_logo.h + dekoder w app.c; wspólne rysowanie karty i chipów;
app.h: wersja).

## v1.6.4 — 2026-11-09
Źródła MPL OG Synth 1.6.4 „Wake” (GLO > SCREEN OFF: podświetlenie zostaje, ekran czarny — app.c i łatka fm1-base lcd.c `lcd_dark`;
ekrany systemowe UPDATE / RESTORE FROM USB? / UPDATE MODE widoczne na czarnym ekranie — unity.c, main_fm1.c; app.h: wersja).

## v1.6.3 — 2026-11-09
Źródła MPL OG Synth 1.6.3 „Hub” (własny numer firmware FM-1_1313XXYYZZ dla The Firmware Hub — fm1.toml id_prefix, stary
FM-1_11 w id_old; łatka fm1-base: strona kopii i WIPE przed stock znają oba numery, „Back to the stock firmware” czeka na
zamknięcie portów MIDI po WIPE — błąd od 1.0.2; app.h: wersja). README: sekcja Switching firmware.

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
