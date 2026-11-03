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
static const char *const FM1_NAME[NFM1] = {
    "PIANO 1", "PIANO 2", "PIANO 3", "PIANO 4", "PIANO 5", "PIANO 6", "PIANO 7", "PIANO 8", "ORGAN 1", "ORGAN 2",
    "ORGAN 3", "ORGAN 4", "ORGAN 5", "ORGAN 6", "ORGAN 7", "ORGAN 8", "GUITAR 1", "GUITAR 2", "GUITAR 3", "GUITAR 4",
    "GUITAR 5", "GUITAR 6", "GUITAR 7", "GUITAR 8", "DS GUITAR 1", "DS GUITAR 2", "DS GUITAR 3", "DS GUITAR 4",
    "DS GUITAR 5", "DS GUITAR 6", "DS GUITAR 7", "DS GUITAR 8", "BASS 1", "BASS 2", "BASS 3", "BASS 4", "BASS 5",
    "BASS 6", "BASS 7", "BASS 8", "SYN BASS 1", "SYN BASS 2", "SYN BASS 3", "SYN BASS 4", "SYN BASS 5", "SYN BASS 6",
    "SYN BASS 7", "SYN BASS 8", "SYN LEAD 1", "SYN LEAD 2", "SYN LEAD 3", "SYN LEAD 4", "SYN LEAD 5", "SYN LEAD 6",
    "SYN LEAD 7", "SYN LEAD 8", "SYN PAD 1", "SYN PAD 2", "SYN PAD 3", "SYN PAD 4", "SYN PAD 5", "SYN PAD 6",
    "SYN PAD 7", "SYN PAD 8", "STRING 1", "STRING 2", "STRING 3", "STRING 4", "STRING 5", "STRING 6", "STRING 7",
    "STRING 8", "BRASS 1", "BRASS 2", "BRASS 3", "BRASS 4", "BRASS 5", "BRASS 6", "BRASS 7", "BRASS 8", "WOODWIND 1",
    "WOODWIND 2", "WOODWIND 3", "WOODWIND 4", "WOODWIND 5", "WOODWIND 6", "WOODWIND 7", "WOODWIND 8", "VOICE 1",
    "VOICE 2", "VOICE 3", "VOICE 4", "VOICE 5", "VOICE 6", "VOICE 7", "VOICE 8", "FROG BELL", "CLANG BELL",
    "TUBULAR BELL", "BIG BEN", "ELECTRO GONG", "TIBET", "GAMALONG", "KALIMBA", "STEEL DRUM", "TORIMTORA 2",
    "BD WHO CARES", "TOM TOMS", "TIMBALES", "TIMPANI", "CONGA", "LOG DRUM", "BLOCK", "COWBELL", "TAMBOURINE",
    "FINGER CYMB", "BRUSHES", "PERCUSSION", "SAW EM UP 1", "SAW EM UP 2", "SAW EM UP 3", "DESCENT", "WAVES", "ECHO",
    "BOWOAN", "MONO M", "MONO Z", "WHAT IF"
};
static const struct { uint8_t first; const char *name; } FM1_SEC[NFM1_SEC] = {
    {0, "PIANO"}, {8, "ORGAN"}, {16, "GUITAR"}, {24, "DS GUITAR"}, {32, "BASS"}, {40, "SYN BASS"}, {48, "SYN LEAD"}, {56, "SYN PAD"}, {64, "STRING"}, {72, "BRASS"}, {80, "WOODWIND"}, {88, "VOICE"}, {96, "BELLS"}, {106, "PERC"}, {118, "FX"}};
#endif
