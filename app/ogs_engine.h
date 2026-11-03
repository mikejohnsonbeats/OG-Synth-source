/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Mike Johnson Beats */
/* OGS: wspólne dla silników — opis parametru i to, co aplikacja (app.c) wie o silniku.
 * Parametr: id < np = prawdziwy (p[id], zapisywany w presecie); id >= np = wirtualny (vget / vset: np. ADSR DX7
 * liczony z obwiedni, wybór widoku pod SEL). Strony: gałka K1..K4 -> id (knob), SEL -> id (sel), trzymany SEL +
 * gałka -> id (more: parametry rzadziej używane, strona zostaje prosta). */
#pragma once
#include <stdint.h>

enum { F_NUM, F_SIGNED, F_LIST, F_NONE, F_CUSTOM };   /* F_CUSTOM: tekst wartości robi silnik (text) */
typedef struct {
    const char *name;
    int16_t lo, hi, def;
    uint8_t fmt;
    const char *const *list;
} pdef_t;

static const char *const L_ONOFF[] = {"OFF", "ON"};

/* kontrolery wspólne dla silników: ustawia przerwanie audio (MIDI, nacisk klawiszy), czytają silniki */
static struct {
    float bend;                              /* pitch bend -1..1 */
    float wheel;                             /* mod wheel (CC 1) 0..1 */
    float press;                             /* nacisk: klawisze FM1 i aftertouch MIDI, 0..1 */
    float a4;                                /* strój: A4 w Hz (GLO TUNE) */
    uint8_t slide;                           /* sekwencer: ta nuta wjeżdża glide z poprzedniej (legato, jak slide w FL) */
    int8_t slide_from;                       /* z której nuty (trzymana jeszcze przez sekwencer) */
    float bpm;                               /* tempo (SEQ / zegar MIDI): LFO SYNC */
    int8_t dsemi;                            /* (v0.56) perkusja: OCT (+ TRANSPOSE) = przestrojenie wszystkich dźwięków
                                              * o półtony, układ klawiszy zostaje (decyzja użytkownika 2026-10-29) */
} ctl = {0.0f, 0.0f, 0.0f, 440.0f, 0, 0, 120.0f, 0};
#define SLIDE_GLIDE 32                       /* czas glide nuty z SLIDE, gdy brzmienie ma GLIDE 0 */

/* przeciążenie CPU (app_overload, przerwanie audio): silnik przy następnym renderze płynnie wycisza najstarszy
 * wybrzmiewający głos i zeruje znacznik */
static volatile uint8_t ogs_overload;

/* Wspólny RAM silników: gra jeden silnik naraz (zmiana silnika = cisza), więc stan głosów każdego leży w tym samym
 * miejscu: ogs_eram (.bss) i ogs_epool (.pool, osobna pamięć — co było w .pool, zostaje w .pool). Silnik opisuje swój
 * stan typem *_ram_t, a dawne nazwy (wc, zd, dx...) to makra na ten obszar; rozmiar obu daje app.c (największy z
 * silników: *_ERAM, *_EPOOL). Kto jest w RAM (eng_play), zmienia przerwanie audio na początku bloku: zera, potem
 * reset() nowego silnika. Poza przerwaniem stan głosów tylko czytamy i tylko silnika, który jest w RAM. */
/* (v0.60, MULTI etap 1) ogs_eram / ogs_epool to wskaźniki bazowe stanu silnika (pamięć: ogs_eram_mem / ogs_epool_mem
 * w app.c). SOLO: zawsze na wspólny obszar. MULTI: każda ścieżka ustawi swój obszar przed wejściem do silnika. */
extern uint8_t *ogs_eram, *ogs_epool;
#define ERAM_UP(n) (((n) + 7u) & ~7u)
#define ERAM_AT(off) (ogs_eram + ERAM_UP(off))

enum { G_EDIT, G_ENV, G_LFO, G_ALL, NG };   /* sekcje stron; ALL = SELECT z HOME */
/* podpisane czarne klawisze FM1 (skróty) */
enum { K_OP1, K_OP2, K_OP3, K_OP4, K_OP5, K_OP6, K_PITCH, K_GLOBAL, K_MONO, K_POLY, NLABEL };
/* (v0.64.5) skróty SEL + klawisz: silnik daje koła stron t[OP1..GLOBAL][KEY_CYC] (-1 = koniec / brak). Klawisz z
 * innej strony = pierwsza strona koła, ze strony koła = następna (po ostatniej pierwsza); świeci na stronach koła */
#define KEY_CYC 5
static inline int32_t key_page(const int8_t (*t)[KEY_CYC], int32_t label, int32_t page)
{
    int32_t i;
    if (label < K_OP1 || label > K_GLOBAL || t[label][0] < 0)
        return -1;
    for (i = 0; i < KEY_CYC && t[label][i] >= 0; i++)
        if (t[label][i] == page)
            return i + 1 < KEY_CYC && t[label][i + 1] >= 0 ? t[label][i + 1] : t[label][0];
    return t[label][0];
}
static inline int32_t key_lit(const int8_t (*t)[KEY_CYC], int32_t page)   /* klawisz bieżącej strony (-1 żaden) */
{
    int32_t l, i;
    for (l = K_OP1; l <= K_GLOBAL; l++)
        for (i = 0; i < KEY_CYC && t[l][i] >= 0; i++)
            if (t[l][i] == page)
                return l;
    return -1;
}

typedef struct {
    const char *name;
    int16_t *p;                              /* brzmienie, które gra */
    int32_t np, npages, nfact;               /* parametry, strony, presety fabryczne (z INIT) */
    const char *const *page_name;
    const uint8_t *grp[NG];                  /* strony sekcji (ALL: ENV, LFO, EDIT) */
    int8_t ngrp[NG];
    int8_t quick;                            /* strona gałek z HOME (wraca po 2 s) */
    int16_t mode;                            /* parametr MONO (0) / POLY lub PARA (1) */
    int16_t algo;                            /* parametr pod pokrętłem ALGO */
    int16_t retrig[3];                       /* zmiana = nowy przydział nut (wszystko cichnie); -1 koniec */
    void (*init)(void);                      /* przy starcie: tablice poza wspólnym RAM, brzmienie (0: nic) */
    void (*reset)(void);                     /* wchodzi do wspólnego RAM (wyzerowanego): stan głosów od nowa */
    void (*load)(int32_t i);                 /* fabryczny preset i (0 = INIT) */
    const char *(*preset_name)(int32_t i);
    void (*note_on)(int32_t note, float vel);
    void (*note_off)(int32_t note);
    void (*all_off)(void);
    void (*render)(float *l, float *r, uint32_t n);
    int32_t (*knob)(int32_t page, int32_t k);   /* id pod gałką, -1: puste pole */
    int32_t (*sel)(int32_t page);               /* id pod SEL, -1: brak */
    int32_t (*more)(int32_t page, int32_t k);   /* SEL trzymany: id pod gałką w warstwie MORE, -1: puste (0: brak MORE) */
    const pdef_t *(*def)(int32_t id);
    int32_t (*vget)(int32_t id);
    int (*vset)(int32_t id, int32_t v);         /* 1: zmiana brzmienia (gwiazdka, SAVE), 0: tylko widok */
    void (*text)(int32_t id, int32_t v, char *b);
    int32_t (*label_page)(int32_t label, int32_t grp, int32_t page);   /* skrót: dokąd (-1 nic); grp G_ALL = z HOME */
    int32_t (*page_label)(int32_t page);        /* klawisz, który świeci na tej stronie (-1 żaden) */
    int (*has_graph)(int32_t page);
    void (*graph)(int32_t page);                /* wykres pod kolumnami (y 92, wys. 50) */
    int32_t (*held_note)(void);                 /* nuta na dole ekranu, -1 żadna */
    float (*scope_semi)(void);                  /* wysokość do oscyloskopu */
    int (*sounding)(void);
    int (*graph_live)(int32_t page);            /* wykres z tym, co gra teraz: odświeżany (0: żaden) */
    /* v0.35 (0: brak) */
    int32_t (*step)(int32_t id, int32_t v, int32_t d);   /* gałka po liście w innej kolejności niż numery (WAVE TABLE);
                                                          * -1: zwykły krok */
    int32_t (*gest)(int32_t src, int32_t id, int32_t op, const char **sname);   /* gest LFO / ENV + gałka (GEST_*) */
    int32_t (*moddepth)(int32_t id);            /* ile parametr jest modulowany (-100..100, pasek przy wartości), 0 nic */
    /* v0.87, perkusja (0: brak): koniec nuty skróconej w piano rollu (tylko SEQ; note_off zostaje bez skutku: klawisze i MIDI
     * grają do końca), czas wybrzmienia dźwięku tej nuty w sekundach (długość nuty na ekranie) */
    void (*note_cut)(int32_t note);
    float (*sound_s)(int32_t note);
} engine_t;

/* v0.35: gest „trzymaj LFO / ENV + kręć gałką”. src = GEST_LFO / GEST_ENV | klawisz OP1..OP4 (0..3) albo GEST_DEF
 * (bez klawisza: domyślne źródło silnika). gest() zwraca id ilości modulacji parametru id z tego źródła; op: ASK tylko
 * sprawdza (sname: nazwa źródła), MAKE szuka połączenia albo zakłada w wolnym slocie matrycy, DROP kasuje połączenie
 * z ilością 0. Błędy: GEST_NOTGT (parametr bez modulacji), GEST_FULL (matryca pełna), GEST_NOSRC (nie ma takiego
 * źródła: klawisz OP nic nie wybiera). */
enum { GEST_ASK, GEST_MAKE, GEST_DROP };
enum { GEST_NOTGT = -1, GEST_FULL = -2, GEST_NOSRC = -3 };
#define GEST_LFO 0x10
#define GEST_ENV 0x20
#define GEST_DEF 0xF

/* matryca: wiersz = SOURCE, DEST, AMOUNT, VIA (row(i): id SOURCE slotu i); s, d: numery źródła i celu w silniku */
static inline int32_t gest_matrix(int16_t *p, int32_t n, int32_t (*row)(int32_t i), int32_t s, int32_t d, int32_t op)
{
    int32_t i, fr = -1;
    for (i = 0; i < n; i++) {
        int32_t b = row(i);
        if (p[b] == s && p[b + 1] == d) {
            if (op == GEST_DROP && p[b + 2] == 0)
                p[b] = p[b + 1] = 0;
            return b + 2;
        }
        if (fr < 0 && p[b] == 0 && p[b + 1] == 0)
            fr = b;
    }
    if (op != GEST_MAKE)
        return op == GEST_ASK ? 0 : GEST_NOTGT;
    if (fr < 0)
        return GEST_FULL;
    p[fr + 2] = 0;                                  /* (ilość 0: założenie niczego nie zmienia w dźwięku) */
    p[fr + 3] = 0;
    p[fr + 1] = (int16_t)d;
    p[fr] = (int16_t)s;
    return fr + 2;
}

/* silniki ze stałymi ścieżkami (MINI, VCO, SEM, DCO): parametr -> gałka ilości z LFO / z obwiedni (-1: brak). Jedno
 * LFO i jedna obwiednia: klawisze OP nic nie wybierają. Gałki ilości unsigned: obrót w lewo do 0. */
typedef struct {
    int16_t id, lfo, env;
} gest_fix_t;
static inline int32_t gest_fixed(const gest_fix_t *t, int32_t n, int32_t src, int32_t id, const char **sname,
                                 const char *lname, const char *ename)
{
    int32_t i, a;
    if ((src & 0xF) != GEST_DEF && (src & 0xF) != 0)   /* (v0.65) OP1 = to jedno LFO / ta jedna obwiednia */
        return GEST_NOSRC;
    if (sname)
        *sname = src & GEST_ENV ? ename : lname;
    if (id < 0)
        return 0;
    for (i = 0; i < n; i++)
        if (t[i].id == id) {
            a = src & GEST_ENV ? t[i].env : t[i].lfo;
            return a >= 0 ? a : GEST_NOTGT;
        }
    return GEST_NOTGT;
}
/* pasek: większa z ilości (LFO, obwiednia) w skali -100..100 */
static inline int32_t gest_fixdepth(const gest_fix_t *t, int32_t n, const int16_t *p, const pdef_t *(*def)(int32_t),
                                    int32_t id)
{
    int32_t i, j, best = 0;
    for (i = 0; i < n; i++)
        if (t[i].id == id)
            for (j = 0; j < 2; j++) {
                int32_t a = j ? t[i].env : t[i].lfo, v;
                if (a < 0 || def(a)->hi <= 0)
                    continue;
                v = p[a] * 100 / def(a)->hi;
                if ((v < 0 ? -v : v) > (best < 0 ? -best : best))
                    best = v;
            }
    return best;
}

/* (v0.65) matryca modulacji silników na szkielecie (VCO, SVF, DCO, ALPHA, 8BIT, LPG; decyzja użytkownika 2026-10-30:
 * strona MOD z nowymi celami): SK_MXN wierszy SOURCE (0 OFF, 1 LFO, 2 ENV 1 = obwiednia filtra / ENV, 3 ENV 2 = AMP),
 * DEST (numer parametru + 1, 0 = brak), AMOUNT (-100..100 = cały zakres celu), VIA (nieużywane: wiersz jak w WAVE)
 * na końcu parametrów silnika (stare presety: zera = brzmienie bez zmian) */
#define SK_MXN 4
#define SK_MX_ROW(srcl, n) {"SOURCE", 0, 3, 0, F_LIST, srcl}, {"DEST", 0, n, 0, F_CUSTOM, 0}, \
                           {"AMOUNT", -100, 100, 0, F_SIGNED, 0}, {"VIA", 0, 0, 0, F_NONE, 0}
#define SK_MX_PDEF(srcl, n) SK_MX_ROW(srcl, n), SK_MX_ROW(srcl, n), SK_MX_ROW(srcl, n), SK_MX_ROW(srcl, n)

/* pasek modulacji: suma ilości połączeń do celu d (bez wyłączonych źródeł) */
static inline int32_t gest_depth(const int16_t *p, int32_t n, int32_t (*row)(int32_t i), int32_t d)
{
    int32_t i, a = 0;
    for (i = 0; d > 0 && i < n; i++) {
        int32_t b = row(i);
        if (p[b] && p[b + 1] == d)
            a += p[b + 2];
    }
    return a < -100 ? -100 : a > 100 ? 100 : a;
}

/* (v0.65) gest / strona MOD w silnikach na szkielecie: stała ścieżka (t: ta sama gałka ilości co na stronie LFO >)
 * ma pierwszeństwo, reszta celów z listy mxl idzie do matrycy (row: wiersz slotu). Źródła: LFO = OP1, ENV: OP1 =
 * ENV 1 (e1), OP2 = ENV 2 (e2, gdy jest) */
static inline int32_t gest_sk(const gest_fix_t *t, int32_t nt, const int16_t *mxl, int32_t nmx, int16_t *p,
                              int32_t (*row)(int32_t i), int32_t src, int32_t id, int32_t op, const char **sname,
                              const char *lname, const char *e1, const char *e2)
{
    int32_t k = (src & 0xF) == GEST_DEF ? 0 : src & 0xF, s, i;
    if (k > (src & GEST_ENV && e2 ? 1 : 0))
        return GEST_NOSRC;
    s = src & GEST_ENV ? 2 + k : 1;
    if (sname)
        *sname = s == 1 ? lname : s == 2 ? e1 : e2;
    if (id < 0)
        return 0;
    if (s <= 2)
        for (i = 0; i < nt; i++)
            if (t[i].id == id && (s == 2 ? t[i].env : t[i].lfo) >= 0)
                return s == 2 ? t[i].env : t[i].lfo;
    for (i = 0; i < nmx && mxl[i] != id; i++)
        ;
    return i < nmx ? gest_matrix(p, SK_MXN, row, s, id + 1, op) : GEST_NOTGT;
}
static inline int32_t gest_skdepth(const gest_fix_t *t, int32_t nt, const int16_t *p, const pdef_t *(*def)(int32_t),
                                   int32_t (*row)(int32_t i), int32_t id)   /* pasek: większa ze stałej i z matrycy */
{
    int32_t a = gest_fixdepth(t, nt, p, def, id), b = gest_depth(p, SK_MXN, row, id + 1);
    return (b < 0 ? -b : b) > (a < 0 ? -a : a) ? b : a;
}
static inline int sk_dest_text(int32_t id, int32_t mx, int32_t v, const pdef_t *(*def)(int32_t), char *b)
{                                                /* DEST w matrycy: nazwa celu (1: obsłużone) */
    const char *n;
    if (id < mx || id >= mx + 4 * SK_MXN || (id - mx) % 4 != 1)
        return 0;
    for (n = v > 0 ? def(v - 1)->name : "OFF"; *n; n++)
        *b++ = *n;
    *b = 0;
    return 1;
}
