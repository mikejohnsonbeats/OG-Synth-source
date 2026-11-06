/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal; OGS changes Copyright (C) 2026 Mike Johnson Beats */
/* OGS: firmware dla M-VAVE FM1 na fm1-base.
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "OG SYNTH"
#define OGS_VERSION "1.3.0"              /* GLO > SYSTEM > INFO */
#define APP_SPLASH_RGB RGB(255, 154, 31)       /* #FF9A1F, pomarańczowy OG Deck */
#define APP_SPLASH                       /* ekran startowy: logo OG Synth (app_splash, app.c) */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_CANVAS_PX (240u * 182u)      /* strony i okna: 182 wiersze (bez podpowiedzi w dolnym pasku poza HOME) */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */

/* the store: the app's objects, the panel calibration (the platform's) and Bluetooth's address and pairing among
 * them, and their flash offsets */
/* OGS: OBJ_GLOBAL = ustawienia GLO, OBJ_USER0..7 = presety USER po 16 (01-16 .. 113-128), OBJ_FACT = nadpisane fabryczne, OBJ_SYX = spis banków DX7 (dane banków: obszar danych 0xA0000, plat_data) */
/* OBJ_FXP = efekty globalne: bieżące każdego silnika, presetów USER i nadpisanych fabrycznych (v0.9) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_GLOBAL, OBJ_USER0, OBJ_USER1, OBJ_FACT, OBJ_USER2, OBJ_USER3, OBJ_SYX, OBJ_FXP,
       OBJ_FXP2, OBJ_FXX, OBJ_USER4, OBJ_USER5, OBJ_USER6, OBJ_USER7, OBJ_FXH, OBJ_FXH2, OBJ_SET2, OBJ_XTRA, OBJ_NOBJ };
                                              /* OBJ_FXP2: dalsza część efektów (6 slotów nie mieści się w jednym);
                                               * OBJ_FXX: gałki efektów ponad 8 (v0.14: COMPRESSOR, MULTIBAND);
                                               * v0.16: OBJ_USER4..7 = USER 65-128, OBJ_FXH(2) = ich efekty;
                                               * v0.38: OBJ_SET2 = brzmienia na start silników 9-16, OBJ_XTRA = dodatki
                                               * presetów poza silnikiem (CHORD ...) — oba w oknie XDATA1 (0xF8000,
                                               * 0xFA000; łatka fm1-base unity.c: zapis obiektów także tam) */
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0xDA000u, 0xD4000u, 0xD6000u, 0xD8000u, 0xD0000u, 0xD2000u, 0xCE000u, 0xCC000u, 0xCA000u, \
                       0xC8000u, 0xC6000u, 0xC4000u, 0xC2000u, 0xC0000u, 0x9E000u, 0x9C000u, 0xF8000u, 0xFA000u}   /* (banki DX7: 0xA0000-0xBFFFF) */

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* OGS: SysEx z USB (główna pętla): pojedyncze brzmienie DX7, bank DX7 (z OG SYNTH MANAGER w kawałkach albo w całości) */
#define APP_SYSEX
int app_sysex(const uint8_t *p, uint32_t n);

/* OGS: kopia zapasowa przez USB (fm1.toml backup = true; OG Synth Manager > BACKUP, tools/fm1_backup.py --magic OGS1):
 * ustawienia, brzmienia na start, presety USER, nadpisane fabryczne, spis banków DX7 (dane banków DX7 w obszarze danych
 * nie wchodzą: ich .syx zostają na komputerze) */
#define APP_BACKUP_OBJS {OBJ_SETTINGS, OBJ_GLOBAL, OBJ_USER0, OBJ_USER1, OBJ_FACT, OBJ_USER2, OBJ_USER3, OBJ_SYX, OBJ_FXP, OBJ_FXP2, OBJ_FXX, \
                         OBJ_USER4, OBJ_USER5, OBJ_USER6, OBJ_USER7, OBJ_FXH, OBJ_FXH2, OBJ_SET2, OBJ_XTRA}
/* (v0.38: dwa obiekty więcej na końcu; starsza kopia (17 obiektów) wczytuje się dalej — brakujące obiekty puste,
 * łatka fm1-base backup.c; kopii z nowej wersji starszy OG Synth nie przyjmie) */
#define APP_BACKUP_MAGIC 0x3153474Fu       /* "OGS1" */
#define APP_BACKUP_VERSION 1
#define APP_BACKUP_WHAT "REPLACES ALL PRESETS"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);

/* OGS (v0.20): render powyżej 90 % czasu bloku (FM1) -> app_overload: silnik płynnie wycisza najstarszy wybrzmiewający
 * głos zamiast przeciążać procesor (przeciążenie = trzeszczenie) */
#define APP_OVERLOAD
void app_overload(void);

/* the host simulator's checks (host/plat_host.c "expect"): octave, wave, note (-1 none), dirty, turnsN */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
