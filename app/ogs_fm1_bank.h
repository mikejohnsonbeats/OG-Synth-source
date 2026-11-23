/* SPDX-License-Identifier: Apache-2.0
 * Oryginalny bank 128 brzmień M-VAVE FM1 (V15), Copyright (c) 2026 珠海笙科智能科技有限公司 (M-VAVE).
 * Źródło: app/fm_factory.inc z github.com/m-vave/fm1_open (wyciągnięty z obrazu FM-1_015.fwsc, Apache-2.0).
 * ZMIENIONE przez Mike Johnson Beats (2026-10-28): kolejność brzmień wg sekcji i nazwy (OG Synth pokazuje FM1_NAME;
 * v0.41: pole nazwy brzmienia, 10 B, pominięte). Parametry brzmień bez zmian. Licencja: app/LICENSE-FM1-BANK.txt.
 * Wygenerowane: node tools/fm1_bank.mjs <fm_factory.inc> (VMEM SHA-256 ecad0b8620f9d784d46c8da1e3c5de02c40214649ed8dec218026431046aad25). */
#ifndef OGS_FM1_BANK_H
#define OGS_FM1_BANK_H
#include <stdint.h>
#define NFM1 128
#define NFM1_SEC 15
#define NFM1_BANK 4                      /* (v0.53) brzmienia jako 4 banki DX7 po 32 (app/web/fm1/FM1-1..4.syx) */
/* (v0.53) skróty FNV-1a (4096 B) oryginalnych banków FM1 wgranych jako banki DX7: firmware rozpoznaje je w miejscach DX7
 * i pokazuje jako sekcję FM1 (sekcje + nazwy niżej); brzmienia nie siedzą już w firmware (-15 KB flasha programu) */
static const uint32_t FM1_BANK_FNV[NFM1_BANK] = {0x57eafe0fu, 0xad2771b7u, 0x0c40b138u, 0xa489146du};
/* (2.0, miejsce) brzmienia 1-96 (12 sekcji po 8): nazwa = sekcja + spacja + numer 1-8 (PIANO 1 .. VOICE 8, jak dotąd;
 * app.c preset_name); tu tylko własne nazwy 97-128 (BELLS, PERC, FX) */
#define NFM1_NUM 96
static const char *const FM1_NAME[NFM1 - NFM1_NUM] = {
    "FROG BELL", "CLANG BELL", "TUBULAR BELL", "BIG BEN", "ELECTRO GONG", "TIBET", "GAMALONG", "KALIMBA",
    "STEEL DRUM", "TORIMTORA 2", "BD WHO CARES", "TOM TOMS", "TIMBALES", "TIMPANI", "CONGA", "LOG DRUM", "BLOCK",
    "COWBELL", "TAMBOURINE", "FINGER CYMB", "BRUSHES", "PERCUSSION", "SAW EM UP 1", "SAW EM UP 2", "SAW EM UP 3",
    "DESCENT", "WAVES", "ECHO", "BOWOAN", "MONO M", "MONO Z", "WHAT IF"
};
static const struct { uint8_t first; const char *name; } FM1_SEC[NFM1_SEC] = {
    {0, "PIANO"}, {8, "ORGAN"}, {16, "GUITAR"}, {24, "DS GUITAR"}, {32, "BASS"}, {40, "SYN BASS"}, {48, "SYN LEAD"}, {56, "SYN PAD"}, {64, "STRING"}, {72, "BRASS"}, {80, "WOODWIND"}, {88, "VOICE"}, {96, "BELLS"}, {106, "PERC"}, {118, "FX"}};
#endif
