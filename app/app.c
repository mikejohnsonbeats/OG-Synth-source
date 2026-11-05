/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Mike Johnson Beats; based on the fm1-base template, Copyright (C) 2026 Charles Vestal */
/* OGS: aplikacja — panel, ekran, zapis. Silniki: MOOG (ogs_moog.c), DX7 (ogs_dx7.c); co aplikacja o nich wie:
 * ogs_engine.h. Nowy silnik = jego pliki + wpis w ENG (na dole) + miejsce w ustawieniach (ENG_POFF).
 * Część unity builda (device/unity.c, host/plat_host.c), po platformie: gfx.c i announce.c są w zasięgu.
 * Obsługa (wersja robocza, layout ustala użytkownik):
 *   EDIT / ENV / LFO  strony sekcji (kolejne naciśnięcie: następna; z innej sekcji: ta, na której z niej wyszliśmy)
 *   SELECT      strony w bieżącej sekcji; z HOME zawsze od 1 (ENV, LFO, EDIT); bez żadnej akcji wraca do HOME po 2 s
 *   SEL         przycisk funkcyjny. Krótko: dodatkowa opcja strony w pętli (opis w nagłówku); w oknach: zatwierdza.
 *               Trzymany + podpisany czarny klawisz: OP n (oscylator / operator; na stronach ENV jego obwiednia),
 *               PITCH, GLOBAL (VOICE), MONO / POLY; dostępne świecą przygaszone tylko przy trzymanym SEL;
 *               + gałka: warstwa MORE (rzadsze parametry, „+” przy nazwie strony)
 *   klawisz bieżącej strony (OP2 na OSC 2 / OP 2, GLOBAL na VOICE, PITCH ...) świeci, dopóki na niej jesteśmy
 *   K1..K4      4 parametry strony; z HOME od razu strona szybka silnika (MOOG: FILTER, DX7: ALGO), wraca po 2 s.
 *               Gdy strona potrzebuje dodatkowego wyboru, a SEL jest zajęty, K1 może być wyborem (DX7 LFO 2: operator)
 *   ALGO        MINI: CUTOFF (z każdej strony), FM6: ALGORITHM
 *   HOME        na HOME: okno ENGINE (SELECT wybiera, SEL ok, HOME wraca) — każdy silnik jak osobny instrument
 *   PRESETS     presety bieżącego silnika: INIT, fabryczne, USER, banki DX7; HOME trzymany + PRESETS = okno ALL PRESETS
 *               (wszystkie silniki; kręcenie wczytuje, SEL ok, HOME wraca do poprzedniego)
 *   SAVE        świeci, gdy brzmienie zmienione. Na INIT i brzmieniu z banku: nowy preset USER; na innym: okno
 *               OVERWRITE / NEW PRESET (SELECT wybiera, SEL zatwierdza, HOME nie); potem nazwa (12 znaków): SELECT
 *               litera, OCT- / OCT+ kursor, SEL zapisuje, HOME nie. Preset, oktawa, silnik zapisują się same (cisza, 5 s)
 *   HOME        ekran główny (oscyloskop)         OCT- / OCT+  oktawa
 *   GLO         GLOBAL (TUNE, MIDI CH, VELOCITY, PRESSURE) i DISPLAY (wizualizacje); SELECT: strona; zapis przy wyjściu
 *   SysEx       brzmienie DX7 (VCED) wczytuje się od razu; bank DX7 (32 brzmienia): okno IMPORT, SELECT miejsce 1-32 */
#include "fastmath.h"
/* (v0.71) flash: w unity (sterowanie silników, ekran, zapis; -Os) funkcje matematyczne jako jedna kopia zamiast
 * wklejania w ~300 miejscach (po 100-300 B każde). Ta sama arytmetyka (bez FMA): dźwięk i obraz bit w bit. Pętle DSP
 * (pliki o2 w fm1.toml) bez zmian; fm_tanhf / fm_sin_turns zostają wklejane (wyjście co próbkę, LFO). */
static float __attribute__((noinline)) ogs_m_exp2f(float x) { return fm_exp2f(x); }
static float __attribute__((noinline)) ogs_m_expf(float x) { return fm_expf(x); }
static float __attribute__((noinline)) ogs_m_log2f(float x) { return fm_log2f(x); }
static float __attribute__((noinline)) ogs_m_powf(float x, float y) { return fm_powf(x, y); }
static float __attribute__((noinline)) ogs_m_tanf(float x) { return fm_tanf(x); }
static float __attribute__((noinline)) ogs_m_sqrtf(float x) { return fm_sqrtf(x); }
static float __attribute__((noinline)) ogs_m_sinf(float x) { return fm_sinf(x); }
#define fm_exp2f ogs_m_exp2f
#define fm_expf ogs_m_expf
#define fm_log2f ogs_m_log2f
#define fm_powf ogs_m_powf
#define fm_tanf ogs_m_tanf
#define fm_sqrtf ogs_m_sqrtf
#define fm_sinf ogs_m_sinf
/* (v0.76) drugi rdzeń w MULTI dzieli się z wysyłkami (ogs_fx.c „MULTI: WYSYŁKI”, taśma): silnik, zanim da swoją część
 * głosów, czeka na koniec zadania wysyłek. SOLO: zadania nie ma — jak dotąd. */
void ogs_fx_sends_sync(void);
int ogs_fx_sends_busy(void);
static int ogs_cpu2_run(void (*fn)(uint32_t), uint32_t a)
{
    ogs_fx_sends_sync();
    return plat_cpu2_run(fn, a);
}
#define plat_cpu2_run(fn, a) ogs_cpu2_run(fn, a)
#include "ogs_vint.h"
#include "ogs_moog.c"
#include "ogs_dx7.c"
#include "ogs_fm1_bank.h"
#include "ogs_wt.c"
#include "ogs_pd.c"
#include "ogs_voice.c"                         /* (v0.48) wspólny szkielet głosu: VCO, SVF, DCO, 8BIT, LPG */
#include "ogs_va.c"
#include "ogs_sem.c"
#include "ogs_dco.c"
#include "ogs_bit.c"
#include "ogs_west.c"
#include "ogs_morph.c"
#include "ogs_dm.c"                            /* (v0.52) 88 i 99 */
#include "ogs_dual.c"                          /* (v0.54) DUAL */
#include "ogs_phys.c"                          /* (v0.78) PHYS */
#include "ogs_formant.c"                       /* (v0.84) FORMANT */
#include "ogs_fx.h"
/* (v0.88) perkusja: strony pokazują dźwięk uderzony klawiszem / MIDI (numer przed tytułem, światło klawisza); nuty SEQ go nie
 * zmieniają, inaczej przy PLAY gałki edytowałyby co chwilę inny dźwięk */
#define LANE_KEEP() const int32_t lk_mp = mp_lane, lk_dm = dm_lane, lk_du = du_lane
#define LANE_BACK() (mp_lane = lk_mp, dm_lane = lk_dm, du_lane = lk_du)

/* wspólny RAM silników (ogs_engine.h): tyle, ile potrzebuje największy */
#define EMAX(a, b) ((a) > (b) ? (a) : (b))
#define ERAM_N ERAM_UP(EMAX(EMAX(EMAX(MG_ERAM, DX_ERAM), EMAX(WT_ERAM, PD_ERAM)), EMAX(EMAX(VA_ERAM, SEM_ERAM), EMAX(DCO_ERAM, EMAX(BIT_ERAM, EMAX(WEST_ERAM, EMAX(MP_ERAM, EMAX(DM_ERAM, EMAX(DU_ERAM, EMAX(PH_ERAM, FO_ERAM))))))))))
#define EPOOL_N ERAM_UP(EMAX(EMAX(EMAX(MG_EPOOL, DX_EPOOL), EMAX(WT_EPOOL, PD_EPOOL)), EMAX(EMAX(VA_EPOOL, SEM_EPOOL), EMAX(DCO_EPOOL, EMAX(BIT_EPOOL, EMAX(WEST_EPOOL, EMAX(MP_EPOOL, EMAX(DM_EPOOL, EMAX(DU_EPOOL, EMAX(PH_EPOOL, FO_EPOOL))))))))))
static uint8_t ogs_eram_mem[ERAM_N] __attribute__((aligned(8)));
static uint8_t ogs_epool_mem[EPOOL_N] __attribute__((section(".pool"), aligned(8)));
uint8_t *ogs_eram = ogs_eram_mem, *ogs_epool = ogs_epool_mem;   /* (v0.60) wskaźniki bazowe (ogs_engine.h) */

/* zapisy flasha rzadkie (FLASH_IDLE_MS bez dotykania panelu, w ciszy); od v0.14.2 kasowanie w kawałkach (fm1-base
 * unity.c soft_erase): skan diod i ekran chodzą, diody nie gasną na czas zapisu */
#define FLASH_IDLE_MS 10000u

enum { ENG_MOOG, ENG_DX7, ENG_WT, ENG_PD, ENG_VCO, ENG_SEM, ENG_DCO, ENG_BIT, ENG_WEST, ENG_ALPHA, ENG_MORPH, ENG_88, ENG_99, ENG_DUAL, ENG_PHYS, ENG_FORMANT, NENG };   /* BIT: v0.42,
                                                                                                      * WEST: v0.44, ALPHA: v0.49,
                                                                                                      * MORPH (perkusja): v0.50 */
static const engine_t ENG[NENG];
#define NENG_MAX 16                          /* miejsca na silniki (v0.26; do v0.25: 8) */
#define NENG_V8 8                            /* do v0.25: miejsca w zapisach (ustawienia, efekty, GLO ALGO) */
_Static_assert(NENG <= NENG_MAX, "silniki: miejsca");
#define SET_P 768                            /* brzmienia silników w ustawieniach: stałe miejsca (ENG_POFF) */
#define SET_P_V15 640                        /* v0.15 - v0.16: PHASE 128 miejsc (te same miejsca, krótsze) */
#define SET_P_V13 512                        /* do v0.14: bez PHASE */
#define SET_PX 512                           /* v0.22: silniki od VCO w px[] (za pw[]): po 128 miejsc, 4 silniki */
/* Silniki 1-8 w OBJ_SETTINGS (settings_t): MOOG do 128, DX7 do 256, WAVE 128 (+ pw[]), PHASE 256; od SET_P: px[] — VCO
 * 128, SEM 128 (v0.23), DCO 128 (v0.27), 8. silnik 128 (zapas). Silniki 9-16 (v0.38): osobny obiekt OBJ_SET2 (set2_t,
 * po SET2_SLOT miejsc każdy), budowany w bk przy zapisie i czytany do bk przy starcie (bez kopii w RAM). Nowy silnik
 * = nic w układzie zapisu (docs/funkcje/pamiec.md). */
static const int16_t ENG_POFF[NENG_V8 + 1] = {0, 128, 384, 512, 768, 896, 1024, 1152, SET_P + SET_PX};
#define SET2_FIRST NENG_V8                   /* pierwszy silnik w OBJ_SET2 (numer 8 = 9. silnik) */
#define SET2_SLOT 220                        /* miejsc na brzmienie (rekord presetu mieści najwyżej 217 parametrów) */
#define SET2_N ((NENG_MAX - NENG_V8) * SET2_SLOT)
#define SET2_MAGIC 0x3254534Fu               /* "OST2" */
typedef struct {
    uint32_t magic;
    int16_t q[SET2_N];                       /* silnik e: q[(e - SET2_FIRST) * SET2_SLOT ..] */
} set2_t;
/* Obiekty w oknie XDATA1 (0xF8000, 0xFA000): loader aktualizacji po wgraniu kasuje tam każdy sektor, którego bajty
 * 0xF00-0xF4F wyglądają jak rekord aktualizacji (ldr_core.c, „AT” + CRC) — zapis (nagłówek 256 B + dane) kończy się
 * przed 0xF00, więc tych bajtów nie ma (zostają skasowane, 0xFF) */
#define XD1_OBJ_MAX (0xF00u - 256u)
_Static_assert(sizeof(set2_t) <= XD1_OBJ_MAX, "ustawienia: silniki 9-16 w jednym obiekcie, przed rekordem loadera");
#define WX_SLOT 128                          /* WAVE: miejsca w p[] (ENG_POFF); dalsze parametry w pw[] (v0.19) */
#define WX_N_V23 64                          /* do v0.23: WAVE pw[] (przed DRAW Q/X v0.24) */
#define WX_N 96                              /* v0.24: +32 (DRAW: Q x8, X x8) */
_Static_assert(NP <= 128 && NX <= 256 && NZ <= 256 && NW <= WX_SLOT + WX_N && NV <= 128 && NS <= 128 && NJ <= 128 && NBT <= 128,
               "ustawienia: brzmienie silnika mieści się w swoim miejscu");

#define SET_MAGIC 0x4D4A4239u            /* "MJB9" (v0.26: 16 miejsc na silniki) */
#define SET_MAGIC_8 0x4D4A4238u          /* "MJB8" (v0.6 - v0.25: miejsca na 8 silników, liczba parametrów) */
#define SET_MAGIC_7 0x4D4A4237u          /* v0.5 */
#define SET_MAGIC_6 0x4D4A4236u          /* v0.4: sam MOOG */
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +3 (v0.21; do v0.20: -2 .. +2) */
    uint8_t eng;                         /* bieżący silnik */
    uint8_t rsv[2];
    uint16_t preset[NENG_MAX];           /* ostatnio wczytany preset każdego silnika (nazwa na ekranie) */
    uint8_t edited[NENG_MAX];            /* brzmienie zmienione względem presetu */
    uint16_t np[NENG_MAX];               /* ile parametrów zapisano (silnik urósł: reszta domyślna) */
    int16_t p[SET_P];
    int16_t pw[WX_N];                    /* v0.19: WAVE ponad swoje 128 miejsc (parametry 128..) */
    int16_t px[SET_PX];                  /* v0.22: silniki od VCO (ENG_POFF >= SET_P) */
} settings_t;
_Static_assert(sizeof(settings_t) <= PLAT_STORE_MAX, "ustawienia: jeden obiekt");
static settings_t set;
static uint8_t set_dirty;
static uint32_t splash_until;            /* ekran startowy z logo (app_splash): do kiedy */
static uint8_t splash_on;                /* preset, oktawa, silnik, brzmienie zmienione od zapisu: zapis sam w ciszy */
#define E (&ENG[set.eng])
#define CUR (set.preset[set.eng])

static volatile uint8_t eng_play = 0xFF; /* silnik, który jest we wspólnym RAM (gra); zmienia tylko przerwanie audio */
static int eng_ready(void) { return set.eng == eng_play; }   /* stan głosów bieżącego silnika wolno czytać */
static int eng_sounding(void) { return eng_ready() && E->sounding(); }
static int32_t eng_held_note(void) { return eng_ready() ? E->held_note() : -1; }
static float eng_scope_semi(void) { return eng_ready() ? E->scope_semi() : 60.0f; }

static void nt_clear(void);
static void eng_enter(uint32_t e)        /* przerwanie audio: silnik e do wspólnego RAM (wszystko cichnie) */
{
    uint32_t i, *a = (uint32_t *)ogs_eram_mem, *b = (uint32_t *)ogs_epool_mem;
    ogs_eram = ogs_eram_mem;                 /* (v0.60) SOLO: silnik na wspólnym obszarze */
    ogs_epool = ogs_epool_mem;
    for (i = 0; i < ERAM_N / 4u; i++)
        a[i] = 0;
    for (i = 0; i < EPOOL_N / 4u; i++)    /* (.pool nie jest zerowane przy starcie) */
        b[i] = 0;
    ENG[e].reset();
    nt_clear();                              /* (v0.39) nuty i akordy od zera */
    eng_play = (uint8_t)e;
}


/* GLO: ustawienia całego instrumentu, osobny obiekt w pamięci (nie zależą od brzmienia ani SAVE). Pola dopisujemy na
 * końcu (stare zapisy wczytują się, brakujące = domyślne); numery pól się nie zmieniają. */
#define GLOB_MAGIC 0x4D4A4247u           /* "MJBG" (v0.4: 4 pola, v0.6: 8, v0.8: 24) — znacznik zostaje: ustawienia! */
enum { G_VHOME, G_VPAGES, G_TUNE, G_MIDICH, G_VEL, G_PRESS, G_TRANSP, G_ACCEL, G_MIDIOUT, G_LOCAL, G_PROGCH, G_SPKEQ,
       G_CPU2, G_LIGHTS, G_KEYL, G_SCROFF, G_COLOR, G_TOPKEYS, G_CLKIN, G_CLKOUT, G_VELFIX,
       G_ALGO0,                          /* ALGO: parametr pod gałką, osobno dla każdego silnika (-1: jak w silniku) */
       G_SQZINV = G_ALGO0 + 8,           /* v0.25.1, SEQ > PATTERN > TOOLS: odwrócony zoom SEL + K1, */
       G_SQK1INV,                        /* odwrócone K1 w NOTES (nuty akordu / przewijanie); stare zapisy: 0 = NORMAL */
       G_SQVEL,                          /* (v0.88) SEQ > OPTIONS: VEL nowej nuty (pole 31, stare zapisy 0: poza zakresem = 100) */
       G_ALGO8 = 32,                     /* v0.26: ALGO silników 9-16 (pole 31 wolne) */
       G_HINTS = 40,                     /* v0.29: SYSTEM > HINTS, 0 = ON (stare zapisy mają tu 0) */
       G_MTMST,                          /* (v0.77) AUDIO > MULTI MASTER: OFF, LIMIT, COMP (stare zapisy: 0 = OFF) */
       G_MTS0,                           /* (v0.82) wysyłki ścieżki T1..T4 (mt_sf): bity 0-6 REVERB, 7-13 DELAY; 0 = jak MIX w
                                          * presecie (stare zapisy), inaczej wartość 0-100 + 1 */
       G_MFXOFF = G_MTS0 + 4,            /* (v0.88) MASTER FX odpięty od ścieżki: bity 0-3 REVERB T1-T4, 4-7 DELAY; bit 8: zapis już z v0.88 */
       G_SQUNIT,                         /* (v0.88) SEQ > OPTIONS: SEL + ALGO co BEAT (0), STEP, BAR; (v0.92) COUNT IN: 8 | ON | LEN << 1
                                          * (poniżej 8: stare pole = bez odliczania, LEN 1 BAR) */
       G_MULTI = 48,                     /* v0.64 MULTI (docs/funkcje/multi.md): 0 SOLO, 1 MULTI */
       G_MT0,                            /* silnik ścieżki T1..T4 (NENG: pusta) */
       G_MTV0 = G_MT0 + 4,               /* głośność ścieżki 0-100 (80 = jak SOLO) */
       G_MTMUTE = G_MTV0 + 4,            /* MUTE: bit = ścieżka */
       G_MTFOC,                          /* wybrana ścieżka 0-3 */
       G_MTSYN,                          /* (v0.66.2) ostatnio wybrana ścieżka syntezatora 1-3: SEQ w SOLO (inne: T2) */
       G_MTP0,                           /* (v0.72) PAN ścieżki -50..50 (0 = środek, jak dotąd) */
       NGLOB = 64,
       G_MCTH = NGLOB,                   /* (v0.91) pola GLO ponad 64: glob.gx (za pfs, zapis bez nich = domyślne); MULTI MASTER COMP: */
       G_MCRA, G_MCAT, G_MCRE,           /* THRESH, RATIO, ATTACK, RELEASE (surowe 0-127 jak gałki COMPRESSOR; domyślne = stałe z v0.77) */
       G_MTCH0,                          /* (v1.0.1) MIDI > T1-T4 CHANNEL: kanał ścieżki MULTI 1-16 (domyślnie 1-4 = jak od v0.64;
                                          * zapis 1.0.0 ma tu 0: poza zakresem = domyślne) */
       G_MTCTL = G_MTCH0 + 4,            /* (v1.1.0) pola od 72: glob.gx2 (na końcu obiektu, zapis bez nich = domyślne);
                                          * MIDI > OTHER TRACKS: niewybrane ścieżki MULTI: NOTES (jak dotąd) / + BEND, WHEEL, PRESS (P6) */
       G_CCIN,                           /* (v1.1.0) MIDI > CC IN: OFF (jak dotąd) / ON: CC 21-24 = K1-K4 strony (P7) */
       G_TUNEK,                          /* (v1.1.0) PLAY > TUNE KNOB: FINE (jak dotąd) / SEMITONES (P9) */
       G_CHNAME,                         /* (v1.1.0) DISPLAY > CHORD NAME: OFF (jak dotąd) / ON (akordy A) */
       NGALL };
#define NGX 8                            /* miejsca glob.gx (zapas na kolejne pola) */
#define NGX2 8                           /* (v1.1.0) miejsca glob.gx2 */
_Static_assert(G_MTCTL == NGLOB + NGX, "GLO: gx pełne, dalej gx2 (v1.1.0)");
#define GV(q) (*((q) < NGLOB ? &glob.g[q] : (q) < NGLOB + NGX ? &glob.gx[(q) - NGLOB] : &glob.gx2[(q) - NGLOB - NGX]))
_Static_assert(G_MTP0 + 4 == NGLOB, "GLO: PAN T1-T4 w ostatnich polach");
#define G_ALGO_N 8                       /* miejsca na silniki w jednym bloku (G_ALGO0: 1-8, G_ALGO8: 9-16) */
_Static_assert(NENG_MAX == 2 * G_ALGO_N && G_ALGO8 + G_ALGO_N <= NGLOB, "GLO: ALGO każdego silnika");
static int32_t algo_g(int32_t e) { return e < G_ALGO_N ? G_ALGO0 + e : G_ALGO8 + e - G_ALGO_N; }   /* pole ALGO silnika */
static int32_t algo_eng(int32_t g)       /* silnik pola ALGO, -1: inne pole */
{
    if (g >= G_ALGO0 && g < G_ALGO0 + G_ALGO_N)
        return g - G_ALGO0;
    return g >= G_ALGO8 && g < G_ALGO8 + G_ALGO_N ? g - G_ALGO8 + G_ALGO_N : -1;
}
static const char *const L_MIDICH[] = {"ALL", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13",
                                       "14", "15", "16"};
static const char *const L_CPU2[] = {"OFF", "AUTO"};
static const char *const L_LIGHTS[] = {"OFF", "LOW", "HIGH"};
static const char *const L_KEYL[] = {"PLAYED", "+ C KEYS", "+ WHITE", "OFF"};
static const char *const L_SCROFF[] = {"NEVER", "1 MIN", "5 MIN", "15 MIN", "30 MIN"};
static const char *const L_COLOR[] = {"ORANGE", "GREEN", "PURPLE", "CYAN", "RED", "MONO"};
/* kolory OG Synth (v0.11.1: nowocześnie, jak OG Deck na Stream Decku — żywe, czyste barwy zamiast pastelowych):
 * [0] kolor główny (wypełnienia, drugi tekst), [1] wartości i wyróżnienia (pełna barwa). ORANGE = #FF9A1F z OG Deck,
 * kolor ekranu startowego (APP_SPLASH_RGB), domyślny. Numery zostają (zapisane GLO): AMBER -> PURPLE */
static const uint16_t OGS_PAL[6][2] = {
    {RGB(224, 122, 10), RGB(255, 154, 31)},  {RGB(52, 186, 78), RGB(76, 217, 100)},
    {RGB(146, 104, 230), RGB(179, 136, 255)}, {RGB(40, 160, 214), RGB(79, 195, 247)},
    {RGB(224, 64, 56), RGB(255, 90, 79)},    {RGB(188, 190, 196), RGB(240, 242, 246)},
};
/* (v0.64.2) MULTI: ten sam kolor w innym odcieniu (decyzja użytkownika 2026-10-29: MULTI ma się różnić od SOLO) —
 * ORANGE -> bursztyn / złoto, GREEN -> limonka, PURPLE -> magenta, CYAN -> głęboki niebieski, RED -> malina,
 * MONO -> ciepła kość */
static const uint16_t OGS_PAL_MT[6][2] = {
    {RGB(226, 168, 18), RGB(255, 204, 64)},  {RGB(128, 196, 36), RGB(168, 230, 64)},
    {RGB(196, 84, 214), RGB(228, 120, 250)}, {RGB(56, 106, 226), RGB(98, 146, 255)},
    {RGB(222, 52, 112), RGB(255, 92, 150)},  {RGB(206, 192, 166), RGB(248, 236, 212)},
};
/* ciemne stopnie: chłodne szarości OG Deck (#3A3D44, #5F6269, #8A8D94), przyciemnione pod ekran FM1 (mocno rozjaśnia
 * ciemne) */
static const uint16_t OGS_GREY[3] = {RGB(30, 32, 37), RGB(70, 73, 80), RGB(138, 141, 148)};
static const uint16_t SCROFF_S[5] = {0, 60, 300, 900, 1800};
/* klawisze 20..26 (górny rząd) nie mają przetwornika: velocity mierzone przez linię ich diod (PA10), stąd lekkie
 * żarzenie; NO GLOW wyłącza ten pomiar (stała siła na tych 7 klawiszach) */
static const char *const L_TOPKEYS[] = {"VELOCITY", "NO GLOW"};
/* zegar MIDI: INTERNAL = zawsze tempo wzoru; AUTO = gdy przychodzi F8 (DAW), tempo i start / stop z MIDI */
static const char *const L_CLKIN[] = {"INTERNAL", "AUTO"};
static const char *const L_INV[] = {"NORMAL", "INVERTED"};
static const char *const L_HINTS[] = {"ON", "OFF"};
static const char *const L_MTMST[] = {"OFF", "LIMIT", "COMP"};
static const char *const L_MTCTL[] = {"NOTES", "+ BEND, WHEEL"};   /* (v1.1.0) G_MTCTL */
static const char *const L_TUNEK[] = {"FINE", "SEMITONES"};        /* (v1.1.0) G_TUNEK */
static const pdef_t GDEF[NGALL] = {
    {"VISUAL HOME", 0, 1, 1, F_LIST, LI(L_ONOFF)}, {"VISUAL PAGES", 0, 1, 1, F_LIST, LI(L_ONOFF)},
    {"TUNE A4", 430, 450, 440, F_NUM, 0},      {"MIDI CH", 0, 16, 0, F_LIST, LI(L_MIDICH)},
    {"VELOCITY", -5, 5, 0, F_SIGNED, 0},       {"PRESSURE", -5, 5, 0, F_SIGNED, 0},
    {"TRANSPOSE", -12, 12, 0, F_SIGNED, 0},    {"KNOB ACCEL", 0, 1, 1, F_LIST, LI(L_ONOFF)},
    {"MIDI OUT", 0, 1, 1, F_LIST, LI(L_ONOFF)},    {"LOCAL", 0, 1, 1, F_LIST, LI(L_ONOFF)},
    {"PROG CHANGE", 0, 1, 0, F_LIST, LI(L_ONOFF)}, {"SPEAKER EQ", 0, 1, 0, F_LIST, LI(L_ONOFF)},
    {"2ND CORE", 0, 1, 1, F_LIST, LI(L_CPU2)},     {"LIGHTS", 0, 2, 2, F_LIST, LI(L_LIGHTS)},
    {"KEY LIGHTS", 0, 3, 0, F_LIST, LI(L_KEYL)},   {"SCREEN OFF", 0, 4, 0, F_LIST, LI(L_SCROFF)},
    {"COLOR", 0, 5, 0, F_LIST, LI(L_COLOR)},       {"TOP KEYS", 0, 1, 0, F_LIST, LI(L_TOPKEYS)},
    {"CLOCK IN", 0, 1, 1, F_LIST, LI(L_CLKIN)},    {"CLOCK OUT", 0, 1, 0, F_LIST, LI(L_ONOFF)},
    {"FIXED VEL", 0, 127, 0, F_NUM, 0},
#define GALGO(n) {n, -1, 255, -1, F_CUSTOM, 0}
    GALGO("MINI"), GALGO("FM6"), GALGO("WAVE"), GALGO("PHASE"), GALGO("VCO"), GALGO("SVF"), GALGO("DCO"), GALGO("8BIT"),
    {"ZOOM", 0, 1, 0, F_LIST, LI(L_INV)},          {"K1", 0, 1, 0, F_LIST, LI(L_INV)},
    {"NEW VEL", 1, 127, 100, F_NUM, 0},        /* 31 (v0.88): G_SQVEL */
    GALGO("LPG"), GALGO("ALPHA"), GALGO("MORPH"), GALGO("88"), GALGO("99"), GALGO("DUAL"), GALGO("PHYS"), GALGO("FORMANT"),   /* G_ALGO8 */
    {"HINTS", 0, 1, 0, F_LIST, LI(L_HINTS)},       /* 40 */
    {"MULTI MASTER", 0, 2, 0, F_LIST, LI(L_MTMST)},   /* 41 (v0.77) */
    {"SEND", 0, 16383, 0, F_NUM, 0}, {"SEND", 0, 16383, 0, F_NUM, 0},   /* 42-45 (v0.82): G_MTS0 */
    {"SEND", 0, 16383, 0, F_NUM, 0}, {"SEND", 0, 16383, 0, F_NUM, 0},
    {"LINK", 0, 511, 0, F_NUM, 0}, {"", 0, 15, 0, F_NUM, 0},   /* 46-47 (v0.88): G_MFXOFF, G_SQUNIT (v0.92: COUNT IN) */
    {"MULTI", 0, 1, 0, F_LIST, LI(L_ONOFF)},     /* 48 (v0.64): MULTI, ścieżki T1-T4 (silnik, NENG = pusta) */
    {"T1", 0, NENG, ENG_MORPH, F_NUM, 0}, {"T2", 0, NENG, ENG_MOOG, F_NUM, 0}, {"T3", 0, NENG, ENG_VCO, F_NUM, 0},
    {"T4", 0, NENG, ENG_DCO, F_NUM, 0},
    {"VOL", 0, 100, 80, F_NUM, 0}, {"VOL", 0, 100, 80, F_NUM, 0}, {"VOL", 0, 100, 80, F_NUM, 0},
    {"VOL", 0, 100, 80, F_NUM, 0},
    {"MUTE", 0, 15, 0, F_NUM, 0}, {"TRACK", 0, 3, 0, F_NUM, 0},
    {"TRACK", 0, 3, 0, F_NUM, 0},            /* 59: G_MTSYN (v0.73: zakres 0..3 — do v0.72 0..0, po restarcie T2) */
    {"PAN", -50, 50, 0, F_SIGNED, 0}, {"PAN", -50, 50, 0, F_SIGNED, 0}, {"PAN", -50, 50, 0, F_SIGNED, 0},   /* 60-63 */
    {"PAN", -50, 50, 0, F_SIGNED, 0},                                                                       /* (v0.72) */
    {"COMP THRESH", 0, 127, 21, F_CUSTOM, 0}, {"COMP RATIO", 0, 127, 29, F_CUSTOM, 0},   /* 64-67 (v0.91): glob.gx */
    {"COMP ATTACK", 0, 127, 79, F_CUSTOM, 0}, {"COMP RELEASE", 0, 127, 62, F_CUSTOM, 0},
    {"T1 CHANNEL", 1, 16, 1, F_NUM, 0}, {"T2 CHANNEL", 1, 16, 2, F_NUM, 0},   /* 68-71 (v1.0.1): G_MTCH0 */
    {"T3 CHANNEL", 1, 16, 3, F_NUM, 0}, {"T4 CHANNEL", 1, 16, 4, F_NUM, 0},
    {"OTHER TRACKS", 0, 1, 0, F_LIST, LI(L_MTCTL)}, {"CC IN", 0, 1, 0, F_LIST, LI(L_ONOFF)},   /* 72-75 (v1.1.0): glob.gx2 */
    {"TUNE KNOB", 0, 1, 0, F_LIST, LI(L_TUNEK)}, {"CHORD NAME", 0, 1, 0, F_LIST, LI(L_ONOFF)},
};
_Static_assert(NGALL <= NGLOB + NGX + NGX2, "GLO: miejsca gx2");
_Static_assert(G_ALGO0 + G_ALGO_N == G_SQZINV, "GLO: miejsca ALGO");
/* sekcje GLO: lista (SELECT wybiera, SEL wchodzi, HOME wraca); w sekcji gałki K1-K4, SELECT: strony po 4 */
enum { GS_PLAY, GS_MIDI, GS_AUDIO, GS_DISPLAY, GS_ALGO, GS_SYSTEM, NGS };
static const char *const GS_NAME[NGS] = {"PLAY", "MIDI", "AUDIO", "DISPLAY", "ALGO KNOB", "SYSTEM"};
static const char *const GS_HINT[NGS] = {"TUNE, TRANSPOSE, VELOCITY", "CHANNELS, OUT, LOCAL, CLOCK", "SPEAKER EQ, CORE, MASTER",
                                         "VISUALS, LIGHTS, COLOR", "PER ENGINE",
                                         "INFO, RESET, BACKUP, HINTS"};
#define GS_N 16                          /* pozycji w sekcji (ALGO KNOB: jedna na silnik, NENG_MAX) */
static const int8_t GS_ITEM[NGS][GS_N] = {
    {G_TUNE, G_TRANSP, G_VEL, G_VELFIX, G_PRESS, G_ACCEL, G_TOPKEYS, G_TUNEK, -1},
    {G_MIDICH, G_MIDIOUT, G_LOCAL, G_PROGCH, G_CLKIN, G_CLKOUT, G_MTCH0, G_MTCH0 + 1, G_MTCH0 + 2, G_MTCH0 + 3, G_MTCTL, G_CCIN, -1},
    {G_SPKEQ, G_CPU2, G_MTMST, G_MCTH, G_MCRA, G_MCAT, G_MCRE, -1},
    {G_VHOME, G_VPAGES, G_LIGHTS, G_KEYL, G_SCROFF, G_COLOR, G_CHNAME, -1},
    {G_ALGO0, G_ALGO0 + 1, G_ALGO0 + 2, G_ALGO0 + 3, G_ALGO0 + 4, G_ALGO0 + 5, G_ALGO0 + 6, G_ALGO0 + 7, G_ALGO8, G_ALGO8 + 1, G_ALGO8 + 2, G_ALGO8 + 6, G_ALGO8 + 7, -1},   /* silniki (NENG; od
                                                                                        * dziewiątego: G_ALGO8) */
    {-1},
};
enum { SY_INFO, SY_RESET, SY_BACKUP, SY_CALIB, SY_HINTS, SY_UPDATE, NSY };   /* SYSTEM: lista działań (SEL); (v1.1.0, P39) UPDATE MODE */
static const char *const SY_NAME[NSY] = {"INFO", "FACTORY RESET", "BACKUP", "CALIBRATION", "HINTS", "UPDATE MODE"};
/* (v0.73) sceny MULTI (multi.md etap 7): 8 miejsc na końcu obiektu GLO (kopia zapasowa je obejmuje; starsza wersja
 * czyta tylko pola GLO): silniki T1-T4, presety (zapisane brzmienia), VOL, PAN, MUTE, wybrana ścieżka; (v0.82) za nimi
 * wysyłki REVERB / DELAY (pola G_MTS0; zapis z v0.73-v0.81 ich nie ma: 0 = jak presety) */
#define SCN_N 8
#define SCN_NUM 4                        /* (v0.78) scn_t.rsv: numeracja presetów (0: sprzed PHYS, 1: z PHYS, 2: z FORMANT, 3: zestawy v0.88,
                                          * 4: mniej zestawów v0.90) */
typedef struct {
    uint8_t ok, foc, mute, rsv;
    uint8_t eng[4], vol[4];
    int8_t pan[4];
    uint16_t pre[4];
} scn_t;
/* (v0.85, decyzja użytkownika 2026-10-30) MASTER FX w MULTI: wspólne REVERB i DELAY, zawsze sloty 1-2 strony FX: gałki tu (nie w
 * presetach ścieżek; z presetu tylko MIX = start wysyłki), można zamienić kolejnością, nie można usunąć. Zapis w GLO i w
 * scenach; zapis bez nich: domyślne (HALL, TAPE, DELAY > REVERB). (v0.88, decyzja użytkownika 2026-11-02) p[3] = MIX: ile efektu w
 * miksie, 0: stoi (CPU 0), domyślnie 0; SEL odpina efekt od wybranej ścieżki (G_MFXOFF, sceny: scs bity 14-15), mode zawsze rodzaj;
 * odpięty od wszystkich: stoi. Ścieżka bez własnej wysyłki i bez efektu w presecie wysyła całą. */
typedef struct {
    fx_slot_t s[2];                      /* REVERB (0), DELAY (1); mode: rodzaj (zapis sprzed v0.88: 0 OFF, mfx_upgrade) */
    uint8_t x[FX_NX_V90];                /* ich gałki ponad 8 (v0.92: układ zapisu GLO bez ogona EQ) */
    uint8_t rev1st;                      /* kolejność: 0 DELAY > REVERB, 1 REVERB > DELAY */
    uint8_t sync;                        /* DELAY SYNC */
} mfx_t;
static struct {
    uint32_t magic;
    int16_t g[NGLOB];
    scn_t scn[SCN_N];
    uint16_t scs[SCN_N][4];
    mfx_t mfx;
    mfx_t scm[SCN_N];
    uint16_t pfs[PF_N];                  /* (v0.90) PERFORM: SYNC, START, LENGTH każdego klawisza (ogs_fx.h ogs_pf_cfg); zapis bez nich: 0 = jak dotąd */
    int16_t gx[NGX];                     /* (v0.91) pola GLO od 64 (GV); zapis bez nich: domyślne */
    uint8_t gbc[PF_NCV * 32 + PF_NCV];   /* (v0.91) PERFORM: krzywe (ogs_fx.h ogs_pf_cvd); zapis bez nich: domyślne */
    int16_t gx2[NGX2];                   /* (v1.1.0) pola GLO od 72 (GV); zapis bez nich: domyślne */
} glob;
_Static_assert(sizeof glob <= 4096u - 256u, "GLO: mieści się w obiekcie pamięci (ST_PAYLOAD_MAX)");
#define GLOB_V110 ((int32_t)__builtin_offsetof(__typeof__(glob), gx2))   /* zapis do 1.0.2: bez gx2 */
#define GLOB_V90 ((int32_t)__builtin_offsetof(__typeof__(glob), gx))   /* zapis do v0.90: bez gx */
static inline uint32_t mt_ch(int32_t t) { return (uint32_t)GV(G_MTCH0 + t) - 1u; }   /* (v1.0.1) kanał MIDI ścieżki t (0-15) */
#define GLOB_V91C ((int32_t)__builtin_offsetof(__typeof__(glob), gbc) + PF_NCV * 32 + PF_NCV)   /* z krzywymi PERFORM */
/* (v0.91) krzywe domyślne (propozycja): 1 GATE (1/16 na zmianę), 2 PUMP (narasta co ćwierćnutę), 3 REPEAT (ostatnia ćwierćnuta
 * powtarza trzecią), 4 HALF (każda szesnastka dwa razy: połowa tempa); SMOOTH 20 */
static void gb_def(uint8_t *g)
{
    static const uint8_t PU[4] = {0, 40, 75, 100}, HA[16] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8};
    int32_t s;
    for (s = 0; s < 16; s++) {
        g[s * 2] = (uint8_t)(s & 1 ? 0 : 100);
        g[32 + s * 2] = PU[s & 3];
        g[64 + s * 2] = 100;
        g[64 + s * 2 + 1] = (uint8_t)(s >= 12 ? 4 : 0);
        g[96 + s * 2] = 100;
        g[96 + s * 2 + 1] = HA[s];
        g[s * 2 + 1] = g[32 + s * 2 + 1] = 0;
    }
    for (s = 0; s < PF_NCV; s++)
        g[PF_NCV * 32 + s] = 20;
}
#define GLOB_V89 ((int32_t)__builtin_offsetof(__typeof__(glob), pfs))   /* zapis do v0.89: bez pfs */
#define GLOB_V81 (4 + 2 * NGLOB + SCN_N * (int32_t)sizeof(scn_t))   /* zapis do v0.81: bez scs */
#define GLOB_V84 (GLOB_V81 + 2 * 4 * SCN_N)  /* zapis do v0.84: bez MASTER FX */
_Static_assert(sizeof(scn_t) == 24, "scena: 24 B");
static uint8_t glob_dirty;
static int hints(void) { return !glob.g[G_HINTS]; }   /* GLO > SYSTEM > HINTS: podpowiedzi obsługi na pasku */

/* (v0.64) MULTI: 4 ścieżki naraz (docs/funkcje/multi.md). Ścieżka = silnik (każdy najwyżej raz; DCO / ALPHA i 88 / 99
 * mają wspólny kod: nie naraz), T1 perkusja, T2-T4 syntezatory. Stan każdej ścieżki w puli efektów
 * (ogs_fx_arena_take: łańcuch i PERFORM w MULTI stoją). Wybrana ścieżka = set.eng = eng_play: UI, klawisze FM1, SEQ
 * i ARP jak w SOLO; reszta gra obok, MIDI kanał 1-4 = T1-T4. Ustawienia w GLO (G_MULTI ...). Zmiana trybu albo
 * silnika ścieżki: całe wyjście przygasa (MT_XN), pamięć ścieżek od nowa, wyjście wraca. G_MULTI 0 (SOLO): nic. */
#define MT_N 4
#define MT_NONE NENG                         /* pusta ścieżka */
#define MT_XN 512u                           /* przygaszenie przy zmianie: 12 ms w dół, 12 ms w górę */
static const uint32_t MT_SZ[NENG][2] = {    /* stan silnika: *_ERAM, *_EPOOL (ALPHA: kod DCO) */
    {MG_ERAM, MG_EPOOL}, {DX_ERAM, DX_EPOOL}, {WT_ERAM, WT_EPOOL}, {PD_ERAM, PD_EPOOL}, {VA_ERAM, VA_EPOOL},
    {SEM_ERAM, SEM_EPOOL}, {DCO_ERAM, DCO_EPOOL}, {BIT_ERAM, BIT_EPOOL}, {WEST_ERAM, WEST_EPOOL}, {DCO_ERAM, DCO_EPOOL},
    {MP_ERAM, MP_EPOOL}, {DM_ERAM, DM_EPOOL}, {DM_ERAM, DM_EPOOL}, {DU_ERAM, DU_EPOOL}, {PH_ERAM, PH_EPOOL}, {FO_ERAM, FO_EPOOL}};
static volatile uint16_t mt_ver;             /* UI: zmiana trybu / silników ścieżek (przerwanie: po przygaszeniu) */
static uint16_t mt_seen;
static uint8_t mt_on, mt_eng[MT_N] = {MT_NONE, MT_NONE, MT_NONE, MT_NONE};
static uint8_t *mt_er[MT_N], *mt_ep[MT_N];
static float mt_g[MT_N];                     /* głośność ścieżki (wygładzona) */
static float mt_xg = 1.0f, mt_xs;            /* przygaszenie całego wyjścia (mt_xs < 0: w dół) */
static volatile uint32_t mt_used;            /* bajty puli zajęte przez ścieżki (SysEx 0x1E) */
static uint8_t mt_gdirty;                    /* UI: ustawienia MULTI do zapisu (w ciszy, jak brzmienia) */
static uint32_t mt_ovl;                      /* przeciążenie: ścieżki (bit), które jeszcze nie zrzuciły głosu */
#define MT_LIM 12                            /* (v0.79) wspólny limit trzymanych nut T2-T4 (mt_lim_on, niżej) */
#define MT_DRUM2 16u                         /* (v0.91) perkusja na T2-T4: punkty budżetu (szacunek; pomiar FM1: multi.md) */
/* (v0.80) limit ważony kosztem silnika, punkty z pomiaru FM1 (tools/pomiary/2026-10-30-multi-v0.79.0.txt): nuta ścieżki
 * kosztuje MT_PTS, MINI stale MT_MINI (mono, cała praca na jednym rdzeniu), razem najwyżej MT_BUDGET; PHASE obok MINI
 * drożej (pomiar v0.80.1: MINI + PHASE 1 + 6 i PHASE 4 + SVF 4 nadal z przerwami, sam PHASE 4 bez) */
#define MT_BUDGET 92u
#define MT_MINI 26u
#define MT_PD_MINI 16u
static const uint8_t MT_PTS[NENG] = {[ENG_DX7] = 8, [ENG_WT] = 5, [ENG_PD] = 12, [ENG_VCO] = 5, [ENG_SEM] = 5, [ENG_DCO] = 5,
                                     [ENG_BIT] = 5, [ENG_WEST] = 5, [ENG_ALPHA] = 5, [ENG_PHYS] = 5, [ENG_FORMANT] = 6};
/* (v0.76, do-konca B8) wysyłki MULTI (ogs_fx.c „MULTI: WYSYŁKI”): ścieżka, której preset ma REVERB / DELAY w łańcuchu,
 * wysyła MIX tego slotu (za głośnością i PAN) na wspólny efekt, a jej suche zostaje jak w SOLO (REVERB: 1 - 0,3 MIX^2).
 * Cele raz na blok (mt_send_pick, niżej przy fxc), wysyłki i suche wygładzone jak głośność. Bez wysyłek: jak v0.75. */
static float mt_sv[MT_N][2], mt_dv[MT_N] = {1.0f, 1.0f, 1.0f, 1.0f};   /* wysyłki REVERB / DELAY i suche (wygładzone) */
static float mt_st[MT_N][2], mt_dt[MT_N] = {1.0f, 1.0f, 1.0f, 1.0f};   /* ich cele */
static int8_t mt_mst = -1;                   /* (v0.77) pomiar (SysEx 0x20, 3. bajt 1..3 = master 0..2): zamiast GLO */
static void mt_send_pick(void);
static void mt_sends(float *l, float *r, uint32_t n);
/* (v0.83, decyzja użytkownika 2026-10-30) efekty na ścieżkę: DISTORTION, FILTER, COMPRESSOR, EQ z presetu ścieżki. Punkty jak nuty
 * (MT_BUDGET): mt_fx_pick na blok wybiera T1..T4, slot po slocie, ile się mieści w MT_FX_MAX; reszta stoi (na czerwono, NO CPU);
 * punkty grających efektów odchodzą od budżetu nut */
static uint32_t mt_fxa[MT_N], mt_fxp;        /* rodzaje, które grają (bity FX_*), punkty razem */
static uint8_t mt_ftest[MT_N];               /* pomiar (SysEx 0x22): efekty ścieżki (bity: DIST, FILTER, COMP, EQ), gałki pomiaru */
static void mt_fx_pick(void);
static int mt_fx_live(int32_t t);
static void mt_fx_run(uint32_t dm, uint32_t n);
static struct { int8_t t, note; } mt_hq[MT_LIM];   /* trzymane nuty T2-T4, od najstarszej */
static uint32_t mt_hn;
/* (v0.66) SEQ 4 ścieżki: nuty ścieżek obok wybranej z tego bloku (sq_apply, chwila = próbka): mt_mix dzieli blok
 * ścieżki w tych chwilach (co do próbki, jak wybraną sq_render) */
#define MT_NEV 64
static struct { uint16_t at; uint8_t t, on, note, vel, slide; int8_t from, pit; uint16_t dl; } mt_ev[MT_NEV];   /* (v0.92) pit, dl: ogs_seq.c sq_q */
static uint32_t mt_nev;
static uint32_t mt_code(uint32_t e)          /* ten sam kod = ten sam stan poza wspólnym RAM */
{
    return e == ENG_ALPHA ? ENG_DCO : e == ENG_99 ? ENG_88 : e;
}
static int mt_drum(uint32_t e) { return e == ENG_MORPH || e == ENG_88 || e == ENG_99 || e == ENG_DUAL; }
/* ciężkie (pomiar MULTI v0.64 na FM1): WAVE + FM6 obok perkusji i MINI = 98-127 % już przy 1 nucie — w MULTI jeden */
static int mt_heavy(uint32_t e) { return e == ENG_WT || e == ENG_DX7; }
/* (v0.80) MINI + WAVE: pomiar v0.79 MORPH + MINI + WAVE 101 %, 357 przerw przy 1 + 4 nutach; koszt WAVE prawie nie
 * zależy od nut, więc limit nut tego nie naprawi */
static int mt_clash(uint32_t a, uint32_t b)
{
    return (mt_heavy(a) && mt_heavy(b)) || (a == ENG_MOOG && b == ENG_WT) || (a == ENG_WT && b == ENG_MOOG);
}
static int mt_ok(int32_t t, int32_t e, const int16_t *eng)   /* silnik e na ścieżce t obok eng[] (UI i przerwanie) */
{
    int32_t k;
    if (e == MT_NONE)
        return 1;
    if (e < 0 || e > NENG || (t == 0 && !mt_drum((uint32_t)e)))   /* (v0.91, decyzja użytkownika 2026-11-03) T2-T4 też perkusja
                                                                      * (inna niż na T1: 88 i 99 to ten sam kod) */
        return 0;
    for (k = 0; k < MT_N; k++)
        if (k != t && eng[k] < NENG && (mt_code((uint32_t)eng[k]) == mt_code((uint32_t)e)
                                        || mt_clash((uint32_t)e, (uint32_t)eng[k])))
            return 0;
    return 1;
}
static int32_t mt_track(uint32_t e)          /* ścieżka, na której gra silnik e (-1: żadna) */
{
    int32_t t;
    for (t = 0; mt_on && t < MT_N; t++)
        if (mt_eng[t] == e)
            return t;
    return -1;
}
static void mt_at(int32_t t)
{
    ogs_eram = mt_er[t];
    ogs_epool = mt_ep[t];
}
static void mt_home(void)                    /* wskaźniki na wybraną ścieżkę (UI czyta jej stan); SOLO: wspólny RAM */
{
    int32_t t = mt_track(eng_play);
    if (t >= 0) {
        mt_at(t);
    } else {
        ogs_eram = ogs_eram_mem;
        ogs_epool = ogs_epool_mem;
    }
}
static void mt_apply(void)                   /* przerwanie audio, wyjście przygaszone: tryb i ścieżki z GLO */
{
    uint32_t t, k, at = 0, cap = 0, *w;
    uint8_t *base;
    int16_t eng[MT_N];
    mt_nev = 0;                              /* (v0.66) nuty SEQ dla starych ścieżek: nieważne */
    if (!glob.g[G_MULTI]) {
        if (mt_on) {
            mt_on = 0;
            ogs_fx_arena_give();
            eng_play = 0xFF;                 /* SOLO: set.eng od zera we wspólnym RAM (app_render) */
        }
        for (t = 0; t < MT_N; t++)
            mt_eng[t] = MT_NONE;
        mt_used = 0;
        mt_home();
        return;
    }
    base = (uint8_t *)ogs_fx_arena_take(&cap);
    k = (uint32_t)((8u - ((uintptr_t)base & 7u)) & 7u);   /* (pula floatów: do 8 bajtów jak wspólny RAM) */
    base += k;
    cap -= k;
    mt_on = 1;
    for (t = 0; t < MT_N; t++)
        eng[t] = mt_eng[t] = MT_NONE;
    for (t = 0; t < MT_N; t++) {
        int32_t e = glob.g[G_MT0 + t];
        uint32_t ne, np;
        if (e >= NENG || !mt_ok((int32_t)t, e, eng))
            continue;
        ne = ERAM_UP(MT_SZ[e][0]);
        np = ERAM_UP(MT_SZ[e][1]);
        if (at + ne + np > cap)
            continue;
        mt_er[t] = base + at;
        mt_ep[t] = base + at + ne;
        for (w = (uint32_t *)(void *)mt_er[t]; w < (uint32_t *)(void *)(base + at + ne + np); w++)
            *w = 0;
        at += ne + np;
        mt_at((int32_t)t);
        ENG[e].reset();
        eng[t] = (int16_t)e;
        mt_eng[t] = (uint8_t)e;
        mt_g[t] = 0.0f;                      /* (wjeżdża z wygładzeniem) */
    }
    for (t = 0; t < MT_N; t++) {             /* (v0.76) wysyłki od zera */
        mt_sv[t][0] = mt_sv[t][1] = 0.0f;
        mt_dv[t] = 1.0f;
    }
    mt_used = at;
    ogs_fx_multi(k + at);                    /* (v0.74) PERFORM, (v0.76) wysyłki: reszta puli za ścieżkami */
    mt_hn = 0;                               /* (v0.79) limit nut: ścieżki od nowa */
    nt_clear();
    eng_play = 0xFF;
    if (mt_track(set.eng) >= 0)
        eng_play = set.eng;
    for (t = 0; eng_play == 0xFF && t < MT_N; t++)
        if (mt_eng[t] < NENG)
            eng_play = mt_eng[t];
    if (eng_play == 0xFF) {                  /* żadnej ścieżki: SOLO */
        mt_on = 0;
        ogs_fx_arena_give();
    }
    mt_home();
}
/* (v0.75, do-konca B8; v0.79 decyzja użytkownika: wspólny, bez limitu na ścieżkę) limit trzymanych nut T2-T4 w MULTI:
 * razem MT_LIM, dowolnie między ścieżkami (bas 1 nuta = więcej na akordy). Nowa nuta ponad limit puszcza najstarszą
 * trzymaną z dowolnej ścieżki (wybrzmiewa jak puszczona). Perkusja (T1) bez limitu. Wywołania z przerwania audio,
 * wskaźniki silnika już na ścieżce t. */
static void mt_lim_off(int32_t t, int32_t note)
{
    uint32_t i, j;
    for (i = 0; i < mt_hn; i++)
        if (mt_hq[i].t == t && mt_hq[i].note == note) {
            for (j = i; j + 1u < mt_hn; j++)
                mt_hq[j] = mt_hq[j + 1u];
            mt_hn--;
            return;
        }
}
static uint32_t mt_pts(int32_t t, uint32_t mini)   /* koszt nuty ścieżki t */
{
    return mt_eng[t] >= NENG ? 0u : mini && mt_eng[t] == ENG_PD ? MT_PD_MINI : MT_PTS[mt_eng[t]];
}
static void mt_lim_on(int32_t t, int32_t note)   /* przed note on: miejsce dla nuty (najstarsze puszczone) */
{
    uint32_t i, c, mini = 0, dr = 0;
    if (t <= 0 || t >= MT_N || !mt_on || mt_eng[t] >= NENG || mt_drum(mt_eng[t]))   /* (v0.91) perkusja na T2-T4: uderzenia bez limitu (jak T1) */
        return;
    mt_lim_off(t, note);
    for (i = 1; i < MT_N; i++) {
        if (mt_eng[i] == ENG_MOOG)
            mini = 1;
        if (mt_eng[i] < NENG && mt_drum(mt_eng[i]))
            dr += MT_DRUM2;                  /* (v0.91) druga perkusja: stały koszt (jak MINI) */
    }
    c = (mini ? MT_MINI : 0u) + dr + mt_fxp + mt_pts(t, mini);   /* (v0.83) efekty ścieżek też z budżetu */
    for (i = 0; i < mt_hn; i++)
        c += mt_pts(mt_hq[i].t, mini);
    while (mt_hn && (mt_hn >= MT_LIM || c > MT_BUDGET)) {
        const int32_t ot = mt_hq[0].t, old = mt_hq[0].note;
        mt_lim_off(ot, old);
        c -= mt_pts(ot, mini);
        if (mt_eng[ot] < NENG) {
            mt_at(ot);
            ENG[mt_eng[ot]].note_off(old);
            mt_at(t);
        }
    }
    mt_hq[mt_hn].t = (int8_t)t;
    mt_hq[mt_hn].note = (int8_t)note;
    mt_hn++;
}
static void mt_kick(int32_t t, int32_t note);
static char ch_txt[16];                      /* (v1.1.0) CHORD NAME: napis na HOME (chord_name_upd) */
static volatile uint8_t cc_req[4];          /* (v1.1.0, P7) CC 21-24 z przerwania: 0x80 | wartość, UI wpisuje (cc_knob) */
static float mt_cb[MT_N], mt_cw[MT_N], mt_cp[MT_N];   /* (v1.1.0, P6) bend, wheel, nacisk niewybranych ścieżek (G_MTCTL) */
static void mt_midi(int32_t t, uint32_t ty, uint32_t d1, uint32_t d2)   /* przerwanie audio: nuta ścieżki t (nie wybranej) */
{
    if (ty != 0x90u && ty != 0x80u) {
        if (!GV(G_MTCTL))                    /* (v1.1.0) OTHER TRACKS + BEND, WHEEL: też sterowniki z kanału ścieżki */
            return;
        if (ty == 0xE0u)
            mt_cb[t] = (float)((int32_t)(d2 << 7 | d1) - 8192) * (1.0f / 8192.0f);
        else if (ty == 0xB0u && d1 == 1u)
            mt_cw[t] = (float)d2 / 127.0f;
        else if (ty == 0xD0u || ty == 0xA0u)
            mt_cp[t] = (float)(ty == 0xD0u ? d1 : d2) / 127.0f;
        return;
    }
    mt_at(t);
    if (ty == 0x90u && d2) {
        mt_lim_on(t, (int32_t)d1);
        ENG[mt_eng[t]].note_on((int32_t)d1, (float)d2 / 127.0f);
        mt_kick(t, (int32_t)d1);
    } else {
        mt_lim_off(t, (int32_t)d1);
        ENG[mt_eng[t]].note_off((int32_t)d1);
    }
    mt_home();
}
static float mt_target(int32_t t)            /* głośność ścieżki: 80 = 1 (jak SOLO), MUTE = 0 */
{
    float v = (float)glob.g[G_MTV0 + t] * (1.0f / 80.0f);
    return glob.g[G_MTMUTE] >> t & 1 ? 0.0f : v * v;
}
/* (v0.72) PAN ścieżki: balans (środek = 1 / 1, jak dotąd; w bok druga strona ciszej), wygładzony jak głośność */
static float mt_pl[MT_N] = {1.0f, 1.0f, 1.0f, 1.0f}, mt_pr[MT_N] = {1.0f, 1.0f, 1.0f, 1.0f};
static void mt_pan_t(int32_t t, float *pl, float *pr)
{
    float p = (float)glob.g[G_MTP0 + t] * 0.02f;
    *pl = p > 0.0f ? 1.0f - p : 1.0f;
    *pr = p < 0.0f ? 1.0f + p : 1.0f;
}
static void mt_ev_do(uint32_t k)             /* przerwanie audio: zdarzenie k (wskaźniki już na jego ścieżce) */
{
    const engine_t *x = &ENG[mt_eng[mt_ev[k].t]];
    if (mt_ev[k].on) {
        ctl.slide = mt_ev[k].slide & 1u;
        ctl.slide_from = mt_ev[k].from;
        ctl.npit = mt_ev[k].pit;
        ctl.dlen = (float)mt_ev[k].dl * 0.001f;
        LANE_KEEP();
        mt_lim_on(mt_ev[k].t, (int32_t)mt_ev[k].note);   /* (v0.75) limit nut ścieżki */
        x->note_on((int32_t)mt_ev[k].note, (float)mt_ev[k].vel / 127.0f);
        LANE_BACK();
        mt_kick(mt_ev[k].t, (int32_t)mt_ev[k].note);
        ctl.slide = 0;
        ctl.npit = 0;
        ctl.dlen = 0.0f;
    } else {
        mt_lim_off(mt_ev[k].t, (int32_t)mt_ev[k].note);
        if ((mt_ev[k].slide & 4u) && x->note_cut)   /* (v0.87) SQ_CUT */
            x->note_cut((int32_t)mt_ev[k].note);
        x->note_off((int32_t)mt_ev[k].note);
    }
}
_Static_assert(APP_BLOCK <= FX_BUS_N, "wysyłki MULTI: blok w szynie");
static int32_t mt_mini(void)                 /* (v0.76) ścieżka MINI (nie wybrana) przy wysyłkach: liczy głosy sama — zadanie wysyłek obok */
{
    int32_t t, f = mt_track(eng_play);
    for (t = 0; ogs_fx_bus(0) && t < MT_N; t++)
        if (t != f && mt_eng[t] == ENG_MOOG)
            return t;
    return -1;
}
static int mt_plain(int32_t t)               /* (v0.76) ścieżka bez wysyłek: pętla jak w v0.75 (bit w bit) */
{
    return mt_st[t][0] == 0.0f && mt_st[t][1] == 0.0f && mt_dt[t] == 1.0f && mt_sv[t][0] == 0.0f && mt_sv[t][1] == 0.0f
           && mt_dv[t] == 1.0f;
}
/* (v0.76) blok ścieżki t z wysyłkami: y = x * głośność * PAN, szyny += y * wysyłka, wyjście y * suche (wybrana: x to
 * l / r, w miejscu; reszta: dodane). Zwraca głośność (wygładzoną). */
static float mt_sendloop(int32_t t, const float *xl, const float *xr, float *l, float *r, uint32_t n, float g, float gt)
{
    float pl = mt_pl[t], pr = mt_pr[t], tpl, tpr, s0 = mt_sv[t][0], s1 = mt_sv[t][1], d = mt_dv[t];
    const float t0 = mt_st[t][0], t1 = mt_st[t][1], td = mt_dt[t];
    float *a = ogs_fx_bus(0), *b = ogs_fx_bus(1);
    const int add = xl != l;
    uint32_t i;
    mt_pan_t(t, &tpl, &tpr);
    for (i = 0; i < n; i++) {
        float yl, yr;
        g += (gt - g) * 0.005f;
        pl += (tpl - pl) * 0.005f;
        pr += (tpr - pr) * 0.005f;
        s0 += (t0 - s0) * 0.005f;
        s1 += (t1 - s1) * 0.005f;
        d += (td - d) * 0.005f;
        yl = xl[i] * (g * pl);
        yr = xr[i] * (g * pr);
        a[i] += yl * s0;
        a[FX_BUS_N + i] += yr * s0;
        b[i] += yl * s1;
        b[FX_BUS_N + i] += yr * s1;
        if (add) {
            l[i] += yl * d;
            r[i] += yr * d;
        } else {
            l[i] = yl * d;
            r[i] = yr * d;
        }
    }
    mt_pl[t] = pl;
    mt_pr[t] = pr;
    mt_sv[t][0] = fm_fabsf(t0 - s0) < 1e-4f ? t0 : s0;   /* (blisko celu: cel — potem znowu pętla bez wysyłek) */
    mt_sv[t][1] = fm_fabsf(t1 - s1) < 1e-4f ? t1 : s1;
    mt_dv[t] = fm_fabsf(td - d) < 1e-4f ? td : d;
    return g;
}
static void mt_add(int32_t t, const float *xl, const float *xr, float *l, float *r, uint32_t n)   /* blok ścieżki do miksu */
{
    float g = mt_g[t], gt = mt_target(t);
    uint32_t i;
    if (!mt_plain(t)) {                      /* (v0.76) z wysyłkami */
        g = mt_sendloop(t, xl, xr, l, r, n, g, gt);
    } else {
        float pl = mt_pl[t], pr = mt_pr[t], tpl, tpr;
        mt_pan_t(t, &tpl, &tpr);
        for (i = 0; i < n; i++) {
            g += (gt - g) * 0.005f;
            pl += (tpl - pl) * 0.005f;       /* (v0.72) PAN (0: 1 x g = g, bez zmian) */
            pr += (tpr - pr) * 0.005f;
            l[i] += xl[i] * (g * pl);
            r[i] += xr[i] * (g * pr);
        }
        mt_pl[t] = pl;
        mt_pr[t] = pr;
    }
    mt_g[t] = g;
}
static void mt_mix(float *l, float *r, uint32_t n)   /* przerwanie audio: blok wybranej ścieżki (l, r) + reszta */
{
    static float tl[APP_BLOCK], tr[APP_BLOCK];
    int32_t t, j, f = mt_track(eng_play), mini;
    uint32_t i, dm = 0;                      /* (v0.83) ścieżki z efektami: do miksu po efektach (mt_fx_run) */
    const uint8_t ovl = ogs_overload;        /* (znacznik wybranej: wraca po reszcie) */
    mt_send_pick();                          /* (v0.76) wysyłki: cele ścieżek, gałki wspólnych efektów */
    mt_fx_pick();                            /* (v0.83) efekty na ścieżkę */
    mini = mt_mini();
    if (ogs_fx_bus(0)) {                     /* szyny: suma tego bloku */
        float *a = ogs_fx_bus(0), *b = ogs_fx_bus(1);
        for (i = 0; i < n; i++)
            a[i] = a[FX_BUS_N + i] = b[i] = b[FX_BUS_N + i] = 0.0f;
    }
    if (f >= 0 && mt_fx_live(f)) {           /* (v0.83) wybrana z efektami: do swojego bufora */
        float *b = ogs_fx_track_buf((uint32_t)f);
        for (i = 0; i < n; i++) {
            b[i] = l[i];
            b[FX_BUS_N + i] = r[i];
            l[i] = r[i] = 0.0f;
        }
        dm |= 1u << f;
    } else if (f >= 0 && !mt_plain(f)) {     /* (v0.76) wybrana z wysyłkami */
        mt_g[f] = mt_sendloop(f, l, r, l, r, n, mt_g[f], mt_target(f));
    } else if (f >= 0) {                     /* najpierw wybrana: jej głośność na tym, co już jest w bloku */
        float g = mt_g[f], gt = mt_target(f), pl = mt_pl[f], pr = mt_pr[f], tpl, tpr;
        mt_pan_t(f, &tpl, &tpr);
        for (i = 0; i < n; i++) {
            g += (gt - g) * 0.005f;
            pl += (tpl - pl) * 0.005f;           /* (PAN 0: 1 x g = g, bez zmian) */
            pr += (tpr - pr) * 0.005f;
            l[i] *= g * pl;
            r[i] *= g * pr;
        }
        mt_g[f] = g;
        mt_pl[f] = pl;
        mt_pr[f] = pr;
    }
    for (j = 0; j < MT_N; j++) {
        float g, gt, *xl = tl, *xr = tr, sb = 0.0f, sw = 0.0f, sp = 0.0f;
        uint32_t from = 0, k;
        t = mini < 0 ? j : j == 0 ? mini : j <= mini ? j - 1 : j;   /* (v0.76) z wysyłkami: MINI pierwsza, obok zadanie */
        if (t == f || mt_eng[t] >= NENG)
            continue;
        g = mt_g[t];
        gt = mt_target(t);
        if (t == mini)
            ogs_fx_sends_kick();
        mt_at(t);
        if (gt <= 0.0f && g < 1e-5f) {       /* wyciszona: nie liczy (stoi do odciszenia; nuty SEQ: sam stan) */
            for (k = 0; k < mt_nev; k++)
                if (mt_ev[k].t == t)
                    mt_ev_do(k);
            mt_g[t] = 0.0f;
            continue;
        }
        ogs_overload = (uint8_t)(mt_ovl >> t & 1u);   /* przeciążenie: ta ścieżka też zrzuca (silnik zeruje znacznik) */
        if (GV(G_MTCTL)) {                   /* (v1.1.0, P6) sterowniki tej ścieżki na czas jej bloku (ctl wybranej: niżej z powrotem) */
            sb = ctl.bend;
            sw = ctl.wheel;
            sp = ctl.press;
            ctl.bend = mt_cb[t];
            ctl.wheel = mt_cw[t];
            ctl.press = mt_cp[t];
        }
        if (mt_fx_live(t)) {                 /* (v0.83) z efektami: do swojego bufora */
            xl = ogs_fx_track_buf((uint32_t)t);
            xr = xl + FX_BUS_N;
            dm |= 1u << t;
        }
        for (k = 0; k < mt_nev; k++)         /* (v0.66) nuty SEQ tej ścieżki: blok dzielony w ich chwilach */
            if (mt_ev[k].t == t) {
                const uint32_t a = mt_ev[k].at < n ? mt_ev[k].at : n;
                if (a > from) {
                    ENG[mt_eng[t]].render(xl + from, xr + from, a - from);
                    from = a;
                }
                mt_ev_do(k);
            }
        if (n > from)
            ENG[mt_eng[t]].render(xl + from, xr + from, n - from);
        if (GV(G_MTCTL)) {
            ctl.bend = sb;
            ctl.wheel = sw;
            ctl.press = sp;
        }
        if (!ogs_overload)
            mt_ovl &= ~(1u << t);
        if (!(dm >> t & 1u))
            mt_add(t, tl, tr, l, r, n);
    }
    if (dm) {                                /* (v0.83) efekty ścieżek: po silnikach, oba rdzenie; potem do miksu */
        mt_fx_run(dm, n);
        for (t = 0; t < MT_N; t++)
            if (dm >> t & 1u)
                mt_add(t, ogs_fx_track_buf((uint32_t)t), ogs_fx_track_buf((uint32_t)t) + FX_BUS_N, l, r, n);
    }
    mt_nev = 0;
    ogs_overload = ovl;
    mt_home();
}
static void mt_focus_in(void)                /* przerwanie audio: UI wybrało inną ścieżkę (set.eng) */
{
    int32_t o = mt_track(eng_play), n;
    if (mt_track(set.eng) < 0 || set.eng == eng_play)
        return;
    if (o >= 0) {                            /* trzymane nuty starej ścieżki: puszczone (wybrzmiewają) */
        mt_at(o);
        for (n = 0; n < 128; n++)
            ENG[eng_play].note_off(n);
    }
    nt_clear();
    eng_play = set.eng;
    mt_home();
}

static uint8_t mc_p[4] = {21u, 29u, 79u, 62u};   /* (v0.91) MULTI MASTER COMP: THRESH, RATIO, ATTACK, RELEASE z GLO (przerwanie czyta) */
static void glob_apply(void)             /* GLO -> silniki, klawisze, ekran */
{
    {
        int32_t i;
        for (i = 0; i < 4; i++)
            mc_p[i] = (uint8_t)GV(G_MCTH + i);
    }
    uint32_t i;
    for (i = 0; i < PF_N; i++)               /* (v0.90) PERFORM: ustawienia klawiszy */
        ogs_pf_cfg[i] = glob.pfs[i];
    ctl.a4 = (float)glob.g[G_TUNE];
    touch_curve(glob.g[G_VEL]);
    touch_sens(glob.g[G_PRESS]);
    palette_set(0);                          /* trzy ciemne stopnie i dwa jasne: kolory OG Synth */
    pal[0] = OGS_GREY[0];
    pal[1] = OGS_GREY[1];
    pal[2] = OGS_GREY[2];
    {                                        /* (v0.64.2) MULTI: inny odcień koloru z GLO COLOR (pomysł użytkownika) */
        const uint16_t *c = glob.g[G_MULTI] ? OGS_PAL_MT[glob.g[G_COLOR]] : OGS_PAL[glob.g[G_COLOR]];
        pal[3] = c[0];
        pal[4] = c[1];
    }
#ifndef PLAT_HOST
    fm1_an.probe = glob.g[G_TOPKEYS] ? 0u : 1u;   /* GLO TOP KEYS (fm1_input.h: górny rząd przez PA10) */
#endif
}

/* siła klawisza: GLO FIXED VEL (1..127) zastępuje pomiar na wszystkich klawiszach; OFF (0): zmierzona, bez pomiaru 100 */
static uint32_t key_vel(uint32_t k)
{
    uint32_t v = glob.g[G_VELFIX] ? (uint32_t)glob.g[G_VELFIX] : touch_vel(k);
    return v ? v : 100u;
}

/* oscyloskop HOME: przerwanie audio dopisuje gotowy dźwięk (mono), ekran czyta */
#define SC_LEN 4096u
static int16_t scope[SC_LEN] __attribute__((section(".pool")));
static volatile uint32_t scope_w;
static volatile uint8_t scope_on;

/* zapis flasha przy dźwięku (v0.20): kasowanie na FM1 wycisza wyjście kawałkami (fm1-base unity.c soft_erase:
 * audio_silence) — to trzaski. Więc gdy coś słychać, całe wyjście (z efektami) najpierw przygasa do zera (MUTE_N),
 * zapis idzie w ciszy, a dźwięk wraca płynnie w następnej klatce (app_frame: out_unmute). Zapisy w ciszy bez zmian. */
#define MUTE_N 256u
static volatile uint8_t mute_req;
static volatile float mute_g = 1.0f, out_lvl;   /* out_lvl: szczyt wyjścia ostatniego bloku (0..1) */
static void out_mute(void)               /* główna pętla, przed zapisem flasha */
{
    if (mute_req || (out_lvl < 1e-4f && mute_g >= 1.0f))
        return;                              /* już cicho (albo już przygaszone) */
    mute_req = 1;
#ifndef PLAT_HOST                            /* (symulator: render w tej samej pętli, zapis i tak nie przerywa dźwięku) */
    {
        uint32_t t0 = plat_ms();
        while (mute_g > 0.0f && plat_ms() - t0 < 40u)
            ;                                /* przerwanie audio przygasza (ok. 6 ms) */
    }
#endif
}
static void out_unmute(void) { mute_req = 0; }   /* app_frame: po zapisach dźwięk wraca */
static int store_save_q(uint32_t o, const void *p, uint32_t n) { out_mute(); return plat_store_save(o, p, n); }
static int store_patch_q(uint32_t o, uint32_t len, uint32_t off, const void *p, uint32_t n)
{
    out_mute();
    return plat_store_patch(o, len, off, p, n);
}
static int data_erase_q(uint32_t off) { out_mute(); return plat_data_erase(off); }
static int data_write_q(uint32_t off, const void *p, uint32_t n) { out_mute(); return plat_data_write(off, p, n); }
#define plat_store_save store_save_q
#define plat_store_patch store_patch_q
#define plat_data_erase data_erase_q
#define plat_data_write data_write_q

static void eng_on(const engine_t *e, int32_t key, float v);   /* (v0.39) nuty do silnika: CHORD, SUB (niżej) */
static void eng_on_m(const engine_t *e, int32_t key, float v, uint32_t m);   /* ... z podanym akordem (1: bez) */
static uint32_t chord_at(int32_t key);       /* akord brzmienia, które gra, na klawiszu key (bit k = +k półtonów; v1.1.0: KEY 3 / 7) */
static void eng_off(const engine_t *e, int32_t key);
#include "ogs_seq.c"

/* ------------------------------------------------------------ nuty z UI --- */
/* UI wrzuca, przerwanie audio wyjmuje: kolejność zostaje */
static volatile uint32_t ev_q[64], ev_w, ev_r;
static void ev_push(uint32_t e)
{
    if (ev_w - ev_r < 64u) {
        ev_q[ev_w % 64u] = e;
        ev_w++;
    }
}
#define EV_ON(note, vel) (1u << 24 | (uint32_t)(note) << 8 | (vel))
#define EV_OFF(note) (2u << 24 | (uint32_t)(note) << 8)
#define EV_ALL_OFF (3u << 24)
#define EV_ARP_OFF (4u << 24)

/* bez trzasków (v0.20): to, co skacze w dźwięku — przełącznik z listy (fala, TYPE, ON / OFF), parametr z nowym
 * przydziałem nut, „wszystko cicho” (CC 123, ARP off, preset) — czeka, aż wyjście silnika przygaśnie (DK_N próbek);
 * zmiana wchodzi przy zerze, dźwięk wraca w DK_N. Zmiany parametrów z UI w kolejce dk_q: UI wrzuca, przerwanie audio
 * wpisuje przy zerze (dk_apply). Preset z panelu czeka w UI (pend_preset), aż przerwanie powie: cisza (dk_quiet). */
#define DK_N 256u                            /* 5,8 ms w dół i tyle w górę */
#define DK_Q 8u
static volatile uint32_t dk_w, dk_r, dk_done;   /* dk_done: ile razy wpisano kolejkę (UI odświeża gwiazdkę) */
static volatile struct {
    uint8_t eng;
    int16_t id, v;
} dk_q[DK_Q];
static volatile uint8_t dk_quiet_req, dk_quiet, dk_kill;
static float dk_g = 1.0f, dk_epk;            /* dk_epk: szczyt wyjścia silnika (przed bramką) w ostatnim bloku */

/* (v0.39) NUTY DO SILNIKA — jedno miejsce (przerwanie audio; ogs_seq.c woła eng_on / eng_off / eng_all_off):
 * CHORD (dodatki presetu, bajt 0) rozkłada klawisz na akord, SUB (efekt) śledzi najniższą brzmiącą nutę. Nuta może grać
 * z kilku klawiszy naraz (akordy się nakładają): licznik, note off dopiero przy zerze. Klawisz pamięta akord, z którym
 * zagrał (zmiana CHORD przy trzymanym klawiszu nie zostawia wiszących nut). CHORD OFF: jak do v0.38 (podwójny note on
 * i jeden note off = cisza). */
enum { CH_OFF, CH_MIN7, CH_MIN9, CH_MAJ7, CH_MAJ9, CH_SUS2, CH_SUS4, CH_MIN, CH_MAJ, CH_DOM7, CH_MIN11, CH_OCT,
       CH_FIFTH, CH_LEARN, CH_KEY3, CH_KEY7, NCHORD };   /* (v1.1.0) KEY 3 / KEY 7: ze skali wzoru (SEQ > KEY; OFF: C-dur) */
_Static_assert(NCHORD <= 16, "CHORD: rodzaj w bitach 0-3 dodatku [0], VOICING w bitach 4-5 (v1.1.0)");
static const char *const CHORD_NAME[NCHORD] = {"OFF", "MIN7", "MIN9", "MAJ7", "MAJ9", "SUS2", "SUS4", "MIN", "MAJ", "7",
                                               "MIN11", "OCT", "FIFTH", "LEARNED", "KEY 3", "KEY 7"};
static const uint32_t CHORD_IV[CH_LEARN] = {   /* bit k = +k półtonów (bit 0: klawisz) */
    1u, 1u | 1u << 3 | 1u << 7 | 1u << 10, 1u | 1u << 3 | 1u << 7 | 1u << 10 | 1u << 14, 1u | 1u << 4 | 1u << 7 | 1u << 11,
    1u | 1u << 4 | 1u << 7 | 1u << 11 | 1u << 14, 1u | 1u << 2 | 1u << 7, 1u | 1u << 5 | 1u << 7, 1u | 1u << 3 | 1u << 7,
    1u | 1u << 4 | 1u << 7, 1u | 1u << 4 | 1u << 7 | 1u << 10, 1u | 1u << 3 | 1u << 7 | 1u << 10 | 1u << 14 | 1u << 17,
    1u | 1u << 12, 1u | 1u << 7};
#define CHORD_LN 6                           /* nauczony akord: do 6 nut nad klawiszem (dodatki presetu, bajty 1-6) */
static uint8_t nt_cnt[128];                  /* ile klawiszy gra tę nutę */
static uint32_t nt_key[128];                 /* klawisz: akord, z którym zagrał (0: nie gra) */
static const uint8_t *xt_play(void);         /* dodatki presetu silnika, który gra (niżej) */
#define XT_SYNC 7                            /* (v0.46) dodatki presetu [7..9]: SYNC efektów (maska po rodzaju, fx_sync_on) */
/* (v1.1.0) KEY 3 / KEY 7: od klawisza w górę po skali wzoru (bez skali: C-dur) druga i czwarta (i szósta) nuta skali =
 * trójdźwięk / septyma z tonacji; klawisz spoza skali: to samo w górę od niego */
static uint32_t chord_key(uint32_t key, int seventh)
{
    uint32_t sc = sq_scale(), m = 1u, n = 0, k;
    if (!sc)
        sc = 0xAB5u;
    for (k = 1; k < 24u && n < (seventh ? 6u : 4u); k++)
        if (sc >> ((key + k) % 12u) & 1u && (++n & 1u) == 0u)
            m |= 1u << k;
    return m;
}
/* (v1.1.0) VOICING (K2 strony CHORD, bity 4-5 dodatku [0]; stare presety 0 = CLOSE): OPEN = druga nuta oktawę wyżej,
 * INV 1 / INV 2 = najniższa / dwie najniższe oktawę wyżej */
static uint32_t chord_voice(uint32_t m, uint32_t vo)
{
    uint32_t k, b;
    for (k = 0; vo && (m & (m - 1u)) && k < (vo == 3u ? 2u : 1u); k++) {
        b = vo == 1u ? (m & (m - 1u)) & (~(m & (m - 1u)) + 1u) : m & (~m + 1u);
        if (b >= 1u << 20)
            break;
        m = (m & ~b) | b << 12;
    }
    return m;
}
static uint32_t chord_mask(const uint8_t *x, int32_t key)
{
    uint32_t t = x[0] & 15u, m = 1u, k;
    if (t == CH_LEARN) {
        for (k = 1; k <= CHORD_LN && x[k]; k++)
            if (x[k] < 32u)
                m |= 1u << x[k];
    } else if (t == CH_KEY3 || t == CH_KEY7) {
        m = chord_key((uint32_t)key, t == CH_KEY7);
    } else if (t && t < CH_LEARN) {
        m = CHORD_IV[t];
    }
    return chord_voice(m, x[0] >> 4 & 3u);
}
static void sub_track(void)                  /* SUB: najniższa brzmiąca nuta (z GLO TUNE) */
{
    uint32_t n;
    for (n = 0; n < 128u && !nt_cnt[n]; n++)
        ;
    if (n < 128u)
        ogs_fx_sub_semi = (float)n + 12.0f * fm_log2f(ctl.a4 * (1.0f / 440.0f));
    ogs_fx_sub_gate = n < 128u;
}
static void nt_clear(void)
{
    uint32_t n;
    for (n = 0; n < 128u; n++) {
        nt_cnt[n] = 0;
        nt_key[n] = 0;
    }
    ogs_fx_sub_gate = 0;
}
static void eng_off(const engine_t *e, int32_t key)
{
    uint32_t m = nt_key[key & 127], k;
    if (!m) {                                /* (nie grał z tego klawisza: jak dawniej) */
        e->note_off(key);
        return;
    }
    for (k = 0; k < 32u; k++)
        if (m >> k & 1u && key + (int32_t)k < 128 && nt_cnt[key + k] && --nt_cnt[key + k] == 0) {
            mt_lim_off(mt_track(eng_play), key + (int32_t)k);   /* (v0.75) MULTI: limit nut ścieżki */
            e->note_off(key + (int32_t)k);
        }
    nt_key[key & 127] = 0;
    sub_track();
}
static uint32_t chord_at(int32_t key) { return eng_play < NENG && mt_drum(eng_play) ? 1u : chord_mask(xt_play(), key); }   /* (v0.92) perkusja: bez CHORD */
static void eng_on(const engine_t *e, int32_t key, float v) { eng_on_m(e, key, v, chord_at(key)); }
static void eng_on_m(const engine_t *e, int32_t key, float v, uint32_t m)
{
    uint32_t old = nt_key[key & 127], k;
    for (k = 0; old && k < 32u; k++)         /* klawisz jeszcze gra (podwójny note on): poprzedni akord zdjęty */
        if (old >> k & 1u && key + (int32_t)k < 128 && nt_cnt[key + k] && --nt_cnt[key + k] == 0 && !(m >> k & 1u))
            e->note_off(key + (int32_t)k);
    for (k = 0; k < 32u; k++)
        if (m >> k & 1u && key + (int32_t)k < 128) {
            if (nt_cnt[key + k] < 255u)
                nt_cnt[key + k]++;
            if (mt_on)                       /* (v0.75) MULTI: limit nut wybranej ścieżki */
                mt_lim_on(mt_track(eng_play), key + (int32_t)k);
            e->note_on(key + (int32_t)k, v);
        }
    nt_key[key & 127] = m;
    sub_track();
    mt_kick(mt_on ? mt_track(eng_play) : -1, key);
    ogs_fx_note_n++;                         /* (v0.55) AUTO WAH ENV: nowa nuta (akord = jedna) */
}
static void eng_all_off(const engine_t *e)
{
    e->all_off();
    nt_clear();
}

static int kill_soft(const engine_t *e)  /* przerwanie audio: wszystko cicho; 1 = najpierw przygaszenie (reszta potem) */
{
    if (dk_g > 0.0f && e->sounding()) {
        dk_kill = 1;
        return 1;
    }
    eng_all_off(e);
    return 0;
}

static void dk_apply(void)               /* przerwanie audio, przy zerze: zmiany parametrów z UI */
{
    while (dk_r != dk_w) {
        uint32_t k = dk_r % DK_Q;
        ENG[dk_q[k].eng].p[dk_q[k].id] = dk_q[k].v;
        dk_r++;
    }
    dk_done++;
}

static volatile uint32_t midi_last, midi_count, midi_non;   /* midi_non: note on z USB (pomiar gubienia, 0x19) */
static volatile int32_t pc_req = -1;     /* Program Change z MIDI (GLO PROG CHANGE): wczytuje główna pętla */
static volatile float press_keys;        /* nacisk klawiszy FM1 (UI), MIDI dokłada swój */
static float press_midi;
static void events_in(void)              /* przerwanie audio: nuty z panelu i z MIDI */
{
    const engine_t *e = &ENG[eng_play];
    uint32_t pkt, ch = (uint32_t)glob.g[G_MIDICH];
    int port;
    if (dk_kill) {                           /* wszystko cicho: po przygaszeniu, dopiero potem następne nuty */
        if (dk_g > 0.0f && e->sounding())
            return;
        eng_all_off(e);
        dk_kill = 0;
    }
    while (ev_r != ev_w) {
        uint32_t ev = ev_q[ev_r % 64u], t = ev >> 24, note = (ev >> 8) & 0x7Fu;
        ev_r++;
        if (t == 1u)
            sq_in_on(e, (int32_t)note, (float)(ev & 0x7Fu) / 127.0f);   /* ARP albo silnik; nagrywanie */
        else if (t == 2u)
            sq_in_off(e, (int32_t)note);
        else if (t == 4u) {                  /* ARP wyłączony / LATCH zdjęty: arp cichnie */
            ar.nh = ar.nl = 0;
            sq_release_all();                /* (sq_apply po events_in) */
            if (kill_soft(e))                /* nuty trzymane przed przełączeniem nie dostałyby note off */
                return;
        } else if (kill_soft(e))
            return;
    }
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24, ty = st & 0xF0u;
        if (st != 0xF8u && st != 0xFEu) {
            midi_last = pkt;
            midi_count++;
            if (ty == 0x90u && d2)
                midi_non++;
        }
        if (st >= 0xF8u) {                   /* zegar MIDI (GLO CLOCK IN AUTO) */
            if (st == 0xF8u)
                sq_f8();
            else if (glob.g[G_CLKIN] && (st == 0xFAu || st == 0xFBu || st == 0xFCu))
                sq.ext_req = (uint8_t)(st == 0xFAu ? 1u : st == 0xFCu ? 2u : 3u);
            continue;
        }
        if (mt_on && st < 0xF0u) {           /* (v0.64) MULTI: kanał ścieżki = ta ścieżka; (v1.0.1) kanały z GLO MIDI T1-T4
                                              * CHANNEL (domyślnie 1-4), kilka ścieżek na jednym kanale gra razem */
            const uint32_t c = st & 0x0Fu;
            int32_t t, hit = 0, foc = 0;
            for (t = 0; t < MT_N; t++) {
                if (mt_ch(t) != c)
                    continue;
                hit = 1;
                if (mt_eng[t] >= NENG)       /* pusta ścieżka: nic */
                    continue;
                if (mt_eng[t] == eng_play)   /* wybrana: dalej jak SOLO (ARP, nagrywanie, bend, wheel, nacisk) */
                    foc = 1;
                else
                    mt_midi(t, ty, d1, d2);
            }
            if (hit && !foc)
                continue;
            if (!hit && ty != 0x90u && ty != 0x80u && ch && c != ch - 1u)   /* inne kanały: nuty do wybranej, reszta wg MIDI CH */
                continue;
        } else if (st < 0xF0u && ch && (st & 0x0Fu) != ch - 1u)   /* GLO MIDI CH: tylko ten kanał */
            continue;
        if (ty == 0x90u && d2)
            sq_in_on(e, (int32_t)d1, (float)d2 / 127.0f);
        else if (ty == 0x80u || ty == 0x90u)
            sq_in_off(e, (int32_t)d1);
        else if (ty == 0xE0u)
            ctl.bend = (float)((int32_t)(d2 << 7 | d1) - 8192) * (1.0f / 8192.0f);
        else if (ty == 0xB0u && d1 == 1u)
            ctl.wheel = (float)d2 / 127.0f;
        else if (ty == 0xB0u && d1 >= 21u && d1 <= 24u && GV(G_CCIN))   /* (v1.1.0, P7) CC IN: K1-K4 strony (cc_knob w UI) */
            cc_req[d1 - 21u] = (uint8_t)(0x80u | d2);
        else if (ty == 0xD0u || ty == 0xA0u)
            press_midi = (float)(ty == 0xD0u ? d1 : d2) / 127.0f;
        else if (ty == 0xB0u && (d1 == 120u || d1 == 123u)) {
            if (kill_soft(e))
                return;
        }
        else if (ty == 0xC0u && glob.g[G_PROGCH])
            pc_req = (int32_t)d1;
    }
    ctl.press = fm_maxf(press_keys, press_midi);
}

static float gain_now;
#define ENG_FADE_N 512u                  /* zmiana silnika: stary cichnie przez tyle próbek (12 ms), potem wymiana RAM */
static float eng_fade = 1.0f;
static const fx_slot_t *fx_play(void);   /* efekty bieżącego silnika (fxp niżej) */
static const uint8_t *fx_xplay(void);    /* i ich gałki ponad 8 (fxx niżej) */
static int16_t bend_id[NENG_MAX];        /* (v0.45) id BEND RANGE silnika + 1 (0: nie szukano, -1: brak) */
static float eng_bend(const engine_t *e) /* pitch bend w półtonach tego silnika (SUB, COMB KEY idą za nim) */
{
    int32_t x = (int32_t)(e - ENG), id;
    if (!bend_id[x]) {
        for (id = 0; id < e->np; id++) {
            const char *a = e->def(id)->name, *b = "BEND RANGE";
            while (*a && *a == *b)
                a++, b++;
            if (!*a && !*b)
                break;
        }
        bend_id[x] = (int16_t)(id < e->np ? id + 1 : -1);
    }
    return bend_id[x] > 0 ? ctl.bend * (float)e->p[bend_id[x] - 1] : 0.0f;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static float bl[APP_BLOCK], br[APP_BLOCK];
    const engine_t *e;
    static float hx[2], hy[2], eq_g;         /* SPEAKER EQ: górnoprzepustowy ok. 150 Hz (mały głośnik nie charczy) */
    float gain = (float)master_q12 / 4096.0f * 7500000.0f;
    uint32_t i, done = 0, w = scope_w;
    int sc = scope_on, eq = glob.g[G_SPKEQ], fading, want;
    float pk = 0.0f;
    if (mt_seen != mt_ver || mt_xs < 0.0f) {   /* (v0.64) MULTI: zmiana trybu / ścieżek po przygaszeniu wyjścia */
        if ((!mt_on && eng_play >= NENG) || mt_xg <= 0.0f) {   /* (start: od razu) */
            mt_seen = mt_ver;
            mt_apply();
            mt_xs = 1.0f;
        } else {
            mt_xs = -1.0f;
        }
    }
    i = set.eng;                             /* zmiana silnika: nowy do wspólnego RAM, zanim cokolwiek zagra */
    if (i >= NENG)                           /* (ustawienia w trakcie wczytywania) */
        i = eng_play < NENG ? eng_play : ENG_MOOG;
    if (mt_on) {                             /* MULTI: wybrana ścieżka gra jako „silnik”, bez wymiany RAM */
        mt_focus_in();
        i = eng_play;
    }
    if (i != eng_play && (eng_play >= NENG || eng_fade <= 0.0f)) {
        eng_enter(i);
        eng_fade = 1.0f;
    }
    fading = i != eng_play;                  /* stary jeszcze gra: wycisza się (bez trzasku), nuty czekają na nowy */
    e = &ENG[eng_play];
    if (dk_g < 1.0f && !dk_kill && !dk_quiet_req && dk_r == dk_w && !e->sounding() && dk_epk < 1e-4f)
        dk_g = 1.0f;                         /* nic nie gra (i nic nie zostało w wyjściu): od razu pełny (atak nowej nuty) */
    dk_epk = 0.0f;
    sq_block_start(e);                       /* SEQ / ARP: start, stop, tempo */
    ogs_fx_bpm = ctl.bpm;                    /* (v0.43) STUTTER: tempo i pozycja SEQ (-1: stoi) */
    ogs_fx_bend_semi = eng_bend(e);
    {                                        /* (v0.46) SYNC efektów: dodatki presetu, który gra */
        const uint8_t *x = xt_play();
        ogs_fx_sync = ((uint32_t)x[XT_SYNC] | (uint32_t)x[XT_SYNC + 1] << 8 | (uint32_t)x[XT_SYNC + 2] << 16) & FX_SYNC_SET;
    }
    ogs_fx_beat = sq.play ? ((float)(int32_t)(sq.T - sq.start_T) + sq.frac) * (1.0f / (float)SQ_PPQN) : -1.0f;
    if (!fading)
        events_in();
    sq_apply(e);
    want = dk_kill || dk_quiet_req || dk_r != dk_w;
    while (done < n) {
        uint32_t c = n - done > APP_BLOCK ? APP_BLOCK : n - done;
        if (mt_on) {                         /* (v0.76) MULTI: wysyłki poprzedniego bloku na drugim rdzeniu (taśma): start
                                              * przed MINI innej ścieżki (mt_mix), inaczej teraz */
            ogs_fx_sends_go();
            if (mt_mini() < 0)
                ogs_fx_sends_kick();
        }
        sq_render(e, bl, br, c);             /* silnik; SEQ i ARP dzielą blok w chwilach nut */
        if (fading || want || dk_g < 1.0f)
            for (i = 0; i < c; i++) {
                float g;
                dk_epk = fm_maxf(dk_epk, fm_maxf(fm_fabsf(bl[i]), fm_fabsf(br[i])));
                if (fading)
                    eng_fade = eng_fade > 1.0f / ENG_FADE_N ? eng_fade - 1.0f / ENG_FADE_N : 0.0f;
                dk_g = want ? fm_maxf(dk_g - 1.0f / DK_N, 0.0f) : fm_minf(dk_g + 1.0f / DK_N, 1.0f);
                g = eng_fade * dk_g;
                g = g * g * (3.0f - 2.0f * g);   /* krzywa S: łagodny początek i koniec */
                bl[i] *= g;
                br[i] *= g;
            }
        if (mt_on) {                         /* (v0.64) MULTI: reszta ścieżek; pula efektów zajęta */
            if (sc)                          /* (v0.90, decyzja użytkownika 2026-11-03) oscyloskop: sama wybrana ścieżka (było: cały
                                              * miks, „wariował”, gdy ona milczy), przed głośnością i efektami */
                for (i = 0; i < c; i++)
                    scope[w++ & (SC_LEN - 1u)] = (int16_t)(fm_clampf((bl[i] + br[i]) * 0.5f, -1.0f, 1.0f) * 32767.0f);
            mt_mix(bl, br, c);
            mt_sends(bl, br, c);             /* (v0.76) wspólne REVERB / DELAY (efekty presetów ścieżek) */
            ogs_pf_render(bl, br, c);        /* (v0.74) PERFORM: tylko w MULTI, na całym miksie (pamięć za ścieżkami) */
            ogs_fx_master(mt_mst >= 0 ? (uint32_t)mt_mst : (uint32_t)glob.g[G_MTMST], mc_p, bl, br, c);   /* (v0.77) master */
            if (mt_track(eng_play) >= 0)
                ogs_fx_track_meter((uint32_t)mt_track(eng_play));   /* (v0.83) GR meter strony FX: wybrana ścieżka */
        } else {
            ogs_fx_render(fx_play(), fx_xplay(), bl, br, c);   /* efekty globalne za silnikiem; (v0.74) bez PERFORM */
        }
        sq_click_mix(bl, br, c);             /* metronom za efektami */
        if (eq || eq_g > 0.0f) {             /* SPEAKER EQ włącza się / wyłącza przenikaniem (bez trzasku) */
            if (eq_g <= 0.0f) {              /* od nowa: stan filtra z bieżącego dźwięku */
                hx[0] = bl[0];
                hx[1] = br[0];
                hy[0] = hy[1] = 0.0f;
            }
            for (i = 0; i < c; i++) {
                float yl = 0.979f * (hy[0] + bl[i] - hx[0]), yr = 0.979f * (hy[1] + br[i] - hx[1]);
                hx[0] = bl[i];
                hx[1] = br[i];
                hy[0] = yl;
                hy[1] = yr;
                eq_g = eq ? fm_minf(eq_g + 1.0f / 256.0f, 1.0f) : fm_maxf(eq_g - 1.0f / 256.0f, 0.0f);
                bl[i] += eq_g * (yl - bl[i]);
                br[i] += eq_g * (yr - br[i]);
            }
        }
        {
            float mu = mute_g, ms = mute_req ? -1.0f / MUTE_N : 1.0f / MUTE_N;
            for (i = 0; i < c; i++) {
                float l = fm_tanhf(bl[i]), r = fm_tanhf(br[i]), m;
                pk = fm_maxf(pk, fm_maxf(fm_fabsf(l), fm_fabsf(r)));
                gain_now += (gain - gain_now) * 0.004f;
                m = gain_now;
                if (mu < 1.0f || ms < 0.0f) {    /* zapis flasha: krzywa S do zera i z powrotem */
                    mu = fm_clampf(mu + ms, 0.0f, 1.0f);
                    m *= mu * mu * (3.0f - 2.0f * mu);
                }
                if (mt_xg < 1.0f || mt_xs < 0.0f) {   /* (v0.64) MULTI: zmiana trybu / ścieżek */
                    mt_xg = fm_clampf(mt_xg + mt_xs * (1.0f / MT_XN), 0.0f, 1.0f);
                    m *= mt_xg * mt_xg * (3.0f - 2.0f * mt_xg);
                }
                lr[2 * (done + i)] = (int32_t)(l * m);
                lr[2 * (done + i) + 1] = (int32_t)(r * m);
                if (sc && !mt_on)
                    scope[w++ & (SC_LEN - 1u)] = (int16_t)((l + r) * 16383.0f);
            }
            mute_g = mu;
        }
        done += c;
    }
    if (want && dk_g <= 0.0f) {              /* przy zerze: zmiany z UI wchodzą, UI może wczytać preset */
        dk_apply();
        dk_quiet = dk_quiet_req;
    } else {
        dk_quiet = 0;
    }
    scope_w = w;
    out_lvl = pk;
}

void app_overload(void)                  /* (przerwanie audio, po renderze: silnik zrzuca głos następnym razem) */
{
    ogs_overload = 1;
    mt_ovl = (1u << MT_N) - 1u;              /* (v0.64) MULTI: każda ścieżka zrzuca swój głos */
}

void app_panic(void)
{
    ev_push(EV_ALL_OFF);
    if (eng_ready())                         /* inny w RAM: i tak cichnie przy zmianie */
        E->all_off();
}

/* ------------------------------------------------------------ parametry --- */
static int32_t pget(int32_t id)
{
    uint32_t k;
    for (k = dk_w; k != dk_r; k--)           /* czeka w kolejce bez trzasków: ta wartość (najnowsza) */
        if (dk_q[(k - 1u) % DK_Q].id == id && dk_q[(k - 1u) % DK_Q].eng == set.eng)
            return dk_q[(k - 1u) % DK_Q].v;
    return id < E->np ? E->p[id] : E->vget(id);
}

static uint32_t vol_pct(void)            /* VOL na ekranie: MASTER w %, zaokrąglony (max 4095: 100 %); (v0.88) z histerezą: gałka
                                           * na granicy dwóch wartości (szum przetwornika) nie miga 10 / 11 / 10 % */
{
    static uint32_t shown;
    const uint32_t x = plat_master() * 10000u / 4095u;   /* setne % */
    if (x + 70u < shown * 100u || x > shown * 100u + 70u)
        shown = (x + 50u) / 100u;
    return shown > 100u ? 100u : shown;
}

static void user_num(char *t, int32_t u)  /* numer USER: 01..99, 100..128 */
{
    if (u >= 100)
        *t++ = (char)('0' + u / 100);
    t[0] = (char)('0' + u / 10 % 10);
    t[1] = (char)('0' + u % 10);
    t[2] = 0;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b, int plus)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    else if (plus && v > 0)
        *b++ = '+';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

/* GLO ALGO KNOB: parametry silnika e, które są pod gałkami jego stron (kolejność stron), bez powtórzeń */
static int32_t algo_list(int32_t e, int16_t *ids)
{
    const engine_t *g = &ENG[e];
    int32_t n = 0, p, k, j;
    for (p = 0; p < g->npages; p++)
        for (k = 0; k < 4; k++) {
            int32_t id = g->knob(p, k);
            if (id < 0 || id >= g->np)
                continue;
            for (j = 0; j < n && ids[j] != id; j++)
                ;
            if (j == n && n < 128)
                ids[n++] = (int16_t)id;
        }
    return n;
}
static int algo_ok(int32_t e, int32_t id)
{
    return e < NENG && id >= 0 && id < ENG[e].np && ENG[e].def(id)->fmt != F_NONE;
}
static int32_t algo_id(void)             /* ALGO bieżącego silnika: z GLO albo jego własny */
{
    int32_t v = glob.g[algo_g(set.eng)];
    return algo_ok(set.eng, v) ? v : E->algo;
}
static int32_t algo_page(int32_t e, int32_t id)   /* strona z tym parametrem pod gałką (nazwa w podpowiedzi) */
{
    int32_t p, k;
    for (p = 0; p < ENG[e].npages; p++)
        for (k = 0; k < 4; k++)
            if (ENG[e].knob(p, k) == id)
                return p;
    return 0;
}

static void fx_text(const fx_slot_t *c, int32_t k, char *t);
static void def_text(const pdef_t *d, int32_t id, int32_t v, char *b)
{
    b[0] = 0;
    if (d >= &GDEF[G_MCTH] && d <= &GDEF[G_MCRE]) {   /* (v0.91) MASTER COMP: jak gałki COMPRESSOR */
        fx_slot_t c = {FX_COMP, 7u, {0}};
        c.p[d - &GDEF[G_MCTH]] = (uint8_t)v;
        fx_text(&c, (int32_t)(d - &GDEF[G_MCTH]), b);
    } else if (d->fmt == F_LIST)
        text_cat(b, PLIST(d)[v - d->lo]);
    else if (d == &GDEF[G_VELFIX] && !v)
        text_cat(b, "OFF");
    else if (d >= GDEF && d < GDEF + NGLOB && algo_eng((int32_t)(d - GDEF)) >= 0) {   /* ALGO KNOB: nazwa parametru */
        int32_t e = algo_eng((int32_t)(d - GDEF));
        text_cat(b, algo_ok(e, v) ? ENG[e].def(v)->name : "DEFAULT");
    } else if (d->fmt == F_CUSTOM)
        E->text(id, v, b);
    else if (d->hi == 99)                /* DX7 / CZ w środku 0..99, na ekranie jak wszędzie 0..100 (v0.28) */
        int_text((v * 100 + (v < 0 ? -49 : 49)) / 99, b, d->fmt == F_SIGNED);
    else
        int_text(v, b, d->fmt == F_SIGNED);
}

static void value_text(int32_t id, int32_t v, char *b) { def_text(E->def(id), id, v, b); }

/* --------------------------------------------------------------- zapis --- */
/* presety (numery): fabryczne silników po kolei (każdy od swojego INIT), USER 01-128 (numer = miejsce, na liście tylko
 * zajęte), brzmienia banków DX7 (32 banki po 32), banków CZ, bank FM1. Kolejność na liście PRESETS jest inna niż
 * numery (v0.36, preset_walk): silnik po silniku INIT, USER, fabryczne, FM1, wgrane banki. Rekord (USER i nadpisany fabryczny): nazwa (12 znaków), silnik,
 * numer fabrycznego (0xFF: USER), liczba parametrów, parametry upakowane: 1 bajt, gdy zakres mieści się w int8,
 * inaczej 2 (TUNE MOOGA). Silnik urósł: brakujące parametry domyślne. Bank USER: 16 rekordów; nadpisane fabryczne:
 * lista do 19 rekordów (rozpoznane po silniku i numerze, więc nowe presety fabryczne niczego nie przesuwają). */
#define NFACT_OLD (NPRESETS + NDXP)            /* do v0.12: bez WAVE (numery USER od tego miejsca) */
#define NFACT_V13 (NPRESETS + NDXP + NWTP)     /* do v0.14: bez PHASE */
#define NFACT_V15 (NPRESETS + NDXP + NWTP + NZP)   /* do v0.21: bez VCO */
#define NFACT_V22 (NFACT_V15 + NVP)           /* do v0.22: bez SEM */
#define NFACT_V23 (NFACT_V22 + NSP)           /* do v0.26: bez DCO */
#define NFACT_V27 (NFACT_V23 + NJP)           /* do v0.41: bez BIT */
#define NFACT_V42 (NFACT_V27 + NBTP)           /* do v0.43: bez WEST */
#define NFACT_V44 (NFACT_V42 + NWSP)           /* do v0.48: bez ALPHA */
#define NFACT_V49 (NFACT_V44 + NAP)            /* do v0.49: bez MORPH */
#define NFACT_V51 (NFACT_V49 + NMPK)           /* do v0.51: bez 88 / 99 */
#define NFACT_V52 (NFACT_V51 + ND8K + ND9K)    /* do v0.53: bez DUAL (układ dziś: 88 i 99 z zestawami v0.56) */
#define ND8K_V54 5                             /* do v0.55: zestawy 88, 99, DUAL (v0.56: więcej — przy wczytaniu starszych */
#define ND9K_V54 4                             /* ustawień numery przesunięte, rsv 14) */
#define NDUK_V54 4
#define NMPK_V87 15                            /* do v0.87: zestawy MORPH, 88, 99, DUAL (v0.88: więcej na końcu każdego silnika, */
#define ND8K_V87 8                             /* numery dalszych przesunięte, rsv 17, sceny SCN_NUM 3) */
#define ND9K_V87 7
#define NDUK_V87 7
#define NFACT_V54 (NFACT_V52 + NDUK)           /* do v0.77: bez PHYS */
#define NFACT_V78 (NFACT_V54 + NPHP)           /* do v0.83: bez FORMANT */
#define NFACT (NFACT_V78 + NFOP)
/* (v0.90, decyzja użytkownika 2026-11-03: mniej, a bardziej różnych) zestawy perkusji: z 20 zostało INIT + 5 w MORPH i DUAL, INIT + 3 w 88 i 99
 * (wybór z pomiaru brzmienia: najbardziej od siebie różne; MORPH: INDUS, METAL, ELECTRO z decyzji). Stare numery v0.88-v0.89 (po 20) ->
 * nowe: zostawiony na swoje miejsce, usunięty na najbardziej podobny zostawiony. Ustawienia (rsv 18), sceny (SCN_NUM 4) i nadpisane
 * fabryczne (bez znacznika KIT90_NEW: stara numeracja, przeliczana przy każdym odczycie, flash bez zmian) */
#define NKIT_V89 20
#define KIT90_NEW 0x80u                         /* bajt numeru nadpisanego zestawu perkusji: zapis z v0.90 (nowa numeracja) */
static const uint8_t KIT90[4][NKIT_V89] = {
    {0, 0, 2, 2, 1, 3, 0, 3, 3, 4, 5, 0, 3, 5, 3, 0, 0, 5, 4, 5},   /* MORPH (ELECTRO pierwszy po INIT: domyślny) */
    {0, 3, 0, 0, 0, 1, 0, 0, 1, 3, 1, 3, 0, 0, 2, 0, 0, 0, 3, 0},   /* 88 */
    {0, 0, 0, 0, 3, 3, 2, 1, 2, 2, 0, 0, 3, 0, 1, 3, 2, 3, 3, 0},   /* 99 */
    {0, 1, 1, 2, 4, 3, 4, 0, 3, 0, 0, 0, 0, 0, 0, 5, 0, 5, 0, 0},   /* DUAL */
};
static const uint32_t KIT90_KEEP[4] = {790553u, 278785u, 262785u, 131181u};   /* bit = stary numer został (dziesiętnie: kontrola CI bierze 0x5 cyfr za adres) */
static int32_t kit90_g(int32_t e) { return e == ENG_MORPH ? 0 : e == ENG_88 ? 1 : e == ENG_99 ? 2 : e == ENG_DUAL ? 3 : -1; }
static uint32_t kit90(uint32_t id)               /* numer presetu v0.89 -> v0.90 (silniki za DUAL, USER, banki: o różnicę wcześniej) */
{
    static const uint8_t NN[4] = {NMPK, ND8K, ND9K, NDUK};
    uint32_t g, at = NFACT_V49;
    if (id == 0xFFFFu || id < NFACT_V49)
        return id;
    id -= NFACT_V49;
    for (g = 0; g < 4u; g++) {
        if (id < NKIT_V89)
            return at + KIT90[g][id];
        id -= NKIT_V89;
        at += NN[g];
    }
    return at + id;
}
#define NUSER 128
#define NUSER_LO 64                      /* USER 1-64: efekty w fxp / fxx (układ we flashu do v0.15), 65-128 w fxh */
#define BANK_N 16
#define NBANK (NUSER / BANK_N)
#define NSYX 32                          /* banki DX7: numeracja (ID_CZ za nimi) */
#define NSYX_USE 8                       /* v0.19: używane banki DX7 1-8 (0xA0000-0xA7FFF); dalej tablice USER WAVE
                                          * (decyzja użytkownika: gdy zabraknie flasha, banki DX7 po 8) */
#define ID_USER NFACT
#define ID_SYX (NFACT + NUSER)
#define NCZB 16                          /* banki Casio CZ (v0.17.1): po 16 brzmień (128 B), dwa banki na sektor */
#define NCZB_USE 8                       /* v0.53 (decyzja użytkownika 2026-10-29): używane banki CZ 1-8 (0x93000-0x96FFF);
                                          * 9-16 (0x97000-0x9AFFF, 16 KiB) wolne na przyszłość, numeracja bez zmian */
#define ID_CZ (ID_SYX + NSYX * 32)
#define ID_FM1 (ID_CZ + NCZB * 16)       /* v0.36: oryginalny bank FM1 (128, FM6; ogs_fm1_bank.h) — na końcu numeracji,
                                          * więc zapisane numery presetów bez zmian */
/* (v0.39) presety fabryczne z efektami i akordem (pakiet DNB / UKG): na końcu numeracji (numery zapisów bez zmian), na
 * liście w sekcji FACTORY swojego silnika, za zwykłymi fabrycznymi. Brzmienie = fabryczny preset `base` silnika + zmiany
 * (id, wartość; -1 kończy), łańcuch efektów (nfx slotów: rodzaj, tryb, 8 gałek; reszta pusta, gałki ponad 8 domyślne),
 * CHORD (dodatki presetu). Tylko do odczytu jak FM1 (SAVE = nowy USER). Brzmienia: do odsłuchu przez użytkownika. */
#define XF_NSET 64
typedef struct {
    uint8_t type, mode, p[FX_NP];
} xfx_t;
typedef struct {
    const char *name;
    uint8_t eng, base, chord, nfx;
    int16_t set[XF_NSET];
    xfx_t fx[4];
} xfpreset_t;
#define XFX(t, m, a, b, c, d, e, f, g, h) {t, m, {a, b, c, d, e, f, g, h}}
#define XENV(b, a, d, s, r) b, a, b + 1, d, b + 2, s, b + 3, r
#define XSUB(lvl) XFX(FX_SUB, 1, lvl, 64, 64, 8, 0, 0, 40, 0)   /* SUB -1 OCT, LOW CUT 70 Hz, krótki GLIDE */
static const xfpreset_t XF[] = {
    {"REESE", ENG_VCO, 0, 0, 4,
     {V_MODE, 0, V_LEGATO, 1, V_GLIDE, 8, V_AWAVE, 0, V_BWAVE, 0, V_BFINE, -12, V_LA, 80, V_LB, 80, V_UNI, 2, V_DET, 20,
      V_CUT, 38, V_RES, 15, V_FENV, 10, XENV(V_FA, 0, 50, 80, 30), XENV(V_AA, 0, 50, 100, 25), V_VINT, 20, V_REV, 0, -1},
     {XFX(FX_DIST, 1, 30, 70, 127, 64, 0, 64, 20, 127), XFX(FX_PHASER, 1, 20, 60, 40, 50, 0, 64, 100, 0),
      XFX(FX_MBAND, 1, 70, 64, 127, 127, 0, 49, 51, 64), XSUB(70)}},
    {"NEURO GROWL", ENG_WT, 3, 0, 4, {-1},
     {XFX(FX_DIST, 5, 45, 80, 90, 60, 0, 64, 30, 127), XFX(FX_VOWEL, 1, 20, 80, 90, 100, 0, 75, 40, 64),
      XFX(FX_MBAND, 1, 90, 64, 127, 127, 0, 49, 51, 64), XSUB(75)}},
    {"HOOVER", ENG_ALPHA, 0, 0, 2,                  /* (v0.39 - v0.48: DCO MODEL ALPHA; ten sam dźwięk) */
     {J_MODEL, 2, J_SAW, 100, J_PULSE, 40, J_PW, 30, J_LPW, 55, J_LRATE, 55, J_UNI, 4, J_DET, 45, J_MODE, 0,
      J_LEGATO, 1, J_GLIDE, 30, J_CUT, 85, J_RES, 5, J_FENV, 0, XENV(J_EA, 0, 40, 0, 30), J_VCA, 2,
      XENV(J_AA, 0, 50, 100, 35), J_EPITCH, -30, J_CHO, 3, J_CMIX, 100, J_WIDTH, 80, -1},
     {XFX(FX_DIST, 1, 20, 80, 127, 64, 0, 64, 20, 127), XFX(FX_REVERB, 3, 50, 60, 10, 25, 2, 2, 20, 90)}},
    {"SUB", ENG_MOOG, 0, 0, 1,
     {P_O1W, 0, P_O1R, 1, P_L1, 90, P_CUT, 50, P_MODE, 0, P_LEGATO, 1, P_GLIDE, 12, XENV(P_AA, 0, 50, 100, 15),
      P_DRIFT, 0, -1},
     {XFX(FX_SAT, 1, 30, 50, 60, 64, 0, 0, 0, 0)}},
    {"SPEED GARAGE", ENG_MOOG, 0, 0, 2,
     {P_O1W, 3, P_O1R, 1, P_O2W, 2, P_O2R, 1, P_O2D, 8, P_L2, 70, P_CUT, 45, P_EMPH, 55, P_CONT, 60,
      XENV(P_FA, 0, 35, 15, 25), XENV(P_AA, 0, 50, 100, 20), P_MODE, 0, P_LEGATO, 1, P_GLIDE, 18, P_DIST, 30, -1},
     {XFX(FX_AWAH, 1, 70, 102, 70, 80, 0, 43, 53, 65), XSUB(60)}},
    {"2-STEP ORGAN", ENG_DX7, 5, 0, 3, {-1},
     {XFX(FX_SAT, 2, 35, 64, 127, 64, 1, 0, 0, 0), XFX(FX_CHORUS, 1, 40, 40, 20, 40, 40, 70, 110, 100),
      XFX(FX_REVERB, 1, 30, 40, 5, 15, 0, 2, 0, 80)}},
    {"GARAGE STAB", ENG_DCO, 0, CH_MIN7, 1,
     {J_SAW, 80, J_PULSE, 40, J_PW, 25, J_CUT, 45, J_RES, 25, J_FENV, 55, XENV(J_EA, 0, 30, 0, 25), J_VCA, 2,
      XENV(J_AA, 0, 40, 0, 30), J_CHO, 1, J_VELF, 30, -1},
     {XFX(FX_REVERB, 3, 40, 50, 10, 25, 2, 2, 10, 100)}},
    {"LIQUID PAD", ENG_DCO, 0, CH_MAJ7, 1,
     {J_MODEL, 0, J_HPF, 1, J_SAW, 70, J_PULSE, 50, J_PW, 30, J_LPW, 40, J_LRATE, 30, J_CUT, 50, J_RES, 10,
      J_FENV, 15, XENV(J_EA, 60, 60, 80, 70), J_CHO, 2, -1},
     {XFX(FX_REVERB, 4, 85, 90, 20, 40, 3, 2, 40, 110)}},
    {"TINE KEYS", ENG_DX7, 1, 0, 3, {-1},
     {XFX(FX_TREM, 2, 50, 50, 0, 0, 1, 0, 0, 0), XFX(FX_CHORUS, 1, 35, 50, 10, 40, 40, 70, 110, 100),
      XFX(FX_REVERB, 3, 35, 45, 5, 20, 2, 2, 0, 90)}},
    /* (v0.51) ALPHA: COMB SAW, CHORUS RATE, obwiednia 4-odcinkowa (propozycja, do odsłuchu; na końcu numeracji: numery
     * USER bez zmian) */
    {"RAVE HOOVER", ENG_ALPHA, 0, 0, 2,
     {J_MODEL, 3, J_SAW, 100, J_PULSE, 30, J_PW, 30, J_LPW, 40, J_LRATE, 55, J_UNI, 4, J_DET, 50, J_MODE, 0,
      J_LEGATO, 1, J_GLIDE, 25, J_CUT, 80, J_RES, 10, J_FENV, 0, XENV(J_EA, 0, 40, 0, 30), J_VCA, 2,
      XENV(J_AA, 0, 50, 100, 35), J_EPITCH, -35, J_CHO, 3, J_CX, 75, J_CMIX, 100, J_WIDTH, 85, -1},
     {XFX(FX_DIST, 1, 20, 80, 127, 64, 0, 64, 20, 127), XFX(FX_REVERB, 3, 50, 60, 10, 25, 2, 2, 20, 90)}},
    {"COMB LEAD", ENG_ALPHA, 0, 0, 1,
     {J_MODEL, 3, J_SAW, 100, J_SUB, 30, J_MODE, 0, J_LEGATO, 1, J_GLIDE, 15, J_CUT, 70, J_RES, 25, J_FENV, 30,
      XENV(J_EA, 0, 45, 60, 30), J_VCA, 2, XENV(J_AA, 0, 50, 100, 30), J_LPITCH, 12, J_LRATE, 62, J_LDLY, 45,
      J_LWHEEL, 100, J_CHO, 1, J_CX, 40, -1},
     {XFX(FX_REVERB, 3, 45, 55, 10, 25, 2, 2, 20, 90)}},
    {"STAGE BRASS", ENG_ALPHA, 0, 0, 1,
     {J_MODEL, 2, J_SAW, 100, J_PULSE, 20, J_PW, 20, J_CUT, 50, J_RES, 15, J_FENV, 50, XENV(J_EA, 25, 55, 70, 40),
      J_EL1, 100, J_ET2, 35, J_EL2, 45, J_UNI, 2, J_DET, 15, J_CHO, 1, J_VINT, 15, -1},
     {XFX(FX_REVERB, 3, 40, 50, 10, 25, 2, 2, 10, 100)}},
};
#define NXF (int32_t)(sizeof XF / sizeof XF[0])
#define ID_XF (ID_FM1 + NFM1)
#define NALL (ID_XF + NXF)
#define NAME_LEN 12
#define REC_HDR (NAME_LEN + 3)
#define REC_SIZE 232u                    /* 217 bajtów parametrów: zapas na rozwój silników (v0.24: 200 -> 232,
                                          * DRAW Q/X zjadło resztę starego zapasu; nadpisanych fabrycznych odtąd
                                          * mniej na raz: 19 -> 16, banki USER (128 miejsc) bez zmian) */
#define FACT_N (int32_t)((PLAT_STORE_MAX - 8u) / REC_SIZE)
#define BANK_MAGIC (0x4D450000u | REC_SIZE)
#define SYX_BASE 0xA0000u                /* banki DX7 w obszarze danych: 4 KiB każdy (32 x 128 B) */
#define CZ_BASE 0x93000u                 /* banki CZ: 0x93000-0x9AFFF (XDATA0 + początek obszaru danych), 2 KiB każdy.
                                          * W XDATA0 bajty 0xF06-0xF07 sektora nie mogą być „AT” (plat.h): to bajty 6-7
                                          * brzmienia 15 drugiego banku — bajt 7 (DELAY, starszy) zawsze <= 3 (cz_clean) */
_Static_assert(CZ_BASE + NCZB / 2 * 0x1000u <= 0x9C000u, "banki CZ przed OBJ_FXH2 (0x9C000)");
_Static_assert(NP + 3 <= (int)REC_SIZE - REC_HDR && NX <= (int)REC_SIZE - REC_HDR && NZ <= (int)REC_SIZE - REC_HDR
               && NW <= (int)REC_SIZE - REC_HDR && NMPP <= (int)REC_SIZE - REC_HDR && NDMP <= (int)REC_SIZE - REC_HDR
               && NDUP <= (int)REC_SIZE - REC_HDR && NPH <= (int)REC_SIZE - REC_HDR && NFOR <= (int)REC_SIZE - REC_HDR, "rekord presetu");
_Static_assert(8u + BANK_N * REC_SIZE <= PLAT_STORE_MAX, "bank USER: jeden obiekt");
_Static_assert(REC_SIZE - REC_HDR <= SET2_SLOT, "ustawienia: każde brzmienie z rekordu presetu mieści się w OBJ_SET2");
static const uint8_t UOBJ[NBANK] = {OBJ_USER0, OBJ_USER1, OBJ_USER2, OBJ_USER3, OBJ_USER4, OBJ_USER5, OBJ_USER6, OBJ_USER7};
static struct {
    uint32_t magic, used;
    uint8_t d[PLAT_STORE_MAX - 8u];
} bk;                                    /* jeden bufor na odczyt / zapis banku (i OBJ_SET2: set2_build, settings_load) */
_Static_assert(sizeof(set2_t) <= sizeof bk, "ustawienia: OBJ_SET2 w buforze banku");
static uint32_t used[NBANK + 1];         /* maski: banki USER, na końcu nadpisane fabryczne */
static uint8_t reng[NUSER];              /* silnik presetu USER */
static int8_t fact_at[NFACT];            /* nadpisany fabryczny: miejsce na liście, -1 brak */

/* HOME: wielka nazwa silnika w tle, mapa krycia 2 bity na piksel (bardzo ciemna: więcej odcieni nie widać); rysowana
 * od nowa, gdy bg_for się nie zgadza. Ta sama pamięć: przenoszenie starych zapisów (migrate, fx_migrate). */
#define BG_H 186
static uint8_t bg_a[240 * BG_H / 4] __attribute__((section(".pool"), aligned(4)));
static int32_t bg_for = -1;

/* efekty globalne (ogs_fx.h): bieżące każdego silnika (gra fxc.cur[set.eng]), presetów USER i nadpisanych fabrycznych;
 * osobne obiekty (OBJ_FXP...), żeby rekord presetu i jego zapisy zostały jak były. Wpis niepoprawny = domyślny łańcuch.
 * v0.26: w RAM tylko bieżące (fxc); efekty presetów czytane w miejscu we flashu (plat_store_view), zapisywane
 * fragmentami (plat_store_patch). Układ we flashu bez zmian (fxp_t, fxx_t, fxh_t), bieżące silników 9-16 na końcu fxh. */
#define FXP_MAGIC 0x3758464Fu                /* "OFX7": DISTORTION MORE: TYPE, BIAS, LOW CUT, HIGH CUT (v0.16) */
#define FXP_MAGIC_6 0x3658464Fu              /* "OFX6": 18 rodzajów (v0.14: HYPER/DIM, MULTIBAND; FX_NONE 16 -> 18) */
#define FXP_MAGIC_5 0x3558464Fu              /* "OFX5": 8 slotów (v0.11.1) */
#define FXP_MAGIC_4 0x3458464Fu              /* "OFX4": 6 slotów, puste sloty (FX_NONE), domyślnie pusty łańcuch */
#define FXP_MAGIC_3 0x3358464Fu              /* "OFX3": 16 rodzajów, TYPE na MORE K1 (p[4]) */
#define FXP_MAGIC_2 0x3258464Fu              /* "OFX2": 6 slotów, DIST TYPE p[6], REVERB TYPE p[7] (v0.9.4) */
#define FXP_MAGIC_1 0x3158464Fu              /* "OFX1": 4 sloty, jeden obiekt (v0.9.0 - v0.9.3) */
/* miejsca na efekty nadpisanych fabrycznych: zamrożone na 19 (FACT_N przy rekordzie 200 B) — układ OBJ_FXP / OBJ_FXX
 * we flashu się nie zmienia, gdy rekord presetu rośnie (v0.24: FACT_N 16, używane tylko pierwsze FACT_N) */
#define FX_FACT_N 19
_Static_assert(FACT_N <= FX_FACT_N, "efekty: miejsce na każdy nadpisany fabryczny");
typedef struct {
    uint32_t magic;
    fx_slot_t cur[NENG_V8][FX_SLOTS];
    fx_slot_t user[NUSER_LO][FX_SLOTS];
    fx_slot_t fact[FX_FACT_N][FX_SLOTS];
} fxp_t;
#define FXP_A PLAT_STORE_MAX                 /* pierwsza część w OBJ_FXP, reszta w OBJ_FXP2 */
_Static_assert(sizeof(fxp_t) > FXP_A && sizeof(fxp_t) - FXP_A <= PLAT_STORE_MAX, "efekty: dwa obiekty");
/* gałki ponad 8 (ogs_fx.h FX_X_*): te same wpisy co fxp_t (bieżące, USER, nadpisane fabryczne), osobny obiekt OBJ_FXX.
 * Pierwsze FX_NX0 gałek (COMPRESSOR, MULTIBAND) jak do v0.29; v0.30: DELAY (FX_NXT) w ogonie na końcu obiektu, każdy
 * bajt z bitem 7 (0 = wpis sprzed v0.30: DELAY w starym układzie, przeliczany przy odczycie, fx_dl_old) */
#define FX_NX0 20
#define FX_NXT 4
#define FX_NXR 6                             /* v0.34: ogon REVERB — dla WSZYSTKICH wpisów w OBJ_FXX (w OBJ_FXH brak miejsca) */
#define FX_NXA 4                             /* v0.55: ogon AUTO WAH (MODE, TRIG, DECAY, SUSTAIN), jak REVERB w OBJ_FXX */
_Static_assert(FX_X_DELAY == FX_NX0 && FX_X_REVERB == FX_NX0 + FX_NXT && FX_X_AWAH == FX_X_REVERB + FX_NXR
               && FX_X_EQ == FX_X_AWAH + FX_NXA && FX_NX_V90 == FX_X_EQ, "efekty: ogony");
#define FXX_MAGIC 0x3158584Fu                /* "OXX1" */
#define FXX_NENT (NENG_V8 + NUSER_LO + FX_FACT_N)
typedef struct {
    uint32_t magic;
    uint8_t cur[NENG_V8][FX_NX0];
    uint8_t user[NUSER_LO][FX_NX0];
    uint8_t fact[FX_FACT_N][FX_NX0];
    uint8_t tcur[NENG_V8][FX_NXT];           /* v0.30 */
    uint8_t tuser[NUSER_LO][FX_NXT];
    uint8_t tfact[FX_FACT_N][FX_NXT];
    uint8_t rcur[NENG_V8][FX_NXR];           /* v0.34: ogon REVERB (bit 7 jak DELAY; 0 = wpis sprzed v0.34, fx_rv_old) */
    uint8_t ruser[NUSER_LO][FX_NXR];
    uint8_t rfact[FX_FACT_N][FX_NXR];
    uint8_t rhuser[NUSER - NUSER_LO][FX_NXR];   /* USER 65-128 i silniki 9-16 (ich łańcuchy w OBJ_FXH) */
    uint8_t rhcur[NENG_MAX - NENG_V8][FX_NXR];
    uint8_t awah[FXX_NENT + NUSER - NUSER_LO + NENG_MAX - NENG_V8][FX_NXA];   /* v0.55: ogon AUTO WAH, wpisy w kolejności
                                                                             * rcur .. rhcur (bit 7 jak REVERB; 0 = domyślne) */
} fxx_t;
_Static_assert(sizeof(fxx_t) <= PLAT_STORE_MAX, "efekty: gałki ponad 8 w jednym obiekcie");
_Static_assert(__builtin_offsetof(fxx_t, awah) == __builtin_offsetof(fxx_t, rcur)
               + (FXX_NENT + NUSER - NUSER_LO + NENG_MAX - NENG_V8) * FX_NXR, "efekty: ogony REVERB po kolei");
#define FX_AWO(orv) (FXOFF(fxx_t, awah) + ((orv) - FXOFF(fxx_t, rcur)) / FX_NXR * FX_NXA)   /* ogon AUTO WAH wpisu (z REVERB) */
_Static_assert(__builtin_offsetof(fxx_t, tcur) == __builtin_offsetof(fxx_t, cur) + FXX_NENT * FX_NX0,
               "efekty: cur, user, fact po kolei");
_Static_assert(__builtin_offsetof(fxp_t, cur) == 4u, "efekty: za magic same wpisy (fx_migrate: sizeof - 4)");
/* USER 65-128 (v0.16): efekty i gałki ponad 8 razem, dwa obiekty (OBJ_FXH, OBJ_FXH2); v0.26: za nimi bieżące silników
 * 9-16 (starszy zapis jest krótszy: ich brak = domyślne; starsza wersja czyta swoją część i resztę pomija) */
#define FXH_MAGIC 0x3148584Fu                /* "OXH1" */
typedef struct {
    uint32_t magic;
    fx_slot_t user[NUSER - NUSER_LO][FX_SLOTS];
    uint8_t xuser[NUSER - NUSER_LO][FX_NX0];
    fx_slot_t cur[NENG_MAX - NENG_V8][FX_SLOTS];
    uint8_t xcur[NENG_MAX - NENG_V8][FX_NX0];
    uint8_t txuser[NUSER - NUSER_LO][FX_NXT];   /* v0.30: ogon DELAY (jak w fxx_t) */
    uint8_t txcur[NENG_MAX - NENG_V8][FX_NXT];
} fxh_t;
_Static_assert(sizeof(fxh_t) > PLAT_STORE_MAX && sizeof(fxh_t) - PLAT_STORE_MAX <= PLAT_STORE_MAX,
               "efekty USER 65-128: dwa obiekty");
static struct {                              /* bieżące efekty każdego silnika (gra cur[set.eng]) */
    fx_slot_t cur[NENG_MAX][FX_SLOTS];
    uint8_t x[NENG_MAX][FX_NX];
} fxc;
static uint8_t fx_dirty;
/* trzy zapisy efektów we flashu: obiekt albo para (A: pierwsze PLAT_STORE_MAX bajtów, B: reszta) */
enum { FO_P, FO_X, FO_H, NFO };
static const struct {
    uint8_t a, b;
    uint16_t n;
    uint32_t magic;
} FXO[NFO] = {{OBJ_FXP, OBJ_FXP2, sizeof(fxp_t), FXP_MAGIC}, {OBJ_FXX, OBJ_FXX, sizeof(fxx_t), FXX_MAGIC},
              {OBJ_FXH, OBJ_FXH2, sizeof(fxh_t), FXH_MAGIC}};
static uint8_t fx_ok[NFO];                   /* zapis jest (znacznik); 0: pusty, przy pierwszym zapisie zakładany */
#define FXOFF(t, f) ((uint32_t)__builtin_offsetof(t, f))
#define FX_SL (FX_SLOTS * sizeof(fx_slot_t))  /* łańcuch jednego wpisu */
static uint32_t fxo_len(int32_t w, int32_t part)   /* długość obiektu A (0) / B (1) w pełnym układzie */
{
    return part ? (uint32_t)FXO[w].n - PLAT_STORE_MAX : FXO[w].n < PLAT_STORE_MAX ? FXO[w].n : PLAT_STORE_MAX;
}
static int fxo_read(int32_t w, uint32_t off, void *dst, uint32_t n)   /* 1: cały zakres jest we flashu */
{
    uint8_t *d = dst;
    if (!fx_ok[w])
        return 0;
    while (n) {
        uint32_t part = off >= PLAT_STORE_MAX, at = off - part * PLAT_STORE_MAX, len = 0, k, i;
        const uint8_t *v = plat_store_view(part ? FXO[w].b : FXO[w].a, &len);
        k = PLAT_STORE_MAX - at < n ? PLAT_STORE_MAX - at : n;
        if (!v || at + k > len)
            return 0;
        for (i = 0; i < k; i++)
            d[i] = v[at + i];
        off += k;
        d += k;
        n -= k;
    }
    return 1;
}
static int fxo_create(int32_t w)         /* pusty zapis: sam znacznik (wpisy dopisują się z zerami: niepoprawne) */
{
    uint32_t m = FXO[w].magic;
    if (plat_store_patch(FXO[w].a, 4, 0, &m, 4) != 0
        || (FXO[w].b != FXO[w].a && plat_store_patch(FXO[w].b, 0, 0, 0, 0) != 0))
        return -1;
    fx_ok[w] = 1;
    return 0;
}
static int fxo_write(int32_t w, uint32_t off, const void *src, uint32_t n)   /* 0: zapisane */
{
    const uint8_t *s = src;
    if (!fx_ok[w] && fxo_create(w) != 0)
        return -1;
    while (n) {
        uint32_t part = off >= PLAT_STORE_MAX, at = off - part * PLAT_STORE_MAX;
        uint32_t k = PLAT_STORE_MAX - at < n ? PLAT_STORE_MAX - at : n;
        if (plat_store_patch(part ? FXO[w].b : FXO[w].a, fxo_len(w, (int32_t)part), at, s, k) != 0)
            return -1;
        off += k;
        s += k;
        n -= k;
    }
    return 0;
}
static int fxo_check(int32_t w)          /* zapis w bieżącym układzie (części nie dłuższe niż pełne) */
{
    uint32_t la = 0, lb = 0;
    const uint8_t *a = plat_store_view(FXO[w].a, &la);
    if (!a || la < 4u || la > fxo_len(w, 0)
        || (uint32_t)(a[0] | a[1] << 8 | a[2] << 16 | (uint32_t)a[3] << 24) != FXO[w].magic)
        return 0;
    if (FXO[w].b != FXO[w].a && plat_store_view(FXO[w].b, &lb) && lb > fxo_len(w, 1))
        return 0;
    return 1;
}
/* domyślne gałki: K1..K4, MORE K1..K4 */
static const uint8_t FXDEF[FX_NTYPE][FX_NP] = {   /* K1..K4 | MORE K1..K4; TYPE zawsze MORE K1 (p[4]); dalej FXXDEF */
    {40, 90, 127, 64, 0, 64, 20, 127},       /* DISTORTION: DRIVE, TONE, MIX, LEVEL | TYPE, BIAS, LOW CUT, HIGH CUT (v0.16) */
    {59, 32, 0, 127, 0, 127, 64, 0},         /* BITCRUSHER: BITS (8), RATE, JITTER, MIX | -, TONE, LEVEL */
    {50, 64, 20, 64, 40, 70, 110, 100},      /* CHORUS: RATE, DEPTH, FEEDBACK, MIX | DELAY 1, DELAY 2, LPF, WIDTH */
    {60, 70, 10, 40, 0, 2, 20, 90},          /* REVERB (v0.34): DECAY, SIZE, PREDELAY, MIX | MODE, COLOR (NOW), ATTACK,
                                              * DIFFUSION */
    {64, 64, 64, 64, 0, 127, 64, 0},         /* EQ: LOW, MID, FREQ, HIGH | LOW CUT, HIGH CUT, LEVEL */
    {40, 40, 79, 55, 0, 0, 127, 32},         /* COMPRESSOR: THRESH, RATIO, ATTACK (10 ms), RELEASE (100 ms) | STYLE, MAKEUP
                                              * (AUTO), MIX, KNEE (6 dB) */
    {95, 50, 25, 45, 0, 0, 0, 64},           /* DELAY: TIME (300 ms), FEEDBACK, CHARACTER (AGE), MIX | MODE (TAPE), STYLE
                                              * (SINGLE), DIFFUSION, SIZE (v0.30) */
    {50, 64, 127, 64, 0, 0, 0, 0},           /* SATURATION: DRIVE, TONE, MIX, LEVEL | TYPE */
    {100, 0, 100, 60, 0, 0, 0, 0},           /* OCTAVER: OCT -1, OCT -2, DRY, TONE */
    {80, 30, 0, 127, 0, 40, 0, 0},           /* FILTER: CUTOFF, RESO, DRIVE, MIX | TYPE, LFO RATE, LFO DEPTH */
    {40, 30, 20, 127, 30, 80, 0, 0},         /* LO-FI: WOW, FLUTTER, NOISE, MIX | SATURATE, TONE */
    {64, 60, 64, 64, 0, 0, 0, 0},            /* TONE SHIFT: SHIFT (0), RANGE (12), MIX, WINDOW | FEEDBACK */
    {60, 64, 30, 0, 0, 0, 0, 0},             /* RING MOD: FREQ, MIX, LFO RATE, LFO DEPTH | TYPE */
    {60, 80, 0, 0, 0, 0, 0, 0},              /* TREMOLO: RATE, DEPTH, SHAPE, PHASE | TYPE */
    {30, 80, 90, 64, 40, 100, 0, 0},         /* FLANGER: RATE, DEPTH, FEEDBACK, MIX | MANUAL, WIDTH */
    {30, 80, 60, 64, 1, 64, 100, 0},         /* PHASER: RATE, DEPTH, FEEDBACK, MIX | TYPE (8), CENTER, WIDTH */
    {50, 50, 64, 64, 0, 40, 0, 0},           /* HYPER/DIM: RATE, DETUNE, VOICES (4), MIX | -, DIM SIZE, DIM MIX */
    {100, 64, 127, 127, 0, 49, 51, 64},      /* MULTIBAND: DEPTH, TIME, UPWARD, DOWNWARD | TYPE, X LOW (88 Hz), X HIGH
                                              * (2.5 kHz), OUT GAIN */
    {70, 102, 70, 127, 0, 43, 53, 65},       /* AUTO WAH (v0.34): SENS, RANGE (+3 okt.), RESO, MIX | TYPE (WAH), FREQ
                                              * (300 Hz), ATTACK (5 ms), RELEASE (150 ms) */
    {80, 64, 64, 0, 0, 0, 40, 0},            /* SUB (v0.39): LEVEL, OCTAVE (-1), LOW CUT (70 Hz), GLIDE (OFF) | TYPE (SINE),
                                              * ATTACK (1 ms), RELEASE (55 ms) */
    {0, 60, 40, 100, 0, 50, 0, 64},          /* VOWEL (v0.40): VOWEL (A), RESO, MOD, MIX | TYPE (MALE), RATE (0,5 Hz), ENV,
                                              * SHIFT (0) */
    {64, 96, 30, 64, 0, 40, 0, 0},           /* COMB (v0.40): TUNE (0), FEEDBACK (+48 %), DAMP, MIX | TYPE (KEY), LFO RATE,
                                              * MOD */
    {55, 50, 0, 127, 0, 64, 0, 20},          /* STUTTER (v0.43): DIV (1/16), CHANCE (39 %), LENGTH (1), MIX | TYPE (REPEAT),
                                              * PITCH (0), DECAY, SMOOTH */
    {48, 127, 90, 127, 0, 64, 8, 0},         /* SIDECHAIN (v0.88): DIV (1/4), DEPTH (100), LENGTH, MIX | TRIG (BEAT), CURVE (0),
                                              * ATTACK (1 ms) */
    {64, 64, 50, 64, 127, 0, 0, 0},          /* (v0.93) TRANSIENT: ATTACK, SUSTAIN (0), SPEED, OUTPUT (0 dB) | MIX */
    {72, 0, 0, 64, 40, 0, 0, 0},             /* FREQ SHIFT: SHIFT (+5 Hz), FEEDBACK, SPREAD, MIX | LFO RATE, LFO DEPTH */
    {60, 70, 64, 64, 0, 30, 60, 0},          /* GRAIN: SIZE (~70 ms), DENSITY, PITCH (0), MIX | TYPE (CLOUD), SPRAY, SPREAD, JITTER */
    {80, 70, 60, 50, 0, 90, 40, 0},          /* SHIMMER: DECAY, SHIMMER, TONE, MIX | TYPE (OCTAVE), SIZE, MOD */
};
/* gałki ponad 8: COMPRESSOR RANGE (FULL), LOOKAHEAD, HOLD, SC HP (OFF); MULTIBAND IN GAIN, LOW, MID, HIGH (jak OTT:
 * +10.5, +5.6, +10.5 dB), pasma LOW / MID / HIGH: DN THR, DN RATIO (INF), UP THR, UP RATIO (4.2:1) jak w OTT;
 * DELAY MOD, LOW CUT (35 Hz), HIGH CUT (8 kHz), DUCK */
static const uint8_t FXXDEF[FX_NX] = {127, 0, 0, 0, 64, 92, 79, 92, 56, 127, 41, 44, 63, 127, 39, 44, 52, 127, 41, 44,
                                      20, 20, 102, 0, 50, 30, 30, 77, 64, 110,   /* REVERB (v0.34): MOD RATE (0.7 Hz),
                                      * MOD DEPTH, LOW CUT (52 Hz), HIGH CUT (7 kHz), BASS (1x), WIDTH */
                                      0, 0, 80, 127,    /* AUTO WAH (v0.55): MODE FOLLOW, TRIG EVERY, DECAY (300 ms), SUSTAIN 100 */
                                      64, 33, 89, 105, 29, 38, 38, 29, 0, 0, 0, 0, 0};   /* (v0.92) EQ: GAIN 3 (0 dB), FREQ 1 (120 Hz),
                                      * FREQ 3 (2,5 kHz), FREQ 4 (6 kHz), Q 1-4 (0,7 0,9 0,9 0,7), SLOPE 12 dB, SHAPE SHELF, ANALYZER ON */
static int32_t fx_xoff(uint32_t t)
{
    return t == FX_COMP ? FX_X_COMP : t == FX_MBAND ? FX_X_MBAND : t == FX_DELAY ? FX_X_DELAY : t == FX_REVERB ? FX_X_REVERB
         : t == FX_AWAH ? FX_X_AWAH : t == FX_EQ ? FX_X_EQ : -1;
}
static int32_t fx_nx(uint32_t t)
{
    return t == FX_COMP || t == FX_DELAY ? 4 : t == FX_MBAND ? 16 : t == FX_REVERB ? FX_NXR : t == FX_AWAH ? FX_NXA : t == FX_EQ ? FX_NXE : 0;
}
static void fx_xdef(uint8_t *x, uint32_t t)   /* gałki ponad 8 efektu t: domyślne */
{
    int32_t i, o = fx_xoff(t);
    for (i = 0; o >= 0 && i < fx_nx(t); i++)
        x[o + i] = FXXDEF[o + i];
}
static void fx_xall(uint8_t *x)          /* cały blok domyślny */
{
    int32_t i;
    for (i = 0; i < FX_NX; i++)
        x[i] = FXXDEF[i];
}
static void fx_xfix(uint8_t *x)          /* poza zakresem: domyślna */
{
    int32_t i;
    for (i = 0; i < FX_NX; i++)
        if (x[i] > 127u)
            x[i] = FXXDEF[i];
}
static void fx_xcopy(uint8_t *d, const uint8_t *s)
{
    int32_t i;
    for (i = 0; i < FX_NX; i++)
        d[i] = s[i];
}
static const uint8_t FX_NMODE[FX_NTYPE] = {6, 1, 1, 10, 1, 7, 7, 2, 1, 4, 1, 1, 2, 2, 1, 3, 1, 2, 4, 2, 2, 2, 5, 2, 1, 1, 3, 3};   /* rodzaje poza OFF */
/* rodzaj efektu na gałce TYPE (MORE K1), -1: jeden rodzaj; mode = włączony ? TYPE + 1 : 0 */
static int32_t fx_typep(uint32_t t) { return t < FX_NTYPE && FX_NMODE[t] > 1 ? FX_TYPEK : -1; }

static void fx_fix(fx_slot_t *c)         /* TYPE zgodny z trybem (zapisy sprzed v0.9.3: rodzaj tylko w trybie) */
{
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++) {
        int32_t tp = fx_typep(c[k].type);
        if (tp < 0)
            continue;
        if (c[k].mode)
            c[k].p[tp] = (uint8_t)(c[k].mode - 1u);
        else if (c[k].p[tp] >= FX_NMODE[c[k].type])
            c[k].p[tp] = 0;
    }
}

static void fx_default(fx_slot_t *c)     /* pusty łańcuch (decyzja użytkownika: domyślnie bez efektów) */
{
    int32_t k, j;
    for (k = 0; k < FX_SLOTS; k++) {
        c[k].type = FX_NONE;
        c[k].mode = 0;
        for (j = 0; j < FX_NP; j++)
            c[k].p[j] = 0;
    }
}

static int fx_valid(const fx_slot_t *c)  /* każdy efekt raz, rodzaje i gałki w zakresie */
{
    uint32_t seen = 0;
    int32_t k, j;
    for (k = 0; k < FX_SLOTS; k++) {
        if (c[k].type == FX_NONE && !c[k].mode)
            continue;
        if (c[k].type >= FX_NTYPE || (seen >> c[k].type & 1u) || c[k].mode > FX_NMODE[c[k].type])
            return 0;
        seen |= 1u << c[k].type;
        for (j = 0; j < FX_NP; j++)
            if (c[k].p[j] > 127u)
                return 0;
    }
    return 1;
}

static void fx_copy(fx_slot_t *d, const fx_slot_t *s)
{
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++)
        d[k] = s[k];
}

/* (v0.34, v0.39) numer we flashu <-> w pamięci: pusty slot zapisany jako 18 (jak do v0.33, starsze wersje czytają
 * dalej), rodzaje od AUTO WAH o jeden dalej (AUTO WAH 19, SUB 20; starsza wersja: łańcuch z nieznanym rodzajem
 * niepoprawny = domyślny). out: pamięć -> flash, inaczej flash -> pamięć */
_Static_assert(FX_AWAH == FX_NONE_ST && FX_NONE == FX_NTYPE, "efekty: numery we flashu");
static void fx_io(fx_slot_t *c, uint32_t n, int out)
{
    uint32_t k, t;
    for (k = 0; k < n; k++) {
        t = c[k].type;
        if (out)
            c[k].type = (uint8_t)(t == FX_NONE ? FX_NONE_ST : t >= FX_NONE_ST && t < FX_NTYPE ? t + 1u : t);
        else
            c[k].type = (uint8_t)(t == FX_NONE_ST ? FX_NONE : t > FX_NONE_ST && t <= FX_NTYPE ? t - 1u : t);
    }
}
_Static_assert(NENG_MAX - NENG_V8 <= NENG_V8, "efekty: kopia na zapis");
static fx_slot_t fx_iob[NENG_V8][FX_SLOTS];   /* zapis bieżących: kopia w numerach flasha (przerwanie gra z fxc) */
static int fxo_wslots(int32_t w, uint32_t off, const fx_slot_t *c, uint32_t ne)   /* ne łańcuchów; 0: zapisane */
{
    fx_slot_t *b = &fx_iob[0][0];
    uint32_t k;
    for (k = 0; k < ne * FX_SLOTS; k++)
        b[k] = c[k];
    fx_io(b, ne * FX_SLOTS, 1);
    return fxo_write(w, off, b, ne * FX_SLOTS * sizeof(fx_slot_t));
}

/* DELAY zapisany przed v0.30 (TIME, FEEDBACK, TONE, MIX | TYPE MONO / PING-PONG / TAPE, LOW CUT, WIDTH, WOBBLE) -> MODE
 * + STYLE (MONO: DIGITAL SINGLE, PING-PONG: DIGITAL PING-PONG, TAPE: TAPE SINGLE), WOBBLE -> MOD, TONE -> HIGH CUT
 * (te same Hz), LOW CUT bez zmian, WIDTH znika */
static void fx_dl_old(fx_slot_t *c, uint8_t *x)
{
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++) {
        uint8_t *q = c[k].p, ty = q[4] > 2u ? 2u : q[4], tape = ty == 2u;
        if (c[k].type != FX_DELAY)
            continue;
        x[FX_X_DELAY] = (uint8_t)(tape ? q[7] : q[7] / 8u);
        x[FX_X_DELAY + 1] = q[5];
        x[FX_X_DELAY + 2] = (uint8_t)(38u + q[2] * 89u / 128u);
        x[FX_X_DELAY + 3] = 0;
        q[1] = (uint8_t)((q[1] * 105u + 55u) / 110u);   /* FEEDBACK: było do 105 %, jest do 110 % */
        q[2] = tape ? 25u : 0u;
        q[4] = tape ? 0u : 3u;               /* TAPE, DIGITAL */
        q[5] = ty == 1u ? 1u : 0u;
        q[6] = 0;
        q[7] = 64;
        if (c[k].mode)
            c[k].mode = (uint8_t)(q[4] + 1u);
    }
}

/* REVERB zapisany przed v0.34 (SIZE, DECAY, TONE, MIX | TYPE ROOM / PLATE / HALL, PREDELAY, LOW CUT, WIDTH) -> DECAY,
 * SIZE, PREDELAY, MIX | MODE (te same), COLOR NOW, ATTACK 0, DIFFUSION (jak dawna stała trybu); TONE -> HIGH CUT (te same
 * Hz), LOW CUT i WIDTH bez zmian, MOD DEPTH 0 (dawny nie modulował), BASS 1x */
static void fx_rv_old(fx_slot_t *c, uint8_t *x)
{
    static const uint8_t MODE[3] = {0, 2, 3}, DIFF[3] = {87, 119, 127};
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++) {
        uint8_t *q = c[k].p, ty = q[4] > 2u ? 2u : q[4], o0 = q[0];
        if (c[k].type != FX_REVERB)
            continue;
        x[FX_X_REVERB + 1] = 0;
        x[FX_X_REVERB + 2] = q[6];
        x[FX_X_REVERB + 3] = (uint8_t)((220122u + q[2] * 10963u + 6400u) / 12800u);   /* 1500 * 2^(t * 3.7) = 1000 * 2^(k * 4.32) */
        x[FX_X_REVERB + 5] = q[7];
        q[0] = q[1];
        q[1] = o0;
        q[2] = q[5];
        q[4] = MODE[ty];
        q[5] = 2;
        q[6] = 0;
        q[7] = DIFF[ty];
        if (c[k].mode)
            c[k].mode = (uint8_t)(q[4] + 1u);
    }
}

/* ogon (bit 7 w każdym bajcie = zapisany); 0: ogona nie ma (x bez zmian) */
static int fx_tread(int32_t w, uint32_t o, uint8_t *x, uint32_t n)
{
    uint8_t tb[FX_NXR];
    uint32_t i;
    int ok = fxo_read(w, o, tb, n);
    for (i = 0; i < n; i++)
        ok = ok && (tb[i] & 0x80u);
    for (i = 0; ok && i < n; i++)
        x[i] = (uint8_t)(tb[i] & 0x7Fu);
    return ok;
}

/* gałki ponad 8 jednego wpisu: pierwsze FX_NX0, ogon DELAY (od v0.30) i ogon REVERB (od v0.34, zawsze w OBJ_FXX, orv);
 * brak ogona = wpis sprzed tej wersji, łańcuch c przeliczony; 0: któregoś ogona nie było */
static int fx_xread(int32_t wx, uint32_t ox, int32_t wtl, uint32_t ot, uint32_t orv, fx_slot_t *c, uint8_t *x)
{
    int32_t i;
    int okd, okr;
    if (!fxo_read(wx, ox, x, FX_NX0))
        fx_xall(x);
    for (i = FX_NX0; i < FX_NX; i++)
        x[i] = FXXDEF[i];
    if (!(okd = fx_tread(wtl, ot, x + FX_X_DELAY, FX_NXT)))
        fx_dl_old(c, x);
    if (!(okr = fx_tread(FO_X, orv, x + FX_X_REVERB, FX_NXR)))
        fx_rv_old(c, x);
    fx_tread(FO_X, FX_AWO(orv), x + FX_X_AWAH, FX_NXA);   /* (v0.55) brak: domyślne (FOLLOW = jak dotąd) */
    fx_xfix(x);
    return okd && okr;
}
static int fx_xwrite(int32_t w, uint32_t ox, uint32_t ot, uint32_t orv, const uint8_t *x, int32_t ne)   /* ne wpisów po FX_NX;
                                                                                                       * 0: zapisane */
{
    uint8_t x0[NENG_V8 * FX_NX0], xt[NENG_V8 * FX_NXT], xr[NENG_V8 * FX_NXR], xa[NENG_V8 * FX_NXA];
    int32_t e, i;
    for (e = 0; e < ne; e++)
        for (i = 0; i < FX_X_EQ; i++) {         /* (v0.92: ogon EQ w dodatkach presetu, eq_pack) */
            if (i < FX_NX0)
                x0[e * FX_NX0 + i] = x[e * FX_NX + i];
            else if (i < FX_X_REVERB)
                xt[e * FX_NXT + i - FX_NX0] = (uint8_t)(x[e * FX_NX + i] | 0x80u);
            else if (i < FX_X_AWAH)
                xr[e * FX_NXR + i - FX_X_REVERB] = (uint8_t)(x[e * FX_NX + i] | 0x80u);
            else
                xa[e * FX_NXA + i - FX_X_AWAH] = (uint8_t)(x[e * FX_NX + i] | 0x80u);
        }
    return fxo_write(FO_X, FX_AWO(orv), xa, (uint32_t)ne * FX_NXA) != 0 || fxo_write(FO_X, orv, xr, (uint32_t)ne * FX_NXR) != 0
                   || fxo_write(w, ot, xt, (uint32_t)ne * FX_NXT) != 0 || fxo_write(w, ox, x0, (uint32_t)ne * FX_NX0) != 0 ? -1 : 0;
}

static uint32_t fnv(const uint8_t *b, uint32_t n)   /* odcisk części: zapis tylko, gdy się zmieniła */
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

static uint32_t fx_saved[4];               /* odciski bieżących we flashu: silniki 1-8 (fxp, fxx), 9-16 (fxh: dwie) */

static void fx_hash(uint32_t *h)
{
    h[0] = fnv((const uint8_t *)fxc.cur[0], NENG_V8 * sizeof fxc.cur[0]);
    h[1] = fnv(fxc.x[0], NENG_V8 * sizeof fxc.x[0]);
    h[2] = fnv((const uint8_t *)fxc.cur[NENG_V8], (NENG_MAX - NENG_V8) * sizeof fxc.cur[0]);
    h[3] = fnv(fxc.x[NENG_V8], (NENG_MAX - NENG_V8) * sizeof fxc.x[0]);
}

/* starsze zapisy efektów (OFX1 - OFX6, do v0.15) -> bieżący układ: całość w pamięci tła HOME (bg_a; tło odbudowuje się
 * samo, bg_for), przeliczenie jak dawniej i zapis w całości. Rzadko: raz po aktualizacji z bardzo starej wersji albo po
 * przywróceniu takiej kopii zapasowej. */
_Static_assert(sizeof bg_a >= sizeof(fxp_t) + sizeof(fxx_t), "efekty: przeliczenie w pamięci tła");
static void fx_migrate(void)
{
#define fxp (*(fxp_t *)(void *)bg_a)
#define fxx (*(fxx_t *)(void *)(bg_a + sizeof(fxp_t)))

    uint8_t *b = (uint8_t *)&fxp;
    /* (v0.34.2) wszystkie wpisy po kolei (cur, user, fact) przez wskaźnik na cały obiekt — indeks za pierwszym
     * wierszem tablicy cur[0] to w C niezdefiniowane zachowanie (GCC: -Waggressive-loop-optimizations) */
    fx_slot_t *fxp_all = (fx_slot_t *)(void *)(b + FXOFF(fxp_t, cur));
    uint8_t *fxx_all = (uint8_t *)&fxx + FXOFF(fxx_t, cur);
    int32_t e, na = plat_store_load(OBJ_FXP, b, FXP_A), nb = plat_store_load(OBJ_FXP2, b + FXP_A, sizeof fxp - FXP_A);
    const uint32_t nent = 8u + NUSER_LO + (uint32_t)FX_FACT_N, old6 = 4u + nent * 6u * sizeof(fx_slot_t);
    int mig = 0;
    if (na == (int)FXP_A && nb == (int)(old6 - FXP_A)
        && (fxp.magic == FXP_MAGIC_2 || fxp.magic == FXP_MAGIC_3 || fxp.magic == FXP_MAGIC_4)) {
        /* 6 slotów -> 8: te same wpisy (cur, user, fact), od końca, w miejscu; dwa nowe sloty puste */
        int32_t k;
        for (e = (int32_t)nent - 1; e >= 0; e--) {
            uint8_t *to = b + 4 + (uint32_t)e * FX_SLOTS * sizeof(fx_slot_t);
            const uint8_t *from = b + 4 + (uint32_t)e * 6u * sizeof(fx_slot_t);
            int32_t i;
            for (i = 6 * (int32_t)sizeof(fx_slot_t) - 1; i >= 0; i--)   /* od końca: to >= from */
                to[i] = from[i];
            for (k = 6; k < FX_SLOTS; k++) {
                fx_slot_t *q = (fx_slot_t *)(void *)to + k;
                q->type = FX_NONE_ST;
                q->mode = 0;
                for (i = 0; i < FX_NP; i++)
                    q->p[i] = 0;
            }
        }
        mig = 1;
    }
    if (na == (int)(4u + (8u + NUSER_LO + (uint32_t)FX_FACT_N) * 4u * sizeof(fx_slot_t)) && fxp.magic == FXP_MAGIC_1) {
        /* 4 sloty -> 6: te same wpisy w tej samej kolejności (cur, user, fact), od końca, w miejscu; EQ i COMPRESSOR
         * dopisane wyłączone */
        int32_t n = 8 + NUSER_LO + FX_FACT_N, k;
        for (e = n - 1; e >= 0; e--) {
            fx_slot_t *to = (fx_slot_t *)(void *)(b + 4 + (uint32_t)e * FX_SLOTS * sizeof(fx_slot_t));
            const uint8_t *from = b + 4 + (uint32_t)e * 4u * sizeof(fx_slot_t);
            uint8_t t[4 * sizeof(fx_slot_t)];
            uint32_t i;
            for (i = 0; i < sizeof t; i++)
                t[i] = from[i];
            for (i = 0; i < sizeof t; i++)
                ((uint8_t *)to)[i] = t[i];
            for (k = 4; k < FX_SLOTS; k++) {
                to[k].type = (uint8_t)k;
                to[k].mode = 0;
                for (i = 0; i < FX_NP; i++)
                    to[k].p[i] = FXDEF[k][i];
            }
        }
        fxp.magic = FXP_MAGIC_2;             /* dalej jak OFX2 (niżej: TYPE na MORE K1) */
        mig = 1;
    }
    if (fxp.magic == FXP_MAGIC_2 && (na == (int)FXP_A || na > 0)) {   /* OFX2 -> OFX3: gałki DIST i REVERB po nowemu */
        fx_slot_t *all = fxp_all;
        uint32_t i, nall = (uint32_t)(sizeof fxp - 4u) / sizeof(fx_slot_t);
        for (i = 0; i < nall; i++) {
            uint8_t *q = all[i].p, t4 = q[4], t5 = q[5], t6 = q[6], t7 = q[7];
            if (all[i].type == FX_DIST) {    /* było: LOW CUT, BIAS, TYPE */
                q[4] = t6;
                q[5] = t4;
                q[6] = t5;
            } else if (all[i].type == FX_REVERB) {   /* było: PREDELAY, LOW CUT, WIDTH, TYPE */
                q[4] = t7;
                q[5] = t4;
                q[6] = t5;
                q[7] = t6;
            } else if (all[i].type == FX_CRUSH) {    /* było: TONE, LEVEL na MORE K1, K2 */
                q[5] = t4;
                q[6] = t5;
                q[4] = 0;
            }
        }
        fxp.magic = FXP_MAGIC_3;
    }
    if (fxp.magic == FXP_MAGIC_3 && na > 0) {   /* OFX3 -> OFX4: wyłączony efekt z domyślnymi gałkami = pusty slot */
        fx_slot_t *all = fxp_all;
        uint32_t i, j, nall = (uint32_t)(sizeof fxp - 4u) / sizeof(fx_slot_t);
        for (i = 0; i < nall; i++) {
            int def = all[i].type < FX_NONE_ST && !all[i].mode;
            for (j = 0; def && j < FX_NP; j++)
                def = j == FX_TYPEK || all[i].p[j] == FXDEF[all[i].type][j];
            if (def) {
                all[i].type = FX_NONE_ST;
                for (j = 0; j < FX_NP; j++)
                    all[i].p[j] = 0;
            }
        }
        fxp.magic = FXP_MAGIC_4;
    }
    if (fxp.magic == FXP_MAGIC_4 && mig)     /* OFX4 (6 slotów, już rozszerzone) -> OFX5 */
        fxp.magic = FXP_MAGIC_5;
    if (fxp.magic == FXP_MAGIC_5 && (mig || (na == (int)FXP_A && nb == (int)(sizeof fxp - FXP_A)))) {
        /* OFX5 -> OFX6: dwa nowe rodzaje przed pustym slotem (FX_NONE było 16) */
        fx_slot_t *all = fxp_all;
        uint32_t i, nall = (uint32_t)(sizeof fxp - 4u) / sizeof(fx_slot_t);
        for (i = 0; i < nall; i++)
            if (all[i].type == 16u)
                all[i].type = FX_NONE_ST;
        fxp.magic = FXP_MAGIC_6;
    } else if (na != (int)FXP_A || nb != (int)(sizeof fxp - FXP_A) || (fxp.magic != FXP_MAGIC && fxp.magic != FXP_MAGIC_6)) {
        uint32_t i;
        for (i = 0; i < sizeof fxp; i++)
            b[i] = 0;
        fxp.magic = FXP_MAGIC;
    }
    if (fxp.magic == FXP_MAGIC_6) {          /* OFX6 -> OFX7: DISTORTION BIAS i LOW CUT zamienione, HIGH CUT wyłączony */
        fx_slot_t *all = fxp_all;
        uint32_t i, nall = (uint32_t)(sizeof fxp - 4u) / sizeof(fx_slot_t);
        for (i = 0; i < nall; i++)
            if (all[i].type == FX_DIST) {
                uint8_t t = all[i].p[5];
                all[i].p[5] = all[i].p[6];
                all[i].p[6] = t;
                all[i].p[7] = 127;
            }
        fxp.magic = FXP_MAGIC;
    }
    for (e = 0; e < (int32_t)sizeof fxx; e++)   /* ogon DELAY (v0.30): brak = przeliczenie przy odczycie */
        ((uint8_t *)&fxx)[e] = 0;
    if (plat_store_load(OBJ_FXX, &fxx, sizeof fxx) < (int)FXOFF(fxx_t, tcur) || fxx.magic != FXX_MAGIC) {
        /* bez gałek ponad 8 (do v0.13): domyślne, a COMPRESSOR na nowy układ (było MAKEUP p4, MIX p5; jest STYLE p4,
         * MAKEUP p5 z AUTO na 0, MIX p6, KNEE p7; włączony = CLEAN) */
        fx_slot_t *all = fxp_all;
        uint32_t i, nall = (uint32_t)(sizeof fxp - 4u) / sizeof(fx_slot_t);
        for (i = 0; i < nall; i++)
            if (all[i].type == FX_COMP) {
                uint8_t *q = all[i].p;
                q[6] = q[5];
                q[5] = (uint8_t)(1u + q[4] * 126u / 127u);
                q[4] = 0;
                q[7] = FXDEF[FX_COMP][7];
                if (all[i].mode)
                    all[i].mode = 1;
            }
        fxx.magic = FXX_MAGIC;
        for (i = 0; i < (uint32_t)FXX_NENT * FX_NX0; i++)
            fxx_all[i] = FXXDEF[i % FX_NX0];
    }
    for (e = 0; e < (int32_t)FXX_NENT * FX_NX0; e++)
        if (fxx_all[e] > 127u)
            fxx_all[e] = FXXDEF[e % FX_NX0];
    bg_for = -1;
    if (plat_store_save(OBJ_FXX, &fxx, sizeof fxx) == 0)
        fx_ok[FO_X] = 1;
    if (plat_store_save(OBJ_FXP, &fxp, FXP_A) == 0 && plat_store_save(OBJ_FXP2, b + FXP_A, sizeof fxp - FXP_A) == 0)
        fx_ok[FO_P] = 1;
#undef fxp
#undef fxx
}

static void fx_load(void)                /* bieżące efekty silników z flasha (brak: domyślne) */
{
    uint32_t la = 0, m = 0, old = 0;
    const uint8_t *a = plat_store_view(OBJ_FXP, &la);
    int32_t w, e;
    for (w = 0; w < NFO; w++)
        fx_ok[w] = (uint8_t)fxo_check(w);
    if (a && la >= 4u)
        m = (uint32_t)(a[0] | a[1] << 8 | a[2] << 16 | (uint32_t)a[3] << 24);
    if (m == FXP_MAGIC_1 || m == FXP_MAGIC_2 || m == FXP_MAGIC_3 || m == FXP_MAGIC_4 || m == FXP_MAGIC_5
        || m == FXP_MAGIC_6)
        fx_migrate();
    for (e = 0; e < NENG_MAX; e++) {
        int ok = e < NENG_V8 ? fxo_read(FO_P, FXOFF(fxp_t, cur) + (uint32_t)e * FX_SL, fxc.cur[e], FX_SL)
                             : fxo_read(FO_H, FXOFF(fxh_t, cur) + (uint32_t)(e - NENG_V8) * FX_SL, fxc.cur[e], FX_SL);
        fx_io(fxc.cur[e], FX_SLOTS, 0);
        if (!ok || !fx_valid(fxc.cur[e]))
            fx_default(fxc.cur[e]);
        fx_fix(fxc.cur[e]);
        ok = e < NENG_V8 ? fx_xread(FO_X, FXOFF(fxx_t, cur) + (uint32_t)e * FX_NX0, FO_X,
                                    FXOFF(fxx_t, tcur) + (uint32_t)e * FX_NXT, FXOFF(fxx_t, rcur) + (uint32_t)e * FX_NXR,
                                    fxc.cur[e], fxc.x[e])
                         : fx_xread(FO_H, FXOFF(fxh_t, xcur) + (uint32_t)(e - NENG_V8) * FX_NX0, FO_H,
                                    FXOFF(fxh_t, txcur) + (uint32_t)(e - NENG_V8) * FX_NXT,
                                    FXOFF(fxx_t, rhcur) + (uint32_t)(e - NENG_V8) * FX_NXR, fxc.cur[e], fxc.x[e]);
        if (!ok)
            old |= e < NENG_V8 ? 1u : 2u;
    }
    fx_hash(fx_saved);                       /* to, co jest we flashu (brak: domyślne, zapis dopiero po zmianie) */
    if (old & 1u) {                          /* przeliczone sprzed v0.30 / v0.34: zapis w nowym układzie (z ogonami) */
        fx_saved[0] ^= 1u;
        fx_saved[1] ^= 1u;
        fx_dirty = 1;
    }
    if (old & 2u) {
        fx_saved[2] ^= 1u;
        fx_saved[3] ^= 1u;
        fx_dirty = 1;
    }
}

static const fx_slot_t *fx_play(void) { return fxc.cur[set.eng]; }
static const uint8_t *fx_xplay(void) { return fxc.x[set.eng]; }

/* zapis do flasha wstrzymuje przerwania (też skan diod: mignięcie), więc tylko zmienione części */
static void fx_store(void)
{
    uint32_t h[4];
    fx_dirty = 0;
    fx_hash(h);
    if (h[1] != fx_saved[1]
        && fx_xwrite(FO_X, FXOFF(fxx_t, cur), FXOFF(fxx_t, tcur), FXOFF(fxx_t, rcur), fxc.x[0], NENG_V8) == 0)
        fx_saved[1] = h[1];                  /* gałki ponad 8 przed łańcuchem (ogon DELAY = nowy układ łańcucha) */
    if (h[0] != fx_saved[0] && fxo_wslots(FO_P, FXOFF(fxp_t, cur), fxc.cur[0], NENG_V8) == 0)
        fx_saved[0] = h[0];
    if (h[3] != fx_saved[3]
        && fx_xwrite(FO_H, FXOFF(fxh_t, xcur), FXOFF(fxh_t, txcur), FXOFF(fxx_t, rhcur), fxc.x[NENG_V8],
                     NENG_MAX - NENG_V8) == 0)
        fx_saved[3] = h[3];
    if (h[2] != fx_saved[2]
        && fxo_wslots(FO_H, FXOFF(fxh_t, cur), fxc.cur[NENG_V8], NENG_MAX - NENG_V8) == 0)
        fx_saved[2] = h[2];
}

/* efekty presetu we flashu: USER u (0..127) albo nadpisany fabryczny (fact: miejsce na liście u) */
static void fx_where(int32_t u, int fact, int32_t *w, uint32_t *oc, int32_t *wx, uint32_t *ox, uint32_t *ot, uint32_t *orv)
{
    *orv = fact ? FXOFF(fxx_t, rfact) + (uint32_t)u * FX_NXR
                : u < NUSER_LO ? FXOFF(fxx_t, ruser) + (uint32_t)u * FX_NXR : FXOFF(fxx_t, rhuser) + (uint32_t)(u - NUSER_LO) * FX_NXR;
    if (fact) {
        *w = FO_P, *oc = FXOFF(fxp_t, fact) + (uint32_t)u * FX_SL;
        *wx = FO_X, *ox = FXOFF(fxx_t, fact) + (uint32_t)u * FX_NX0, *ot = FXOFF(fxx_t, tfact) + (uint32_t)u * FX_NXT;
    } else if (u < NUSER_LO) {
        *w = FO_P, *oc = FXOFF(fxp_t, user) + (uint32_t)u * FX_SL;
        *wx = FO_X, *ox = FXOFF(fxx_t, user) + (uint32_t)u * FX_NX0, *ot = FXOFF(fxx_t, tuser) + (uint32_t)u * FX_NXT;
    } else {
        *w = *wx = FO_H;
        *oc = FXOFF(fxh_t, user) + (uint32_t)(u - NUSER_LO) * FX_SL;
        *ox = FXOFF(fxh_t, xuser) + (uint32_t)(u - NUSER_LO) * FX_NX0;
        *ot = FXOFF(fxh_t, txuser) + (uint32_t)(u - NUSER_LO) * FX_NXT;
    }
}
static int fx_get(int32_t u, int fact, fx_slot_t *c, uint8_t *x)   /* 1: są i poprawne (c, x); 0: c do wyrzucenia */
{
    int32_t w, wx;
    uint32_t oc, ox, ot, orv;
    fx_where(u, fact, &w, &oc, &wx, &ox, &ot, &orv);
    if (!fxo_read(w, oc, c, FX_SL))
        return 0;
    fx_io(c, FX_SLOTS, 0);
    if (!fx_valid(c))
        return 0;
    fx_xread(wx, ox, wx, ot, orv, c, x);
    return 1;
}
static int fx_put(int32_t u, int fact, const fx_slot_t *c, const uint8_t *x)   /* 0: zapisane */
{
    int32_t w, wx;
    uint32_t oc, ox, ot, orv;
    fx_where(u, fact, &w, &oc, &wx, &ox, &ot, &orv);
    return fx_xwrite(wx, ox, ot, orv, x, 1) != 0 || fxo_wslots(w, oc, c, 1) != 0 ? -1 : 0;   /* ogony przed łańcuchem */
}
/* -------------------------------------------------------------- dodatki presetu (v0.38) --- */
/* To, co należy do brzmienia, a leży poza silnikiem i poza efektami — jak efekty osobny obiekt (OBJ_XTRA, xt_t), żeby
 * rekord presetu i silniki zostały jak były: wpis XT_N bajtów na bieżące każdego silnika, USER 1-128 i nadpisane
 * fabryczne (miejsca na liście jak efekty, FX_FACT_N). Zero = domyślne; brak zapisu = wszystko domyślne.
 * Układ wpisu: [0] CHORD (v0.39: 0 OFF, 1.. rodzaj akordu), [1..6] nuty nauczonego akordu (półtony nad pierwszą,
 * 0 koniec), [7..15] wolne (na przyszłość: dopisywać tu, numery bajtów się nie zmieniają). */
#define XT_N 16
#define XT_MAGIC 0x3154584Fu                 /* "OXT1" */
typedef struct {
    uint32_t magic;
    uint8_t cur[NENG_MAX][XT_N];
    uint8_t user[NUSER][XT_N];
    uint8_t fact[FX_FACT_N][XT_N];
} xt_t;
_Static_assert(sizeof(xt_t) <= XD1_OBJ_MAX, "dodatki presetu: jeden obiekt, przed rekordem loadera (XD1_OBJ_MAX)");
static uint8_t xtc[NENG_MAX][XT_N];          /* bieżące każdego silnika (gra xtc[set.eng]) */
static const uint8_t *xt_play(void) { return xtc[eng_play < NENG ? eng_play : 0]; }
static uint32_t xt_sync(const uint8_t *x)    /* (v0.46) SYNC efektów z dodatków presetu */
{
    return ((uint32_t)x[XT_SYNC] | (uint32_t)x[XT_SYNC + 1] << 8 | (uint32_t)x[XT_SYNC + 2] << 16) & FX_SYNC_SET;
}
/* (v0.92) ogon EQ w zapisie: dodatki presetu [10..15] = 48 bitów (GAIN 3, FREQ 1, 3, 4 po 7, Q 1-4 po 5: krok 4), przełączniki
 * w p[7] slotu EQ (bity: LC SLOPE, HC SLOPE, LOW SHAPE, HIGH SHAPE, ANALYZER OFF). Zapisane jako różnica od domyślnych (XOR): zera
 * (stare presety, brak zapisu) = domyślne. W RAM (fxc.x) rozpakowane: gałki jak wszystkie inne. */
#define XT_EQ 10
static uint8_t xt_dirty;                     /* (deklaracja wstępna; niżej przy zapisie dodatków) */
static const uint8_t EQ_BITS[8] = {7, 7, 7, 7, 5, 5, 5, 5};
static fx_slot_t *eq_slot(int32_t e)
{
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++)
        if (fxc.cur[e][k].type == FX_EQ)
            return &fxc.cur[e][k];
    return 0;
}
static void eq_pack(int32_t e)
{
    static const uint8_t EQD[FX_NXE] = {64, 33, 89, 105, 29, 38, 38, 29, 0, 0, 0, 0, 0};
    fx_slot_t *s = eq_slot(e);
    const uint8_t *x = fxc.x[e] + FX_X_EQ;
    uint64_t b = 0;
    uint32_t i, at = 0, f = 0;
    if (!s)
        return;
    for (i = 0; i < 8u; i++) {
        const uint32_t v = (uint32_t)(x[i] ^ EQD[i]) >> (EQ_BITS[i] == 5u ? 2u : 0u);
        b |= (uint64_t)(v & ((1u << EQ_BITS[i]) - 1u)) << at;
        at += EQ_BITS[i];
    }
    for (i = 0; i < 6u; i++)
        xtc[e][XT_EQ + i] = (uint8_t)(b >> (8u * i));
    for (i = 0; i < 5u; i++)
        f |= (x[EQX_LCS + i] ? 1u : 0u) << i;
    s->p[7] = (uint8_t)f;
    xt_dirty = 1;
}
static void eq_unpack(int32_t e)
{
    static const uint8_t EQD[FX_NXE] = {64, 33, 89, 105, 29, 38, 38, 29, 0, 0, 0, 0, 0};
    const fx_slot_t *s = eq_slot(e);
    uint8_t *x = fxc.x[e] + FX_X_EQ;
    uint64_t b = 0;
    uint32_t i, at = 0, f = s ? s->p[7] : 0u;
    for (i = 0; i < 6u; i++)
        b |= (uint64_t)xtc[e][XT_EQ + i] << (8u * i);
    for (i = 0; i < 8u; i++) {
        const uint32_t v = (uint32_t)(b >> at) & ((1u << EQ_BITS[i]) - 1u);
        x[i] = (uint8_t)(EQ_BITS[i] == 5u ? (v << 2) ^ EQD[i] : v ^ EQD[i]);
        at += EQ_BITS[i];
    }
    for (i = 0; i < 5u; i++)
        x[EQX_LCS + i] = (uint8_t)(f >> i & 1u);
}

/* (v0.76) wysyłki MULTI: źródło gałek wspólnego REVERB / DELAY = wybrana ścieżka, gdy jej preset ma ten efekt, inaczej
 * pierwsza (T1..T4), która go ma; cele ścieżek dla mt_mix (przerwanie audio, raz na blok) */
static const fx_slot_t *mt_ssrc[2];
static const uint8_t *mt_ssx[2];
static uint32_t mt_swant;                    /* wysyłane efekty (bity FX_*: pamięć wysyłek) */
static uint8_t mt_stest[2];                  /* pomiar (SysEx 0x20): każda ścieżka wysyła tyle (MIX), gałki domyślne */
static mfx_t mt_tm;                          /* (v0.85) gałki pomiaru */
static void mfx_def(mfx_t *m)                /* MASTER FX domyślnie: REVERB HALL, DELAY TAPE, oba przypięte, MIX 0, DELAY > REVERB */
{
    uint32_t k, j;
    for (k = 0; k < 2u; k++) {
        const uint32_t ty = k ? FX_DELAY : FX_REVERB;
        fx_slot_t *c = &m->s[k];
        c->type = (uint8_t)ty;
        for (j = 0; j < FX_NP; j++)
            c->p[j] = FXDEF[ty][j];
        c->mode = k ? 1u : 4u;
        c->p[fx_typep(ty)] = (uint8_t)(c->mode - 1u);
        c->p[3] = 0u;
        fx_xdef(m->x, ty);
    }
    m->rev1st = 0;
    m->sync = 0;
}
static void mt_tdef(void)
{
    mfx_def(&mt_tm);
    mt_tm.s[0].p[3] = mt_tm.s[1].p[3] = 127u;
}
static const fx_slot_t *mt_fxe(uint32_t e, uint32_t k)   /* REVERB (k 0) / DELAY (k 1) włączony w łańcuchu presetu silnika e */
{
    const uint32_t type = k ? FX_DELAY : FX_REVERB;
    const fx_slot_t *c;
    uint32_t j;
    if (e >= NENG)
        return 0;
    c = fxc.cur[e];
    for (j = 0; j < FX_SLOTS; j++)
        if (c[j].type == type && c[j].mode)
            return &c[j];
    return 0;
}
/* (v0.88) zapis sprzed v0.88: MASTER FX OFF = odpięty od wszystkich; RETURN 100 bez żadnej wysyłki (nic nie było słychać): MIX 0; ktoś
 * wysyłał: RETURN zostaje MIX, a ścieżki, które nie wysyłały, dostają wysyłkę 0 (dziś pusta wysyłka = cała) — brzmi jak było */
static void mfx_upgrade(void)
{
    uint32_t b, k;
    int32_t t;
    if ((uint32_t)glob.g[G_MFXOFF] & 256u)
        return;
    glob.g[G_MFXOFF] = 256;
    glob_dirty = 1;
    for (b = 0; b <= SCN_N; b++) {
        mfx_t *m = b ? &glob.scm[b - 1u] : &glob.mfx;
        for (k = 0; k < 2u; k++) {
            fx_slot_t *c = &m->s[k];
            const int32_t tp = fx_typep(c->type);
            uint32_t any = 0;
            for (t = 0; t < MT_N; t++) {
                const uint32_t v = (b ? (uint32_t)glob.scs[b - 1u][t] : (uint32_t)glob.g[G_MTS0 + t]) >> (k * 7u) & 127u;
                any |= (uint32_t)(v > 1u) | (uint32_t)(!b && !v && mt_fxe((uint32_t)glob.g[G_MT0 + t], k) != 0);
            }
            if (!c->mode) {
                c->mode = (uint8_t)(tp >= 0 ? c->p[tp] + 1u : 1u);
                for (t = 0; t < MT_N; t++)
                    if (b)
                        glob.scs[b - 1u][t] |= (uint16_t)(1u << (14u + k));
                    else
                        glob.g[G_MFXOFF] |= (int16_t)(1 << (k * 4u + (uint32_t)t));
            }
            if (!any) {
                if (c->p[3] == 127u)
                    c->p[3] = 0;
                continue;
            }
            if (!c->p[3])                        /* (zapis sprzed v0.85: bez MASTER FX, tu już domyślne) */
                c->p[3] = 127u;
            for (t = 0; t < MT_N; t++) {         /* (nie wysyłała: zostaje bez wysyłki) */
                const uint32_t v = (b ? (uint32_t)glob.scs[b - 1u][t] : (uint32_t)glob.g[G_MTS0 + t]) >> (k * 7u) & 127u;
                if (v || (!b && mt_fxe((uint32_t)glob.g[G_MT0 + t], k)))
                    continue;
                if (b)
                    glob.scs[b - 1u][t] |= (uint16_t)(1u << (k * 7u));
                else
                    glob.g[G_MTS0 + t] |= (int16_t)(1 << (k * 7u));
            }
        }
    }
}
/* (v0.82, decyzja użytkownika 2026-10-30) wysyłka ścieżki: pole GLO albo, póki gałka K3 / K4 na HOME nie ruszona, MIX efektu
 * w presecie (nowy preset ścieżki znowu od MIX); osobno od presetu (nie zapala SAVE), zapis w scenie */
static uint32_t mt_sf(int32_t t, uint32_t k) { return (uint32_t)glob.g[G_MTS0 + t] >> (k * 7u) & 127u; }
static int mt_linked(int32_t t, uint32_t k) { return !((uint32_t)glob.g[G_MFXOFF] >> (k * 4u + (uint32_t)t) & 1u); }   /* (v0.88) */
static float mt_send(int32_t t, uint32_t k)
{
    const uint32_t v = mt_sf(t, k);
    const fx_slot_t *c;
    if (mt_stest[k])
        return (float)mt_stest[k] * (1.0f / 127.0f);
    if (mt_eng[t] >= NENG || !mt_linked(t, k) || !glob.mfx.s[k].p[3])   /* (v0.88) odpięty, MIX 0: nie wysyła */
        return 0.0f;
    if (v)
        return (float)(v - 1u) * 0.01f;
    c = mt_fxe(mt_eng[t], k);
    return c ? (float)c->p[3] * (1.0f / 127.0f) : 1.0f;
}
static void mt_send_pick(void)
{
    int32_t t;
    uint32_t k;
    mt_swant = 0;
    for (t = 0; t < MT_N; t++) {
        float d = 1.0f;
        for (k = 0; k < 2u; k++) {
            const float s = mt_eng[t] < NENG ? mt_send(t, k) : 0.0f;
            mt_st[t][k] = 0.0f;
            if (s <= 0.0f)
                continue;
            mt_swant |= 1u << (k ? FX_DELAY : FX_REVERB);
            if (ogs_fx_send_on(k)) {             /* (efekt bez pamięci: ścieżka sucha jak w v0.75) */
                mt_st[t][k] = s;
                d *= ogs_fx_send_dry(k, mt_stest[k] ? s : s * (float)glob.mfx.s[k].p[3] * (1.0f / 127.0f));   /* (v0.88) za MIX */
            }
        }
        mt_dt[t] = d;
        if (!ogs_fx_bus(0)) {                    /* bez szyn: od razu bez wysyłek */
            mt_sv[t][0] = mt_sv[t][1] = 0.0f;
            mt_dv[t] = 1.0f;
        }
    }
    for (k = 0; k < 2u; k++) {
        const mfx_t *m = mt_stest[k] ? &mt_tm : &glob.mfx;
        mt_ssrc[k] = mt_swant >> (k ? FX_DELAY : FX_REVERB) & 1u ? &m->s[k] : 0;   /* nikt nie wysyła: ogon wybrzmiewa, efekt staje */
        mt_ssx[k] = m->x;
    }
}
static void mt_sends(float *l, float *r, uint32_t n)   /* przerwanie audio, za mt_mix */
{
    const uint32_t keep = ogs_fx_sync;
    ogs_fx_sync = (keep & ~(1u << FX_DELAY)) | (!mt_stest[1] && glob.mfx.sync ? 1u << FX_DELAY : 0u);   /* (v0.85) DELAY SYNC: master */
    ogs_fx_sends(mt_ssrc, mt_ssx, mt_swant, glob.mfx.rev1st, l, r, n);
    ogs_fx_sync = keep;
}
#define MT_FX_MAX 24u                        /* (v0.83) punkty na efekty ścieżek razem */
#define MT_FX_BASE 3u                        /* punkty, gdy gra choć jeden */
static uint32_t mt_fxc(uint32_t type)        /* punkty efektu (pomiar FM1: multi.md „Efekty na ścieżkę”) */
{
    return type == FX_DIST ? 4u : type == FX_COMP ? 4u : type == FX_EQ ? 5u : type == FX_SIDECH ? 1u : 3u;
}
static void mt_fx_pick(void)                 /* przerwanie audio, raz na blok */
{
    uint32_t t, j, left = MT_FX_MAX, ft = 0;
    for (t = 0; t < MT_N; t++) {
        mt_fxa[t] = 0;
        if (mt_ftest[t]) {                   /* pomiar: punkty jak prawdziwe (limit nut), bez górnej granicy */
            ft += (mt_ftest[t] & 1u ? mt_fxc(FX_DIST) : 0u) + (mt_ftest[t] & 2u ? mt_fxc(FX_FILTER) : 0u)
                + (mt_ftest[t] & 4u ? mt_fxc(FX_COMP) : 0u) + (mt_ftest[t] & 8u ? mt_fxc(FX_EQ) : 0u);
        } else if (mt_eng[t] < NENG && !(glob.g[G_MTMUTE] >> t & 1)) {
            const fx_slot_t *c = fxc.cur[mt_eng[t]];
            for (j = 0; j < FX_SLOTS; j++)
                if (c[j].mode && c[j].type < FX_NTYPE && (FX_TRACK_TYPES >> c[j].type & 1u) && mt_fxc(c[j].type) <= left) {
                    left -= mt_fxc(c[j].type);
                    mt_fxa[t] |= 1u << c[j].type;
                }
        }
    }
    mt_fxp = MT_FX_MAX - left + ft;
    if (mt_fxp)
        mt_fxp += MT_FX_BASE;                /* (pomiar FM1: faza efektów na dwóch rdzeniach, 88 + MINI + FM6 + VCO: bez tego 94 %) */
}
static int mt_fx_live(int32_t t) { return mt_ftest[t] || mt_fxa[t] || ogs_fx_track_busy((uint32_t)t); }
static void mt_track_fx(int32_t t, uint32_t core, uint32_t n)
{
    if (mt_ftest[t]) {                       /* pomiar: gałki domyślne, EQ nie płaski, bez budżetu */
        static const uint8_t TY[4] = {FX_DIST, FX_FILTER, FX_COMP, FX_EQ};
        fx_slot_t ch[FX_SLOTS];
        uint8_t x[FX_NX];
        uint32_t k, j;
        for (k = 0; k < FX_SLOTS; k++) {
            const uint32_t ty = k < 4u && (mt_ftest[t] >> k & 1u) ? TY[k] : FX_NONE;
            ch[k].type = (uint8_t)ty;
            ch[k].mode = ty == FX_NONE ? 0u : 1u;
            for (j = 0; j < FX_NP; j++)
                ch[k].p[j] = ty == FX_NONE ? 0u : FXDEF[ty][j];
            if (ty == FX_EQ)
                ch[k].p[0] = ch[k].p[1] = ch[k].p[3] = 76u;
        }
        for (j = 0; j < FX_NX; j++)
            x[j] = 0u;
        fx_xdef(x, FX_COMP);
        ogs_fx_track((uint32_t)t, ch, x, FX_TRACK_TYPES, 0u, core, n);
        return;
    }
    ogs_fx_track((uint32_t)t, fxc.cur[mt_eng[t]], fxc.x[mt_eng[t]], mt_fxa[t],
                 (xt_sync(xtc[mt_eng[t]]) >> FX_FILTER & 1u) | (xt_sync(xtc[mt_eng[t]]) >> FX_SIDECH & 1u) << 1, core, n);
}
static uint32_t mt_fxj_m, mt_fxj_n;          /* zadanie drugiego rdzenia: ścieżki, próbki */
static void mt_fx_tracks(uint32_t m, uint32_t core, uint32_t n)
{
    int32_t t;
    for (t = 0; t < MT_N; t++)
        if (m >> t & 1u)
            mt_track_fx(t, core, n);
}
static void mt_fx_job(uint32_t arg)
{
    (void)arg;
    mt_fx_tracks(mt_fxj_m, 1u, mt_fxj_n);
}
static uint32_t mt_fx_pts(int32_t t)         /* punkty ścieżki (podział na rdzenie) */
{
    uint32_t k, c = 0;
    static const uint8_t TY[4] = {FX_DIST, FX_FILTER, FX_COMP, FX_EQ};
    for (k = 0; k < 4u; k++)
        if ((mt_ftest[t] >> k & 1u) || (mt_fxa[t] >> TY[k] & 1u))
            c += mt_fxc(TY[k]);
    return c + 1u;                           /* (przygasające: nie zero) */
}
static void mt_fx_run(uint32_t dm, uint32_t n)   /* przerwanie audio: efekty ścieżek dm na buforach; dwa rdzenie, po równo punktami */
{
    uint32_t a = 0, b = 0, ma = 0, mb = 0;
    int32_t t;
    for (t = 0; t < MT_N; t++)
        if (dm >> t & 1u) {
            const uint32_t p = mt_fx_pts(t);
            if (a <= b) {
                a += p;
                ma |= 1u << t;
            } else {
                b += p;
                mb |= 1u << t;
            }
        }
    ogs_fx_track_begin();
    ogs_fx_sends_sync();                     /* (kopie wejścia: yl / yr; zadanie wysyłek ma drugi rdzeń) */
    mt_fxj_m = mb;
    mt_fxj_n = n;
    if (mb && plat_cpu2_on() && plat_cpu2_run(mt_fx_job, 0u)) {
        mt_fx_tracks(ma, 0u, n);
        if (plat_cpu2_wait() < 0)
            mt_fx_tracks(mb, 0u, n);
    } else {
        mt_fx_tracks(ma | mb, 0u, n);
    }
}
static uint8_t xt_dirty, xt_ok;              /* do zapisu (cisza, 5 s); zapis we flashu jest (znacznik) */
static uint32_t xt_saved;
static void xt_zero(uint8_t *d)
{
    int32_t i;
    for (i = 0; i < XT_N; i++)
        d[i] = 0;
}
static uint32_t xt_off(int32_t u, int fact)   /* wpis USER u (0..127) albo nadpisanego fabrycznego (miejsce u) */
{
    return (fact ? FXOFF(xt_t, fact) : FXOFF(xt_t, user)) + (uint32_t)u * XT_N;
}
static const uint8_t *xt_view(uint32_t *n)   /* zapis w bieżącym układzie, w miejscu we flashu; 0: brak */
{
    const uint8_t *v = plat_store_view(OBJ_XTRA, n);
    if (!v || *n < 4u || *n > sizeof(xt_t)
        || (uint32_t)(v[0] | v[1] << 8 | v[2] << 16 | (uint32_t)v[3] << 24) != XT_MAGIC)
        return 0;
    return v;
}
static int xt_read(uint32_t off, uint8_t *d)   /* wpis z flasha -> d; 0: brak (d = domyślne) */
{
    uint32_t n = 0, i;
    const uint8_t *v = xt_view(&n);
    xt_zero(d);
    if (!v || off + XT_N > n)
        return 0;
    for (i = 0; i < XT_N; i++)
        d[i] = v[off + i];
    return 1;
}
static int xt_create(void)               /* pusty zapis: sam znacznik, reszta zerami (= domyślne) */
{
    uint32_t m = XT_MAGIC;
    if (plat_store_patch(OBJ_XTRA, sizeof(xt_t), 0, &m, 4) != 0)
        return -1;
    xt_ok = 1;
    return 0;
}
static int xt_write(uint32_t off, const uint8_t *src, uint32_t n)   /* 0: zapisane */
{
    if (!xt_ok && xt_create() != 0)
        return -1;
    return plat_store_patch(OBJ_XTRA, sizeof(xt_t), off, src, n);
}
static void xt_load(void)                /* bieżące z flasha (brak: domyślne) */
{
    uint32_t n = 0, e;
    xt_ok = xt_view(&n) != 0;
    for (e = 0; e < NENG_MAX; e++)
        xt_read(FXOFF(xt_t, cur) + e * XT_N, xtc[e]);
    xt_saved = fnv(&xtc[0][0], sizeof xtc);
}
static void xt_store(void)               /* bieżące do flasha, gdy się zmieniły */
{
    uint32_t h = fnv(&xtc[0][0], sizeof xtc);
    xt_dirty = 0;
    if (h != xt_saved && xt_write(FXOFF(xt_t, cur), &xtc[0][0], sizeof xtc) == 0)
        xt_saved = h;
}

static struct {
    uint32_t magic, used;                /* banki DX7: zajęte */
    char name[NSYX][NAME_LEN];
    uint32_t cz_used;                    /* banki CZ (v0.17.1; starszy zapis: bez tych pól) */
    char cz_name[NCZB][NAME_LEN];
} syx;
#define SYX_SIZE_OLD (8u + NSYX * NAME_LEN)
#define SYX_MAGIC 0x4D4A5358u            /* "MJSX" */

/* okno ENGINE (v0.35): silniki wg rodzaju — VA (MINI, SEM, VCO, DCO), DIGI (FM6, PHASE, WAVE), DRUM; w grupie po kolei
 * powstania oryginałów. Tylko kolejność w oknie (i w ALL PRESETS, v0.36): numery silników w zapisach (presety,
 * ustawienia) bez zmian. Nowy silnik: dopisać w obu tablicach. */
enum { EG_VA, EG_DIGI, EG_DRUM, NEG };
static const char *const EG_NAME[NEG] = {"VA ENGINE", "DIGI ENGINE", "DRUM ENGINE"};
static const uint8_t ENG_ORDER[NENG] = {ENG_MOOG, ENG_SEM, ENG_VCO, ENG_DCO, ENG_ALPHA, ENG_WEST, ENG_DX7, ENG_PD, ENG_WT,
                                        ENG_BIT, ENG_PHYS, ENG_FORMANT, ENG_88, ENG_99, ENG_MORPH, ENG_DUAL};   /* (v0.78: PHYS, v0.84: FORMANT na końcu DIGI) */   /* (v0.49: ALPHA za DCO — ten sam kod, rok 1985; v0.50:
                                                                                 * DRUM; v0.52: 88 (1980), 99 (1983) przed MORPH;
                                                                                 * v0.54: DUAL (2019) za MORPH (2003)) */
static const uint8_t ENG_GRP[NENG] = {EG_VA, EG_DIGI, EG_DIGI, EG_DIGI, EG_VA, EG_VA, EG_VA, EG_DIGI, EG_VA, EG_VA, EG_DRUM, EG_DRUM, EG_DRUM, EG_DRUM, EG_DIGI, EG_DIGI};   /* wg
                                                                                                                  * numeru silnika */

static int32_t eng_at(int32_t eng)       /* miejsce silnika w oknie ENGINE */
{
    int32_t i;
    for (i = 0; i < NENG && ENG_ORDER[i] != eng; i++)
        ;
    return i < NENG ? i : 0;
}

static int32_t eng_first(int32_t e)
{
    return e == ENG_MOOG ? 0 : e == ENG_DX7 ? NPRESETS : e == ENG_WT ? NPRESETS + NDXP : e == ENG_PD ? NFACT_V13
         : e == ENG_VCO ? NFACT_V15 : e == ENG_SEM ? NFACT_V22 : e == ENG_DCO ? NFACT_V23 : e == ENG_BIT ? NFACT_V27
         : e == ENG_WEST ? NFACT_V42 : e == ENG_ALPHA ? NFACT_V44 : e == ENG_88 ? NFACT_V51 : e == ENG_99 ? NFACT_V51 + ND8K
         : e == ENG_DUAL ? NFACT_V52 : e == ENG_PHYS ? NFACT_V54 : e == ENG_FORMANT ? NFACT_V78 : NFACT_V49;   /* (ENG_MORPH) */
}
static int wide(const engine_t *e, int32_t k) { return e->def(k)->lo < -128 || e->def(k)->hi > 127; }

static int32_t obj_bank(uint32_t o)      /* indeks maski: bank USER 0..7, nadpisane fabryczne NBANK */
{
    int32_t b;
    for (b = 0; b < NBANK; b++)
        if (UOBJ[b] == o)
            return b;
    return NBANK;
}
static uint32_t bank_size(uint32_t o) { return 8u + (o == OBJ_FACT ? (uint32_t)FACT_N : BANK_N) * REC_SIZE; }
static uint32_t rec_obj(int32_t id) { return id < NFACT ? OBJ_FACT : UOBJ[(id - ID_USER) / BANK_N]; }
static int32_t rec_slot(int32_t id) { return id < NFACT ? fact_at[id] : (id - ID_USER) % BANK_N; }
static int rec_used(int32_t id)
{
    if (id >= ID_SYX)
        return 0;
    if (id < NFACT)
        return fact_at[id] >= 0;
    return (int)(used[obj_bank(rec_obj(id))] >> rec_slot(id) & 1u);
}
static uint8_t *rec(int32_t slot) { return bk.d + (uint32_t)slot * REC_SIZE; }
static int rec_name(int32_t id, char *t) /* nazwa zapisanego, w miejscu we flashu (v0.26: bez kopii w RAM); 0: bez nazwy */
{
    uint32_t n = 0, o;
    int32_t s, c;
    const uint8_t *v, *r;
    if (!rec_used(id) || (s = rec_slot(id)) < 0)
        return 0;
    o = rec_obj(id);
    if (!(v = plat_store_view(o, &n)) || n < bank_size(o))
        return 0;
    r = v + 8u + (uint32_t)s * REC_SIZE;
    for (c = 0; c < NAME_LEN && r[c] >= ' ' && r[c] < 127; c++)
        t[c] = (char)r[c];
    t[c] = 0;
    return c > 0;
}

static int32_t preset_eng(int32_t id)    /* silnik presetu: fabryczny wg miejsca, USER wg zapisu, banki: DX7 */
{
    return id >= ID_XF ? XF[id - ID_XF].eng : id >= ID_FM1 ? ENG_DX7 : id >= ID_CZ ? ENG_PD : id >= ID_SYX ? ENG_DX7 : id >= ID_USER ? reng[id - ID_USER] : id >= NFACT_V78 ? ENG_FORMANT : id >= NFACT_V54 ? ENG_PHYS : id >= NFACT_V52 ? ENG_DUAL : id >= NFACT_V51 + ND8K ? ENG_99 : id >= NFACT_V51 ? ENG_88 : id >= NFACT_V49 ? ENG_MORPH : id >= NFACT_V44 ? ENG_ALPHA : id >= NFACT_V42 ? ENG_WEST : id >= NFACT_V27 ? ENG_BIT : id >= NFACT_V23 ? ENG_DCO
         : id >= NFACT_V22 ? ENG_SEM
         : id >= NFACT_V15 ? ENG_VCO : id >= NFACT_V13 ? ENG_PD : id >= NPRESETS + NDXP ? ENG_WT : id >= NPRESETS ? ENG_DX7
         : ENG_MOOG;
}

static const uint8_t *syx_voice(int32_t id)   /* brzmienie z banku: 128 B w obszarze danych */
{
    return plat_data(SYX_BASE + (uint32_t)(id - ID_SYX) / 32u * 0x1000u + (uint32_t)(id - ID_SYX) % 32u * 128u);
}

static const uint8_t *cz_voice(int32_t id)    /* brzmienie z banku CZ: 128 B */
{
    uint32_t b = (uint32_t)(id - ID_CZ) / 16u;
    return plat_data(CZ_BASE + b / 2u * 0x1000u + (b & 1u) * 0x800u + (uint32_t)(id - ID_CZ) % 16u * 128u);
}

/* (v0.49) zapis DCO z MODEL ALPHA (v0.39 - v0.48) jest presetem silnika ALPHA (te same parametry, ten sam dźwięk); przy
 * następnym SAVE zapisze się już jako ALPHA. Nadpisany fabryczny DCO zostaje w DCO (MODEL ALPHA -> 106). */
static int rec_alpha(const uint8_t *r)
{
    const uint8_t *q = r + REC_HDR;
    int32_t k;
    if (r[NAME_LEN] != ENG_DCO || r[NAME_LEN + 2] <= J_MODEL)
        return 0;
    for (k = 0; k < J_MODEL; k++)
        q += wide(&ENG[ENG_DCO], k) ? 2 : 1;
    return (int8_t)q[0] == 2;
}

static void bank_read(uint32_t o)        /* obiekt -> bk (pusty, gdy brak albo inny układ) + maska, silniki */
{
    int32_t b = obj_bank(o), n = b == NBANK ? FACT_N : BANK_N, i, pass;
    if (plat_store_load(o, &bk, bank_size(o)) != (int)bank_size(o) || bk.magic != BANK_MAGIC) {
        bk.magic = BANK_MAGIC;
        bk.used = 0;
    }
    if (b == NBANK)
        for (i = 0; i < NFACT; i++)
            fact_at[i] = -1;
    for (pass = 0; pass < 2; pass++)             /* (v0.90) drugi przebieg: stare nadpisane usuniętych zestawów (na wolne miejsca) */
    for (i = 0; i < n; i++) {
        const uint8_t *r = rec(i);
        int32_t id;
        if (!(bk.used >> i & 1u) || r[NAME_LEN] >= NENG) {   /* nieznany silnik: jak pusty */
            bk.used &= ~(1u << i);
            continue;
        }
        if (b == NBANK) {                    /* nadpisany fabryczny: silnik + numer */
            const int32_t g = kit90_g(r[NAME_LEN]);
            int32_t x = r[NAME_LEN + 1];
            const int old = g >= 0 && !(x & KIT90_NEW);   /* (v0.90) zestaw perkusji w starej numeracji */
            if (pass != (old && x < NKIT_V89 && !(KIT90_KEEP[g] >> x & 1u)))
                continue;
            if (old && x < NKIT_V89)
                x = KIT90[g][x];
            x &= ~(int32_t)KIT90_NEW;
            if (x >= ENG[r[NAME_LEN]].nfact || fact_at[eng_first(r[NAME_LEN]) + x] >= 0) {   /* (zajęty: usunięty na zostawionym) */
                bk.used &= ~(1u << i);
                continue;
            }
            id = eng_first(r[NAME_LEN]) + x;
            fact_at[id] = (int8_t)i;
        } else if (!pass) {
            id = ID_USER + b * BANK_N + i;
            reng[id - ID_USER] = rec_alpha(r) ? ENG_ALPHA : r[NAME_LEN];   /* (v0.49) */
        }
    }
    used[b] = bk.used;
}

static void rec_write(uint8_t *r, const char *name, int32_t eng, int32_t idx, int32_t np, const uint8_t *pk, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < REC_SIZE; k++)
        r[k] = 0;
    for (k = 0; k < NAME_LEN && name[k]; k++)
        r[k] = (uint8_t)name[k];
    r[NAME_LEN] = (uint8_t)eng;
    r[NAME_LEN + 1] = (uint8_t)(idx | (idx != 0xFF && kit90_g(eng) >= 0 ? (int32_t)KIT90_NEW : 0));   /* (v0.90) nowa numeracja zestawów */
    r[NAME_LEN + 2] = (uint8_t)np;
    for (k = 0; k < n && k < REC_SIZE - REC_HDR; k++)
        r[REC_HDR + k] = pk[k];
}

/* starsze banki: v0.4 (sam MOOG, 32 rekordy po 71 B w USER0 / USER1, 9 nadpisanych fabrycznych na miejscach 0-8),
 * v0.5 (rekord 176 B: nazwa, silnik, parametry; banki po 16; nadpisane fabryczne na miejscach numerów: 0-8 MOOG,
 * 9-17 DX7). Przenosimy raz, w kolejności bezpiecznej przy zaniku zasilania: najpierw nowe obiekty, potem nadpisywane. */
#define OLD4_MAGIC (0x4D430000u | 56u)
#define OLD4_REC 71u
#define OLD5_MAGIC (0x4D440000u | 176u)
#define OLD5_REC 176u
/* v0.6 - v0.23: rekord 200 B (przed DRAW Q/X v0.24, które zjadły resztę zapasu). Sam układ nagłówka (nazwa, silnik,
 * numer, liczba parametrów) się nie zmienił — tylko miejsce na parametry urosło — więc kopiujemy rekord po
 * rekordzie bez przeliczania; nadpisanych fabrycznych mogło być więcej (19) niż się teraz mieści (16): nadmiar gubimy. */
#define OLD6_MAGIC (0x4D450000u | 200u)
#define OLD6_REC 200u
#define OLD6_FACT_N (int32_t)((PLAT_STORE_MAX - 8u) / OLD6_REC)
typedef struct {
    uint32_t magic, used;
    uint8_t d[PLAT_STORE_MAX - 8u];
} old_t;
/* przy starcie, zanim cokolwiek się rysuje, pamięć tła HOME (bg_a) służy przenoszeniu starych banków (old0 / old1) */
_Static_assert(sizeof bg_a >= 2 * sizeof(old_t), "migracja: dwa bufory w pamięci tła");
#define old0 (*(old_t *)(void *)bg_a)
#define old1 (*(old_t *)(void *)(bg_a + sizeof(old_t)))

static int old_load(uint32_t o, void *b, uint32_t magic)
{
    return plat_store_load(o, b, sizeof(old_t)) > 8 && ((uint32_t *)b)[0] == magic;
}

/* rekordy from..from+n starego banku -> obiekt o; fact: nadpisane fabryczne (lista) */
static void old_write(uint32_t o, const void *src, uint32_t rs, int32_t from, int32_t n, int fact)
{
    const struct { uint32_t magic, used; uint8_t d[PLAT_STORE_MAX - 8u]; } *s = src;
    int32_t i, at = 0;
    bk.magic = BANK_MAGIC;
    bk.used = 0;
    for (i = 0; i < n; i++) {
        const uint8_t *q;
        int32_t eng = ENG_MOOG, idx = from + i;
        char nm[NAME_LEN + 1];
        if (!(s && (s->used >> (from + i) & 1u)))
            continue;
        q = s->d + (uint32_t)(from + i) * rs;
        if (rs == OLD5_REC) {
            eng = q[NAME_LEN] < NENG ? q[NAME_LEN] : ENG_MOOG;
            if (fact && idx >= 9) {          /* v0.5: 9 presetów MOOGA, potem DX7 */
                eng = ENG_DX7;
                idx -= 9;
            }
        }
        for (at = 0; at < NAME_LEN; at++)
            nm[at] = (char)q[at];
        nm[NAME_LEN] = 0;
        at = fact ? (int32_t)__builtin_popcount(bk.used) : i;
        if (at >= (fact ? FACT_N : BANK_N))
            break;
        rec_write(rec(at), nm, eng, fact ? idx : 0xFF, eng == ENG_MOOG ? 56 : 163, q + NAME_LEN + (rs == OLD5_REC),
                  rs - NAME_LEN - (rs == OLD5_REC));
        bk.used |= 1u << at;
    }
    plat_store_save(o, &bk, bank_size(o));
}

/* v0.6 - v0.23 -> v0.24: sam układ nagłówka rekordu bez zmian, tylko miejsce na parametry urosło (200 -> REC_SIZE);
 * kopiujemy rekord po rekordzie na te same miejsca (fact: nadmiar ponad nową pojemność gubimy, reszta parametrów
 * nowego rekordu zostaje zerem — jak przy silniku, który urósł). */
static void old6_write(uint32_t o, const void *src, int32_t n, int fact)
{
    const struct { uint32_t magic, used; uint8_t d[PLAT_STORE_MAX - 8u]; } *s = src;
    int32_t i, cap = fact ? FACT_N : BANK_N;
    bk.magic = BANK_MAGIC;
    bk.used = 0;
    for (i = 0; i < n && i < cap; i++) {
        const uint8_t *q = s->d + (uint32_t)i * OLD6_REC;
        uint8_t *r = rec(i);
        uint32_t k;
        if (!(s->used >> i & 1u))
            continue;
        for (k = 0; k < REC_SIZE; k++)
            r[k] = k < OLD6_REC ? q[k] : 0;
        bk.used |= 1u << i;
    }
    plat_store_save(o, &bk, bank_size(o));
}

static void migrate(void)
{
    int a = old_load(OBJ_USER0, &old0, OLD4_MAGIC), b = old_load(OBJ_USER1, &old1, OLD4_MAGIC);
    int32_t i;
    if (b) {                                 /* v0.4 */
        old_write(OBJ_USER2, &old1, OLD4_REC, 0, BANK_N, 0);
        old_write(OBJ_USER3, &old1, OLD4_REC, BANK_N, BANK_N, 0);
    }
    if (a || b)
        old_write(OBJ_USER1, a ? &old0 : 0, OLD4_REC, BANK_N, BANK_N, 0);
    if (a)
        old_write(OBJ_USER0, &old0, OLD4_REC, 0, BANK_N, 0);
    if (old_load(OBJ_FACT, &old0, OLD4_MAGIC))
        old_write(OBJ_FACT, &old0, OLD4_REC, 0, 9, 1);
    for (i = 0; i < NBANK; i++)              /* v0.5: te same miejsca, nowy rekord */
        if (old_load(UOBJ[i], &old0, OLD5_MAGIC))
            old_write(UOBJ[i], &old0, OLD5_REC, 0, BANK_N, 0);
    if (old_load(OBJ_FACT, &old0, OLD5_MAGIC))
        old_write(OBJ_FACT, &old0, OLD5_REC, 0, 18, 1);
    for (i = 0; i < NBANK; i++)              /* v0.24: rekord 200 -> 232 B (DRAW Q/X), te same miejsca */
        if (old_load(UOBJ[i], &old0, OLD6_MAGIC))
            old6_write(UOBJ[i], &old0, BANK_N, 0);
    if (old_load(OBJ_FACT, &old0, OLD6_MAGIC))
        old6_write(OBJ_FACT, &old0, OLD6_FACT_N, 1);
}

/* SAVE świeci tylko, gdy brzmienie naprawdę różni się od presetu: kopia parametrów z chwili wczytania / zapisu.
 * Bez kopii (po starcie ze zmienionym brzmieniem, pojedyncze brzmienie DX7 z SysEx) zmiana zostaje do zapisu. */
/* (v0.42) silniki po kolei, każdy tyle miejsc, ile ma parametrów (było NENG x 256: pula RAM się kończyła); kolejność
 * jak ENG[] — nowy silnik: dopisać jego liczbę parametrów do SNAP_N */
#define SNAP_N (NP + NX + NW + NZ + NV + NS + NJ + NBT + NWS + NJ + NMPP + 2 * NDMP + NDUP + NPH + NFOR)   /* (NJ: ALPHA v0.49, NMPP: MORPH v0.50, NDUP: DUAL v0.54,
                                                                                    * NDMP: 88 / 99 v0.52) */
static int16_t snap[SNAP_N];                 /* (v0.69.1: z puli do RAM danych — pula miała 44 B ponad 8 KB zapasu CI) */
static int16_t *snap_of(int32_t e)       /* miejsce silnika e; 0: się nie mieści (wtedy SAVE świeci zawsze) */
{
    int32_t i, o = 0;
    for (i = 0; i < e; i++)
        o += ENG[i].np;
    return o + ENG[e].np <= SNAP_N ? snap + o : 0;
}
static fx_slot_t snap_fx[NENG][FX_SLOTS];    /* efekty też: zmiana efektu to zmiana brzmienia */
static uint8_t snap_fxx[NENG][FX_NX];
static uint8_t snap_xt[NENG][XT_N];          /* dodatki presetu (v0.38) */
static uint8_t snap_ok[NENG];

static void snap_take(int32_t e)
{
    int32_t k;
    int16_t *sp = snap_of(e);
    if (!sp)
        return;                              /* (SAVE świeci zawsze: kopii nie ma) */
    for (k = 0; k < ENG[e].np; k++)
        sp[k] = ENG[e].p[k];
    fx_copy(snap_fx[e], fxc.cur[e]);
    fx_xcopy(snap_fxx[e], fxc.x[e]);
    for (k = 0; k < XT_N; k++)
        snap_xt[e][k] = xtc[e][k];
    snap_ok[e] = 1;
}

static int snap_differs(int32_t e)
{
    int32_t k;
    const int16_t *sp = snap_of(e);
    if (!snap_ok[e] || !sp)
        return 1;
    for (k = 0; k < ENG[e].np; k++)
        if (sp[k] != ENG[e].p[k])
            return 1;
    {
        const uint8_t *a = (const uint8_t *)snap_fx[e], *b = (const uint8_t *)fxc.cur[e];
        for (k = 0; k < (int32_t)sizeof snap_fx[e]; k++)
            if (a[k] != b[k])
                return 1;
        for (k = 0; k < FX_NX; k++)
            if (snap_fxx[e][k] != fxc.x[e][k])
                return 1;
        for (k = 0; k < XT_N; k++)
            if (snap_xt[e][k] != xtc[e][k])
                return 1;
    }
    return 0;
}


/* (v0.64) MULTI — strona UI (główna pętla). Ustawienia w GLO; przerwanie stosuje je po przygaszeniu (mt_ver). */
static int mt_ui(void) { return glob.g[G_MULTI] != 0; }
static int32_t mt_first(void)                /* pierwsza niepusta ścieżka (-1: żadna) */
{
    int32_t t;
    for (t = 0; t < MT_N; t++)
        if (glob.g[G_MT0 + t] < NENG)
            return t;
    return -1;
}
/* (v0.85) ścieżka pod SELECT na HOME / w oknie ENGINE: pusta nie może być wybrana (set.eng = silnik wybranej), więc kursor osobno;
 * na niepustej zawsze ta sama co wybrana */
static uint8_t mt_cur;
static int32_t mt_at_cur(void) { return glob.g[G_MT0 + mt_cur] >= NENG ? mt_cur : glob.g[G_MTFOC]; }
static void mt_fix(void)                     /* GLO: ścieżki poprawne (silniki, powtórzenia), wybrana niepusta */
{
    int32_t t;
    for (t = 0; t < MT_N; t++)
        if (!mt_ok(t, glob.g[G_MT0 + t], glob.g + G_MT0))
            glob.g[G_MT0 + t] = MT_NONE;
    if (glob.g[G_MT0 + glob.g[G_MTFOC]] >= NENG && mt_first() >= 0)
        glob.g[G_MTFOC] = (int16_t)mt_first();
    if (mt_first() < 0)
        glob.g[G_MULTI] = 0;
}
static void mt_boot(void)
{
    if (!glob.g[G_MULTI])
        return;
    mt_fix();
    if (glob.g[G_MULTI]) {
        set.eng = (uint8_t)glob.g[G_MT0 + glob.g[G_MTFOC]];
        mt_cur = (uint8_t)glob.g[G_MTFOC];
        mt_ver++;
    }
    glob_apply();                            /* (kolor MULTI) */
}
static void mt_focus_go(int32_t t)           /* wybrana ścieżka: jej silnik na stronach, klawiszach, SEQ */
{
    int32_t e = glob.g[G_MT0 + t];
    if (e >= NENG)
        return;
    mt_cur = (uint8_t)t;
    if (glob.g[G_MTFOC] != t) {
        glob.g[G_MTFOC] = (int16_t)t;
        mt_gdirty = 1;
    }
    if (set.eng != e) {
        set.eng = (uint8_t)e;
        set_dirty = 1;
    }
}
static void engine_switch(int32_t e)
{
    if (e == set.eng)
        return;
    if (mt_ui()) {                           /* (v0.64) MULTI: tylko silniki ścieżek (wybór ścieżki) */
        int32_t t;
        for (t = 0; t < MT_N; t++)
            if (glob.g[G_MT0 + t] == e)
                mt_focus_go(t);
        return;
    }
    ev_push(EV_ALL_OFF);
    set.eng = (uint8_t)e;                    /* przerwanie audio wymienia wspólny RAM: stary cichnie, nowy od zera */
    set_dirty = 1;
}

/* (v0.53) brzmienia FM1 w bankach DX7 (decyzja użytkownika 2026-10-29: z 8 banków DX7 na start 4 = FM1, do podmiany):
 * oryginalny bank FM1 (skrót FNV-1a 4096 B, ogs_fm1_bank.h) w miejscu DX7 gra jako sekcja FM1 jak dotąd (te same numery
 * ID_FM1, sekcje PIANO / ORGAN ..., poprawione nazwy); tego miejsca nie ma drugi raz wśród wgranych banków. Brzmień nie
 * ma już w firmware — wgrywa je OG SYNTH MANAGER (app/web/fm1_banks.js) albo tools/fm1_banki.py do miejsc 1-4. */
static int8_t fm1_at[NFM1_BANK];         /* miejsce DX7 z bankiem FM1 b, -1: brak */
static uint8_t fm1_dirty = 1;            /* banki DX7 się zmieniły: policzyć od nowa */
static void fm1_scan(void)
{
    uint32_t h[NSYX_USE];
    int32_t b, s;
    for (s = 0; s < NSYX_USE; s++)
        h[s] = (syx.used >> s & 1u) ? fnv(plat_data(SYX_BASE + (uint32_t)s * 0x1000u), 4096u) : 0u;
    for (b = 0; b < NFM1_BANK; b++) {
        fm1_at[b] = -1;
        for (s = 0; s < NSYX_USE && fm1_at[b] < 0; s++)
            if ((syx.used >> s & 1u) && h[s] == FM1_BANK_FNV[b])
                fm1_at[b] = (int8_t)s;
    }
    fm1_dirty = 0;
}
static int32_t fm1_slot(int32_t b)
{
    if (fm1_dirty)
        fm1_scan();
    return fm1_at[b];
}
static int syx_is_fm1(int32_t s)              /* miejsce DX7 s trzyma oryginalny bank FM1 */
{
    int32_t b;
    for (b = 0; b < NFM1_BANK; b++)
        if (fm1_slot(b) == s)
            return 1;
    return 0;
}

static void rec_apply(const uint8_t *r)  /* parametry z rekordu do bieżącego silnika */
{
    int32_t k, n = r[NAME_LEN + 2];
    r += REC_HDR;
    for (k = 0; k < E->np && k < n; k++) {
        int32_t v = wide(E, k) ? (int16_t)(r[0] | r[1] << 8) : (int8_t)r[0];
        r += wide(E, k) ? 2 : 1;
        E->p[k] = (int16_t)clamp(v, E->def(k)->lo, E->def(k)->hi);
    }
}

/* (v1.1.0, P33) SEL trzymany + PRESETS: nowe brzmienie, ten sam łańcuch efektów (sloty, gałki ponad 8, SYNC, ogon EQ);
 * ustawia tylko preset_turn, zużywa load_preset (też po przygaszeniu: pend_preset) */
static uint8_t keep_fx;
static void load_preset(int32_t i)
{
    int32_t e = preset_eng(i);
    const int keep = keep_fx;
    fx_slot_t kc[FX_SLOTS];
    uint8_t kx[FX_NX], ks[3];
    keep_fx = 0;
    if (keep) {
        memcpy(kc, fxc.cur[set.eng], sizeof kc);
        memcpy(kx, fxc.x[set.eng], sizeof kx);
        memcpy(ks, xtc[set.eng] + XT_SYNC, sizeof ks);
    }
    engine_switch(e);
    if (i >= ID_XF) {
        E->load(XF[i - ID_XF].base);         /* (zmiany niżej, po CUR) */
    } else if (i >= ID_FM1) {                /* (v0.53) z banku DX7, w którym leży bank FM1; nie wgrany: INIT */
        const int32_t s = fm1_slot((i - ID_FM1) / 32);
        if (s >= 0)
            dx_unpack(plat_data(SYX_BASE + (uint32_t)s * 0x1000u + (uint32_t)((i - ID_FM1) % 32) * 128u));
        else
            E->load(0);
    } else if (i >= ID_CZ) {
        pd_cz(cz_voice(i));
    } else if (i >= ID_SYX) {
        dx_unpack(syx_voice(i));
    } else {
        E->load(i < NFACT ? i - eng_first(e) : 0);
        if (rec_used(i)) {
            bank_read(rec_obj(i));
            rec_apply(rec(rec_slot(i)));
        }
    }
    CUR = (uint16_t)i;
    if (i >= ID_XF) {                        /* (v0.39) fabryczny z efektami: brzmienie = base + zmiany */
        const xfpreset_t *x = &XF[i - ID_XF];
        int32_t k;
        for (k = 0; k + 1 < XF_NSET && x->set[k] >= 0; k += 2)
            E->p[x->set[k]] = (int16_t)clamp(x->set[k + 1], E->def(x->set[k])->lo, E->def(x->set[k])->hi);
    }
    if (!(i >= ID_USER && i < ID_SYX && fx_get(i - ID_USER, 0, fxc.cur[e], fxc.x[e]))   /* efekty presetu */
        && !(i < NFACT && fact_at[i] >= 0 && fx_get(fact_at[i], 1, fxc.cur[e], fxc.x[e]))) {   /* (fabryczne: domyślne) */
        fx_default(fxc.cur[e]);
        fx_xall(fxc.x[e]);
    }
    if (i >= ID_XF) {                        /* (v0.39) łańcuch fabrycznego z efektami */
        const xfpreset_t *x = &XF[i - ID_XF];
        int32_t k, j;
        for (k = 0; k < x->nfx && k < FX_SLOTS; k++) {
            fx_slot_t *c = &fxc.cur[e][k];
            c->type = x->fx[k].type;
            c->mode = x->fx[k].mode;
            for (j = 0; j < FX_NP; j++)
                c->p[j] = x->fx[k].p[j];
        }
    }
    fx_fix(fxc.cur[e]);
    fx_dirty = 1;
    if (!(i >= ID_USER && i < ID_SYX && xt_read(xt_off(i - ID_USER, 0), xtc[e]))   /* dodatki presetu (v0.38) */
        && !(i < NFACT && fact_at[i] >= 0 && xt_read(xt_off(fact_at[i], 1), xtc[e])))
        xt_zero(xtc[e]);                     /* (xt_read zeruje; tu: także INIT, banki, FM1) */
    if (i >= ID_XF)
        xtc[e][0] = XF[i - ID_XF].chord;
    eq_unpack(e);                            /* (v0.92) ogon EQ z dodatków presetu */
    xt_dirty = 1;
    set.edited[e] = 0;
    snap_take(e);
    if (keep) {                              /* (v1.1.0) łańcuch sprzed zmiany: brzmienie zmienione względem presetu (SAVE) */
        memcpy(fxc.cur[e], kc, sizeof kc);
        memcpy(fxc.x[e], kx, sizeof kx);
        memcpy(xtc[e] + XT_SYNC, ks, sizeof ks);
        eq_pack(e);
        set.edited[e] = (uint8_t)snap_differs(e);
    }
    set_dirty = 1;
    {                                        /* (v0.82) MULTI: nowy preset ścieżki: wysyłki znowu od jego MIX */
        int32_t t;
        for (t = 0; t < MT_N; t++)
            if (glob.g[G_MULTI] && glob.g[G_MT0 + t] == e && glob.g[G_MTS0 + t]) {
                glob.g[G_MTS0 + t] = 0;
                mt_gdirty = 1;
            }
    }
}

/* preset z panelu / MIDI: przy grającym dźwięku najpierw przygaszenie (bez trzasku), wczytuje app_frame */
static int32_t pend_preset = -1;
static void preset_go(int32_t i)
{
    if (mt_ui() && preset_eng(i) != set.eng) {   /* (v0.64) MULTI: brzmienie innego silnika = jego ścieżka */
        int32_t t;
        for (t = 0; t < MT_N && glob.g[G_MT0 + t] != preset_eng(i); t++)
            ;
        if (t == MT_N) {
            app_say("MULTI", "NOT ON A TRACK");
            return;
        }
        mt_focus_go(t);
    }
    if (!dk_quiet && eng_sounding() && preset_eng(i) == set.eng) {
        pend_preset = i;
        dk_quiet_req = 1;
        return;
    }
    pend_preset = -1;
    load_preset(i);
    ev_push(EV_ALL_OFF);                     /* (przy zerze: głosy gasną od razu) */
    dk_quiet_req = 0;                        /* dopiero teraz dźwięk może wrócić */
}
static int32_t preset_now(void) { return pend_preset >= 0 ? pend_preset : CUR; }   /* PRESETS kręci od tego */

static int is_init(int32_t id) { return id < NFACT && id == eng_first(preset_eng(id)); }

static void syx_bank_name(int32_t b, char *t)   /* nazwa banku albo BANK nn; b >= NSYX: bank CZ b - NSYX */
{
    int32_t c;
    const char *nm = b < NSYX ? syx.name[b] : syx.cz_name[b - NSYX];
    t[0] = 0;
    for (c = 0; c < NAME_LEN && nm[c] >= ' ' && nm[c] < 127; c++)
        t[c] = nm[c];
    t[c] = 0;
    if (b >= NSYX)
        b -= NSYX;
    if (!c) {
        text_cat(t, "BANK ");
        t[5] = (char)('0' + (b + 1) / 10);
        t[6] = (char)('0' + (b + 1) % 10);
        t[7] = 0;
    }
}

static void preset_name(int32_t id, char *t)
{
    t[0] = 0;
    if (id >= ID_XF) {                       /* (v0.39) fabryczne z efektami */
        text_cat(t, XF[id - ID_XF].name);
        return;
    }
    if (id >= ID_FM1) {                      /* bank FM1: nazwy poprawione (ogs_fm1_bank.h) */
        text_cat(t, FM1_NAME[id - ID_FM1]);
        return;
    }
    if (id >= ID_CZ) {                       /* CZ nie ma nazw: bank (do 9 znaków) i numer */
        char n[4];
        syx_bank_name(NSYX + (id - ID_CZ) / 16, t);
        t[9] = 0;
        n[0] = ' ';
        n[1] = (char)('0' + ((id - ID_CZ) % 16 + 1) / 10);
        n[2] = (char)('0' + ((id - ID_CZ) % 16 + 1) % 10);
        n[3] = 0;
        text_cat(t, n);
        return;
    }
    if (id >= ID_SYX) {                      /* nazwa z brzmienia DX7: 10 znaków */
        const uint8_t *v = syx_voice(id);
        int32_t c, n = 0;
        for (c = 0; c < 10; c++)
            t[c] = v[118 + c] >= ' ' && v[118 + c] < 127 ? (char)v[118 + c] : ' ';
        for (c = 0; c < 10; c++)
            if (t[c] != ' ')
                n = c + 1;
        t[n] = 0;
        return;
    }
    if (rec_name(id, t))
        return;
    if (id < NFACT) {
        int32_t e = preset_eng(id);
        text_cat(t, ENG[e].preset_name(id - eng_first(e)));
        return;
    }
    text_cat(t, "USER ");
    user_num(t + 5, id - ID_USER + 1);
}

static int listed(int32_t id, int32_t e)    /* na liście: fabryczny, zajęty USER, brzmienie banku; e >= 0: ten silnik */
{
    int ok;
    if (id < NFACT || id >= ID_XF)
        ok = 1;
    else if (id >= ID_FM1)                   /* (v0.53) FM1: gdy jego bank leży w którymś miejscu DX7 */
        ok = fm1_slot((id - ID_FM1) / 32) >= 0;
    else if (id >= ID_CZ)
        ok = (int)(syx.cz_used >> ((id - ID_CZ) / 16) & 1u);
    else if (id >= ID_SYX)                   /* (miejsce z bankiem FM1: w sekcji FM1, nie drugi raz) */
        ok = (int)(syx.used >> ((id - ID_SYX) / 32) & 1u) && !syx_is_fm1((id - ID_SYX) / 32);
    else
        ok = rec_used(id);
    return ok && (e < 0 || preset_eng(id) == e);
}

/* v0.36 (decyzja użytkownika 2026-10-28): lista PRESETS w każdym silniku = INIT, USER (zapisane), fabryczne OG Synth,
 * FM1 (FM6), wgrane banki DX7 / CZ; ALL PRESETS: silniki w kolejności okna ENGINE, w każdym te same sekcje (SELECT
 * skacze po sekcjach, w FM1 po jego sekcjach: PIANO, ORGAN ...). Numery presetów bez zmian, inna tylko kolejność. */
enum { PS_INIT, PS_USER, PS_FACT, PS_XF, PS_FM1, PS_SYX, PS_CZ, NPS };   /* PS_XF (v0.39): fabryczne z efektami */
/* v0.37 (decyzja użytkownika 2026-10-28): USER = jego, FACTORY = fabryczne OG Synth, IMPORTED = reszta (FM1, banki) */
static const char *const PS_NAME[NPS] = {"INIT", "USER", "FACTORY", "FACTORY", "IMPORTED", "IMPORTED", "IMPORTED"};
/* (v0.88) SIDECHAIN, TRIG KICK: uderzenie T1 (perkusja: pierwszy biały klawisz, inny silnik: każda nuta); -1: SOLO (nuta silnika) */
static void mt_kick(int32_t t, int32_t note)
{
    if (t <= 0 && (ENG_GRP[t < 0 ? eng_play : mt_eng[0]] != EG_DRUM || dm_lane_of(note) == 0))
        ogs_fx_kick_n++;
}
static void ps_range(int32_t e, int32_t s, int32_t *lo, int32_t *hi)   /* numery sekcji s silnika e: [lo, hi) */
{
    *lo = *hi = 0;
    if (s == PS_INIT) {
        *lo = eng_first(e);
        *hi = *lo + 1;
    } else if (s == PS_USER) {               /* wszystkie USER: silnik sprawdza listed */
        *lo = ID_USER;
        *hi = ID_SYX;
    } else if (s == PS_FACT) {
        *lo = eng_first(e) + 1;
        *hi = eng_first(e) + ENG[e].nfact;
    } else if (s == PS_XF) {                 /* wszystkie: silnik sprawdza listed */
        *lo = ID_XF;
        *hi = NALL;
    } else if (s == PS_FM1 && e == ENG_DX7) {
        *lo = ID_FM1;
        *hi = ID_FM1 + NFM1;
    } else if (s == PS_SYX && e == ENG_DX7) {
        *lo = ID_SYX;
        *hi = ID_CZ;
    } else if (s == PS_CZ && e == ENG_PD) {
        *lo = ID_CZ;
        *hi = ID_FM1;
    }
}
static int32_t ps_of(int32_t id)
{
    return id >= ID_XF ? PS_XF : id >= ID_FM1 ? PS_FM1 : id >= ID_CZ ? PS_CZ : id >= ID_SYX ? PS_SYX : id >= ID_USER ? PS_USER
         : id == eng_first(preset_eng(id)) ? PS_INIT : PS_FACT;
}
static int32_t fm1_sec(int32_t id)       /* sekcja banku FM1 (PIANO, ORGAN ...) */
{
    int32_t k = NFM1_SEC - 1;
    while (k > 0 && id - ID_FM1 < FM1_SEC[k].first)
        k--;
    return k;
}

static int32_t preset_walk(int32_t id, int32_t s, int32_t only)   /* o jeden w kolejności listy (s = +-1), w pętli;
                                                                     * only >= 0: tylko ten silnik */
{
    int32_t e = preset_eng(id), at = eng_at(e), sec = ps_of(id), lo, hi;
    if (only >= 0 && e != only)
        return eng_first(only);
    for (;;) {
        id += s;
        ps_range(e, sec, &lo, &hi);
        while (id < lo || id >= hi) {        /* koniec sekcji: następna (pusta też), za ostatnią następny silnik */
            sec += s;
            if (sec < 0 || sec >= NPS) {
                sec = s > 0 ? 0 : NPS - 1;
                if (only < 0) {
                    at = (at + s + NENG) % NENG;
                    e = ENG_ORDER[at];
                }
            }
            ps_range(e, sec, &lo, &hi);
            id = s > 0 ? lo : hi - 1;
        }
        if (listed(id, e))
            return id;
    }
}

static int32_t preset_step(int32_t id, int32_t d, int32_t e)   /* następny na liście, w pętli; e >= 0: ten silnik */
{
    int32_t n;
    for (n = d < 0 ? -d : d; n > 0; n--)
        id = preset_walk(id, d < 0 ? -1 : 1, e);
    return id;
}

static int32_t sec_key(int32_t id)       /* sekcja w ALL PRESETS: silnik, sekcja, w FM1 jego sekcja */
{
    int32_t s = ps_of(id);
    return (eng_at(preset_eng(id)) * NPS + (s == PS_CZ ? PS_SYX : s == PS_XF ? PS_FACT : s)) * 16
         + (s == PS_FM1 ? fm1_sec(id) : 0);   /* (FACTORY z efektami: ta sama sekcja co FACTORY) */
}

static int32_t sec_jump(int32_t id, int32_t s)   /* SELECT w ALL PRESETS: początek następnej / tej (poprzedniej) sekcji */
{
    int32_t k = sec_key(id), n, p;
    if (s > 0) {
        for (n = 0; n < NALL && sec_key(id) == k; n++)
            id = preset_walk(id, 1, -1);
        return id;
    }
    p = preset_walk(id, -1, -1);
    if (sec_key(p) != k) {                   /* już na początku: poprzednia sekcja */
        id = p;
        k = sec_key(id);
    }
    for (n = 0; n < NALL; n++) {
        p = preset_walk(id, -1, -1);
        if (sec_key(p) != k)
            break;
        id = p;
    }
    return id;
}

static void sec_label(int32_t id, char *t)   /* nazwa sekcji: USER, FACTORY, IMPORTED PIANO (FM1), IMPORTED ... */
{
    int32_t s = ps_of(id);
    t[0] = 0;
    text_cat(t, PS_NAME[s]);
    if (s == PS_FM1) {
        text_cat(t, " ");
        text_cat(t, FM1_SEC[fm1_sec(id)].name);
    }
}

static int32_t eng_jump(int32_t id, int32_t e)   /* ALGO w ALL PRESETS: INIT następnego / poprzedniego silnika (v0.37) */
{
    int32_t at = eng_at(preset_eng(id));
    return eng_first(ENG_ORDER[((at + e) % NENG + NENG) % NENG]);
}

static int16_t *set_at(int32_t e, int32_t k)   /* miejsce parametru k silnika e (1-8) w ustawieniach; WAVE ponad
                                                * WX_SLOT: pw[] */
{
    if (e == ENG_WT && k >= WX_SLOT)
        return &set.pw[k - WX_SLOT];
    return ENG_POFF[e] >= SET_P ? &set.px[ENG_POFF[e] - SET_P + k] : &set.p[ENG_POFF[e] + k];
}
static void sync_out(void)               /* silniki -> ustawienia (silniki 9-16: set2_build) */
{
    int32_t e, k;
    for (e = 0; e < NENG; e++) {
        set.np[e] = (uint16_t)ENG[e].np;
        for (k = 0; e < SET2_FIRST && k < ENG[e].np; k++)
            *set_at(e, k) = ENG[e].p[k];
    }
}
static uint32_t set2_build(void)         /* silniki 9-16 -> bk jako OBJ_SET2 (set2_t); 0: nie ma takich silników */
{
    set2_t *s = (set2_t *)(void *)&bk;
    int32_t e, k;
    if (NENG <= SET2_FIRST)
        return 0;
    for (k = 0; k < SET2_N; k++)
        s->q[k] = 0;
    s->magic = SET2_MAGIC;
    for (e = 0; e < NENG; e++)
        for (k = 0; e >= SET2_FIRST && k < ENG[e].np && k < SET2_SLOT; k++)
            s->q[(e - SET2_FIRST) * SET2_SLOT + k] = ENG[e].p[k];
    return sizeof *s;
}

static uint32_t set_saved, set2_saved;
static int8_t fxt_eng = -1;                  /* pomiar efektów (SysEx 0x15): silnik z testowym łańcuchem (-1: brak) */
static void settings_save(void)
{
    uint32_t h, n2;
    int ok;
    if (fx_dirty && fxt_eng < 0)             /* testowy łańcuch nigdy do flasha (zapis po powrocie) */
        fx_store();                          /* bieżące efekty silników razem z resztą (cisza, 5 s) */
    if (xt_dirty)
        xt_store();                          /* dodatki presetu (v0.38) tak samo */
    sync_out();
    h = fnv((const uint8_t *)&set, sizeof set);
    ok = h == set_saved || plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0;   /* bez zmian: bez zapisu */
    if (ok)
        set_saved = h;
    if ((n2 = set2_build()) != 0) {          /* silniki 9-16: osobny obiekt, tak samo (v0.38) */
        uint32_t h2 = fnv((const uint8_t *)&bk, n2);
        if (h2 == set2_saved || plat_store_save(OBJ_SET2, &bk, n2) == 0)
            set2_saved = h2;
        else
            ok = 0;
    }
    if (ok)
        set_dirty = 0;
}

static void settings_load(void)          /* ustawienia (albo starsze) -> silniki; co się nie zgadza: pierwszy preset */
{
    typedef union {                          /* (v0.38: czytane do pamięci tła HOME, bg_a — nie na stałe w RAM) */
        settings_t s;
        struct {                             /* v0.24 - v0.25 ("MJB8"): 8 miejsc na silniki */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P];
            int16_t pw[WX_N];
            int16_t px[SET_PX];
        } s24;
        struct {                             /* v0.22 - v0.23: pw[] 64 (przed DRAW Q/X v0.24), px jest */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P];
            int16_t pw[WX_N_V23];
            int16_t px[SET_PX];
        } s23;
        struct {                             /* v0.19 - v0.21: bez px (silniki od VCO) */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P];
            int16_t pw[WX_N_V23];
        } s19;
        struct {                             /* v0.17 - v0.18: bez pw (WAVE do 128 parametrów) */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P];
        } s17;
        struct {                             /* v0.6 - v0.14: te same pola, brzmienia bez miejsca na PHASE */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P_V13];
        } s13;
        struct {                             /* v0.15 - v0.16: PHASE bez 8 STEP (128 miejsc) */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2];
            uint16_t preset[NENG_V8];
            uint8_t edited[NENG_V8];
            uint16_t np[NENG_V8];
            int16_t p[SET_P_V15];
        } s15;
        struct {                             /* v0.5 */
            uint32_t magic;
            int8_t octave;
            uint8_t eng, rsv[2], preset[2], edited[2];
            int16_t p[56 + 163];
        } s7;
        struct {                             /* v0.4 */
            uint32_t magic;
            int8_t octave;
            uint8_t preset, edited, rsv;
            int16_t p[56];
        } s6;
    } set_old_t;
    _Static_assert(sizeof bg_a >= sizeof(set_old_t), "ustawienia: odczyt w pamięci tła");
#define o (*(set_old_t *)(void *)bg_a)
    int n = plat_store_load(OBJ_SETTINGS, &o, sizeof o);
    int n2 = plat_store_load(OBJ_SET2, &bk, sizeof bk);   /* silniki 9-16 (v0.38); brak: pierwszy preset */
    const set2_t *s2 = (const set2_t *)(const void *)&bk;
    int ok2 = n2 == (int)sizeof *s2 && s2->magic == SET2_MAGIC;
    uint32_t h2 = ok2 ? fnv((const uint8_t *)&bk, sizeof *s2) : 0u;   /* (v0.49) to, co jest we flashu (bk zaraz posłuży bankom) */
    int32_t e, k;
    uint16_t pr[NENG] = {0}, np[NENG] = {0};
    const int16_t *src[NENG] = {0};
    uint8_t ed[NENG] = {0};
    int al_mig = 0;                          /* (v0.49) bieżące DCO z MODEL ALPHA -> silnik ALPHA */
    set.magic = SET_MAGIC;
    set.rsv[0] = 18;                         /* numeracja presetów: z WAVE (v0.13: 1), z PHASE (v0.15: 2), USER 128 (3),
                                              * z VCO (v0.22: 4), z SEM (v0.23: 5), z DCO (v0.27: 6), z BIT (v0.42: 7),
                                              * z WEST (v0.44: 8), z ALPHA (v0.49: 9), z MORPH (v0.50: 10), 15 zestawów
                                              * MORPH (v0.51: 11), z 88 / 99 (v0.52: 12), z DUAL (v0.54: 13), nowe zestawy
                                              * 88 / 99 / DUAL (v0.56: 14), z PHYS (v0.78: 15), z FORMANT (v0.84: 16),
                                              * więcej zestawów (v0.88: 17), mniej zestawów (v0.90: 18) */
    set.octave = 0;
    set.eng = ENG_MOOG;
    int n9 = n == (int)sizeof o.s && o.s.magic == SET_MAGIC;
    if (n9 || ((n == (int)sizeof o.s24 || n == (int)sizeof o.s23 || n == (int)sizeof o.s19 || n == (int)sizeof o.s17
                || n == (int)sizeof o.s15 || n == (int)sizeof o.s13) && o.s24.magic == SET_MAGIC_8)) {
        /* v0.26 albo ("MJB8", 8 miejsc na silniki) v0.24 - v0.25, v0.22 - v0.23, v0.19 - v0.21, v0.17 - v0.18, v0.15 -
         * v0.16, v0.6 - v0.14 (krótsze: brzmienie, które się nie zmieściło (PHASE do v0.14, VCO do v0.21): np = 0 ->
         * pierwszy preset; zmieściło się krótsze (PHASE do v0.16, WAVE do v0.18): reszta domyślna) */
        int32_t np_max = n == (int)sizeof o.s15 ? SET_P_V15 : n == (int)sizeof o.s13 ? SET_P_V13
                       : (n9 || n == (int)sizeof o.s24 || n == (int)sizeof o.s23) ? SET_P + SET_PX : SET_P;   /* p[] (+ px[]) */
        int pw = n9 || n == (int)sizeof o.s24 || n == (int)sizeof o.s23 || n == (int)sizeof o.s19;   /* jest pw[] */
        int32_t ne = n9 ? NENG_MAX : NENG_V8;
        const uint16_t *opr = n9 ? o.s.preset : o.s24.preset, *onp = n9 ? o.s.np : o.s24.np;
        const uint8_t *oed = n9 ? o.s.edited : o.s24.edited, rsv = n9 ? o.s.rsv[0] : o.s24.rsv[0];
        const int16_t *op = n9 ? o.s.p : o.s24.p, *opw = n9 ? o.s.pw : o.s24.pw;   /* (pw[]: v0.19 - v0.23 tu samo) */
        const int16_t *opx = n9 ? o.s.px : n == (int)sizeof o.s23 ? o.s23.px : o.s24.px;
        static int16_t wsrc[WX_SLOT + WX_N];
        set.octave = n9 ? o.s.octave : o.s24.octave;
        set.eng = n9 ? o.s.eng : o.s24.eng;
        for (e = 0; e < NENG && e < ne; e++) {   /* (silnik bez miejsca w starym zapisie: pierwszy preset) */
            pr[e] = opr[e];
            ed[e] = oed[e];
            if (e >= SET2_FIRST && snap_of(e)) {   /* v0.38: silniki 9-16 z OBJ_SET2 (w bk; kopia w snap, bo bk zaraz
                                                  * posłuży bankom; snap_take niżej nadpisze tym samym) */
                np[e] = ok2 && onp[e] <= SET2_SLOT && onp[e] <= ENG[e].np ? onp[e] : 0;   /* (miejsce w snap) */
                for (k = 0; k < np[e]; k++)
                    snap_of(e)[k] = s2->q[(e - SET2_FIRST) * SET2_SLOT + k];
                src[e] = snap_of(e);
            } else {
                int32_t slot = ENG_POFF[e + 1] - ENG_POFF[e], cap = slot + (e == ENG_WT && pw ? WX_N : 0);
                np[e] = onp[e] <= cap && ENG_POFF[e] + (onp[e] < slot ? onp[e] : slot) <= np_max ? onp[e] : 0;
                src[e] = ENG_POFF[e] >= SET_P ? &opx[ENG_POFF[e] - SET_P] : &op[ENG_POFF[e]];
                if (e == ENG_WT && np[e] > WX_SLOT) {   /* WAVE: p[] i pw[] w jedną tablicę */
                    for (k = 0; k < np[e]; k++)
                        wsrc[k] = k < WX_SLOT ? op[ENG_POFF[e] + k] : opw[k - WX_SLOT];
                    src[e] = wsrc;
                }
            }
            if (pr[e] == 0xFFFFu)
                continue;
#pragma push_macro("NMPK")                       /* (v0.90) kroki niżej liczą układ v0.88-v0.89 (po 20 zestawów), na końcu kit90 */
#pragma push_macro("ND8K")
#pragma push_macro("ND9K")
#pragma push_macro("NDUK")
#undef NMPK
#undef ND8K
#undef ND9K
#undef NDUK
#define NMPK NKIT_V89
#define ND8K NKIT_V89
#define ND9K NKIT_V89
#define NDUK NKIT_V89
            if (rsv < 1u && pr[e] >= NFACT_OLD)  /* v0.13: presety WAVE przed USER */
                pr[e] = (uint16_t)(pr[e] + NWTP);
            if (rsv < 2u && pr[e] >= NFACT_V13)  /* v0.15: presety PHASE przed USER */
                pr[e] = (uint16_t)(pr[e] + NZP);
            if (rsv < 3u && pr[e] >= NFACT_V15 + NUSER_LO)   /* v0.16: banki DX7 za USER 128 */
                pr[e] = (uint16_t)(pr[e] + NUSER - NUSER_LO);
            if (rsv < 4u && pr[e] >= NFACT_V15)  /* v0.22: presety VCO przed USER */
                pr[e] = (uint16_t)(pr[e] + NVP);
            if (rsv < 5u && pr[e] >= NFACT_V22)  /* v0.23: presety SEM przed USER */
                pr[e] = (uint16_t)(pr[e] + NSP);
            if (rsv < 6u && pr[e] >= NFACT_V23)  /* v0.27: presety DCO przed USER */
                pr[e] = (uint16_t)(pr[e] + NJP);
            if (rsv < 7u && pr[e] >= NFACT_V27)  /* v0.42: presety BIT przed USER */
                pr[e] = (uint16_t)(pr[e] + NBTP);
            if (rsv < 8u && pr[e] >= NFACT_V42)  /* v0.44: presety WEST przed USER */
                pr[e] = (uint16_t)(pr[e] + NWSP);
            if (rsv < 9u && pr[e] >= NFACT_V44)  /* v0.49: presety ALPHA przed USER */
                pr[e] = (uint16_t)(pr[e] + NAP);
            if (rsv < 10u && pr[e] >= NFACT_V49) /* v0.50: zestawy MORPH przed USER */
                pr[e] = (uint16_t)(pr[e] + NMPK_V50);
            if (rsv < 11u && pr[e] >= NFACT_V49 + NMPK_V50)   /* v0.51: nowe zestawy MORPH przed USER */
                pr[e] = (uint16_t)(pr[e] + NMPK - NMPK_V50);
            else if (rsv < 17u && rsv >= 11u && pr[e] >= NFACT_V49 + NMPK_V87)   /* v0.88: nowe zestawy MORPH (rsv 11-16 mają ich 15) */
                pr[e] = (uint16_t)(pr[e] + NMPK - NMPK_V87);
            if (rsv < 12u && pr[e] >= NFACT_V51) /* v0.52: zestawy 88 i 99 przed USER */
                pr[e] = (uint16_t)(pr[e] + ND8K_V54 + ND9K_V54);
            if (rsv < 13u && pr[e] >= NFACT_V51 + ND8K_V54 + ND9K_V54)   /* v0.54: zestawy DUAL przed USER */
                pr[e] = (uint16_t)(pr[e] + NDUK_V54);
            if (rsv < 14u) {                     /* v0.56: nowe zestawy 88, 99, DUAL (każdy silnik za swoimi) */
                if (pr[e] >= NFACT_V51 + ND8K_V54)
                    pr[e] = (uint16_t)(pr[e] + ND8K - ND8K_V54);
                if (pr[e] >= NFACT_V51 + ND8K + ND9K_V54)
                    pr[e] = (uint16_t)(pr[e] + ND9K - ND9K_V54);
                if (pr[e] >= NFACT_V52 + NDUK_V54)
                    pr[e] = (uint16_t)(pr[e] + NDUK - NDUK_V54);
            }
            if (rsv >= 14u && rsv < 17u) {       /* v0.88: nowe zestawy 88, 99, DUAL (rsv 14-16: układ v0.56, kroki wyżej liczą już nowy) */
                if (pr[e] >= NFACT_V51 + ND8K_V87)
                    pr[e] = (uint16_t)(pr[e] + ND8K - ND8K_V87);
                if (pr[e] >= NFACT_V51 + ND8K + ND9K_V87)
                    pr[e] = (uint16_t)(pr[e] + ND9K - ND9K_V87);
                if (pr[e] >= NFACT_V52 + NDUK_V87)
                    pr[e] = (uint16_t)(pr[e] + NDUK - NDUK_V87);
            }
            if (rsv < 15u && pr[e] >= NFACT_V54) /* v0.78: presety PHYS przed USER */
                pr[e] = (uint16_t)(pr[e] + NPHP);
            if (rsv < 16u && pr[e] >= NFACT_V78) /* v0.84: presety FORMANT przed USER */
                pr[e] = (uint16_t)(pr[e] + NFOP);
#pragma pop_macro("NMPK")
#pragma pop_macro("ND8K")
#pragma pop_macro("ND9K")
#pragma pop_macro("NDUK")
            if (rsv < 18u)                       /* v0.90: mniej zestawów perkusji */
                pr[e] = (uint16_t)kit90(pr[e]);
        }
        al_mig = rsv < 9u && ENG_DCO < ne && src[ENG_DCO] && np[ENG_DCO] > J_MODEL && src[ENG_DCO][J_MODEL] == 2;
    } else if (n == (int)sizeof o.s7 && o.s7.magic == SET_MAGIC_7) {
        set.octave = o.s7.octave;
        set.eng = o.s7.eng;
        pr[ENG_MOOG] = o.s7.preset[0];       /* v0.5: te same numery (18 fabrycznych, potem USER) */
        pr[ENG_DX7] = o.s7.preset[1];
        for (e = 0; e < 2; e++)
            if (pr[e] >= NFACT_OLD)
                pr[e] = (uint16_t)(pr[e] + NWTP + NZP + NVP + NSP + NJP + NBTP + NWSP + NAP + NMPK + ND8K + ND9K + NDUK + NPHP + NFOP);
        ed[ENG_MOOG] = o.s7.edited[0];
        ed[ENG_DX7] = o.s7.edited[1];
        np[ENG_MOOG] = 56;
        np[ENG_DX7] = 163;
        src[ENG_MOOG] = o.s7.p;
        src[ENG_DX7] = o.s7.p + 56;
    } else if (n == (int)sizeof o.s6 && o.s6.magic == SET_MAGIC_6) {
        set.octave = o.s6.octave;
        pr[ENG_MOOG] = o.s6.preset < NPRESETS ? o.s6.preset : 0xFFFF;
        ed[ENG_MOOG] = o.s6.edited;
        np[ENG_MOOG] = 56;
        src[ENG_MOOG] = o.s6.p;
    }
    if (set.octave < -3 || set.octave > 3)
        set.octave = 0;
    if (set.eng >= NENG)
        set.eng = ENG_MOOG;
    if (al_mig) {                            /* (v0.49) efekty i dodatki bieżącego DCO idą z brzmieniem do ALPHA (zanim pętla
                                              * niżej da DCO jego preset); zapiszą się z najbliższym zapisem ustawień */
        fx_copy(fxc.cur[ENG_ALPHA], fxc.cur[ENG_DCO]);
        fx_xcopy(fxc.x[ENG_ALPHA], fxc.x[ENG_DCO]);
        for (k = 0; k < XT_N; k++)
            xtc[ENG_ALPHA][k] = xtc[ENG_DCO][k];
        fx_dirty = xt_dirty = 1;
    }
    for (e = NENG - 1; e >= 0; e--) {        /* każdy silnik po kolei jako bieżący; na końcu zapisany */
        uint8_t keep = set.eng;
        if (e == ENG_ALPHA && al_mig)
            continue;                        /* (niżej) */
        set.eng = (uint8_t)e;
        if (pr[e] < NALL && listed(pr[e], e) && src[e] && np[e] && np[e] <= ENG[e].np) {
            if ((pr[e] >= ID_SYX && pr[e] < ID_CZ) || (pr[e] >= ID_FM1 && pr[e] < ID_XF))
                dx_load(0);
            else if (pr[e] >= ID_XF)
                ENG[e].load(XF[pr[e] - ID_XF].base);
            else
                ENG[e].load(pr[e] < NFACT ? pr[e] - eng_first(e) : 0);   /* brakujące parametry: domyślne */
            for (k = 0; k < np[e]; k++)
                ENG[e].p[k] = (int16_t)clamp(src[e][k], ENG[e].def(k)->lo, ENG[e].def(k)->hi);
            set.preset[e] = pr[e];
            set.edited[e] = ed[e];
            if (!ed[e])
                snap_take(e);
        } else {
            load_preset(eng_first(e) + 1);
        }
        set.eng = keep;
    }
    if (al_mig) {                            /* (v0.49) brzmienie DCO z MODEL ALPHA (v0.39 - v0.48) gra teraz w ALPHA: te same
                                              * parametry; DCO zostaje z tym, co wczytała pętla wyżej (preset albo pierwszy) */
        int32_t dp = pr[ENG_DCO], own = dp < NALL && listed(dp, ENG_ALPHA);
        uint8_t keep = set.eng;
        set.eng = ENG_ALPHA;
        al_load(0);                          /* (parametry spoza zapisu: domyślne ALPHA) */
        for (k = 0; k < np[ENG_DCO]; k++)
            JA[k] = (int16_t)clamp(src[ENG_DCO][k], al_def(k)->lo, al_def(k)->hi);
        set.preset[ENG_ALPHA] = (uint16_t)(own ? dp : eng_first(ENG_ALPHA));
        set.edited[ENG_ALPHA] = own ? ed[ENG_DCO] : 1;
        if (!set.edited[ENG_ALPHA])
            snap_take(ENG_ALPHA);
        set.eng = keep == ENG_DCO ? ENG_ALPHA : keep;   /* grał DCO z ALPHA: dalej ten sam dźwięk */
    }
    set_dirty = 0;
    sync_out();
    set_saved = fnv((const uint8_t *)&set, sizeof set) ^ (al_mig ? 1u : 0u);   /* jak we flashu: bez zmian nie zapisujemy
                                              * (v0.49: po przeniesieniu do ALPHA — przy najbliższym zapisie wszystko) */
    if ((n2 = (int)set2_build()) != 0)       /* (brak albo inny zapis OBJ_SET2: pierwszy zapis go założy) */
        set2_saved = ok2 ? h2 : fnv((const uint8_t *)&bk, (uint32_t)n2) ^ 1u;   /* (v0.49) skrót tego, co we flashu: silnik
                                              * 9-16 bez zapisu (nowy, przeniesiony, pierwszy preset) zapisze się z resztą */
    bg_for = -1;                             /* pamięć tła posłużyła do czytania: tło HOME od nowa */
#undef o
}

static void store_load(void)             /* wszystko z pamięci: przy starcie i po przywróceniu kopii zapasowej */
{
    int32_t k, e, b, ok;
    for (b = 0; b < NBANK; b++)
        bank_read(UOBJ[b]);                  /* maski zajętych presetów */
    bank_read(OBJ_FACT);
    k = plat_store_load(OBJ_SYX, &syx, sizeof syx);
    if (k == (int)SYX_SIZE_OLD && syx.magic == SYX_MAGIC) {   /* do v0.17.0: bez banków CZ */
        syx.cz_used = 0;
        for (b = 0; b < NCZB; b++)
            syx.cz_name[b][0] = 0;
    } else if (k != (int)sizeof syx || syx.magic != SYX_MAGIC) {
        syx.magic = SYX_MAGIC;
        syx.used = syx.cz_used = 0;
    }
    syx.used &= (1u << NSYX_USE) - 1u;       /* v0.19: banki 9-32 to teraz tablice USER WAVE */
    syx.cz_used &= (1u << NCZB_USE) - 1u;    /* v0.53: banki CZ 1-8 */
    fm1_dirty = 1;
    k = plat_store_load(OBJ_GLOBAL, &glob, sizeof glob);
    ok = k >= 4 + 4 * 2 && glob.magic == GLOB_MAGIC;
    e = k == 4 + 4 * 2 ? 2 : (k - 4) / 2;    /* ile pól zapisano (v0.4: 4 pola o innym znaczeniu, ważne 2) */
    for (b = 0; b < NGLOB; b++) {
        if (!ok || b >= e || glob.g[b] < GDEF[b].lo || glob.g[b] > GDEF[b].hi)
            glob.g[b] = GDEF[b].def;
    }
    for (b = 0; b < SCN_N; b++) {            /* (v0.73) sceny: brak (starszy zapis) albo zły silnik = pusta */
        scn_t *c = &glob.scn[b];
        if (!ok || k < GLOB_V81 || c->ok != 1u || c->foc >= MT_N) {
            c->ok = 0;
        }
#pragma push_macro("NMPK")                       /* (v0.90) kroki niżej: układ v0.88-v0.89 (po 20 zestawów), na końcu kit90 */
#pragma push_macro("ND8K")
#pragma push_macro("ND9K")
#pragma push_macro("NDUK")
#undef NMPK
#undef ND8K
#undef ND9K
#undef NDUK
#define NMPK NKIT_V89
#define ND8K NKIT_V89
#define ND9K NKIT_V89
#define NDUK NKIT_V89
        if (c->ok && c->rsv < 3u) {              /* (v0.88) zestawy perkusji: numery przed PHYS w układzie sprzed v0.88 */
            for (e = 0; e < MT_N; e++) {
                uint32_t q = c->pre[e];
                if (q == 0xFFFFu)
                    continue;
                if (q >= NFACT_V49 + NMPK_V87)
                    q += NMPK - NMPK_V87;
                if (q >= NFACT_V51 + ND8K_V87)
                    q += ND8K - ND8K_V87;
                if (q >= NFACT_V51 + ND8K + ND9K_V87)
                    q += ND9K - ND9K_V87;
                if (q >= NFACT_V52 + NDUK_V87)
                    q += NDUK - NDUK_V87;
                c->pre[e] = (uint16_t)q;
            }
            glob_dirty = 1;
        }
        if (c->ok && c->rsv < 1u) {            /* (v0.78) numery presetów sprzed PHYS: USER i banki o NPHP dalej */
            for (e = 0; e < MT_N; e++)
                if (c->pre[e] != 0xFFFFu && c->pre[e] >= NFACT_V54)
                    c->pre[e] = (uint16_t)(c->pre[e] + NPHP);
            c->rsv = 1;
            glob_dirty = 1;
        }
        if (c->ok && c->rsv < 2u) {              /* (v0.84) sprzed FORMANT: USER i banki o NFOP dalej */
            for (e = 0; e < MT_N; e++)
                if (c->pre[e] != 0xFFFFu && c->pre[e] >= NFACT_V78)
                    c->pre[e] = (uint16_t)(c->pre[e] + NFOP);
            glob_dirty = 1;
        }
#pragma pop_macro("NMPK")
#pragma pop_macro("ND8K")
#pragma pop_macro("ND9K")
#pragma pop_macro("NDUK")
        if (c->ok && c->rsv < 4u) {              /* (v0.90) mniej zestawów perkusji */
            for (e = 0; e < MT_N; e++)
                c->pre[e] = (uint16_t)kit90(c->pre[e]);
            glob_dirty = 1;
        }
        if (c->ok)
            c->rsv = SCN_NUM;
        if (!ok || k < GLOB_V84)                 /* (v0.82) zapis bez wysyłek w scenach: jak presety */
            for (e = 0; e < MT_N; e++)
                glob.scs[b][e] = 0;
    }
    for (b = 0; b <= SCN_N; b++) {           /* (v0.85) MASTER FX: zapis bez nich = domyślne; rodzaje na swoich miejscach */
        mfx_t *m = b ? &glob.scm[b - 1] : &glob.mfx;
        if (!ok || k < GLOB_V89 || m->s[0].type != FX_REVERB || m->s[1].type != FX_DELAY)   /* (v0.90: pfs za nimi) */
            mfx_def(m);
    }
    for (b = 0; b < PF_N; b++)               /* (v0.90) PERFORM: zapis bez ustawień klawiszy albo złe: jak dotąd */
        if (!ok || k < GLOB_V90 || (glob.pfs[b] >> 1 & 127u) > 99u || (glob.pfs[b] >> 8) > 4u)
            glob.pfs[b] = 0;
    {                                        /* (v0.91) krzywe PERFORM: zapis bez nich albo złe = domyślne */
        int bad = !ok || k < GLOB_V91C;
        for (b = 0; b < PF_NCV * 32 && !bad; b += 2)
            bad = glob.gbc[b] > 100u || glob.gbc[b + 1] > 15u;
        for (b = 0; b < PF_NCV && !bad; b++)
            bad = glob.gbc[PF_NCV * 32 + b] > 100u;
        if (bad)
            gb_def(glob.gbc);
        ogs_pf_cvd = glob.gbc;
    }
    for (b = NGLOB; b < NGALL; b++)          /* (v0.91) pola ponad 64: zapis bez nich albo złe = domyślne; (v1.1.0) gx2 */
        if (!ok || (b < NGLOB + NGX ? k < GLOB_V90 + 2 * NGX : k < GLOB_V110 + 2 * NGX2)   /* (zapis v0.90 ma 2 B wyrównania) */
            || GV(b) < GDEF[b].lo || GV(b) > GDEF[b].hi)
            GV(b) = GDEF[b].def;
    glob.magic = GLOB_MAGIC;
    glob_apply();
    fx_load();
    xt_load();                               /* dodatki presetów (v0.38) */
    for (b = 0; b < NENG_MAX; b++)           /* (v0.92) ogon EQ z dodatków presetu */
        eq_unpack(b);
    mfx_upgrade();
    settings_load();
    mt_boot();                               /* (v0.64) MULTI z GLO */
    sqg_load();                              /* SEQ / ARP: ustawienia i bieżący wzór */
}

void app_init(void)
{
    int32_t k, e;
    for (e = 0; e < NENG; e++)
        if (ENG[e].init)
            ENG[e].init();
    migrate();
    for (k = 0; k < (int32_t)SC_LEN; k++)
        scope[k] = 0;
    mt_tdef();
    ogs_fx_init();
    store_load();
    if (glob.g[G_CPU2])
        plat_cpu2_set(1);                    /* drugi rdzeń: głosy nieparzyste DX7 (fm1.toml cpu2, cpu2_fallback; GLO 2ND CORE) */
}

static uint32_t loud_at;                 /* ostatnia chwila z dźwiękiem na wyjściu (główna pętla) */
static int out_quiet(void)               /* od 1 s nic nie słychać (-60 dB): zapis flasha nie przerwie dźwięku */
{
    uint32_t now = plat_ms();
    if (out_lvl > 0.001f || eng_sounding())
        loud_at = now;
    return now - loud_at >= 1000u;
}

static void glob_commit(void)            /* wyjście z GLO: zapis tylko gdy coś zmieniono (jeden zapis flasha) */
{
    if (!glob_dirty)
        return;
    glob_dirty = 0;
    if (plat_store_save(OBJ_GLOBAL, &glob, sizeof glob) != 0)
        app_say("GLOBAL", "SAVE FAILED");
}

/* kopia zapasowa przez USB (device/backup.c): ten sam stan co w pamięci, bez zapisów w trakcie */
static uint8_t backup_hold;
static uint8_t backup_back;              /* przywrócono: UI od HOME (główna pętla) */
int app_backup_playing(void) { return eng_sounding(); }
void app_backup_flush(void)
{
    if (mt_gdirty) {                         /* (v0.64) MULTI też */
        mt_gdirty = 0;
        glob_dirty = 1;
    }
    glob_commit();
    if (set_dirty)
        settings_save();
}
void app_backup_hold(int on) { backup_hold = (uint8_t)on; }
void app_backup_restored(void)
{
    store_load();
    backup_back = 1;
}

/* SAVE: brzmienie i nazwa do presetu id (gotowe miejsce: free_slot), plus bieżące brzmienie na start */
static int32_t free_slot(void)           /* pierwszy wolny USER, -1: pełne */
{
    int32_t id;
    for (id = ID_USER; id < ID_SYX; id++)
        if (!rec_used(id))
            return id;
    return -1;
}

static int32_t user_count(void)
{
    int32_t id, n = 0;
    for (id = ID_USER; id < ID_SYX; id++)
        n += rec_used(id);
    return n;
}

static void save(int32_t id, const char *name)
{
    static uint8_t pk[REC_SIZE];
    int32_t k, n, slot;
    uint32_t o = rec_obj(id), len = 0;
    char t[NAME_LEN + 1];
    bank_read(o);
    slot = id < NFACT ? fact_at[id] : rec_slot(id);
    if (slot < 0) {                          /* fabryczny jeszcze nienadpisany: wolne miejsce na liście */
        for (slot = 0; slot < FACT_N && (bk.used >> slot & 1u); slot++)
            ;
        if (slot == FACT_N) {
            app_say("FACTORY", "FULL: SAVE NEW");
            return;
        }
    }
    for (k = 0; k < NAME_LEN; k++)
        t[k] = name[k];
    for (n = NAME_LEN; n > 0 && t[n - 1] == ' '; n--)   /* bez spacji na końcu */
        ;
    t[n] = 0;
    for (k = 0; k < E->np; k++) {
        pk[len++] = (uint8_t)E->p[k];
        if (wide(E, k))
            pk[len++] = (uint8_t)((uint16_t)E->p[k] >> 8);
    }
    rec_write(rec(slot), t, set.eng, id < NFACT ? id - eng_first(set.eng) : 0xFF, E->np, pk, len);
    bk.used |= 1u << slot;
    if (plat_store_save(o, &bk, bank_size(o)) != 0) {
        app_say("SAVE", "FAILED");
        return;
    }
    bank_read(o);                            /* maska i silniki z tego, co zapisane */
    if (fx_put(id >= ID_USER ? id - ID_USER : slot, id < NFACT, fxc.cur[set.eng], fxc.x[set.eng]) != 0) {
        app_say("SAVE", "FX FAILED");        /* (brzmienie zapisane; efekty presetu: domyślne) */
        return;
    }
    fx_dirty = 1;
    if (xt_write(xt_off(id >= ID_USER ? id - ID_USER : slot, id < NFACT), xtc[set.eng], XT_N) != 0) {
        app_say("SAVE", "EXTRA FAILED");     /* (brzmienie i efekty zapisane; dodatki presetu: domyślne) */
        return;
    }
    xt_dirty = 1;
    CUR = (uint16_t)id;
    set.edited[set.eng] = 0;
    snap_take(set.eng);
    settings_save();
    preset_name(id, t);
    app_say("SAVED", t);
}

/* ---------------------------------------------------------- SysEx DX7 --- */
/* pojedyncze brzmienie (VCED): F0 43 0n 00 01 1B dane[155] suma F7 — od razu do DX7 (zmienione, bez zapisu).
 * bank (32 brzmienia): F0 43 0n 09 20 00 dane[4096] suma F7 — za długi dla USB FM1 w jednym kawałku, więc
 * OG SYNTH MANAGER (app/web/manager.html) wysyła go w kawałkach: F0 7D 4F 47 53 01 nr dane[<=512] F7 (bajty banku bez
 * F0 / F7), na końcu F0 7D 4F 47 53 02 nazwa F7. Bank czeka w oknie IMPORT na miejsce 1-32. */
#define SYX_LEN 4102u                    /* bank bez F0 / F7 */
static uint8_t syx_in[SYX_LEN] __attribute__((section(".pool")));
static uint32_t syx_n;
static char syx_in_name[NAME_LEN + 1];
static uint8_t syx_ready;                /* bank czeka w oknie IMPORT */
static uint32_t syx_gen;                 /* zapisy banków (symulator przechowuje obszar danych) */

static int dx7_sum_ok(const uint8_t *d, uint32_t n, uint8_t sum)
{
    uint32_t i, s = 0;
    for (i = 0; i < n; i++)
        s += d[i];
    return ((s + sum) & 0x7Fu) == 0u;
}

static void import_open(void);
static int bank_store(int32_t slot);
static void ui_imp_cz(int cz);
static uint32_t cpu_peak;
static uint16_t cz_in_mask;              /* bank CZ: które brzmienia przyszły (syx_in: 16 x 128 B) */

/* brzmienie CZ: 256 półbajtów -> 128 B; 0 = złe dane. DELAY (bajt 7, starszy) <= 3: nigdy „AT” w sektorze XDATA0
 * (CZ_BASE); CZ wysyła tam najwyżej 0x02 */
static int cz_unpack(const uint8_t *q, uint8_t *d)
{
    uint32_t i;
    for (i = 0; i < 128u; i++) {
        if ((q[2 * i] | q[2 * i + 1]) & 0xF0u)
            return 0;
        d[i] = (uint8_t)(q[2 * i] | q[2 * i + 1] << 4);
    }
    d[7] &= 3u;
    return 1;
}
static int cz_frame(const uint8_t *p, uint32_t n)   /* ramka brzmienia CZ (bez F0 / F7) */
{
    return (n == 262u || n == 261u) && p[0] == 0x44u && p[1] == 0x00u && p[2] == 0x00u && (p[3] & 0xF0u) == 0x70u
        && (p[4] == 0x20u || p[4] == 0x30u);
}                /* najwyższe obciążenie audio (%) od zerowania: pomiar przez SysEx 0x10 */

static int bank_frame(const uint8_t *p, uint32_t n)   /* p: bez F0 / F7 */
{
    if (n != SYX_LEN || p[0] != 0x43u || p[2] != 0x09u || p[3] != 0x20u || p[4] != 0x00u)
        return 0;
    if (!dx7_sum_ok(p + 5, 4096u, p[5 + 4096])) {
        app_say("DX7 BANK", "BAD CHECKSUM");
        return 1;
    }
    if (p != syx_in) {
        uint32_t i;
        for (i = 0; i < n; i++)
            syx_in[i] = p[i];
    }
    ui_imp_cz(0);
    import_open();
    return 1;
}

/* mikro-test CPU (SysEx 0x12): takty 360 MHz x10 na iterację (z przerwaniem audio w tle: mierzyć w ciszy);
 * volatile, żeby kompilator nie wyrzucił pętli */
static volatile float bench_f[4] = {0.5f, 1.0001f, 0.25f, 0.0f};
static volatile uint32_t bench_u[2] = {0x12345678u, 0x01000193u};
/* (v0.34) zapis / odczyt bufora 16 bitów jak w ogs_fx.c: stary (clamp + warunek) i nowy (na bitach), do porównania na FM1 */
static int16_t bench_h[256] __attribute__((section(".pool")));   /* (zapas puli: nie więcej) */
static inline void bench_put_old(int16_t *b, uint32_t i, float v)
{
    v = fm_clampf(v * 16384.0f, -32767.0f, 32767.0f);
    b[i] = (int16_t)(v + (v >= 0.0f ? 0.5f : -0.5f));
}
static inline void bench_put_new(int16_t *b, uint32_t i, float v)
{
    fm_bits_t h, r;
    uint32_t sg;
    h.f = v * 16384.0f;
    sg = h.u & 0x80000000u;
    if ((h.u & 0x7FFFFFFFu) > 0x46FFFE00u)
        h.u = sg | 0x46FFFE00u;
    r.u = sg | 0x3F000000u;
    b[i] = (int16_t)(h.f + r.f);
}
static uint32_t bench(uint32_t k)
{
    const uint32_t N = 20000u;
    uint32_t i, t0, u = bench_u[0], acc = 0;
    float x = bench_f[0], y = bench_f[1], z = bench_f[2], f = 0.0f, v = 0.25f, w = 0.125f;
    t0 = plat_ticks24();
    switch (k) {
    case 11: for (i = 0; i < N; i++) { bench_put_old(bench_h, i & 255u, (float)(int32_t)(u >> 8) * (1.0f / 8388608.0f) - 0.5f); u += bench_u[1]; } break;
    case 12: for (i = 0; i < N; i++) { bench_put_new(bench_h, i & 255u, (float)(int32_t)(u >> 8) * (1.0f / 8388608.0f) - 0.5f); u += bench_u[1]; } break;
    case 13: for (i = 0; i < N; i++) {           /* odczyt z interpolacją: 2 próbki przeskalowane osobno */
            uint32_t a = u >> 24, b = (a + 1u) & 255u;
            float xa = (float)bench_h[a] * (1.0f / 16384.0f);
            f += xa + x * ((float)bench_h[b] * (1.0f / 16384.0f) - xa);
            u += bench_u[1];
        } break;
    case 14: for (i = 0; i < N; i++) {           /* różnica w całkowitych, skala raz (BLERP) */
            uint32_t a = u >> 24, b = (a + 1u) & 255u;
            float xa = (float)bench_h[a];
            f += (xa + x * (float)((int32_t)bench_h[b] - (int32_t)bench_h[a])) * (1.0f / 16384.0f);
            u += bench_u[1];
        } break;
    case 0: for (i = 0; i < N; i++) { acc += bench_u[1]; } break;                                       /* pusta pętla (odczyt + dodawanie) */
    case 1: for (i = 0; i < N; i++) { x = x * y + z; } break;                                      /* mnożenie + dodawanie (łańcuch) */
    case 2: for (i = 0; i < N; i++) { acc += (uint32_t)(int32_t)(x * 16777216.0f); x += z; } break;   /* float -> int */
    case 3: for (i = 0; i < N; i++) { f += dx_sin[u >> 20]; u += bench_u[1]; } break;                  /* sinus z tablicy (.pool) */
    case 4: for (i = 0; i < N; i++) { u = u * bench_u[1] + 1u; } break;                                /* mnożenie całkowite */
    case 5: for (i = 0; i < N; i++) { f += (float)(int32_t)u; u += bench_u[1]; } break;                /* int -> float */
    case 6: for (i = 0; i < N; i++) { acc += (uint32_t)__builtin_ctz(u | 1u); u += bench_u[1]; } break;   /* ctz */
    case 7: for (i = 0; i < N; i++) { x = x * y; } break;                                          /* samo mnożenie */
    case 8: for (i = 0; i < N; i++) { x = x * y + z; f = f * y + z; } break;                       /* 2 niezależne łańcuchy naraz */
    case 9: for (i = 0; i < N; i++) { x = x * y + z; f = f * y + z; v = v * y + z; w = w * y + z; } break;   /* 4 naraz */
    default: for (i = 0; i < N; i++) { x = x / y; } break;                                         /* dzielenie (łańcuch) */
    }
    t0 = plat_ticks24() - t0;
    bench_f[3] = x + f + v + w;
    bench_u[0] = u + acc;
    return t0 * 150u / N;
}

static void bank_ack(uint8_t cmd, uint8_t nr)   /* v0.37: ramka banku DX7 / CZ doszła -> F0 7D 4F 47 53 24 cmd nr F7
                                                  * (OG SYNTH MANAGER czeka na to i ponawia zgubione; powtórzona
                                                  * ramka robi to samo jeszcze raz) */
{
    uint8_t a[9] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x24, cmd, nr, 0xF7};
    plat_sysex_send(a, sizeof a);
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    if (n == 161u && p[0] == 0x43u && (p[1] & 0xF0u) == 0x00u && p[2] == 0x00u && p[3] == 0x01u && p[4] == 0x1Bu) {
        char t[12];
        int32_t c;
        if (!dx7_sum_ok(p + 5, 155u, p[160])) {
            app_say("DX7 VOICE", "BAD CHECKSUM");
            return 1;
        }
        engine_switch(ENG_DX7);
        dx_vced(p + 5);
        CUR = (uint16_t)eng_first(ENG_DX7);
        set.edited[ENG_DX7] = 1;
        snap_ok[ENG_DX7] = 0;
        set_dirty = 1;
        ev_push(EV_ALL_OFF);
        for (c = 0; c < 10; c++)
            t[c] = p[5 + 145 + c] >= ' ' && p[5 + 145 + c] < 127 ? (char)p[5 + 145 + c] : ' ';
        t[10] = 0;
        app_say("DX7 VOICE", t);
        return 1;
    }
    /* Casio CZ: jedno brzmienie — F0 44 00 00 7n 20 pp dane[256] F7 (do CZ) albo F0 44 00 00 7n 30 dane[256] F7 (zrzut
     * z CZ); dane: 128 B w półbajtach, młodszy pierwszy. Od razu do PHASE (zmienione, bez zapisu), jak brzmienie DX7. */
    if (cz_frame(p, n)) {
        static uint8_t cz[128];
        if (!cz_unpack(p + n - 256u, cz)) {
            app_say("CZ TONE", "BAD DATA");
            return 1;
        }
        engine_switch(ENG_PD);
        pd_cz(cz);
        CUR = (uint16_t)eng_first(ENG_PD);
        set.edited[ENG_PD] = 1;
        snap_ok[ENG_PD] = 0;
        set_dirty = 1;
        ev_push(EV_ALL_OFF);
        app_say("CZ TONE", "LOADED");
        return 1;
    }
    if (n >= 1u && p[0] == 0x43u)
        return bank_frame(p, n);
    if (n >= 6u && p[0] == 0x7Du && p[1] == 0x4Fu && p[2] == 0x47u && p[3] == 0x53u) {
        uint32_t i;
        if (p[4] == 0x15u) {                 /* pomiar: 1 = wszystkie efekty bieżącego silnika włączone (kopia), 0 = z powrotem;
                                              * 3 t m f: sam efekt t (FX_NONE: pusty) w rodzaju m (1..), f=1: w 16 bitach */
            static fx_slot_t keep[FX_SLOTS];     /* kopia jednego silnika (test idzie silnik po silniku) */
            static uint8_t keepx[FX_NX];
            int32_t k;
            if (p[5] && fxt_eng != set.eng) {
                if (fxt_eng >= 0) {
                    fx_copy(fxc.cur[fxt_eng], keep);   /* poprzedni silnik wraca */
                    fx_xcopy(fxc.x[fxt_eng], keepx);
                }
                fx_copy(keep, fxc.cur[set.eng]);
                fx_xcopy(keepx, fxc.x[set.eng]);
                fxt_eng = (int8_t)set.eng;
                for (k = 0; k < FX_SLOTS && p[5] != 3u; k++) {
                    fx_slot_t *c = &fxc.cur[set.eng][k];
                    if (p[5] == 2u) {           /* najcięższy łańcuch */
                        static const uint8_t HEAVY[FX_SLOTS] = {FX_REVERB, FX_DELAY, FX_SHIFT, FX_PHASER, FX_LOFI, FX_CHORUS, FX_FLANGER, FX_COMP};
                        uint32_t j;
                        c->type = HEAVY[k];
                        for (j = 0; j < FX_NP; j++)
                            c->p[j] = FXDEF[c->type][j];
                    }
                    if (c->type < FX_NTYPE)
                        c->mode = FX_NMODE[c->type];
                }
            } else if (!p[5] && fxt_eng >= 0) {
                fx_copy(fxc.cur[fxt_eng], keep);
                fx_xcopy(fxc.x[fxt_eng], keepx);
                fxt_eng = -1;
            }
            if (p[5] == 3u && n >= 9u) {         /* jeden efekt w slocie 1 (reszta pusta), gałki domyślne */
                fx_slot_t *c = &fxc.cur[set.eng][0];
                uint32_t t = p[6] < FX_NTYPE ? p[6] : FX_NONE, j;
                fx_default(fxc.cur[set.eng]);
                c->type = (uint8_t)t;
                for (j = 0; j < FX_NP; j++)
                    c->p[j] = t < FX_NTYPE ? FXDEF[t][j] : 0u;
                fx_xdef(fxc.x[set.eng], t);
                c->mode = (uint8_t)(t < FX_NTYPE ? clamp(p[7], 1, FX_NMODE[t]) : 0);
                if (fx_typep(t) >= 0)
                    c->p[fx_typep(t)] = (uint8_t)(c->mode - 1u);
                ogs_fx_force_lo = p[8] ? (1u << FX_NTYPE) - 1u : 0u;   /* (v0.49: było 0x3FFFF — bez COMB, STUTTER) */
            }
            if (!p[5])
                ogs_fx_force_lo = 0;
            return 1;
        }
        if (p[4] == 0x17u) {                 /* pomiar: kawałek taśmy efektów na dwóch rdzeniach (p5 | p6 << 7 próbek; 0 = jeden);
                                              * p7: skrajne kawałki (0 = równe); p8 (v0.79): pierwszy slot części 2 (0 = wg kosztów) */
            ogs_fx_pipe = (uint32_t)p[5] | (n >= 7u ? (uint32_t)p[6] << 7 : 0u);
            if (n >= 8u)
                ogs_fx_edge = p[7];
            if (n >= 9u)
                ogs_fx_split = p[8] < FX_SLOTS ? p[8] : 0u;
            return 1;
        }
        if (p[4] == 0x1Cu) {                 /* pomiar MULTI (v0.61, od v0.64 prawdziwy tryb): 1C e2 e3 e4 (silnik 1..NENG,
                                              * 0 = pusta) — T1 = bieżący silnik; same zera = SOLO. Bez zapisu GLO. */
            uint32_t t, any = 0;
            for (t = 0; t < 3u; t++) {
                uint32_t v = n > 5u + t && p[5 + t] <= NENG ? p[5 + t] : 0u;
                glob.g[G_MT0 + 1 + t] = (int16_t)(v ? v - 1u : MT_NONE);
                any |= v;
            }
            glob.g[G_MT0] = set.eng;
            glob.g[G_MTFOC] = 0;
            glob.g[G_MTMUTE] = 0;
            glob.g[G_MULTI] = any ? 1 : 0;
            mt_ver++;
            return 1;
        }
        if (p[4] == 0x1Eu) {                 /* (v0.61) prototyp MULTI: 1E -> 1F zajęte bajty puli (3 x 7 bit), silniki
                                              * T2-T4 (7F: pusta) */
            uint8_t a[13] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x1F, 0, 0, 0, 0x7F, 0x7F, 0x7F, 0xF7};
            uint32_t u = mt_used, t;
            a[6] = (uint8_t)(u & 0x7Fu);
            a[7] = (uint8_t)(u >> 7 & 0x7Fu);
            a[8] = (uint8_t)(u >> 14 & 0x7Fu);
            for (t = 0; t < 3u; t++)
                if (mt_eng[t + 1] < NENG)
                    a[9 + t] = mt_eng[t + 1];
            plat_sysex_send(a, sizeof a);
            return 1;
        }
        /* (v0.86.1) pomiary MULTI 2A, 2B, 2C: do v0.86.0 były to 20, 21, 22, a 20 i 22 to też import tablicy USER (niżej):
         * od v0.76 pomiar zjadał ramki importu i tablice USER nie wgrywały się z OG SYNTH MANAGER */
        if (p[4] == 0x2Au && n >= 7u) {      /* (v0.76) pomiar MULTI: 2A rv dl — każda ścieżka wysyła na REVERB (HALL) i
                                              * DELAY (TAPE) z MIX rv / dl, gałki domyślne (0 = jak presety). Bez zapisu.
                                              * (v0.77) 2A rv dl m: m 1..3 = MULTI MASTER OFF / LIMIT / COMP (0: GLO) */
            uint32_t k;
            mt_mst = (int8_t)(n >= 8u && p[7] >= 1u && p[7] <= FX_MST_N ? p[7] - 1 : -1);
            for (k = 0; k < 2u; k++)
                mt_stest[k] = p[5 + k] & 127u;
            return 1;
        }
        if (p[4] == 0x2Du && n >= 9u && p[5] == 'W' && p[6] == 'I' && p[7] == 'P' && p[8] == 'E') {
            /* (v1.0.2) powrót do stock (strona instalacji, przed pakietem M-VAVE): dane OG Synth ponad flash.bin stocka
             * (0xA9000) skasowane — stock widzi tam pusty flash jak nowy FM1, nie nasze presety ("janked"). Niżej
             * pisze oficjalny loader. Kalibracja radia i BTIF (0xE8000, 0xE9000) zostają. -> 24 2D 00 */
            uint32_t a;
            for (a = 0xA9000u; a < 0xFC000u; a += 0x1000u)
                if (a < 0xE0000u || a >= 0xF8000u)
                    plat_data_erase(a);
            bank_ack(0x2Du, 0);
            return 1;
        }
        if (p[4] == 0x2Cu && n >= 9u) {      /* (v0.83) pomiar MULTI: 2C t1 t2 t3 t4 — efekty ścieżek (bity: 1 DIST, 2 FILTER,
                                              * 4 COMP, 8 EQ), gałki domyślne, bez budżetu; 0 0 0 0 = koniec. Bez zapisu. */
            uint32_t k;
            for (k = 0; k < MT_N; k++)
                mt_ftest[k] = p[5 + k] & 15u;
            return 1;
        }
        if (p[4] == 0x2Bu && n >= 7u) {      /* (v0.78) pomiar PERFORM (MULTI): 2B pb st — efekt odtwarzania pb (1..PF_N, 0
                                              * żaden) i nakładany st (1..PF_N, 0 żaden), jak trzymane klawisze; 0 0 = koniec */
            ogs_pf_pb = p[5] >= 1u && p[5] <= PF_N ? (int32_t)p[5] - 1 : -1;
            ogs_pf_st = p[6] >= 1u && p[6] <= PF_N ? 1u << (p[6] - 1u) & PF_STACK : 0u;
            return 1;
        }
        if (p[4] == 0x16u) {                 /* pomiar: 1 = PHASE gra z UNISON 4 (brzmienie bez zmian), 0 = z powrotem */
            pd_uni_test = p[5];
            return 1;
        }
        if (p[4] == 0x18u) {                 /* pomiar gubienia (v0.37, tools/sysex_loss.py): 18 nr -> 19 nr i ile ramek 18
                                              * doszło (3 x 7 bit) — brak odpowiedzi + licznik +2 = zginęła odpowiedź;
                                              * v0.38: + ile note on przyszło z USB od startu (3 x 7 bit) */
            static uint32_t got;
            uint8_t a[14] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x19, p[5], 0, 0, 0, 0, 0, 0, 0xF7};
            uint32_t non = midi_non;
            got++;
            a[7] = (uint8_t)(got & 0x7Fu);
            a[8] = (uint8_t)(got >> 7 & 0x7Fu);
            a[9] = (uint8_t)(got >> 14 & 0x7Fu);
            a[10] = (uint8_t)(non & 0x7Fu);
            a[11] = (uint8_t)(non >> 7 & 0x7Fu);
            a[12] = (uint8_t)(non >> 14 & 0x7Fu);
            plat_sysex_send(a, sizeof a);
            return 1;
        }
        if (p[4] == 0x14u) {                 /* pomiar: tryb pętli DX7 (ogs_dsp_mode), bez odpowiedzi */
            ogs_dsp_mode = p[5];
            return 1;
        }
        if (p[4] == 0x1Au && n >= 9u) {      /* (v0.69) pomiar: parametr bieżącego silnika wprost (bez gwiazdki i zapisu; po
                                              * pomiarze przywrócić tym samym): 1A id (2 x 7 bit) wartość + 8192 (2 x 7 bit) */
            int32_t id = p[5] | p[6] << 7;
            if (id < E->np && eng_ready())
                E->p[id] = (int16_t)clamp((int32_t)(p[7] | p[8] << 7) - 8192, E->def(id)->lo, E->def(id)->hi);
            return 1;
        }
        if (p[4] == 0x12u) {                 /* mikro-test CPU: F0 7D 4F 47 53 12 F7 -> 13 + takty x10 na iterację (po 2 x 7 bit) */
            uint8_t a[6 + 2 * 15 + 1] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x13};
            uint32_t k;
            for (k = 0; k < 15u; k++) {
                uint32_t t = bench(k);
                a[6 + 2 * k] = (uint8_t)(t & 0x7Fu);
                a[7 + 2 * k] = (uint8_t)(t >> 7 & 0x7Fu);
            }
            a[sizeof a - 1] = 0xF7;
            plat_sysex_send(a, sizeof a);
            return 1;
        }
        if (p[4] == 0x10u) {                 /* pomiar: F0 7D 4F 47 53 10 r [e] F7 -> 11 cpu% szczyt% xruns (3 x 7 bit) silnik;
                                              * r=1 zeruje szczyt, e=1..NENG przełącza silnik (e-1) */
            uint32_t x = plat_xruns();
            uint8_t a[27] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xF7};
            uint32_t k;
            uint32_t vs = dx_prof[2] ? dx_prof[2] : 1u;
            uint32_t cb = dx_prof[0] * 15u / vs, co = dx_prof[1] * 15u / vs;   /* takty CPU (360 MHz) na próbkę głosu DX7 */
            if (n >= 7u && p[6] >= 1u && p[6] <= (uint8_t)NENG) {
                engine_switch((int32_t)p[6] - 1);
                ev_push(EV_ALL_OFF);
            }
            a[6] = (uint8_t)(plat_cpu_pct() > 127u ? 127u : plat_cpu_pct());
            a[7] = (uint8_t)(cpu_peak > 127u ? 127u : cpu_peak);
            a[8] = (uint8_t)(x & 0x7Fu);
            a[9] = (uint8_t)(x >> 7 & 0x7Fu);
            a[10] = (uint8_t)(x >> 14 & 0x7Fu);
            a[11] = set.eng;
            a[12] = (uint8_t)(cb & 0x7Fu);
            a[13] = (uint8_t)(cb >> 7 & 0x7Fu);
            a[14] = (uint8_t)(co & 0x7Fu);
            a[15] = (uint8_t)(co >> 7 & 0x7Fu);
            a[16] = (uint8_t)plat_cpu2_on();     /* drugi rdzeń: pracuje / dlaczego nie */
            a[17] = (uint8_t)plat_cpu2_gave_up();
            a[18] = (uint8_t)(!eng_ready() ? 100 : set.eng == ENG_WT ? (int32_t)(wt.uni_k * 100.0f)   /* budżet unison: */
                              : set.eng == ENG_PD ? (int32_t)(zt.uni_k * 100.0f) : 100);          /* % (sam ścina) */
            a[19] = (uint8_t)(ogs_fx_lo() & 0x7Fu);   /* efekty w 16 bitach (bit = FX_*) */
            a[20] = (uint8_t)(ogs_fx_lo() >> 7 & 0x7Fu);
            a[21] = (uint8_t)(ogs_fx_lo() >> 14 & 0x7Fu);
            for (k = 0; k < 3u; k++) {           /* taśma efektów: % bloku (256 próbek = 139320 taktów TIMER4) */
                uint32_t v = ogs_fx_prof[k] * 100u / 139320u;
                a[22 + k] = (uint8_t)(v > 127u ? 127u : v);
            }
            a[25] = (uint8_t)(ogs_fx_lo() >> 21 & 0x7Fu);   /* (v0.48.1) 16 bitów: COMB, STUTTER (bity 21-27) */
            if (p[5]) {
                cpu_peak = 0;
                dx_prof[0] = dx_prof[1] = dx_prof[2] = 0;
                ogs_fx_prof_clear();
            }
            plat_sysex_send(a, sizeof a);
            return 1;
        }
        /* tablica USER WAVE (OG SYNTH MANAGER, v0.19): 20 nr(0-2) ramek(1-63) nazwa -> czyści miejsce; 22 nr ramka
         * dane (256 próbek po 2 x 7 bitów: 14 bitów ze znakiem) po kolei; 23 nr -> nagłówek (dopiero wtedy tablica
         * jest). Każdy krok: odpowiedź F0 7D 4F 47 53 21 nr krok F7 (Manager czeka: flash wolniejszy niż USB) */
        if (p[4] >= 0x20u && p[4] <= 0x23u && n >= 6u && p[5] < WT_NUSR) {
            static struct {
                uint8_t on, slot, n, done;       /* done: nr + 1 ostatniej gotowej tablicy (powtórzone 23: znów 7F) */
                uint64_t got;
                char name[12];
            } wi;
            static int16_t fr[WT_N];
            static wtu_head_t hd;
            uint32_t base = WTU_BASE + (uint32_t)p[5] * WTU_SLOT, step = 0;
            uint8_t a[9] = {0xF0, 0x7D, 0x4F, 0x47, 0x53, 0x21, p[5], 0, 0xF7};
            if (p[4] == 0x20u && n >= 7u && p[6] >= 1u && p[6] <= WT_UFRAMES) {
                wt_usr_data[p[5]] = 0;           /* (przerwanie audio: bez tej tablicy, zanim zniknie) */
                wt_usr_n[p[5]] = 0;
                wi.on = wi.done = 0;
                for (i = 0; i < WTU_SLOT; i += 0x1000u)
                    if (plat_data_erase(base + i) != 0) {
                        app_say("WAVE TABLE", "ERASE FAILED");
                        return 1;
                    }
                wi.on = 1;
                wi.slot = p[5];
                wi.n = p[6];
                wi.got = 0;
                for (i = 0; i < 12u; i++)
                    wi.name[i] = 7u + i < n && p[7 + i] >= ' ' && p[7 + i] < 127 ? (char)p[7 + i] : 0;
                app_say("WAVE TABLE", "LOADING");
            } else if (p[4] == 0x22u && n == 7u + 2u * WT_N && wi.on && wi.slot == p[5] && p[6] < wi.n) {
                for (i = 0; i < WT_N; i++)       /* 14 bitów ze znakiem -> int16 */
                    fr[i] = (int16_t)((uint16_t)((uint32_t)p[7 + 2 * i] << 2 | (uint32_t)p[8 + 2 * i] << 9));
                if (plat_data_write(base + 512u + (uint32_t)p[6] * (WT_N * 2u), fr, WT_N * 2u) != 0) {
                    app_say("WAVE TABLE", "WRITE FAILED");
                    wi.on = 0;
                    return 1;
                }
                wi.got |= 1ull << p[6];
                step = (uint32_t)p[6] + 1u;
            } else if (p[4] == 0x23u && wi.on && wi.slot == p[5]) {
                wi.on = 0;
                if (wi.got != (1ull << wi.n) - 1ull) {
                    app_say("WAVE TABLE", "INCOMPLETE");
                    return 1;
                }
                hd.magic = WTU_MAGIC;
                hd.n = wi.n;
                hd.rsv = 0;
                for (i = 0; i < 12u; i++)
                    hd.name[i] = wi.name[i];
                if (plat_data_write(base, &hd, sizeof hd) != 0) {
                    app_say("WAVE TABLE", "WRITE FAILED");
                    return 1;
                }
                wt_usr_scan();
                syx_gen++;
                engine_switch(ENG_WT);           /* od razu w OSC A bieżącego brzmienia WAVE (zmienione, bez zapisu) */
                W[W_AT] = (int16_t)(WT_NTAB + p[5]);
                set.edited[ENG_WT] = 1;
                snap_ok[ENG_WT] = 0;
                set_dirty = 1;
                {
                    char t[24] = "USER ";
                    t[5] = (char)('1' + p[5]);
                    t[6] = 0;
                    app_say(t, wt_usr_name(p[5])[0] ? wt_usr_name(p[5]) : "LOADED");
                }
                wi.done = (uint8_t)(p[5] + 1u);
                step = 0x7Fu;
            } else if (p[4] == 0x23u && !wi.on && wi.done == p[5] + 1u) {
                step = 0x7Fu;                    /* v0.37: odpowiedź zginęła, Manager ponawia: gotowe już było */
            } else {
                return 1;
            }
            a[7] = (uint8_t)(step & 0x7Fu);
            plat_sysex_send(a, sizeof a);
            return 1;
        }
        /* bank CZ (OG SYNTH MANAGER): F0 7D 4F 47 53 03 nr(0-15) <ramka brzmienia CZ bez F0 / F7> F7 po kolei, na końcu
         * F0 7D 4F 47 53 04 nazwa F7 -> okno IMPORT (CZ BANK) na miejsce 1-16; brakujące brzmienia = puste (cisza) */
        if (p[4] == 0x03u && n >= 7u && p[5] < 16u && cz_frame(p + 6, n - 6u)) {
            if (!cz_in_mask || p[5] == 0u) {     /* nowy bank (pierwsza ramka albo nr 0) */
                cz_in_mask = 0;
                for (i = 0; i < 0x800u; i++)
                    syx_in[i] = 0;
            }
            if (cz_unpack(p + n - 256u, syx_in + (uint32_t)p[5] * 128u))
                cz_in_mask |= (uint16_t)(1u << p[5]);
            bank_ack(p[4], p[5]);
            return 1;
        }
        if (p[4] == 0x04u) {
            for (i = 0; i < NAME_LEN && 5u + i < n && p[5 + i] >= ' ' && p[5 + i] < 127; i++)
                syx_in_name[i] = (char)p[5 + i];
            syx_in_name[i] = 0;
            if (!cz_in_mask) {
                app_say("CZ BANK", "NO TONES");
            } else {
                ui_imp_cz(1);
                import_open();               /* (powtórzona ramka: to samo okno jeszcze raz) */
            }
            bank_ack(p[4], 0);
            return 1;
        }
        if (p[4] == 0x01u) {                 /* kawałek nr p[5] po 512 B */
            uint32_t at = (uint32_t)p[5] * 512u;
            if (p[5] == 0u)
                syx_n = cz_in_mask = 0;
            for (i = 6; i < n && at < SYX_LEN; i++)
                syx_in[at++] = p[i];
            syx_n = at > syx_n ? at : syx_n;
            bank_ack(p[4], p[5]);
        } else if (p[4] == 0x05u && n >= 6u && p[5] < NSYX_USE) {   /* (v0.53) koniec z miejscem: F0 7D 4F 47 53 05
                                                                     * miejsce(0-7) nazwa F7 — zapis od razu, bez okna
                                                                     * IMPORT (brzmienia FM1 z OG SYNTH MANAGER) */
            for (i = 0; i < NAME_LEN && 6u + i < n && p[6 + i] >= ' ' && p[6 + i] < 127; i++)
                syx_in_name[i] = (char)p[6 + i];
            syx_in_name[i] = 0;
            if (syx_n != SYX_LEN || syx_in[0] != 0x43u || syx_in[2] != 0x09u || syx_in[3] != 0x20u || syx_in[4] != 0x00u
                || !dx7_sum_ok(syx_in + 5, 4096u, syx_in[5 + 4096])) {
                app_say("DX7 BANK", "NOT A BANK");
            } else {
                char t[16];
                const int32_t slot = p[5];
                if (!bank_store(slot)) {
                    app_say("DX7 BANK", "WRITE FAILED");
                } else {
                    syx_bank_name(slot, t);
                    app_say("SAVED", t);
                }
            }
            bank_ack(p[4], p[5]);
        } else if (p[4] == 0x06u && n >= 6u && p[5] < NSYX_USE) {   /* (v0.80) usuń bank z miejsca p5 (OG SYNTH MANAGER) */
            for (i = 0; i < NAME_LEN; i++)
                syx.name[p[5]][i] = 0;
            syx.used &= ~(1u << p[5]);
            plat_store_save(OBJ_SYX, &syx, sizeof syx);
            syx_gen++;
            fm1_dirty = 1;
            app_say("DX7 BANK", "DELETED");
            bank_ack(p[4], p[5]);
        } else if (p[4] == 0x02u) {          /* koniec: nazwa */
            for (i = 0; i < NAME_LEN && 5u + i < n && p[5 + i] >= ' ' && p[5 + i] < 127; i++)
                syx_in_name[i] = (char)p[5 + i];
            syx_in_name[i] = 0;
            if (!bank_frame(syx_in, syx_n))
                app_say("DX7 BANK", "NOT A BANK");
            bank_ack(p[4], 0);
        }
        return 1;
    }
    return 0;
}

#ifdef PLAT_WEB
#pragma push_macro("__attribute__")          /* host/plat_host.c wyłącza atrybuty: eksporty by znikły */
#undef __attribute__
/* symulator: SysEx z przycisku „Wczytaj bank DX7” (web/emu/app.js) i obszar danych do przechowania w przeglądarce */
static uint8_t web_sx[SYX_LEN + 16] __attribute__((section(".pool")));
__attribute__((used, visibility("default"))) uint8_t *ogs_sysex_buf(void) { return web_sx; }
__attribute__((used, visibility("default"))) int ogs_sysex(uint32_t n)
{
    return app_sysex(web_sx, n > sizeof web_sx ? sizeof web_sx : n);
}
__attribute__((used, visibility("default"))) uint8_t *ogs_syx_area(void) { return (uint8_t *)plat_data(SYX_BASE); }
__attribute__((used, visibility("default"))) uint32_t ogs_syx_size(void) { return NSYX * 0x1000u; }
__attribute__((used, visibility("default"))) uint32_t ogs_syx_gen(void) { return syx_gen; }
__attribute__((used, visibility("default"))) uint8_t *ogs_cz_area(void) { return (uint8_t *)plat_data(CZ_BASE); }
__attribute__((used, visibility("default"))) uint32_t ogs_cz_size(void) { return NCZB / 2u * 0x1000u; }
static int32_t pget(int32_t id);
__attribute__((used, visibility("default"))) int32_t ogs_p(int32_t id) { return pget(id); }   /* testy */
static void set_param(int32_t id, int32_t v);
__attribute__((used, visibility("default"))) void ogs_pset(int32_t id, int32_t v)   /* testy: parametr bieżącego silnika,
                                                                                     * jak gałką (bramka bez trzasków);
                                                                                     * id >= np: wirtualny (lustra LEVEL) */
{
    if (id >= 0 && (id < E->np || E->vset))
        set_param(id, v);
}
/* testy trzasków (tools/click_test.mjs): parametry bieżącego silnika, preset jak z PRESETS, czy gra */
__attribute__((used, visibility("default"))) int32_t ogs_np(void) { return E->np; }
__attribute__((used, visibility("default"))) void ogs_ovl(void) { app_overload(); }   /* testy (v0.48): przeciążenie CPU */
__attribute__((used, visibility("default"))) int32_t ogs_nfact(void) { return E->nfact; }
__attribute__((used, visibility("default"))) const char *ogs_dname(int32_t id) { return E->def(id)->name; }   /* (testy, v0.86.1) nazwa parametru */
__attribute__((used, visibility("default"))) int32_t ogs_def(int32_t id, int32_t w)
{
    const pdef_t *d = E->def(id);
    return w == 0 ? d->lo : w == 1 ? d->hi : w == 2 ? d->def : d->fmt;
}
__attribute__((used, visibility("default"))) int32_t ogs_retrig(int32_t id)
{
    int32_t i;
    for (i = 0; i < 3 && E->retrig[i] >= 0; i++)
        if (id == E->retrig[i])
            return 1;
    return 0;
}
__attribute__((used, visibility("default"))) void ogs_load(int32_t k)   /* k-ty preset fabryczny silnika (0 = INIT) */
{
    preset_go(eng_first(set.eng) + k);
}
__attribute__((used, visibility("default"))) int32_t ogs_sounding(void) { return eng_sounding(); }
__attribute__((used, visibility("default"))) void ogs_fxset(int32_t k, int32_t type, int32_t mode)   /* slot k: rodzaj (domyślne gałki) */
{
    fx_slot_t *c = &fxc.cur[set.eng][(uint32_t)k % FX_SLOTS];
    int32_t j;
    c->type = (uint8_t)type;
    c->mode = (uint8_t)mode;
    for (j = 0; j < FX_NP; j++)
        c->p[j] = type < FX_NTYPE ? FXDEF[type][j] : 0;
}
__attribute__((used, visibility("default"))) void ogs_fxp(int32_t k, int32_t j, int32_t v)   /* slot k, gałka j (0..7) */
{
    fxc.cur[set.eng][(uint32_t)k % FX_SLOTS].p[(uint32_t)j % FX_NP] = (uint8_t)clamp(v, 0, 127);
}
__attribute__((used, visibility("default"))) int32_t ogs_fxlo(void) { return (int32_t)ogs_fx_lo(); }   /* (testy) 16 bitów */
__attribute__((used, visibility("default"))) void ogs_fxdirty(void) { fx_dirty = 1; }   /* (testy, v0.55) efekty do zapisu */
__attribute__((used, visibility("default"))) void ogs_fxxp(int32_t k, int32_t j, int32_t v)   /* slot k, gałka ponad 8 j */
{
    const fx_slot_t *c = &fxc.cur[set.eng][(uint32_t)k % FX_SLOTS];
    int32_t o = fx_xoff(c->type);
    if (o >= 0 && j >= 0 && j < fx_nx(c->type))
        fxc.x[set.eng][o + j] = (uint8_t)clamp(v, 0, 127);
}
__attribute__((used, visibility("default"))) void ogs_budget(int32_t b)   /* testy: budżet unison bieżącego silnika */
{                                                                        /* (WAVE / PHASE), jak przy przeciążeniu */
    if (!eng_ready())
        return;
    if (set.eng == ENG_WT)
        wt.budget = b;
    else if (set.eng == ENG_PD)
        zt.budget = b;
}
__attribute__((used, visibility("default"))) int32_t ogs_mfx(int32_t k, int32_t j, int32_t v)   /* (testy, v0.85) MASTER FX: gałka j (-1: mode, -2: kolejność); v >= 0: ustaw; zwraca wartość */
{
    fx_slot_t *c = &glob.mfx.s[k & 1];
    uint8_t *b = j == -1 ? &c->mode : j == -2 ? &glob.mfx.rev1st : &c->p[(uint32_t)j % FX_NP];
    if (v >= 0)
        *b = (uint8_t)v;
    return *b;
}
static int32_t mw_now(int32_t t);
static int32_t mtl_at(void);
__attribute__((used, visibility("default"))) int32_t ogs_mw(void) { return mw_now(mt_at_cur()); }   /* (testy, v0.85) pozycja w oknie ENGINE w MULTI */
__attribute__((used, visibility("default"))) int32_t ogs_mtl(void) { return mtl_at(); }   /* (testy, v0.86) silnik pod kursorem listy silników */
__attribute__((used, visibility("default"))) int32_t ogs_mfxa(int32_t t) { return t >= 0 && t < MT_N ? (int32_t)mt_fxa[t] : -1; }   /* (testy, v0.83) */
__attribute__((used, visibility("default"))) int32_t ogs_msend(void) { return (int32_t)ogs_fx_send_state(); }   /* (testy, v0.76) */
__attribute__((used, visibility("default"))) int32_t ogs_fxn(int32_t t) { return t < 0 ? FX_NTYPE : FX_NMODE[t % FX_NTYPE]; }
__attribute__((used, visibility("default"))) int32_t ogs_dk(int32_t w)   /* bramka: 0 wzmocnienie x1000, 1 w kolejce */
{
    return w == 0 ? (int32_t)(dk_g * 1000.0f) : w == 1 ? (int32_t)(dk_w - dk_r) : (int32_t)dk_done;
}
__attribute__((used, visibility("default"))) int32_t ogs_eng(void) { return set.eng; }
__attribute__((used, visibility("default"))) void ogs_west_hq(int32_t on) { ogs_west_os = on != 0; }   /* (v0.54) testy:
                                                                                                    * LPG x2 (A / B) */
__attribute__((used, visibility("default"))) uint32_t ogs_midi_non(void) { return midi_non; }   /* testy: licznik 0x19 */
__attribute__((used, visibility("default"))) const char *ogs_chtxt(void) { return ch_txt; }   /* (v1.1.0) testy: CHORD NAME */
__attribute__((used, visibility("default"))) int32_t ogs_nt(int32_t n)   /* testy (v0.39): ile klawiszy gra nutę n; -1: SUB */
{
    return n >= 0 && n < 128 ? nt_cnt[n] : (int32_t)(ogs_fx_sub_gate ? ogs_fx_sub_semi * 100.0f : -1.0f);
}
__attribute__((used, visibility("default"))) int32_t ogs_xt(int32_t i)   /* testy (v0.38): dodatki presetu bieżącego silnika */
{
    return i >= 0 && i < XT_N ? xtc[set.eng][i] : -1;
}
__attribute__((used, visibility("default"))) void ogs_xtset(int32_t i, int32_t v)   /* jak zmiana z panelu */
{
    if (i < 0 || i >= XT_N)
        return;
    xtc[set.eng][i] = (uint8_t)v;
    xt_dirty = set_dirty = 1;
    set.edited[set.eng] = (uint8_t)snap_differs(set.eng);
}
__attribute__((used, visibility("default"))) void ogs_splash_skip(void) { splash_until = 1; }   /* testy: bez logo */
static int32_t grp_n(int32_t g);
static int32_t grp_at(int32_t g, int32_t i);
static const char *pg_name(int32_t page);
__attribute__((used, visibility("default"))) int32_t ogs_label(int32_t l, int32_t page)   /* testy: skrót SEL + klawisz */
{
    (void)page;                                 /* (tools/op_keys.mjs; v0.64.4: skróty stałe, jak z HOME) */
    return E->label_page(l, G_ALL, -1);         /* (-1 = V_HOME, zdefiniowane dalej) */
}
__attribute__((used, visibility("default"))) int32_t ogs_grp(int32_t g, int32_t i) { return i < 0 ? grp_n(g) : grp_at(g, i); }
__attribute__((used, visibility("default"))) const char *ogs_pgname(int32_t page) { return page < 0 ? "HOME" : pg_name(page); }
__attribute__((used, visibility("default"))) int32_t ogs_glob(int32_t g) { return g >= 0 && g < NGALL ? GV(g) : -999; }
__attribute__((used, visibility("default"))) void ogs_gset(int32_t g, int32_t v)   /* testy: pole GLO (bez zapisu) */
{
    if (g >= 0 && g < NGALL)
        GV(g) = (int16_t)clamp(v, GDEF[g].lo, GDEF[g].hi);
}
__attribute__((used, visibility("default"))) void ogs_glob_save(void)   /* testy: zapis GLO teraz */
{
    glob_dirty = 1;
    glob_commit();
}
static const fx_slot_t *fx_play(void);
__attribute__((used, visibility("default"))) int32_t ogs_fx(int32_t k, int32_t what) { const fx_slot_t *c = fx_play() + (uint32_t)k % FX_SLOTS; int32_t o = fx_xoff(c->type); return what == 0 ? c->type : what == 1 ? c->mode : what < 2 + FX_NP ? c->p[what - 2] : o >= 0 && what - 2 - FX_NP < fx_nx(c->type) ? fxc.x[set.eng][o + what - 2 - FX_NP] : -1; }
static inline int32_t ui_page(void);
__attribute__((used, visibility("default"))) int32_t ogs_edited(void) { return set.edited[set.eng]; }
__attribute__((used, visibility("default"))) int32_t ogs_page(void) { return ui_page(); }
__attribute__((used, visibility("default"))) int32_t ogs_preset(void) { return CUR; }
__attribute__((used, visibility("default"))) const char *ogs_pname(int32_t id)   /* testy: nazwa presetu id */
{
    static char t[NAME_LEN + 8];
    preset_name(id, t);
    return t;
}
/* testy SEQ / ARP (tools/seq_test.mjs) */
static int32_t ui_sq(int32_t w);
__attribute__((used, visibility("default"))) int32_t ogs_sq(int32_t w)
{
    int32_t i, n = 0;
    if (w == 2) {
        for (i = 0; i < SQ_NN; i++)
            n += sqp.n[i].vel != 0;
        return n;
    }
    return w == 0 ? sq.play : w == 1 ? (int32_t)sq.pos : w == -2 ? (int32_t)sq_scale() : w == 6 ? sqp.rsv[0] | sqp.rsv[1] << 4 | sqp.rsv[2] << 8   /* (v1.1.0) */
         : w == 3 ? sqg.arp_on : w == 4 ? ar.nl : w == 5 ? sq.rec
         : w == 7 ? sqp.bpm10 : w == 8 ? sqp.grid : w == 9 ? sqg.pat : w == 10 ? sqp_dirty : w == 11 ? sqp.bars : w >= 12 ? ui_sq(w) : -1;
}
__attribute__((used, visibility("default"))) int32_t ogs_sqn(int32_t i, int32_t w)   /* nuta i: t, len, note, vel, fl, chance */
{
    const sqn_t *q = &sqp.n[(uint32_t)i % SQ_NN];
    return w == 0 ? q->t : w == 1 ? q->len : w == 2 ? q->note : w == 3 ? q->vel : w == 4 ? q->fl : w == 6 ? sqp.x[(uint32_t)i % SQ_NN] : q->chance;   /* (v0.92) 6: x */
}
__attribute__((used, visibility("default"))) const uint8_t *ogs_sq_flash(int32_t pat) { return plat_data(sq_off(pat)); }
__attribute__((used, visibility("default"))) const int16_t *ogs_wt_frame(int32_t v, int32_t o) { return wd[v & 7].buf[o & 1]; }
__attribute__((used, visibility("default"))) int32_t ogs_wt_info(int32_t v, int32_t w) { const wtvoice_t *d = &wd[v & 7]; return w == 0 ? d->nuni[0] : w == 1 ? (int32_t)d->inc[0][0] : w == 2 ? d->key[0].hmax : w == 3 ? d->key[0].pos : w == 4 ? (int32_t)(d->gl[0][0] * 1000) : w == 5 ? wc[v & 7].on : (int32_t)(d->amp * 1000); }
#pragma pop_macro("__attribute__")
#endif

#ifdef PLAT_WEB
/* (v0.87, symulator: tools/screen_audit_test.mjs) napis poza płótnem; w firmware tego nie ma */
static uint32_t au_txt;
static char au_str[32];
/* (v0.90) napisy na sobie: mapa właściciela pikseli płótna (który napis go narysował). Napis na pikselu innego napisu = au_ovl;
 * napis na rysunku pod nim (linia, wykres, znak: inny kolor niż tło pod resztą napisu) = au_gfx. Nazwy w au_ostr. */
static uint8_t au_own[CV_MAX], au_n;
static char au_nm[64][24], au_ostr[64];
static uint32_t au_ovl, au_gfx;
static uint16_t au_save[64 * 40];
static void au_reset(void) { uint32_t i; for (i = 0; i < CV_MAX; i++) au_own[i] = 0; au_n = 0; }
static void au_name2(const char *a, const char *b)
{
    uint32_t i = 0, j;
    for (j = 0; a[j] && i < 30u; j++)
        au_ostr[i++] = a[j];
    au_ostr[i++] = ' ';
    au_ostr[i++] = '/';
    au_ostr[i++] = ' ';
    for (j = 0; b[j] && i < 62u; j++)
        au_ostr[i++] = b[j];
    au_ostr[i] = 0;
}
static int32_t au_text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    const int32_t w = text_w(f, s), h = (int32_t)f->h;
    int32_t r, k, x0 = x < 0 ? 0 : x, x1 = x + w > (int32_t)cv_w ? (int32_t)cv_w : x + w, y0 = y + cv_oy, y1 = y + cv_oy + h, ret;
    uint32_t id, i;
    if (x < 0 || x + w > (int32_t)cv_w || y < 0 || y + h > (int32_t)cv_h + cv_oy) {
        au_txt++;
        for (i = 0; i < 31u && s[i]; i++)
            au_str[i] = s[i];
        au_str[i] = 0;
    }
    y0 = y0 < 0 ? 0 : y0;
    y1 = y1 > (int32_t)cv_h ? (int32_t)cv_h : y1;
    if (x1 - x0 > 64 || y1 - y0 > 40 || x1 <= x0 || y1 <= y0 || au_n >= 63u)
        return cv_text(x, y, f, s, c);
    id = ++au_n;
    for (i = 0; i < 23u && s[i]; i++)
        au_nm[id][i] = s[i];
    au_nm[id][i] = 0;
    for (r = y0; r < y1; r++)
        for (k = x0; k < x1; k++)
            au_save[(r - y0) * 64 + (k - x0)] = cv_px[r * cv_w + k];
    ret = cv_text(x, y, f, s, c);
    {
        uint16_t bg = au_save[0];                /* tło pod napisem: najczęstszy kolor ramki (lewy górny róg wystarcza) */
        int gfx = 0;
        uint32_t other = 0;
        for (r = y0; r < y1; r++)
            for (k = x0; k < x1; k++) {
                const uint32_t p = (uint32_t)r * cv_w + (uint32_t)k;
                const uint16_t was = au_save[(r - y0) * 64 + (k - x0)];
                if (cv_px[p] == was)
                    continue;
                if (au_own[p] && au_own[p] != id)
                    other = au_own[p];
                else if (!au_own[p] && was != bg)
                    gfx++;
                au_own[p] = (uint8_t)id;
            }
        if (other) {
            au_ovl++;
            au_name2(au_nm[other], au_nm[id]);
        } else if (gfx > 2) {
            au_gfx++;
            au_name2("GFX", au_nm[id]);
        }
    }
    return ret;
}
#define cv_text au_text
#define cv_begin(w, h, bg) (au_reset(), cv_begin(w, h, bg))
static uint32_t au_hid, au_y;            /* wierszy schowanych pod dolnym paskiem, pierwszy (body_blit) */
#pragma push_macro("__attribute__")
#undef __attribute__
__attribute__((used, visibility("default"))) int32_t ogs_audit(int32_t w)
{
    if (w == 4) {                        /* po odczycie: następny ekran od zera (nie każdy idzie przez body_blit) */
        au_hid = au_txt = au_ovl = au_gfx = 0;
        return 0;
    }
    return w == 0 ? (int32_t)au_hid : w == 1 ? (int32_t)au_y : w == 2 ? (int32_t)au_txt : w == 5 ? (int32_t)au_ovl
         : w == 6 ? (int32_t)au_gfx : w == 7 ? (int32_t)(uintptr_t)au_ostr : (int32_t)(uintptr_t)au_str;
}
#pragma pop_macro("__attribute__")
#endif

/* ------------------------------------------------------------------ UI --- */
enum { V_HOME = -1, V_GLO = -2, V_SAVE = -3, V_NAME = -4, V_ALLP = -5, V_IMPORT = -6, V_ENGP = -7, V_INFO = -8,
       V_RESET = -9, V_NOTE = -10, V_FX = -11, V_FXL = -12, V_SEQ = -13, V_ARP = -14, V_SQT = -15, V_SQN = -16,
       V_PERF = -17,                      /* (v0.47) PERFORM: FX trzymany */
       V_MTL = -18,                       /* (v0.86) MULTI: lista silników ścieżki (SEL trzymany, jak lista efektów) */
       V_MOD = -19,                       /* (v0.65) strona MOD: LFO / ENV trzymany + klawisz OP (gest.md) */
       V_SCN = -20,                       /* (v0.86) MULTI: lista scen (v0.90: HOME na głównym ekranie) */
       V_CPY = -21 };                     /* (v0.92) perkusja: SEL + klawisz + drugi klawisz = kopia dźwięku (pytanie) */
static int8_t mt_red = -1;               /* (v0.81) silnik, który nie może grać obok reszty (czerwony): pod kursorem okna ENGINE, SEL w liście */
static void mt_knob(uint32_t k, int32_t e);
static void mod_knob(uint32_t k, int32_t e);
static void mod_select(int32_t e);
static void mt_step(int32_t d);
static void mt_scene_load(void);
static void mt_mute(void);
static void mt_engine_set(int32_t t, int32_t e);
static void mtl_open(void);
static void mtl_ok(void);
/* V_SEQ, V_ARP: strony jak FX (gałki działają); V_SQT: okno narzędzi wzoru */
#define PANEL_PAGE(p) ((p) == V_FX || (p) == V_SEQ || (p) == V_ARP || (p) == V_PERF)   /* <= V_SAVE: okna (V_FX: strona efektów, gałki działają) */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    int32_t page;                        /* V_*, albo strona 0..npages-1 */
    int32_t grp, idx;                    /* sekcja (G_*) i miejsce w niej: numer na ekranie */
    int32_t last[NENG][G_ALL];           /* ostatnia strona każdej sekcji (każdy silnik): tam wraca jej przycisk */
    uint8_t quick;                       /* strona szybka z HOME: wraca sama po 2 s bez kręcenia */
    uint8_t more_lock;                   /* MORE włączone przyciskiem sekcji (EDIT na EDIT itd.; jak FX na FX) */
    uint32_t quick_at;
    int32_t knob;                        /* ostatnio kręcona gałka K1..K4 (0..3), -1 żadna */
    uint32_t knob_at, enc_at[NE];
    uint32_t scope_at;                   /* ostatnia klatka oscyloskopu HOME */
    uint32_t browse_until;               /* SELECT z HOME: do kiedy czeka na akcję */
    uint32_t dirty_at;                   /* ostatnia zmiana do zapisania samemu */
    uint8_t browse;                      /* SELECT z HOME bez żadnej akcji: po browse_until wraca do HOME */
    uint8_t sel_hold, sel_used;          /* SEL trzymany (skróty, MORE, silnik, ALL); użyty: puszczenie nie
                                          * przełącza opcji strony */
    uint32_t sel_at;                     /* kiedy SEL naciśnięty: opcję przełącza tylko krótkie kliknięcie */
    int8_t glo_sec;                      /* GLO: -1 lista sekcji, albo GS_* */
    uint8_t glo_cur, glo_page, sy_cur;   /* kursor listy sekcji, strona w sekcji, kursor SYSTEM */
    uint8_t reset_opt, reset_sure;       /* FACTORY RESET: co (ustawienia / presety / wszystko), drugie potwierdzenie */
    uint8_t note;                        /* okno V_NOTE: SY_BACKUP albo SY_CALIB */
    uint8_t fx_slot, fx_win;             /* FX: bieżący slot 0..7, pierwszy z czterech widocznych w łańcuchu */
    uint8_t fx_more;                     /* FX: strona gałek efektu (0, MORE, dalej kolejne; FX przełącza) */
    uint8_t pf_hold, pf_used, pf_latch, pf_last;   /* (v0.47) PERFORM: FX trzymany, klawisz użyty (puszczenie FX bez
                                                    * strony FX), LATCH, ostatni klawisz (nazwa na ekranie) */
    uint32_t pf_at, pf_keys, pf_lat;     /* FX wciśnięty kiedy; klawisze trzymane / zatrzaśnięte (bit = klawisz = PF_*) */
    int32_t pf_back;                     /* strona sprzed PERFORM */
    int8_t pf_mac[4];                    /* gałki: FILTER -100..100, CRUSH, THROW, DEPTH 0..100 */
    uint8_t pf_ord[PF_N], pf_n;          /* kolejność wciśnięć efektów odtwarzania (gra ostatni trzymany) */
    uint8_t pf_bank, cv_last, cv_lat, cv_step;   /* (v0.91) PERFORM: OCT = bank CURVES; krzywa ostatnia, zatrzaśnięte (bit), krok edycji */
    uint32_t cv_held;                    /* klawisze krzywych trzymane (bit = klawisz) */
    uint8_t fxl_cur, fxl_top;            /* lista efektów (SEL trzymany): kursor 0..FX_NTYPE (miejsce w FX_ORDER, 0 EMPTY), pierwszy widoczny */
    uint8_t grp_hold, grp_done;          /* EDIT / ENV / LFO trzymany: po chwili pierwsza strona sekcji */
    uint8_t gest, gest_used, gest_src;   /* v0.35: ENV / LFO trzymany na stronie silnika albo HOME (przycisk + 1);
                                          * (v0.65) gałka = AMP ENV / LFO 1, klawisz OP = strona MOD (gest_src: GEST_*);
                                          * bez kręcenia działa przy puszczeniu */
    int8_t qback;                        /* (v0.65) strona chwilowa: powrót na tę stronę (-1: HOME) */
    uint8_t mod_src, mod_pg, mod_n, mod_btn;   /* (v0.65) strona MOD: źródło, strona celów, ile celów, jej przycisk */
    int8_t mod_back;                     /* skąd przyszliśmy (-1: HOME) */
    uint32_t gest_at;
    int32_t glo_back;                    /* GLO: skąd przyszliśmy (drugie GLO wraca tam, na tę samą stronę) */
    uint32_t grp_at;
    uint8_t home_pend, home_used;        /* HOME na HOME: okno ENGINE przy puszczeniu, chyba że kręcono PRESETS */
    uint8_t home_hold;                   /* (v0.86) HOME trzymany na głównym ekranie: po chwili SOLO / MULTI */
    uint32_t home_at;
    uint32_t mode_until;                 /* (v0.86) do kiedy napis SOLO / MULTI po zmianie trybu (0: nie ma) */
    uint8_t mtl_cur, mtl_top;            /* (v0.86) lista silników ścieżki: kursor, pierwszy widoczny wiersz */
    uint8_t scn_cur;                     /* (v0.86) lista scen: kursor */
    int32_t scn_back;                    /* lista scen: skąd przyszliśmy */
    uint32_t input_at;                   /* ostatni dotyk panelu (GLO SCREEN OFF) */
    uint8_t screen_off;
    uint8_t save_new;                    /* okno SAVE: 0 NADPISZ, 1 NOWY */
    int32_t save_back;                   /* okno SAVE: strona, na którą wracamy */
    int32_t save_id, cur;                /* okno nazwy: dokąd zapis, kursor */
    int32_t allp_back;                   /* okno ALL PRESETS: preset sprzed otwarcia (HOME wraca) */
    uint8_t allp_sel, allp_del;          /* (v1.0.2) ALL PRESETS: SEL wciśnięty (puszczony krótko = OK, trzymany = usuń USER),
                                          * okno DELETE? (SEL tak, HOME nie) */
    int32_t engp_back;                   /* okno ENGINE: silnik sprzed otwarcia (HOME wraca) */
    int32_t imp_slot, imp_back;          /* okno IMPORT: miejsce 0..31, strona do powrotu */
    uint8_t imp_cz;                      /* okno IMPORT: bank CZ (16 miejsc) zamiast DX7 */
    char name[NAME_LEN + 1];
    uint8_t scope_tail;                  /* po wybrzmieniu jeszcze kilka klatek (płaska linia) */
    uint8_t sq_pg, arp_more;             /* SEQ: strona (NOTES, PATTERN, FEEL, RACK); ARP: strona 1-3 (SELECT) */
    uint8_t sq_set, sq_rk;               /* (v0.88) ostatnia strona ustawień (PATTERN, FEEL); widok: 0 NOTES, 1 RACK */
    uint8_t rk_row, rk_top;              /* (v0.57) RACK: wybrany wiersz (dźwięk 0-15), pierwszy widoczny */
    uint16_t rk_step;                    /* RACK: ostatnio dotknięty krok (1/16 od początku wzoru) */
    uint8_t arp_hold, arp_done;          /* ARP trzymany: po chwili ustawienia (puszczony wcześniej: ON / OFF) */
    uint32_t arp_at;
    int32_t arp_back;                    /* ustawienia ARP: skąd przyszliśmy */
    uint32_t sq_cur;                     /* NOTES: kursor (tick, w gridzie) */
    int32_t sq_low;                      /* NOTES: najniższa widoczna nuta */
    uint16_t sq_len;                     /* długość nowej nuty: ostatnio edytowana (0: grid) */
    uint32_t sq_moved;                   /* kiedy ruszono kursor (PLAY: widok idzie za odtwarzaniem po 2 s) */
    uint8_t st_added;                    /* wpis krokowy: coś dodane (po puszczeniu kursor dalej) */
    uint32_t st_mine[(SQ_NN + 31) / 32]; /* wpis krokowy: dodane w tym akordzie (nie usuwać) */
    uint8_t sqt_cur, sqt_sure;           /* okno narzędzi wzoru */
    uint8_t sqt_from;                    /* (v0.66.2) MOVE HERE FROM: ścieżka, z której nuty (gałka) */
    int16_t sq_note;                     /* NOTES: podświetlona nuta (jedna, jak w Melodee), -1 pusty krok */
    uint8_t sq_rng;                      /* NOTES: zaznaczony zakres (SEL + SELECT, jak w FL): edycja i karta na wszystkich */
    uint32_t sq_ra, sq_re;               /* zakres: komórka startu i końca (tick w gridzie, w obie strony) */
    uint8_t play_hold;                   /* PLAY w NOTES: krótko = od kursora, trzymany = od początku */
    uint8_t rec_hold;                    /* (v0.92) REC: krótko = nagrywanie, trzymany = strona RECORD */
    uint32_t rec_at;
    uint32_t play_at;
    uint8_t sq_pitch;                    /* wysokość nowej nuty z K2 (ostatnio wpisana / zmieniona) */
    uint8_t sqn_row;                     /* karta nuty: wiersz */
    uint8_t st_on;                       /* klawisze wpisują (pusty krok albo REC) do puszczenia wszystkich */
    uint8_t sq_zv, sq_zh;                /* NOTES: zoom w pionie (SEL + K1: ile rzędów), w poziomie (SEL + K2: takty) */
    uint8_t cp_k, cp_yes;                /* (v0.92) kopia dźwięku: klawisz trzymany z SEL (+ 1), wybór TAK */
    int8_t cp_a, cp_b, cp_back;          /* skąd, dokąd (dźwięk), strona sprzed okna */
    uint8_t redraw_roll;                 /* odtwarzanie: tylko piano roll (bez paska i chipów: mniej przesyłania) */
    uint8_t redraw;
} ui = {.page = V_HOME, .knob = -1, .sq_low = 55, .sq_note = -1, .sq_pitch = 60, .sq_zv = 1, .sq_zh = 2, .pf_mac = {0, 0, 0, 100}};
#ifdef PLAT_WEB
#pragma push_macro("__attribute__")
#undef __attribute__
__attribute__((used, visibility("default"))) int32_t ogs_pf(int32_t w)   /* testy (v0.47): PERFORM — efekt, nakładane, czerwony, LATCH */
{
    if (w >= 100 && w < 100 + PF_N)          /* (v0.90) ustawienia klawisza */
        return glob.pfs[w - 100];
    return w == 0 ? ogs_pf_pb : w == 1 ? (int32_t)ogs_pf_st : w == 2 ? (int32_t)ogs_pf_red : w == 3 ? ui.pf_latch
         : w == 8 ? (int32_t)ogs_pf_mem : w == 9 ? ogs_pf_cv : w == 10 ? ogs_pf_cstep : w == 11 ? ui.pf_bank   /* (v0.91) krzywe */
         : w >= 200 && w < 200 + (int32_t)sizeof glob.gbc ? glob.gbc[w - 200] : ui.pf_mac[w & 3];   /* (8: pamięć PF_MEM_*, v0.47.2) */
}
#pragma pop_macro("__attribute__")
#endif
#define QUICK_MS 2000u
#define AUTOSAVE_MS 5000u
#define SEL_SHOW_MS 400u                     /* SEL trzymany dłużej: dopiero wtedy świecą skróty; (v0.91, decyzja użytkownika 2026-11-03)
                                              * wszędzie 0,4 s (było 0,35 / 0,3 / 0,4), lista efektów na EQ 0,8 s (SEL + gałka = Q) */
#define SEL_EQ_MS 800u
#define HOME_HOLD_MS 700u                    /* (v0.86) HOME trzymany tyle: SOLO / MULTI (krócej: czas na HOME + PRESETS) */
#define MODE_SHOW_MS 1500u                   /* (v0.86) napis SOLO / MULTI po zmianie trybu */
/* skróty świecą; (v0.86) główny ekran MULTI: SEL trzymany = lista silników jak na stronie FX (skróty działają do jej otwarcia, bez światła) */
static int sel_lit(void) { return ui.sel_hold && plat_ms() - ui.sel_at >= SEL_SHOW_MS && !(ui.page == V_HOME && mt_ui()); }
static inline int32_t ui_page(void) { return ui.page; }
static int32_t sq_rows(void);
static int32_t sq_vbars(void);
static int more_on(void);
static uint32_t rk_led_note, rk_led_play;   /* (v0.87) RACK: kroki strony z nutą, krok grający (bity białych klawiszy 0-15) */
static uint32_t sq_led_full, sq_led_mid;    /* (v0.90) NOTES: klawisze nut kroku pod kursorem (podświetlona, reszta) */
static uint32_t sq_shown(const sqn_t *n);
static uint32_t sq_ac_ok;
static uint32_t label_lit(void);
static fx_slot_t *fx_cur(void);
static int32_t ui_sq(int32_t w)          /* (testy; 17 MORE, 18 strona szybka, 19 kursor listy efektów; v0.57: 20 strona SEQ,
                                          * 21 wiersz RACK, 22 krok RACK, 23 MUTE, 24 SOLO, 25 ROLLS) */
{
    return w == 12 ? ui.sq_note : w == 14 ? ui.arp_more : w == 15 ? sq_rows() : w == 16 ? sq_vbars() : w == 17 ? more_on()
         : w == 18 ? ui.quick : w == 19 ? ui.fxl_cur : w == 20 ? ui.sq_pg : w == 21 ? ui.rk_row : w == 22 ? ui.rk_step
         : w == 23 ? sqp.mute : w == 24 ? sqp.solo : w == 25 ? sqp.rolls : w == 26 ? ui.scn_cur : w == 27 ? ui.mode_until != 0u
         : w == 28 ? (int32_t)(sq_len() / SQ_STEP) : w == 29 ? (int32_t)rk_led_note : w == 30 ? (int32_t)rk_led_play   /* (v0.87) */
         : w == 31 ? (int32_t)vol_pct() : w == 32 ? (int32_t)label_lit() : w == 33 ? fx_cur()->type   /* (v0.88) */
         : w == 34 ? (int32_t)sq_led_full : w == 35 ? (int32_t)sq_led_mid : w == 36 ? (int32_t)(uintptr_t)ui.msg[0]   /* (v0.90) */
         : w == 37 ? ui.sqn_row : w >= 40 && w < 40 + SCN_N * MT_N ? glob.scn[(w - 40) / MT_N].pre[(w - 40) % MT_N]   /* (v0.90) testy: presety scen */
         : w >= 1000 ? (sq_ac_ok = 0, (int32_t)sq_shown(&sqp.n[(uint32_t)(w - 1000) % SQ_NN]))
         : (int32_t)ui.sq_cur;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void set_param(int32_t id, int32_t v)
{
    const pdef_t *d = E->def(id);
    int32_t i;
    v = clamp(v, d->lo, d->hi);
    if (v == pget(id))
        return;
    ui.redraw = 1;
    if (id >= E->np) {
        if (!E->vset(id, v))                 /* tylko widok: brzmienie bez zmian */
            return;
    } else {
        int rt = 0;
        for (i = 0; i < 3 && E->retrig[i] >= 0; i++)
            rt |= id == E->retrig[i];
        if ((rt || d->fmt == F_LIST) && eng_sounding() && dk_w - dk_r < DK_Q) {   /* skok w dźwięku: przy zerze */
            uint32_t k = dk_w % DK_Q;
            dk_q[k].eng = set.eng;
            dk_q[k].id = (int16_t)id;
            dk_q[k].v = (int16_t)v;
            dk_w++;
        } else {
            E->p[id] = (int16_t)v;
        }
        if (rt)                              /* nowy przydział nut: bez wiszących dźwięków */
            ev_push(EV_ALL_OFF);
    }
    set.edited[set.eng] = (uint8_t)snap_differs(set.eng);   /* wróciło do presetu: SAVE gaśnie */
    set_dirty = 1;
}

static int text_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* krok gałki (d: kliki po przyspieszeniu): parametr z własnym tekstem (czasy, podziały taktu, Hz) przeskakuje wartości,
 * które na ekranie wyglądają tak samo — każdy klik coś zmienia (LFO SYNC: 8 BARS -> 4 BARS jednym klikiem) */
/* (v1.1.0, P9) GLO > PLAY > TUNE KNOB SEMITONES: strojenie w centach (MINI OSC 2-4 TUNE) płynnie do +-1 półtonu, dalej co
 * półton; zapisane wartości bez zmian (stare presety grają tak samo, np. +350 zostaje, kręcenie idzie do 400 / 300) */
static int32_t tune_semi(int32_t v, int32_t d)
{
    const int32_t t = v + d, f = v >= 0 ? v / 100 * 100 : -((-v + 99) / 100 * 100);   /* f: v w dół do pełnego półtonu */
    if (t >= -100 && t <= 100)
        return t;
    if (v > -100 && v < 100)                     /* z płynnej strefy: najpierw na +-1 półton */
        return t > 0 ? 100 : -100;
    return d > 0 ? f + 100 : (f == v ? v - 100 : f);
}
static int32_t param_step(int32_t id, int32_t d)
{
    const pdef_t *df = E->def(id);
    int32_t v = pget(id), s = d > 0 ? 1 : -1, n = d > 0 ? d : -d;
    char a[24], b[24];
    if (E->step && (n = E->step(id, v, d)) >= 0)   /* v0.35: lista w innej kolejności niż numery (WAVE TABLE) */
        return n;
    if (GV(G_TUNEK) && set.eng == ENG_MOOG && (id == P_O2D || id == P_O3D || id == P_O4D))
        return clamp(tune_semi(v, d), df->lo, df->hi);
    n = d > 0 ? d : -d;
    if (df->fmt != F_CUSTOM)
        return v + d;
    while (n-- > 0) {
        value_text(id, v, a);
        do {
            if (v + s < df->lo || v + s > df->hi)
                return v;
            v += s;
            value_text(id, v, b);
        } while (text_eq(a, b));
    }
    return v;
}

/* (v0.45) CHORD: wspólna strona EDIT każdego silnika, za VOICE (decyzja użytkownika 2026-10-28). Numer PG_CHORD leży
 * poza stronami silników — silnik o niej nie wie: sekcje (grp_n / grp_at), gałka, SEL, ekran robi app.c. Akord to
 * dodatki presetu (xtc: [0] rodzaj, [1..6] nauczony), jak dotąd. */
#define PG_CHORD 100
static int32_t chord_slot(int32_t g)     /* w sekcji g CHORD stoi za tą pozycją (-1: nie w tej sekcji) */
{
    int32_t v = E->label_page(K_GLOBAL, G_EDIT, -1), i;
    if ((g != G_EDIT && g != G_ALL) || ENG_GRP[set.eng] == EG_DRUM)   /* (v0.92, decyzja użytkownika 2026-11-04) perkusja: bez strony CHORD */
        return -1;
    for (i = 0; i < E->ngrp[g]; i++)
        if (E->grp[g][i] == v)
            return i;
    return E->ngrp[g] - 1;
}
static int32_t grp_n(int32_t g) { return E->ngrp[g] + (chord_slot(g) >= 0); }
static int32_t grp_at(int32_t g, int32_t i)
{
    int32_t a = chord_slot(g);
    return a < 0 || i <= a ? E->grp[g][i] : i == a + 1 ? PG_CHORD : E->grp[g][i - 1];
}
static const char *pg_name(int32_t page) { return page == PG_CHORD ? "CHORD" : E->page_name[page]; }
static void chord_turn(uint32_t k, int32_t e);
static void chord_learn(void);

static int32_t page_group(int32_t page)
{
    int32_t g, i;
    for (g = 0; g < G_ALL; g++)
        for (i = 0; i < grp_n(g); i++)
            if (grp_at(g, i) == page)
                return g;
    return G_EDIT;
}

static int has_more(int32_t page)
{
    int32_t k;
    if (page == PG_CHORD)
        return 0;
    for (k = 0; E->more && k < 4; k++)
        if (E->more(page, k) >= 0)
            return 1;
    return 0;
}

/* MORE: tylko przycisk sekcji w tej sekcji (v0.14.2: SEL trzymany + gałka już nie, gałka kręci zwykły parametr) */
static int more_on(void) { return ui.more_lock && ui.page >= 0 && has_more(ui.page); }

static int32_t knob_id(int32_t k)        /* parametr pod gałką na bieżącej stronie (warstwa MORE: przycisk sekcji;
                                           * WAVE DRAW 1-4 / 5-8: SEL trzymany = Q, osobno od MORE) */
{
    if (set.eng == ENG_WT && ui.sel_hold && (ui.page == WP_DRAW1 || ui.page == WP_DRAW2))
        return W_DRAWQ + (ui.page == WP_DRAW2 ? 4 : 0) + k;
    if (ui.page == PG_CHORD)                 /* (gałka CHORD: chord_turn) */
        return -1;
    return more_on() ? E->more(ui.page, k) : E->knob(ui.page, k);
}

static void show(int32_t grp, int32_t idx)
{
    int32_t n = grp_n(grp);
    glob_commit();
    if (ui.page < 0 || page_group(ui.page) != grp)   /* inna sekcja: bez MORE (w sekcji MORE zostaje przy SELECT) */
        ui.more_lock = 0;
    ui.grp = grp;
    ui.idx = (idx % n + n) % n;
    ui.page = grp_at(grp, ui.idx);
    ui.last[set.eng][page_group(ui.page)] = ui.page;
    ui.knob = -1;
    ui.quick = 0;
    ui.browse = 0;
    ui.redraw = 1;
}

static void show_page(int32_t page)      /* strona w swojej sekcji */
{
    int32_t g = page_group(page), i;
    for (i = 0; i < grp_n(g) && grp_at(g, i) != page; i++)
        ;
    show(g, i);
}

/* przycisk sekcji: będąc w niej MORE włącz / wyłącz (strony: SELECT), z innego miejsca strona, na której z niej
 * wyszliśmy. Strona szybka z HOME (gałki z HOME) liczy się jak ta strona: przycisk jej sekcji od razu daje MORE
 * (decyzja użytkownika 2026-10-26), a strona zostaje (bez powrotu do HOME po 2 s) */
static void goto_group(int32_t grp)
{
    int32_t i, at = 0;
    for (i = 0; i < grp_n(grp); i++) {
        if (grp_at(grp, i) == ui.page) {
            if (ui.quick)
                show_page(ui.page);          /* zwykła strona sekcji: zostaje */
            if (has_more(ui.page))
                ui.more_lock ^= 1u;
            else
                app_say(pg_name(ui.page), "NO MORE");
            ui.knob = -1;
            ui.redraw = 1;
            return;
        }
        if (grp_at(grp, i) == ui.last[set.eng][grp])
            at = i;
    }
    show(grp, at);
}

static void go_home(void)
{
    glob_commit();
    ui.more_lock = 0;
    ui.page = V_HOME;
    ui.knob = -1;
    ui.quick = 0;
    ui.browse = 0;
    ui.redraw = 1;
}

/* ------------------------------------------------------------------- FX --- */
/* lista efektów (SEL trzymany): EMPTY pierwsze, dalej w grupach (v0.55, decyzja użytkownika 2026-10-29: jak sekcje
 * w ALL PRESETS, ALGO skacze po grupach). Numery FX_* zostają (zapisane presety) */
static const uint8_t FX_ORDER[FX_NTYPE + 1] = {FX_NONE,
                                               FX_DIST, FX_SAT, FX_CRUSH, FX_LOFI,                         /* DRIVE */
                                               FX_COMP, FX_MBAND, FX_EQ, FX_SIDECH, FX_TRANS,              /* DYNAMICS */
                                               FX_FILTER, FX_AWAH, FX_VOWEL, FX_COMB,                      /* FILTER */
                                               FX_CHORUS, FX_FLANGER, FX_PHASER, FX_TREM, FX_RING, FX_HYPER,   /* MOD */
                                               FX_OCT, FX_SHIFT, FX_SUB, FX_FSHIFT,                        /* PITCH */
                                               FX_DELAY, FX_STUTTER, FX_GRAIN,                             /* TIME */
                                               FX_REVERB, FX_SHIMMER};                                     /* SPACE (v0.93: nowe na końcu grup) */
enum { FXG_N = 7 };
static const struct { const char *name; uint8_t at; } FX_GRP[FXG_N] = {   /* grupa: nazwa, pierwsze miejsce w FX_ORDER */
    {"DRIVE", 1}, {"DYNAMICS", 5}, {"FILTER", 10}, {"MOD", 14}, {"PITCH", 20}, {"TIME", 24}, {"SPACE", 27}};
static int32_t fx_grp(uint32_t r)        /* grupa miejsca r na liście, -1 EMPTY */
{
    int32_t g = FXG_N - 1;
    while (g >= 0 && r < FX_GRP[g].at)
        g--;
    return g;
}
static uint32_t fx_rank(uint32_t t)      /* miejsce efektu t na liście */
{
    uint32_t i;
    for (i = 0; i < FX_NTYPE + 1u; i++)
        if (FX_ORDER[i] == t)
            return i;
    return 0;
}
static const char *const FX_NAME[FX_NTYPE + 1] = {"DISTORTION", "BITCRUSHER", "CHORUS", "REVERB", "EQ", "COMPRESSOR", "DELAY",
                                              "SATURATION", "OCTAVER", "FILTER", "LO-FI", "TONE SHIFT", "RING MOD", "TREMOLO",
                                              "FLANGER", "PHASER", "HYPER/DIM", "MULTIBAND", "AUTO WAH", "SUB", "VOWEL",
                                              "COMB", "STUTTER", "SIDECHAIN", "TRANSIENT", "FREQ SHIFT", "GRAIN", "SHIMMER", "EMPTY"};
static const char *const FX_SHORT[FX_NTYPE + 1] = {"DIST", "CRUSH", "CHORUS", "REVERB", "EQ", "COMP", "DELAY", "SAT", "OCT",
                                                   "FILTER", "LO-FI", "SHIFT", "RING", "TREM", "FLANGE", "PHASER", "HYPER",
                                                   "MBAND", "WAH", "SUB", "VOWEL", "COMB", "STUTTER", "SIDECH", "TRANS", "FREQSH",
                                                   "GRAIN", "SHIMMR", "-"};
static const char *const FX_MODE[FX_NTYPE + 1][11] = {
    {"OFF", "SOFT", "HARD", "TUBE", "FUZZ", "FOLD", "RECT"}, {"OFF", "ON"}, {"OFF", "ON"},
    {"OFF", "ROOM", "CHAMBER", "PLATE", "HALL", "BRT HALL", "CATHEDRAL", "AMBIENCE", "CHORUS SP", "DIRTY PLT", "NONLIN"},   /* maks. 9 znaków (kafelek) */
    {"OFF", "ON"}, {"OFF", "CLEAN", "CLASSIC", "OPTO", "PUNCH", "BUS", "PUMP", "MASTER"}, {"OFF", "TAPE", "HIFI", "BBD", "DIGITAL", "GHOST", "PITCH", "REVERSE"}, {"OFF", "TAPE", "TUBE"}, {"OFF", "ON"},
    {"OFF", "LP", "BP", "HP", "DJ"}, {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "SINE", "SQUARE"}, {"OFF", "TREMOLO", "AUTOPAN"},
    {"OFF", "ON"}, {"OFF", "4 STAGE", "8 STAGE", "12 STAGE"}, {"OFF", "ON"}, {"OFF", "OTT", "MULTIBAND"}, {"OFF", "WAH", "LP", "BP", "HP"}, {"OFF", "SINE", "WARM"},
    {"OFF", "MALE", "FEMALE"}, {"OFF", "KEY", "FREE"}, {"OFF", "REPEAT", "REVERSE", "TAPE STOP", "GATE", "RANDOM"}, {"OFF", "BEAT", "KICK"},
    {"OFF", "ON"}, {"OFF", "ON"}, {"OFF", "CLOUD", "REVERSE", "FREEZE"}, {"OFF", "OCTAVE", "FIFTH", "OCT DOWN"}, {"EMPTY"},
};
static const char *const FX_PNAME[FX_NTYPE + 1][FX_NP] = {
    {"DRIVE", "TONE", "MIX", "LEVEL", "TYPE", "BIAS", "LOW CUT", "HIGH CUT"},
    {"BITS", "RATE", "JITTER", "MIX", "", "TONE", "LEVEL", ""},
    {"RATE", "DEPTH", "FEEDBACK", "MIX", "DELAY 1", "DELAY 2", "LPF", "WIDTH"},
    {"DECAY", "SIZE", "PREDELAY", "MIX", "MODE", "COLOR", "ATTACK", "DIFFUSION"},
    {"GAIN 1", "GAIN 2", "FREQ 2", "GAIN 4", "LOW CUT", "HIGH CUT", "LEVEL", ""},   /* (v0.92) EQ: pasma 1-4 */
    {"THRESH", "RATIO", "ATTACK", "RELEASE", "STYLE", "MAKEUP", "MIX", "KNEE"},
    {"TIME", "FEEDBACK", "AGE", "MIX", "MODE", "STYLE", "DIFFUSION", "SIZE"},   /* AGE: nazwa z trybem (fx_pname) */
    {"DRIVE", "TONE", "MIX", "LEVEL", "TYPE", "", "", ""},
    {"OCT -1", "OCT -2", "DRY", "TONE", "", "", "", ""},
    {"CUTOFF", "RESO", "DRIVE", "MIX", "TYPE", "LFO RATE", "LFO DEPTH", ""},
    {"WOW", "FLUTTER", "NOISE", "MIX", "SATURATE", "TONE", "", ""},
    {"SHIFT", "RANGE", "MIX", "WINDOW", "FEEDBACK", "", "", ""},
    {"FREQ", "MIX", "LFO RATE", "LFO DEPTH", "TYPE", "", "", ""},
    {"RATE", "DEPTH", "SHAPE", "PHASE", "TYPE", "", "", ""},
    {"RATE", "DEPTH", "FEEDBACK", "MIX", "MANUAL", "WIDTH", "", ""},
    {"RATE", "DEPTH", "FEEDBACK", "MIX", "TYPE", "CENTER", "WIDTH", ""},
    {"RATE", "DETUNE", "VOICES", "MIX", "", "DIM SIZE", "DIM MIX", ""},
    {"DEPTH", "TIME", "UPWARD", "DOWNWARD", "TYPE", "X LOW", "X HIGH", "OUT GAIN"},
    {"SENS", "RANGE", "RESO", "MIX", "TYPE", "FREQ", "ATTACK", "RELEASE"},
    {"LEVEL", "OCTAVE", "LOW CUT", "GLIDE", "TYPE", "ATTACK", "RELEASE", "BEND"},   /* SUB (v0.39; BEND v0.51) */
    {"VOWEL", "RESO", "MOD", "MIX", "TYPE", "RATE", "ENV", "SHIFT"},            /* VOWEL (v0.40; propozycja) */
    {"TUNE", "FEEDBACK", "DAMP", "MIX", "TYPE", "LFO RATE", "MOD", ""},         /* COMB (v0.40; propozycja; TUNE = FREQ
                                                                                 * w trybie FREE: fx_pname) */
    {"DIV", "CHANCE", "LENGTH", "MIX", "TYPE", "PITCH", "DECAY", "SMOOTH"},     /* STUTTER (v0.43; propozycja) */
    {"DIV", "DEPTH", "LENGTH", "MIX", "TRIG", "CURVE", "ATTACK", ""},           /* SIDECHAIN (v0.88; propozycja) */
    {"ATTACK", "SUSTAIN", "SPEED", "OUTPUT", "MIX", "", "", ""},               /* (v0.93, propozycja) TRANSIENT */
    {"SHIFT", "FEEDBACK", "SPREAD", "MIX", "LFO RATE", "LFO DEPTH", "", ""},   /* FREQ SHIFT */
    {"SIZE", "DENSITY", "PITCH", "MIX", "TYPE", "SPRAY", "SPREAD", "JITTER"},  /* GRAIN */
    {"DECAY", "SHIMMER", "TONE", "MIX", "TYPE", "SIZE", "MOD", ""},            /* SHIMMER */
    {"", "", "", "", "", "", "", ""},
};
/* gałki ponad 8 (fxx), kolejne strony FX: COMPRESSOR 3. strona; MULTIBAND GAIN, potem pasma (tylko TYPE MULTIBAND) */
static const char *const FX_XNAME[FX_NX] = {"RANGE", "LOOKAHEAD", "HOLD", "SC HP", "IN GAIN", "LOW", "MID", "HIGH",
                                            "DN THR", "DN RATIO", "UP THR", "UP RATIO", "DN THR", "DN RATIO", "UP THR",
                                            "UP RATIO", "DN THR", "DN RATIO", "UP THR", "UP RATIO", "MOD", "LOW CUT",
                                            "HIGH CUT", "DUCK", "MOD RATE", "MOD DEPTH", "LOW CUT", "HIGH CUT", "BASS",
                                            "WIDTH", "MODE", "TRIG", "DECAY", "SUSTAIN",
                                            "GAIN 3", "FREQ 1", "FREQ 3", "FREQ 4", "Q 1", "Q 2", "Q 3", "Q 4", "LC SLOPE", "HC SLOPE",
                                            "LOW SHAPE", "HIGH SHAPE", "ANALYZER"};   /* (v0.92) EQ */

static int32_t accel(uint32_t k, int32_t e, int32_t span);
/* (v0.85) strona FX w MULTI: pozycje 0, 1 = MASTER FX (kolejność glob.mfx.rev1st), dalej sloty presetu bez REVERB / DELAY (te w MULTI
 * nie grają jako efekt ścieżki: MIX = start wysyłki; w SOLO zostają) */
static int fx_hide(uint32_t t) { return t == FX_REVERB || t == FX_DELAY; }
static int32_t fx_view_n(void)
{
    const fx_slot_t *ch = fxc.cur[set.eng];
    int32_t k, n = 2;
    if (!mt_ui())
        return FX_SLOTS;
    for (k = 0; k < FX_SLOTS; k++)
        n += !fx_hide(ch[k].type);
    return n;
}
static fx_slot_t *fx_view(int32_t v)
{
    fx_slot_t *ch = fxc.cur[set.eng], *last = ch;
    int32_t k;
    if (!mt_ui())
        return &ch[(uint32_t)v % FX_SLOTS];
    if (v < 2)
        return &glob.mfx.s[(v == 0) == (glob.mfx.rev1st != 0) ? 0 : 1];
    for (k = 0, v -= 2; k < FX_SLOTS; k++)
        if (!fx_hide(ch[k].type)) {
            last = &ch[k];
            if (!v--)
                break;
        }
    return last;
}
static int32_t fx_view_of(int32_t at)    /* pozycja slotu at łańcucha */
{
    const fx_slot_t *ch = fxc.cur[set.eng];
    int32_t k, n = 2;
    if (!mt_ui())
        return at;
    for (k = 0; k < at; k++)
        n += !fx_hide(ch[k].type);
    return n;
}
static fx_slot_t *fx_cur(void) { return fx_view(ui.fx_slot); }
static int fx_is_mst(const fx_slot_t *c) { return c == &glob.mfx.s[0] || c == &glob.mfx.s[1]; }
static int fx_on(const fx_slot_t *c)     /* (v0.88) gra: MASTER FX: przypięty do wybranej ścieżki */
{
    return fx_is_mst(c) ? mt_linked(glob.g[G_MTFOC], c == &glob.mfx.s[1]) : c->mode != 0;
}
/* gałki efektu: 0..7 w slocie, dalej w fxx (bieżący silnik); strony FX po 4 */
static int32_t fx_syncj(const fx_slot_t *c);
/* (v0.55) AUTO WAH: strony zależne od MODE (propozycja). FOLLOW: SENS, RANGE, RESO, MIX | MORE: TYPE, FREQ, ATTACK,
 * RELEASE | MORE 2: MODE. ENV: START, END, RESO, MIX | MORE: TYPE, TRIG, ATTACK, RELEASE | MORE 2: MODE, DECAY, SUSTAIN
 * (START = bajt FREQ, END = bajt RANGE w Hz). Widok j (strona * 4 + gałka) -> gałka w zapisie, -1: puste pole */
enum { AW_MODE = FX_NP, AW_TRIG, AW_DECAY, AW_SUS };
static int awah_env(const fx_slot_t *c) { return c->type == FX_AWAH && fxc.x[set.eng][FX_X_AWAH] != 0u; }
/* (v0.92, plan zatwierdzony: docs/funkcje/eq.md) EQ: GAIN 1-4 | MORE FREQ 1-4 | MORE 2 Q 1-4 | MORE 3 LOW CUT, LC SLOPE, HIGH CUT,
 * HC SLOPE | MORE 4 LOW SHAPE, HIGH SHAPE, LEVEL, ANALYZER (p: dawne gałki, FX_NP + i: ogon EQ) */
#define EQJ(i) (FX_NP + (i))
static const int8_t EQ_MAP[20] = {0, 1, EQJ(EQX_G3), 3, EQJ(EQX_F1), 2, EQJ(EQX_F3), EQJ(EQX_F4), EQJ(EQX_Q1), EQJ(EQX_Q2),
                                  EQJ(EQX_Q3), EQJ(EQX_Q4), 4, EQJ(EQX_LCS), 5, EQJ(EQX_HCS), EQJ(EQX_LSH), EQJ(EQX_HSH), 6, EQJ(EQX_AN)};
static int32_t fx_sj(const fx_slot_t *c, int32_t j)
{
    static const int8_t FOL[12] = {0, 1, 2, 3, 4, 5, 6, 7, AW_MODE, -1, -1, -1};
    static const int8_t ENV[12] = {5, 1, 2, 3, 4, AW_TRIG, 6, 7, AW_MODE, AW_DECAY, AW_SUS, -1};
    if (c->type == FX_EQ)
        return j >= 0 && j < 20 ? EQ_MAP[j] : -1;
    if (c->type != FX_AWAH || j < 0 || j >= 12)
        return j;
    return (awah_env(c) ? ENV : FOL)[j];
}
static int eq_sw(const fx_slot_t *c, int32_t j)   /* (v0.92) EQ: przełącznik (SLOPE, SHAPE, ANALYZER) */
{
    const int32_t s = fx_sj(c, j);
    return c->type == FX_EQ && s >= EQJ(EQX_LCS) && s <= EQJ(EQX_AN);
}
static int eq_qk(const fx_slot_t *c, int32_t j)   /* (v0.92) EQ: Q (zapis 5 bitów: krok 4) */
{
    const int32_t s = fx_sj(c, j);
    return c->type == FX_EQ && s >= EQJ(EQX_Q1) && s <= EQJ(EQX_Q4);
}
static uint8_t *fx_par(const fx_slot_t *c, int32_t j)
{
    static uint8_t none;
    j = fx_sj(c, j);
    return j < 0 ? &none : j < FX_NP ? (uint8_t *)&c->p[j] : fx_is_mst(c) ? &glob.mfx.x[fx_xoff(c->type) + j - FX_NP]
         : &fxc.x[set.eng][fx_xoff(c->type) + j - FX_NP];
}
static const char *fx_pname(const fx_slot_t *c, int32_t j)
{
    if (j == fx_syncj(c))
        return "SYNC";
    if (c->type == FX_AWAH) {
        int32_t s = fx_sj(c, j);
        return s < 0 ? "" : awah_env(c) && s == 5 ? "START" : awah_env(c) && s == 1 ? "END"
             : s < FX_NP ? FX_PNAME[FX_AWAH][s] : FX_XNAME[FX_X_AWAH + s - FX_NP];
    }
    if (c->type == FX_EQ) {                  /* (v0.92) */
        const int32_t s = fx_sj(c, j);
        return s < 0 ? "" : s < FX_NP ? FX_PNAME[FX_EQ][s] : FX_XNAME[FX_X_EQ + s - FX_NP];
    }
    if (c->type == FX_DELAY && j == 2)       /* CHARACTER: AGE, SHIFT (GHOST), PITCH (PITCH, REVERSE) */
        return c->p[FX_TYPEK] == 4u ? "SHIFT" : c->p[FX_TYPEK] >= 5u ? "PITCH" : "AGE";
    if (j >= FX_NP && j - FX_NP >= fx_nx(c->type))   /* ostatnia strona niepełna (REVERB MORE 3) */
        return "";
    return j < FX_NP ? FX_PNAME[c->type][j] : FX_XNAME[fx_xoff(c->type) + j - FX_NP];
}
/* (v0.46) SYNC (decyzja użytkownika 2026-10-28: opcja synchronizacji i jej braku, wszędzie tak samo): zawsze K4 ostatniej
 * strony efektu. FILTER, RING, TREMOLO, FLANGER, PHASER, HYPER, COMB: MORE K4 (wolne); CHORUS, VOWEL, DELAY, STUTTER:
 * własna ostatnia strona „SYNC”. Zapis: dodatki presetu xtc[XT_SYNC..+2], bit rodzaju = przełączony względem domyślnego
 * (STUTTER domyślnie SYNC). ON: RATE / TIME w nutach (gałka / 8), OFF: Hz / ms jak dotąd. */
#define FX_SYNC_PG (1u << FX_CHORUS | 1u << FX_VOWEL | 1u << FX_DELAY | 1u << FX_STUTTER)
static void fx_changed(void);
static int fx_has_sync(uint32_t t) { return t < FX_NTYPE && (FX_SYNC_SET >> t & 1u); }
static int32_t fx_npages(const fx_slot_t *c)
{
    if (c->type == FX_EQ)                        /* (v0.92) 5 stron */
        return 5;
    if (c->type == FX_MBAND)
        return c->p[FX_TYPEK] ? 6 : 3;           /* OTT: progi pasm stałe, bez stron pasm */
    return 2 + (fx_nx(c->type) + 3) / 4 + (c->type < FX_NTYPE && (FX_SYNC_PG >> c->type & 1u));
}
static int32_t fx_syncj(const fx_slot_t *c) { return fx_has_sync(c->type) ? fx_npages(c) * 4 - 1 : -1; }
/* (v0.51) SUB BEND (decyzja użytkownika 2026-10-29): za PITCH BEND ON (domyślnie, jak od v0.45) / OFF; p[7] 0 = ON, więc
 * stare presety bez zmian. Przełącznik jak SYNC: w prawo ON, w lewo OFF. */
static int32_t fx_bendj(const fx_slot_t *c) { return c->type == FX_SUB ? 7 : -1; }
static int fx_sync_bit(uint32_t t)
{
    if (mt_ui() && t == FX_DELAY)            /* (v0.85) DELAY w MULTI = MASTER FX */
        return glob.mfx.sync & 1u;
    return xtc[set.eng][XT_SYNC + t / 8u] >> (t % 8u) & 1u;
}
static int fx_sync_on(uint32_t t) { return fx_sync_bit(t) ^ (t == FX_STUTTER || t == FX_SIDECH); }   /* SYNC włączony (to, co widać) */
static void fx_sync_set(uint32_t t, int on)
{
    uint8_t *b = &xtc[set.eng][XT_SYNC + t / 8u], m = (uint8_t)(1u << (t % 8u));
    if ((fx_sync_on(t) != 0) == (on != 0))
        return;
    if (mt_ui() && t == FX_DELAY) {
        glob.mfx.sync ^= 1u;
        fx_changed();
        return;
    }
    *b ^= m;
    xt_dirty = 1;
    fx_changed();
}
static int32_t fx_ratej(uint32_t t)      /* gałka RATE / TIME efektu z SYNC (-1: brak) */
{
    return t == FX_FILTER || t == FX_VOWEL || t == FX_COMB ? 5 : t == FX_RING ? 2 : fx_has_sync(t) ? 0 : -1;
}
static const char *fx_pgname(const fx_slot_t *c, int32_t pg)
{
    static const char *const MB[6] = {"", "MORE", "GAIN", "LOW", "MID", "HIGH"};
    if (c->type == FX_MBAND)
        return MB[pg % 6];
    if (pg > 1 && pg == fx_npages(c) - 1 && c->type < FX_NTYPE && (FX_SYNC_PG >> c->type & 1u))
        return "SYNC";
    return pg == 1 ? "MORE" : pg == 2 ? "MORE 2" : pg >= 3 ? "MORE 3" : "";
}

static void fx_changed(void)             /* zmiana efektu = zmiana brzmienia (SAVE, zapis sam w ciszy) */
{
    if (fx_is_mst(fx_cur()) && mt_ui()) {    /* (v0.85) MASTER FX: część miksu, nie presetu (SAVE nie świeci) */
        mt_gdirty = 1;
        ui.redraw = 1;
        return;
    }
    eq_pack(set.eng);                        /* (v0.92) ogon EQ -> dodatki presetu */
    fx_dirty = 1;
    set.edited[set.eng] = (uint8_t)snap_differs(set.eng);
    set_dirty = 1;
    ui.redraw = 1;
}

static void fxn_text(float x, int32_t dec, char *t)   /* liczba z dec (0..2) miejscami po kropce */
{
    char n[8];
    int32_t m = dec == 2 ? 100 : dec == 1 ? 10 : 1, i = (int32_t)(x * (float)m + 0.5f);
    int_text(i / m, t, 0);
    if (dec) {
        text_cat(t, ".");
        int_text(i % m + m, n, 0);           /* zera wiodące: 105 -> "05" */
        text_cat(t, n + 1);
    }
}

static void ms_text(float ms, char *t)   /* czas: 0MS, 0.05MS, 2.5MS, 120MS, 1.2S */
{
    if (ms < 0.005f) {
        t[0] = 0;
        text_cat(t, "0MS");
        return;
    }
    if (ms >= 1000.0f) {
        fxn_text(ms / 1000.0f, 1, t);
        text_cat(t, "S");
        return;
    }
    fxn_text(ms, ms < 1.0f ? 2 : ms < 10.0f ? 1 : 0, t);
    text_cat(t, "MS");
}

static void hz_text(float hz, char *t)   /* 88, 2.5K */
{
    if (hz >= 1000.0f) {
        fxn_text(hz / 1000.0f, 1, t);
        text_cat(t, "K");
    } else {
        fxn_text(hz, hz < 10.0f ? 1 : 0, t);
    }
}

static void db_text(float db, char *t)   /* +10.5DB, -33DB */
{
    float a = db < 0.0f ? -db : db;
    t[0] = 0;
    if (db <= -0.05f)
        text_cat(t, "-");
    else if (db >= 0.05f)
        text_cat(t, "+");
    fxn_text(a, a < 10.0f ? 1 : 0, t + (t[0] ? 1 : 0));
    text_cat(t, "DB");
}

static void ratio_text(float r, char *t) /* 2.6:1, 20:1, INF */
{
    if (r <= 0.0f) {
        t[0] = 0;
        text_cat(t, "INF");
        return;
    }
    fxn_text(r, r < 10.0f ? 1 : 0, t);
    text_cat(t, ":1");
}

static void fx_text(const fx_slot_t *c, int32_t k, char *t)   /* wartość gałki do wyświetlenia */
{
    static const char *const DL_DIV[FX_NSYNC] = {"1/1", "1/2D", "1/2", "1/4D", "1/2T", "1/4", "1/8D", "1/4T", "1/8",
                                                 "1/16D", "1/8T", "1/16", "1/32D", "1/16T", "1/32", "1/64"};
    int32_t v;
    float f;
    if (k == fx_syncj(c) || k == fx_bendj(c)) {   /* (v0.46) SYNC, (v0.51) SUB BEND */
        t[0] = 0;
        text_cat(t, (k == fx_syncj(c) ? fx_sync_on(c->type) : !c->p[k]) ? "ON" : "OFF");
        return;
    }
    v = *fx_par(c, k);
    f = (float)v / 127.0f;
    if (c->type == FX_SIDECH && (k == 1 || k == 2 || k == 3 || k == 5 || k == 6 || (k == 0 && c->p[FX_TYPEK]))) {   /* (v0.88) jak ogs_fx.c sidech */
        t[0] = 0;
        if (k == 1 || k == 3)
            int_text((int32_t)(f * 100.0f + 0.5f), t, 0);   /* (skala 0-100) */
        else if (k == 0)
            text_cat(t, mt_ui() ? "T1 KICK" : "NOTES");
        else if (k == 6)
            ms_text(0.5f + f * f * 50.0f, t);
        else if (k == 5)
            int_text(((int32_t)v * 2 - 127) * 100 / 127, t, 1);
        else if (c->p[FX_TYPEK])
            ms_text(30.0f * fm_exp2f(f * 5.0f), t);
        else {
            int_text(5 + (int32_t)(f * 95.0f + 0.5f), t, 0);
            text_cat(t, "%");
        }
        return;
    }
    if (k == fx_ratej(c->type) && c->type != FX_STUTTER && (c->type != FX_DELAY || fx_sync_on(c->type))) {
        static const float LO[FX_NTYPE] = {[FX_CHORUS] = 0.05f, [FX_FILTER] = 0.05f, [FX_RING] = 0.05f, [FX_TREM] = 0.1f,
                                           [FX_FLANGER] = 0.02f, [FX_PHASER] = 0.02f, [FX_HYPER] = 0.1f, [FX_VOWEL] = 0.05f,
                                           [FX_COMB] = 0.05f, [FX_SIDECH] = 0.25f};
        static const float OC[FX_NTYPE] = {[FX_CHORUS] = 7.6f, [FX_FILTER] = 7.6f, [FX_RING] = 7.6f, [FX_TREM] = 7.6f,
                                           [FX_FLANGER] = 8.0f, [FX_PHASER] = 8.0f, [FX_HYPER] = 6.6f, [FX_VOWEL] = 8.64f,
                                           [FX_COMB] = 7.64f, [FX_SIDECH] = 5.0f};   /* jak ogs_fx.c fx_hz */
        t[0] = 0;
        if (fx_sync_on(c->type)) {           /* nuty (gałka / 8) */
            text_cat(t, c->type == FX_DELAY ? DL_DIV[v >> 3] : LFO_DIV[v >> 3]);
        } else {
            float hz = LO[c->type] * fm_exp2f(f * OC[c->type]);
            fxn_text(hz, hz < 1.0f ? 2 : 1, t);
            text_cat(t, "HZ");
        }
        return;
    }
    if (c->type == FX_STUTTER && k == 0 && !fx_sync_on(FX_STUTTER)) {   /* FREE: krok w ms (jak ogs_fx.c) */
        t[0] = 0;
        ms_text(10.0f * fm_exp2f(f * 5.64f), t);
        return;
    }
    if (k == fx_typep(c->type)) {            /* TYPE: nazwa rodzaju */
        t[0] = 0;
        text_cat(t, FX_MODE[c->type][v + 1]);
        return;
    }
    if (c->type == FX_DELAY && k == 0) {     /* TIME w ms / s */
        float ms = fm_exp2f((float)v * (10.97f / 127.0f));
        char n[8];
        if (ms >= 1000.0f) {
            int_text((int32_t)(ms / 1000.0f), t, 0);
            text_cat(t, ".");
            int_text((int32_t)(ms / 100.0f) % 10, n, 0);
            text_cat(t, n);
            text_cat(t, "S");
        } else {
            int_text((int32_t)ms, t, 0);
            text_cat(t, "MS");
        }
        return;
    }
    if (c->type == FX_DELAY && k > 0) {      /* przeliczenia jak w ogs_fx.c delay() */
        static const char *const STY[FX_DL_NSTYLE] = {"SINGLE", "PING-PONG", "RATIO 1/4", "RATIO 1/3", "RATIO 3/8",
                                                      "RATIO 1/2", "RATIO 5/8", "RATIO 2/3", "RATIO 3/4"};
        uint32_t m = c->p[FX_TYPEK];
        t[0] = 0;
        if (k == FX_DL_STYLE)
            text_cat(t, STY[v < FX_DL_NSTYLE ? v : FX_DL_NSTYLE - 1]);
        else if (k == 2 && m == 4u) {        /* GHOST: SHIFT Hz (64 = 0) */
            float a = ((float)v / 127.0f - 0.5f) * 2.0f, hz = fm_exp2f((a < 0.0f ? -a : a) * 9.0f) - 1.0f;
            text_cat(t, hz < 0.05f ? "" : a < 0.0f ? "-" : "+");
            hz_text(hz, t + (t[0] ? 1 : 0));
            text_cat(t, "HZ");
        } else if (k == 2 && m == 5u && v != 64 && (v > 64 ? v - 64 : 64 - v) <= 27) {   /* (v0.55) PITCH: Hz (jak ogs_fx.c
                                                                                         * delay_phz) */
            int32_t a = v > 64 ? v - 64 : 64 - v;
            text_cat(t, v < 64 ? "-" : "+");
            fxn_text(0.1f * fm_exp2f((float)(a - 1) * (7.643856f / 26.0f)), a < 11 ? 2 : 1, t + 1);
            text_cat(t, "HZ");
        } else if (k == 2 && m == 5u) {      /* PITCH: półtony 1 .. 12 po 3 kroki */
            int32_t a = v > 64 ? v - 64 : 64 - v;
            a = v == 64 ? 0 : 1 + (a - 28) / 3;
            int_text(v < 64 ? -(a > 12 ? 12 : a) : a > 12 ? 12 : a, t, 1);
            text_cat(t, " ST");
        } else if (k == 2 && m >= 6u) {      /* REVERSE: półtony */
            int_text((int32_t)(((float)v - 64.0f) * (12.0f / 63.0f) + (v >= 64 ? 0.5f : -0.5f)), t, 1);
            text_cat(t, " ST");
        } else if (k == FX_NP + 1) {         /* LOW CUT */
            if (v)
                hz_text(20.0f * fm_exp2f(f * 5.3f), t);
            else
                text_cat(t, "OFF");
        } else if (k == FX_NP + 2) {         /* HIGH CUT */
            if (v >= 127)
                text_cat(t, "OFF");
            else
                hz_text(20000.0f * fm_exp2f(-(float)(127 - v) * (6.6f / 127.0f)), t);
        } else {                             /* reszta 0..100 (skala ekranu) */
            int_text((v * 100 + 63) / 127, t, 0);
        }
        return;
    }
    if (c->type == FX_SHIFT && k <= 1) {     /* SHIFT w półtonach (w zakresie RANGE), RANGE */
        int32_t range = 1 + c->p[1] * 23 / 127;
        if (k == 1) {
            int_text(range, t, 0);
            text_cat(t, " ST");
        } else {
            int_text((int32_t)(((float)v - 64.0f) / 63.0f * (float)range + (v >= 64 ? 0.5f : -0.5f)), t, 1);
            text_cat(t, " ST");
        }
        return;
    }
    if (c->type == FX_FLANGER && k == 2) {   /* FEEDBACK wokół zera */
        int_text(v - 64, t, 1);
        return;
    }
    if (c->type == FX_CRUSH && k == 0) {     /* BITS 1..16 */
        int_text(1 + v * 15 / 127, t, 0);
        return;
    }
    if (c->type == FX_CRUSH && k == 1) {     /* RATE w Hz po zmniejszeniu częstotliwości */
        int_text((int32_t)(44100.0f / fm_exp2f((float)v * (6.0f / 127.0f))), t, 0);
        return;
    }
    if (c->type == FX_DIST && k == 0) {      /* DRIVE w dB */
        int_text(v * 42 / 127, t, 0);
        text_cat(t, "DB");
        return;
    }
    if ((c->type == FX_DIST && (k == 5 || k == 3)) || (c->type == FX_CRUSH && k == 6) || (c->type == FX_SAT && k == 3)) {
        int_text(v - 64, t, 1);              /* BIAS, LEVEL: wokół środka */
        return;
    }
    if (c->type == FX_EQ) {                  /* (v0.92) ogon EQ (dawne gałki niżej) */
        static const char *const SW[5][2] = {{"12 DB", "24 DB"}, {"12 DB", "24 DB"}, {"SHELF", "BELL"}, {"SHELF", "BELL"}, {"ON", "OFF"}};
        const int32_t s = fx_sj(c, k) - FX_NP;
        t[0] = 0;
        if (s == EQX_G3) {
            int_text((v - 64) * 12 / 64, t, 1);
            text_cat(t, "DB");
            return;
        }
        if (s >= EQX_F1 && s <= EQX_F4) {
            hz_text(20.0f * fm_exp2f((float)v * (9.9657843f / 127.0f)), t);
            return;
        }
        if (s >= EQX_Q1 && s <= EQX_Q4) {
            text_cat(t, "Q ");
            fxn_text(0.3f * fm_exp2f((float)v * (5.3219281f / 127.0f)), 1, t + 2);
            return;
        }
        if (s >= EQX_LCS && s <= EQX_AN) {
            text_cat(t, SW[s - EQX_LCS][v ? 1 : 0]);
            return;
        }
        k = fx_sj(c, k);                     /* dawne gałki: p[k] */
    }
    if (c->type == FX_EQ && (k <= 3 || k == 6) && k != 2) {   /* LOW, MID, HIGH, LEVEL: dB wokół 0 */
        int_text((v - 64) * (k == 6 ? 12 : 12) / 64, t, 1);
        text_cat(t, "DB");
        return;
    }
    if (c->type == FX_DIST && k >= 6) {      /* LOW CUT, HIGH CUT (przed przesterem): Hz */
        float hz = k == 6 ? 15.0f * fm_exp2f(f * 5.7f) : 20000.0f * fm_exp2f(-(float)(127 - v) * (5.3f / 127.0f));
        t[0] = 0;
        if (k == 7 && v >= 127)
            text_cat(t, "OFF");
        else
            hz_text(hz, t);
        return;
    }
    if ((c->type == FX_EQ && (k == 2 || k == 4 || k == 5))) {   /* FREQ, LOW CUT, HIGH CUT: Hz */
        float hz = k == 2 ? 150.0f * fm_exp2f((float)v * (5.3f / 127.0f)) : k == 4 ? 20.0f * fm_exp2f((float)v * (5.3f / 127.0f))
                                                                             : 20000.0f * fm_exp2f(-(float)(127 - v) * (5.3f / 127.0f));
        if ((k == 4 && !v) || (k == 5 && v == 127)) {
            t[0] = 0;
            text_cat(t, "OFF");
        } else if (hz >= 1000.0f) {
            char n[8];
            int_text((int32_t)(hz / 1000.0f), t, 0);
            text_cat(t, ".");
            int_text((int32_t)(hz / 100.0f) % 10, n, 0);
            text_cat(t, n);
            text_cat(t, "K");
        } else {
            int_text((int32_t)hz, t, 0);
        }
        return;
    }
    if (c->type == FX_COMP) {                /* przeliczenia jak w ogs_fx.c comp() */
        t[0] = 0;
        switch (k) {
        case 0: db_text(-f * 60.0f, t); break;
        case 1: ratio_text(v >= 127 ? 0.0f : fm_exp2f(f * 4.32f), t); break;
        case 2: ms_text(0.05f * fm_exp2f(f * 12.3f), t); break;
        case 3: ms_text(5.0f * fm_exp2f(f * 9.97f), t); break;
        case 5:
            if (v)
                db_text((float)(v - 1) * (24.0f / 126.0f), t);
            else
                text_cat(t, "AUTO");
            break;
        case 6: int_text(v * 100 / 127, t, 0); text_cat(t, "%"); break;
        case 7: db_text(f * 24.0f, t); break;
        case 8:
            if (v >= 127)
                text_cat(t, "FULL");
            else
                db_text(-(1.0f + f * 59.0f), t);
            break;
        case 9: ms_text(f * 10.0f, t); break;
        case 10: ms_text(f * 500.0f, t); break;
        default:
            if (v)
                hz_text(20.0f * fm_exp2f(f * 4.6f), t);
            else
                text_cat(t, "OFF");
            break;
        }
        return;
    }
    if (c->type == FX_MBAND) {               /* przeliczenia jak w ogs_fx.c mband() */
        t[0] = 0;
        if (k >= 12) {                       /* pasma: DN THR, DN RATIO, UP THR, UP RATIO */
            if (k % 4 == 0 || k % 4 == 2)
                db_text(-60.0f + f * 60.0f, t);
            else
                ratio_text(v >= 127 ? 0.0f : fm_exp2f(f * 6.0f), t);
            return;
        }
        switch (k) {
        case 1: int_text((int32_t)(100.0f * fm_exp2f(((float)v - 64.0f) / 32.0f) + 0.5f), t, 0); text_cat(t, "%"); break;
        case 5: hz_text(30.0f * fm_exp2f(f * 4.0f), t); break;
        case 6: hz_text(1000.0f * fm_exp2f(f * 3.32f), t); break;
        case 7: case 8: case 9: case 10: case 11: db_text(((float)v - 64.0f) * 0.375f, t); break;
        default: int_text(v * 100 / 127, t, 0); text_cat(t, "%"); break;
        }
        return;
    }
    if (c->type == FX_HYPER) {               /* przeliczenia jak w ogs_fx.c hyper() */
        t[0] = 0;
        switch (k) {
        case 0: fxn_text(0.1f * fm_exp2f(f * 6.6f), 1, t); text_cat(t, "HZ"); break;
        case 1: int_text((int32_t)(f * 40.0f + 0.5f), t, 0); text_cat(t, " CT"); break;
        case 2: int_text(2 + v * 5 / 127, t, 0); break;
        default: int_text(v * 100 / 127, t, 0); text_cat(t, "%"); break;
        }
        return;
    }
    if (c->type == FX_VOWEL) {               /* przeliczenia jak w ogs_fx.c vowel() */
        static const char VW[] = "AEIOU";
        t[0] = 0;
        switch (k) {
        case 0: {                            /* samogłoska: A, A-E, E ... U */
            int32_t a = v * 4 / 127, b = (v * 8 + 63) / 127;
            t[0] = VW[a];
            t[1] = 0;
            if (b & 1 && a < 4) {
                t[1] = '-';
                t[2] = VW[a + 1];
                t[3] = 0;
            }
            break;
        }
        case 5: fxn_text(0.05f * fm_exp2f(f * 8.64f), 2, t); text_cat(t, "HZ"); break;
        case 7: {
            int32_t c2 = (v - 64) * 12 / 64;
            int_text(c2, t, 1);
            text_cat(t, " ST");
            break;
        }
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    if (c->type == FX_STUTTER) {             /* przeliczenia jak w ogs_fx.c stutter() */
        static const char *const DIV[7] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"};
        t[0] = 0;
        switch (k) {
        case 0: text_cat(t, DIV[v * 7 / 128]); break;
        case 2: int_text(1 + v * 8 / 128, t, 0); text_cat(t, v * 8 / 128 ? " STEPS" : " STEP"); break;
        case 5: int_text((v - 64) * 12 / 64, t, 1); text_cat(t, " ST"); break;
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    if (c->type == FX_COMB) {                /* przeliczenia jak w ogs_fx.c comb() */
        t[0] = 0;
        switch (k) {
        case 0:
            if (c->mode == 2u) {             /* FREE: częstotliwość */
                hz_text(20.0f * fm_exp2f(f * 6.64f), t);
            } else {                         /* KEY: półtony od nuty */
                int32_t st = (v - 64) * 24 / 64;
                int_text(st, t, 1);
                text_cat(t, " ST");
            }
            break;
        case 1: {
            int32_t q = ((int32_t)v - 64) * 97 / 64;
            int_text(q, t, 1);
            text_cat(t, "%");
            break;
        }
        case 5: fxn_text(0.05f * fm_exp2f(f * 7.64f), 2, t); text_cat(t, "HZ"); break;
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    if (c->type == FX_SUB) {                 /* przeliczenia jak w ogs_fx.c subfx() */
        static const char *const OCT[3] = {"-2 OCT", "-1 OCT", "0 OCT"};
        t[0] = 0;
        switch (k) {
        case 1: text_cat(t, OCT[v * 3 / 128]); break;
        case 2: if (v) hz_text(20.0f * fm_exp2f(f * 3.64f), t); else text_cat(t, "OFF"); break;
        case 3: if (v) ms_text(1000.0f * (0.002f + f * f), t); else text_cat(t, "OFF"); break;
        case 5: ms_text(fm_exp2f(f * 6.64f), t); break;
        case 6: ms_text(5.0f * fm_exp2f(f * 8.64f), t); break;
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    if (c->type == FX_AWAH) {                /* przeliczenia jak w ogs_fx.c autowah() */
        int env = awah_env(c);
        t[0] = 0;
        switch (fx_sj(c, k)) {
        case 1: {                            /* RANGE: oktawy, 64 = 0; ENV: END (Hz jak FREQ) */
            float o = (f - 0.5f) * 10.0f;
            if (env) {
                hz_text(40.0f * fm_exp2f(f * 8.5f), t);
                break;
            }
            if (v != 64)
                text_cat(t, v < 64 ? "-" : "+");
            fxn_text(o < 0.0f ? -o : o, 1, t + (t[0] ? 1 : 0));
            text_cat(t, " OCT");
            break;
        }
        case 5: hz_text(40.0f * fm_exp2f(f * 8.5f), t); break;
        case 6: ms_text(env ? fm_exp2f(f * 13.0f) : 0.5f * fm_exp2f(f * 8.0f), t); break;   /* ENV: A / D / R 1 ms .. 8 s */
        case 7: ms_text(env ? fm_exp2f(f * 13.0f) : 10.0f * fm_exp2f(f * 7.6f), t); break;
        case AW_MODE: text_cat(t, v ? "ENV" : "FOLLOW"); break;
        case AW_TRIG: text_cat(t, v ? "LEGATO" : "EVERY"); break;
        case AW_DECAY: ms_text(fm_exp2f(f * 13.0f), t); break;
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    if (c->type == FX_REVERB) {              /* przeliczenia jak w ogs_fx.c reverb() (v0.34) */
        static const float DEC[10] = {0.7f, 0.9f, 1.0f, 1.2f, 1.2f, 2.0f, 0.35f, 1.1f, 1.0f, 1.0f};   /* RVM[].dec */
        static const char *const COL[FX_RV_NCOLOR] = {"70S", "80S", "NOW"};
        uint32_t m = c->p[FX_TYPEK] < 10u ? c->p[FX_TYPEK] : 9u;
        t[0] = 0;
        switch (k) {
        case 0:                              /* DECAY (NONLIN: czas bramki) */
            if (m == 9u)
                ms_text(50.0f * fm_exp2f(f * 6.32f), t);   /* v0.55: do 4 s */
            else
                ms_text(1000.0f * 0.2f * fm_exp2f(f * 6.0f) * DEC[m], t);
            break;
        case 2: ms_text(f * 1000.0f * 8190.0f / 44100.0f, t); break;
        case FX_RV_COLOR: text_cat(t, COL[v < FX_RV_NCOLOR ? v : FX_RV_NCOLOR - 1]); break;
        case FX_NP: fxn_text(0.05f * fm_exp2f(f <= 0.75f ? f * 6.6f : 4.95f + (f - 0.75f) * 14.78f), 2, t);   /* do 20 Hz (v0.55) */
            text_cat(t, "HZ");
            break;
        case FX_NP + 2: hz_text(20.0f * fm_exp2f(f * 5.0f), t); break;
        case FX_NP + 3: hz_text(1000.0f * fm_exp2f(f * 4.32f), t); break;
        case FX_NP + 4: fxn_text(fm_exp2f((f - 0.5f) * 2.0f), 2, t); text_cat(t, "X"); break;
        default: int_text((v * 100 + 63) / 127, t, 0); break;
        }
        return;
    }
    int_text(v, t, 0);
}

static void go_fx(void)                  /* FX: łańcuch efektów, drugi raz = HOME */
{
    if (ui.page == V_FX) {
        go_home();
        return;
    }
    glob_commit();
    ui.page = V_FX;
    ui.fx_more = 0;
    ui.knob = -1;
    ui.quick = 0;
    ui.redraw = 1;
}

static void fx_knob(uint32_t k, int32_t e)   /* K1..K4 na stronie ui.fx_more (FX przełącza: MORE, dalej kolejne) */
{
    fx_slot_t *c = fx_cur();
    int32_t j = (int32_t)k + ui.fx_more * 4;
    const int qs = c->type == FX_EQ && ui.sel_hold;   /* (v0.91, decyzja użytkownika 2026-11-03) EQ: SEL + gałka = Q pasma, jak DRAW */
    uint8_t *pv;
    if (qs) {
        ui.sel_used = 1;
        j = 8 + (int32_t)k;
    }
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    if (ui.fx_more >= fx_npages(c) || !fx_pname(c, j)[0])   /* puste pole */
        return;
    if (j == fx_syncj(c)) {                  /* (v0.46) SYNC: w prawo ON, w lewo OFF */
        fx_sync_set(c->type, e > 0);
        return;
    }
    if (eq_sw(c, j)) {                       /* (v0.92) EQ: SLOPE 24 dB / SHAPE BELL / ANALYZER OFF w prawo */
        uint8_t *b = fx_par(c, j), v = e > 0 ? 1u : 0u;
        if (*b != v) {
            *b = v;
            fx_changed();
        }
        return;
    }
    if (eq_qk(c, j)) {                       /* (v0.92) EQ Q: krok 4 (zapis w dodatkach presetu: 5 bitów) */
        uint8_t *b = fx_par(c, j);
        const uint8_t lo = (uint8_t)(FXXDEF[FX_X_EQ + fx_sj(c, j) - FX_NP] & 3u);
        const int32_t v = clamp((*b & ~3) + 4 * (e > 0 ? 1 : -1), 0, 124) | lo;
        if (v != *b) {
            *b = (uint8_t)v;
            fx_changed();
        }
        if (qs) {                            /* (strona pokazuje co innego: wartość w komunikacie) */
            char t[16];
            fx_text(c, j, t);
            app_say(fx_pname(c, j), t);
        }
        return;
    }
    if (c->type == FX_AWAH && (fx_sj(c, j) == AW_MODE || fx_sj(c, j) == AW_TRIG)) {   /* (v0.55) w prawo ENV / LEGATO */
        uint8_t *b = fx_par(c, j), v = e > 0 ? 1u : 0u;
        if (*b != v) {
            *b = v;
            fx_changed();
        }
        return;
    }
    if (j == fx_bendj(c)) {                  /* (v0.51) SUB BEND: w prawo ON, w lewo OFF */
        uint8_t b = e > 0 ? 0u : 1u;
        if (c->p[j] != b) {
            c->p[j] = b;
            fx_changed();
        }
        return;
    }
    if ((c->type == FX_DELAY && j == FX_DL_STYLE) || (c->type == FX_REVERB && j == FX_RV_COLOR)) {   /* STYLE, COLOR: krok po kroku */
        int32_t v = clamp(c->p[j] + (e > 0 ? 1 : -1), 0, (c->type == FX_DELAY ? FX_DL_NSTYLE : FX_RV_NCOLOR) - 1);
        if (v != c->p[j]) {
            c->p[j] = (uint8_t)v;
            fx_changed();
        }
        return;
    }
    if (j == fx_typep(c->type)) {            /* TYPE: rodzaj, krok po kroku; gra od razu, gdy włączony */
        int32_t v = clamp(c->p[j] + (e > 0 ? 1 : -1), 0, FX_NMODE[c->type] - 1);
        if (v != c->p[j]) {
            c->p[j] = (uint8_t)v;
            if (c->mode)
                c->mode = (uint8_t)(v + 1);
            fx_changed();
        }
        return;
    }
    pv = fx_par(c, j);
    j = clamp(*pv + accel(EN_K1 + k, e, 127), 0, 127);
    if (j != *pv) {
        *pv = (uint8_t)j;
        fx_changed();
    }
}

static void fx_mst_swap(void);
static void fx_select(int32_t e)         /* SELECT: slot; (v0.92) SEL trzymany + SELECT: efekt jedzie w łańcuchu (kursor za nim) */
{
    int32_t to = clamp((int32_t)ui.fx_slot + (e > 0 ? 1 : -1), 0, fx_view_n() - 1), moved = 0;
    if (ui.sel_hold) {                       /* SELECT przy trzymanym SEL: bez listy */
        fx_slot_t *a = fx_cur(), *b = to != (int32_t)ui.fx_slot ? fx_view(to) : 0;
        ui.sel_used = 1;
        if (!b)
            return;
        if (fx_is_mst(a) && fx_is_mst(b)) {  /* (MULTI: MASTER FX tylko między sobą) */
            fx_mst_swap();
            return;
        }
        if (fx_is_mst(a) || fx_is_mst(b)) {
            app_say("MASTER FX", "STAYS ON TOP");
            return;
        }
        if (a->type != FX_NONE || b->type != FX_NONE) {   /* (v0.92, decyzja użytkownika 2026-11-04) zamiana z sąsiadem, jak w liście */
            const fx_slot_t x = *a;
            char t[12] = "SLOT ", n[4];
            *a = *b;
            *b = x;
            moved = 1;
            int_text(to + 1, n, 0);
            text_cat(t, n);
            app_say("FX MOVED", t);
        }
    }
    ui.fx_slot = (uint8_t)to;
    if (to < ui.fx_win)                      /* łańcuch: 4 widoczne, przewija się tylko, gdy kursor wyjdzie poza */
        ui.fx_win = (uint8_t)to;
    else if (to > ui.fx_win + 3)
        ui.fx_win = (uint8_t)(to - 3);
    ui.knob = -1;
    ui.redraw = 1;
    if (moved)
        fx_changed();
}

/* (v0.85) MASTER FX: zamiana kolejnością (ALGO albo SEL trzymany na slocie 1 / 2); kursor idzie za efektem */
static void fx_mst_swap(void)
{
    glob.mfx.rev1st ^= 1u;
    ui.fx_slot = (uint8_t)(1u - (ui.fx_slot & 1u));
    ui.fx_more = 0;
    ui.knob = -1;
    mt_gdirty = 1;
    app_say("MASTER FX", glob.mfx.rev1st ? "REVERB > DELAY" : "DELAY > REVERB");
    ui.redraw = 1;
}
static void fx_pick(int32_t e)           /* ALGO: inny efekt w tym slocie (spośród tych, których nie ma w łańcuchu) */
{
    fx_slot_t *ch = fxc.cur[set.eng], *c = fx_cur();
    uint32_t used = 0, t = c->type, k;
    int on = c->mode != 0;
    if (fx_is_mst(c)) {                      /* (v0.85) MASTER FX: bez wymiany, tylko kolejność */
        fx_mst_swap();
        return;
    }
    for (k = 0; k < FX_SLOTS; k++)
        used |= 1u << ch[k].type;
    if (mt_ui())                             /* (v0.85) MULTI: REVERB, DELAY = MASTER FX */
        used |= 1u << FX_REVERB | 1u << FX_DELAY;
    used &= ~(1u << FX_NONE);
    do                                       /* efekty spoza łańcucha i pusty slot, w kolejności listy */
        t = FX_ORDER[(fx_rank(t) + (e > 0 ? 1u : FX_NTYPE)) % (FX_NTYPE + 1u)];
    while (t != FX_NONE && (used >> t & 1u));
    c->type = (uint8_t)t;
    for (k = 0; k < FX_NP; k++)
        c->p[k] = t < FX_NTYPE ? FXDEF[t][k] : 0u;
    fx_xdef(fxc.x[set.eng], t);
    c->mode = (uint8_t)(t == FX_NONE ? 0u : on ? (fx_typep(t) >= 0 ? c->p[fx_typep(t)] + 1u : 1u) : 0u);
    ui.fx_more = 0;
    fx_changed();
}

/* lista efektów: wiersze ekranu = miejsca FX_ORDER + nagłówek nad pierwszym w każdej grupie (v0.55) */
enum { FXL_ROWS = 7 };
static int32_t fxl_row(uint32_t r)       /* wiersz miejsca r */
{
    return (int32_t)r + fx_grp(r) + 1;
}

static void fxl_show(void)               /* przewija tak, żeby kursor (i nagłówek jego grupy) był widoczny */
{
    int32_t w = fxl_row(ui.fxl_cur), g = fx_grp(ui.fxl_cur);
    int32_t top = g >= 0 && ui.fxl_cur == FX_GRP[g].at ? w - 1 : w;
    if (top < ui.fxl_top)
        ui.fxl_top = (uint8_t)top;
    else if (w > ui.fxl_top + FXL_ROWS - 1)
        ui.fxl_top = (uint8_t)(w - (FXL_ROWS - 1));
    ui.redraw = 1;
}

static void fxl_open(void)               /* SEL trzymany (albo SEL na pustym slocie): lista efektów do tego slotu */
{
    uint32_t t = fx_rank(fx_cur()->type);
    if (fx_is_mst(fx_cur())) {               /* (v0.85) MASTER FX: bez listy (nie można wymienić ani usunąć), SEL trzymany = kolejność */
        fx_mst_swap();
        return;
    }
    ui.fxl_cur = (uint8_t)t;
    ui.fxl_top = (uint8_t)clamp(fxl_row(t) - 3, 0, fxl_row(FX_NTYPE) - (FXL_ROWS - 1));
    fxl_show();
    ui.page = V_FXL;
}

static void fxl_move(int32_t e)          /* lista efektów: SELECT kursor po kolei, EMPTY na górze */
{
    ui.fxl_cur = (uint8_t)clamp((int32_t)ui.fxl_cur + (e > 0 ? 1 : -1), 0, FX_NTYPE);
    fxl_show();
}

static void fxl_group(int32_t e)         /* ALGO: pierwszy efekt następnej / poprzedniej grupy, w kółko (jak ALL PRESETS) */
{
    int32_t g = ((fx_grp(ui.fxl_cur) + 1 + (e > 0 ? 1 : -1)) % (FXG_N + 1) + FXG_N + 1) % (FXG_N + 1) - 1;   /* -1 EMPTY */
    ui.fxl_cur = (uint8_t)(g < 0 ? 0 : FX_GRP[g].at);
    fxl_show();
}

static int32_t fxl_slot_of(uint32_t t)   /* w którym slocie jest efekt t, -1 nigdzie */
{
    int32_t k;
    for (k = 0; k < FX_SLOTS; k++)
        if (fxc.cur[set.eng][k].type == t && t != FX_NONE)
            return k;
    return -1;
}

static void fxl_ok(void)                 /* SEL w liście: ten efekt tutaj (już w łańcuchu: zamiana miejscami) */
{
    fx_slot_t *ch = fxc.cur[set.eng], *c = fx_cur();
    uint32_t t = FX_ORDER[ui.fxl_cur], k;
    int32_t at = fxl_slot_of(t);
    if (mt_ui() && fx_hide(t)) {             /* (v0.85) MULTI: REVERB, DELAY mieszkają w slotach 1-2: tam kursor */
        ui.fx_slot = (uint8_t)((t == FX_REVERB) == (glob.mfx.rev1st != 0) ? 0 : 1);
        ui.fx_win = 0;
        ui.fx_more = 0;
        ui.page = V_FX;
        ui.redraw = 1;
        return;
    }
    if (t != c->type) {
        if (at >= 0) {                       /* przesunięcie: ten efekt (z gałkami) tu, ten stąd tam */
            fx_slot_t x = ch[at];
            ch[at] = *c;
            *c = x;
        } else {
            c->type = (uint8_t)t;
            for (k = 0; k < FX_NP; k++)
                c->p[k] = t < FX_NTYPE ? FXDEF[t][k] : 0u;
            fx_xdef(fxc.x[set.eng], t);
            c->mode = (uint8_t)(t == FX_NONE ? 0u : fx_typep(t) >= 0 ? c->p[fx_typep(t)] + 1u : 1u);   /* wybrany = gra */
        }
        fx_changed();
    }
    ui.fx_more = 0;
    ui.page = V_FX;
    ui.redraw = 1;
}

static void fx_mode(void)                /* SEL krótko: włącza / wyłącza (rodzaj: gałka TYPE w MORE); pusty: lista */
{
    fx_slot_t *c = fx_cur();
    int32_t tp = fx_typep(c->type);
    if (c->type == FX_NONE) {
        fxl_open();
        return;
    }
    if (fx_is_mst(c)) {                      /* (v0.88) MASTER FX: odpina / przypina efekt do wybranej ścieżki */
        const int32_t t = glob.g[G_MTFOC];
        char h[16] = {'T', (char)('1' + t), ' ', 0};
        glob.g[G_MFXOFF] ^= (int16_t)(1 << ((c == &glob.mfx.s[1] ? 4 : 0) + t));
        fx_changed();
        text_cat(h, FX_SHORT[c->type]);
        app_say(h, fx_on(c) ? "ON" : "OFF");
        return;
    }
    c->mode = (uint8_t)(c->mode ? 0 : tp >= 0 ? c->p[tp] + 1 : 1);
    fx_changed();
    app_say(FX_NAME[c->type], FX_MODE[c->type][c->mode]);
}

/* ------------------------------------------------------------- SEQ / ARP --- */
/* (v0.88, decyzja użytkownika 2026-11-02; docs/sekwencer.md „Nawigacja”) SEQ krótko: sekcja (perkusja RACK, reszta NOTES), w niej
 * RACK <-> NOTES (bez perkusji nic). SEQ trzymany: ustawienia (SELECT: PATTERN, FEEL, w MULTI SCENES; SEQ: MORE = OPTIONS).
 * Wyjście HOME albo inna sekcja. NOTES: SELECT = kursor o grid, ALGO = GRID, SEL trzymany + ALGO = BARS (długość wzoru), K1 PITCH,
 * K1 nuta akordu (w prawo niższa), K2 PITCH, K3 LENGTH, K4 VELOCITY (v0.16, decyzja użytkownika), SEL + SELECT = zaznaczenie
 * (karta i gałki na całym zakresie), SEL + K4 = VEL RANDOM, PLAY = od kursora (trzymany: od początku), SEL = karta nuty (GLIDE, CHANCE,
 * RATCHET...), SEL + K1 / K2: zoom (v0.89), SEL + K3: długość co krok GRID, SEL trzymany + klawisz: nuta w kursorze dodaje się / usuwa. Nowa nuta ma długość ostatnio edytowanej (jak w FL). REC + stop: wpis krokowy
 * (akord = kilka klawiszy, po puszczeniu kursor dalej; ta sama nuta w kursorze = usuwa). REC + PLAY: nagrywanie na żywo.
 * ARP: strona jak FX (SEL = ON / OFF, ARP drugi raz = MORE, w MORE SEL = LATCH). PLAY / REC działają wszędzie.
 * (v0.90, decyzje użytkownika 2026-11-03; docs/sekwencer.md na górze) SEL + OP1-4 = ścieżka (było SEQ + OP), NOTES: świecą klawisze nut
 * kroku pod kursorem, SEL + K3 długość co tick, SEL + K4 UNDO / REDO, SEL + ALGO długość co GRID,
 * K1 znowu po wysokości (ta sama wysokość: od lewej), VEL RANDOM i QUANTIZE w karcie nuty, sceny MULTI: HOME na głównym ekranie. */
enum { SQ_NOTES, SQ_PATTERN, SQ_FEEL, SQ_RACK, SQ_OPT, SQ_REC, SQ_KEY, SQ_NPG };   /* (v1.1.0) KEY: tonacja wzoru (za RECORD) */   /* (v0.57) RACK: tylko silniki perkusji; (v0.88) OPT: strona ustawień;
                                                                             * (v0.92) REC: strona nagrywania (REC trzymany) */
static const char *const SQ_PGNAME[SQ_NPG] = {"NOTES", "PATTERN", "FEEL", "RACK", "OPTIONS", "RECORD", "KEY"};
/* (v1.1.0, P14) strona KEY (SELECT za RECORD): K1 KEY, K2 SCALE (OFF = bez tonacji, jak dotąd), K3 SNAP (klawisze i MIDI w skali) */
static const char *const L_PC12[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const L_SCALE[12] = {"OFF", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "HARM MINOR",
                                        "MEL MINOR", "PENTA MAJOR", "PENTA MINOR", "BLUES"};
static const pdef_t KYDEF[4] = {{"KEY", 0, 11, 0, F_LIST, LI(L_PC12)}, {"SCALE", 0, 11, 0, F_LIST, LI(L_SCALE)},
                                {"SNAP", 0, 1, 0, F_LIST, LI(L_ONOFF)}, {"", 0, 0, 0, F_NONE, 0}};
static const int8_t SQ_OPTG[4] = {-1, G_SQVEL, G_SQK1INV, G_SQZINV};   /* (v0.88) OPTIONS: pola GLO pod K1-K4; (v0.90) K1 pusta: SEL + ALGO już bez wyboru */
static const char *const L_CLICK[] = {"OFF", "REC", "ON"};
/* (v0.92, decyzja użytkownika 2026-11-04) REC trzymany: METRONOME (klik wzoru, jak FEEL CLICK), COUNT IN (odliczanie przed nagraniem
 * z PLAY, innym klikiem), QUANTIZE (nagrane nuty na końcu: OFF albo wartość; w FEEL SEL: OFF <-> grid wzoru).
 * (v1.1.0, decyzja użytkownika 2026-11-05) K1 BPM (ten sam co PATTERN), K2 METRONOME, K3 COUNT IN z długością na jednej gałce
 * (OFF, 1 BEAT .. 2 BARS; zapis bez zmian: GLO 47 = ON | LEN, OFF zostawia długość), K4 QUANTIZE; klawisze = TAP TEMPO (rc_tap) */
static const char *const L_CINL[] = {"OFF", "1 BEAT", "2 BEATS", "1 BAR", "2 BARS"};
static const char *const L_RECQ[] = {"OFF", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const pdef_t RCDEF[4] = {{"BPM", 40, 300, 120, F_NUM, 0}, {"METRONOME", 0, 2, 0, F_LIST, LI(L_CLICK)},
                                {"COUNT IN", 0, 4, 0, F_LIST, LI(L_CINL)}, {"QUANTIZE", 0, 6, 0, F_LIST, LI(L_RECQ)}};
static int32_t rc_cin(void) { return glob.g[G_SQUNIT] >= 8 ? glob.g[G_SQUNIT] & 7 : 4; }   /* bit 0 ON, bity 1-2 LEN (domyślnie 1 BAR) */
static int32_t rc_get(int32_t k)
{
    return k == 0 ? (sqp.bpm10 + 5) / 10 : k == 1 ? sqp.click : k == 2 ? (rc_cin() & 1 ? (rc_cin() >> 1) + 1 : 0) : sqp.recq;
}
static void sq_changed(void);
/* (v1.1.0) TAP TEMPO na stronie RECORD: klawisz = stuknięcie z klikiem metronomu (nie nuta); BPM = średnia z ostatnich (do 4)
 * odstępów, co 1 BPM, 40-300; po 2 s przerwy liczenie od nowa */
static void rc_tap(void)
{
    static uint32_t at, n, gap[4];
    const uint32_t now = plat_ms(), d = now - at;
    uint32_t i, m, sum = 0;
    sq.click_cin = 0;                            /* klik jak „raz” metronomu (sq_click_mix) */
    sq.click_inc = 0.0567f;
    sq.click_at = 0;
    at = now;
    if (d > 2000u || d < 150u) {                 /* pierwsze stuknięcie (albo drgnięcie klawisza): od nowa */
        n = 0;
        return;
    }
    gap[n++ & 3u] = d;
    m = n < 4u ? n : 4u;
    for (i = 0; i < m; i++)
        sum += gap[i];
    sqp.bpm10 = (uint16_t)(clamp((int32_t)((60000u * m + sum / 2u) / sum), 40, 300) * 10);
    sq_changed();
    ui.redraw = 1;
}
static void pt_rescan(void);
static const pdef_t SQDEF[2][4] = {
    {{"PATTERN", 1, SQ_NPAT, 1, F_NUM, 0}, {"STEPS", 1, SQ_MAXBARS * 16, 16, F_NUM, 0}, {"BPM", 40, 300, 120, F_NUM, 0},
     {"SWING", 50, 75, 50, F_NUM, 0}},
    {{"HUMANIZE", 0, 100, 0, F_NUM, 0}, {"VEL RANDOM", 0, 100, 0, F_NUM, 0}, {"GATE", 25, 200, 100, F_NUM, 0},
     {"CLICK", 0, 2, 0, F_LIST, LI(L_CLICK)}},
};
enum { ST_QUANT, ST_DOUBLE, ST_CLEAR, ST_CLRALL, ST_MOVE, ST_ZINV, ST_K1INV, NST };   /* (v0.66.1) CLEAR: wybrana ścieżka */
static const char *const ST_NAME[NST] = {"QUANTIZE NOTES", "DOUBLE LENGTH", "CLEAR PATTERN", "CLEAR ALL TRACKS", "MOVE HERE FROM",
                                         "ZOOM (SEL + K1)",
                                         "K1 NOTES / SCROLL"};

static uint32_t sq_sel[(SQ_NN + 31) / 32];       /* zaznaczone nuty (NOTES): z komórki kursora */
static uint32_t sq_dirty_at;
#define SQ_ISSEL(i) (sq_sel[(i) >> 5] >> ((i) & 31u) & 1u)

static uint32_t sq_grid(void) { return SQ_GRID[sqp.grid]; }

static const uint8_t SQ_ZROWS[4] = {8, 13, 18, 26};
static int32_t sq_rows(void) { return SQ_ZROWS[ui.sq_zv & 3u]; }
static int32_t sq_rh(void) { return 182 / sq_rows(); }
/* (v0.92, decyzja użytkownika 2026-11-04: „bardziej przybliżać K2”) zoom w poziomie: 1/4, 1/2, 1, 2, 4 takty (sq_zh 0..4, start 2) */
static uint32_t sq_span(void)                    /* widok: ile ticków na szerokość, nie więcej niż wzór (najmniej takt) */
{
    int32_t z = ui.sq_zh > 4u ? 2 : ui.sq_zh;
    while (z > 2 && (1 << (z - 2)) > sqp.bars)
        z--;
    return (uint32_t)SQ_BAR * (1u << z) / 4u;
}
static int32_t sq_vbars(void) { return (int32_t)(sq_span() / SQ_BAR); }   /* (testy: 0 = mniej niż takt) */

static void sq_zoom(int32_t vert, int32_t e)     /* (v0.89) SEL + K1: pion, SEL + K2: poziom */
{
    char t[16], n[8];
    t[0] = 0;
    if (vert) {
        ui.sq_zv = (uint8_t)clamp((int32_t)ui.sq_zv + (glob.g[G_SQZINV] ? e : -e), 0, 3);   /* (v0.92) odwrotnie niż do v0.91 */
        int_text(sq_rows(), t, 0);
        text_cat(t, " ROWS");
        if (ui.sq_low > 127 - sq_rows() + 1)
            ui.sq_low = 127 - sq_rows() + 1;
        if (ui.sq_note >= 0) {                   /* podświetlona nuta zostaje na środku widoku */
            ui.sq_low = clamp((int32_t)sqp.n[ui.sq_note].note - sq_rows() / 2, 0, 128 - sq_rows());
        }
    } else {
        int32_t z = clamp((int32_t)ui.sq_zh + (glob.g[G_SQZINV] ? e : -e), 0, 4);   /* (v0.92) w lewo dalej, kierunek jak pion */
        while (z > 2 && (1 << (z - 2)) > sqp.bars)   /* wzór krótszy: dalej się nie da */
            z--;
        ui.sq_zh = (uint8_t)z;
        if (z < 2) {
            text_cat(t, z ? "1/2 BAR" : "1/4 BAR");
        } else {
            int_text(sq_vbars(), n, 0);
            text_cat(t, n);
            text_cat(t, sq_vbars() > 1 ? " BARS" : " BAR");
        }
    }
    app_say("ZOOM", t);
    ui.sel_used = 1;
    ui.redraw = 1;
}

/* (v0.56, decyzja użytkownika 2026-10-29) perkusja: OCT i TRANSPOSE nie przesuwają układu klawiszy (stopa / werbel zostają
 * na swoich klawiszach), tylko przestrajają wszystkie dźwięki o półtony (ctl.dsemi, czytają silniki perkusji) */
static int32_t key_note(uint32_t k)
{
    if (ENG_GRP[set.eng] == EG_DRUM)
        return 53 + (int32_t)k;
    return clamp(53 + (int32_t)k + 12 * set.octave + glob.g[G_TRANSP], 0, 127);
}

static void sq_changed(void)                     /* wzór zmieniony: zapis sam w ciszy */
{
    if (!sqp_dirty)
        sq_dirty_at = plat_ms();
    sqp_dirty = 1;
    ui.redraw = 1;
}

/* (v0.90, decyzja użytkownika 2026-11-03) UNDO / REDO w SEQ (SEL + K4: w lewo cofa, w prawo ponawia): dziennik zmian nut w RAM.
 * Przed zmianą nuty i: sq_u(i) zapisuje jej stan sprzed (raz na krok); krok = jedna czynność (sq_um: przycisk, albo kręcenie tą samą
 * gałką na tej samej nucie bez przerwy 1,5 s). Cofanie zamienia wpisy z nutami (wpis trzyma potem stan „po”: ponawianie).
 * Pełny dziennik: najstarsze kroki wypadają; krok większy niż cały dziennik (np. CLEAR dużego wzoru) nie da się cofnąć.
 * Inny wzór: dziennik od zera. SU_HDR = długość wzoru (bars w t, xs w len). */
#define SU_N 224                                 /* 224 x 12 B (v0.92: + x nuty) */
#define SU_START 0x8000u
#define SU_HDR 0x7FFFu
static struct {
    uint16_t i;
    sqn_t o;
    uint8_t x;                                   /* (v0.92) sqp.x nuty */
} su_e[SU_N] __attribute__((section(".pool")));   /* (v0.91) w puli: RAM danych pełny; zawartość ważna tylko do su_n (zero w .bss) */
static uint16_t su_n, su_pos;                    /* wpisy; przed su_pos: do cofnięcia, od su_pos: do ponowienia */
static uint8_t su_new, su_skip;                  /* następny wpis zaczyna krok; krok za duży: do końca bez zapisu */
static uint32_t su_key, su_at;

static void sq_um(uint32_t key)                  /* początek czynności (0: zawsze nowy krok) */
{
    const uint32_t now = plat_ms();
    if (!key || key != su_key || now - su_at > 1500u) {
        su_new = 1;
        su_skip = 0;
    }
    su_key = key;
    su_at = now;
}
static void su_clear(void) { su_n = su_pos = 0; su_new = 1; }

static void sq_u(uint32_t i)                     /* nuta i (albo SU_HDR) zaraz się zmieni */
{
    uint32_t k;
    if (su_skip)
        return;
    su_n = su_pos;                               /* nowa zmiana: ponawianie przepada */
    if (!su_new)                                 /* w tym kroku już zapisana */
        for (k = su_n; k-- > 0;) {
            if ((su_e[k].i & ~SU_START) == i)
                return;
            if (su_e[k].i & SU_START)
                break;
        }
    if (su_n == SU_N) {                          /* pełny: najstarszy krok wypada */
        for (k = 1; k < su_n && !(su_e[k].i & SU_START); k++)
            ;
        if (k >= su_n) {                         /* jeden krok w dzienniku */
            su_n = 0;
            if (!su_new) {                       /* to ten bieżący: za duży, nie da się go cofnąć */
                su_pos = 0;
                su_skip = 1;
                return;
            }
        } else {
            uint32_t j;
            for (j = k; j < su_n; j++)
                su_e[j - k] = su_e[j];
            su_n = (uint16_t)(su_n - k);
        }
    }
    su_e[su_n].i = (uint16_t)(i | (su_new ? SU_START : 0u));
    if (i == SU_HDR) {
        su_e[su_n].o.t = sqp.bars;
        su_e[su_n].o.len = sqp.xs;
    } else {
        su_e[su_n].o = sqp.n[i];
        su_e[su_n].x = sqp.x[i];
    }
    su_pos = ++su_n;
    su_new = 0;
}

static void su_swap(uint32_t k)
{
    const uint32_t i = su_e[k].i & ~SU_START;
    if (i == SU_HDR) {
        const uint16_t b = sqp.bars, x = sqp.xs;
        sqp.xs = 0;                              /* (dźwięk nie widzi długości 0) */
        sqp.bars = (uint8_t)su_e[k].o.t;
        sqp.xs = (uint8_t)su_e[k].o.len;
        su_e[k].o.t = b;
        su_e[k].o.len = x;
    } else {
        const sqn_t c = sqp.n[i];
        const uint8_t x = sqp.x[i];
        sqp.n[i] = su_e[k].o;
        su_e[k].o = c;
        sqp.x[i] = su_e[k].x;
        su_e[k].x = x;
    }
}

static void sq_cursor(int32_t to);
static void sq_undo(int32_t e)                   /* SEL + K4: e < 0 cofa, e > 0 ponawia (o tyle kroków) */
{
    const int32_t e0 = e;
    int32_t done = 0;
    sq.hold = 1;
    for (; e; e += e > 0 ? -1 : 1) {
        if (e < 0 && su_pos) {
            uint32_t k = su_pos;
            do
                su_swap(--k);
            while (k && !(su_e[k].i & SU_START));
            su_pos = (uint16_t)k;
            done--;
        } else if (e > 0 && su_pos < su_n) {
            uint32_t k = su_pos;
            do
                su_swap(k++);
            while (k < su_n && !(su_e[k].i & SU_START));
            su_pos = (uint16_t)k;
            done++;
        }
    }
    sq.hold = 0;
    su_new = 1;
    ui.sel_used = 1;
    if (!done) {
        app_say(e0 < 0 ? "NOTHING TO UNDO" : "NOTHING TO REDO", "");
        return;
    }
    if (ui.rk_step >= sq_len() / SQ_STEP)
        ui.rk_step = 0;
    sq_ac_ok = 0;
    sq_cursor((int32_t)(ui.sq_cur < sq_len() ? ui.sq_cur : 0u));
    sq_changed();
    app_say(done < 0 ? "UNDO" : "REDO", "");
}

/* (v0.87, decyzja użytkownika 2026-10-31: nieregularne metrum) długość wzoru w krokach 1/16 (PATTERN K2, SEL + ALGO): bars taktów
 * bez xs kroków. Kolejność zapisów: dźwięk w przerwaniu nigdy nie widzi długości 0. Nuty za końcem zostają (nie grają). */
static void sq_steps_set(int32_t v)
{
    uint32_t b;
    v = clamp(v, 1, SQ_MAXBARS * 16);
    if ((uint32_t)v == sq_len() / SQ_STEP)
        return;
    sq_u(SU_HDR);                                /* (v0.90) UNDO: długość wzoru */
    b = ((uint32_t)v + 15u) / 16u;
    sqp.xs = 0;
    sqp.bars = (uint8_t)b;
    sqp.xs = (uint8_t)(b * 16u - (uint32_t)v);
    if (ui.sq_cur >= sq_len())
        ui.sq_cur = 0;
    if (ui.rk_step >= (uint32_t)v)
        ui.rk_step = (uint16_t)(v - 1);
    ui.sq_rng = 0;
    sq_changed();
}

static int32_t accel(uint32_t k, int32_t e, int32_t span);
static void sq_len_grid(int32_t e)               /* (v0.91) NOTES: SEL + ALGO = długość wzoru co GRID (najmniej krok 1/16) */
{
    const int32_t g = (int32_t)((sq_grid() + SQ_STEP / 2u) / SQ_STEP) < 1 ? 1 : (int32_t)((sq_grid() + SQ_STEP / 2u) / SQ_STEP);
    const int32_t cur = (int32_t)(sq_len() / SQ_STEP);
    int32_t v = e > 0 ? (cur + g) / g * g : (cur - 1) / g * g;
    char t[16];
    sq_um(0);
    sq_steps_set(v < g ? g : v);
    int_text((int32_t)(sq_len() / SQ_STEP), t, 0);
    text_cat(t, " STEPS");
    app_say("LENGTH", t);
}
static void sq_len_turn(int32_t e)               /* (v0.88) SEL + ALGO: długość wzoru co BEAT; (v0.90) zawsze (G_SQUNIT bez znaczenia); (v0.91) RACK i ustawienia */
{
    const int32_t cur = (int32_t)(sq_len() / SQ_STEP);
    int32_t v = e > 0 ? (cur + 4) / 4 * 4 : (cur - 1) / 4 * 4;
    char t[16];
    sq_um(0);
    sq_steps_set(v < 4 ? 4 : v);
    int_text((int32_t)(sq_len() / SQ_STEP) / 4, t, 0);
    text_cat(t, " BEATS");
    app_say("LENGTH", t);
}

/* (v0.87, decyzja użytkownika 2026-10-31) perkusja: nuta na ekranie trwa tyle, ile brzmienie (w tempie), ustawiona w piano rollu
 * krócej: tyle (SQF_CUT). Dźwięk nie zagra dłużej niż brzmienie, więc dłuższa nuta też pokazuje jego koniec. sq_ac: raz na
 * rysowanie (ten sam wiersz = ten sam dźwięk), sq_ac_new: parametry albo tempo mogły się zmienić */
static uint16_t sq_ac[16];
static void sq_ac_new(void) { sq_ac_ok = 0; }
static uint32_t sq_auto(uint32_t note)           /* ticki brzmienia nuty, 0: silnik bez tego / czarny klawisz */
{
    const int32_t w = rk_row_of(note);
    if (w < 0 || !ENG[set.eng].sound_s)
        return 0;
    if (!(sq_ac_ok >> w & 1u)) {
        const float bpm = ctl.bpm > 1.0f ? ctl.bpm : (float)sqp.bpm10 * 0.1f;
        const float x = ENG[set.eng].sound_s((int32_t)note) * bpm * (float)SQ_PPQN / 60.0f + 0.5f;
        sq_ac[w] = (uint16_t)(x < 1.0f ? 1.0f : x > 60000.0f ? 60000.0f : x);
        sq_ac_ok |= 1u << w;
    }
    return sq_ac[w];
}
static uint32_t sq_shown(const sqn_t *n)          /* (v0.92) SQF_CUT: długość ustawiona, też dłuższa niż brzmienie (zanik rozciągnięty) */
{
    const uint32_t a = n->len ? sq_auto(n->note) : 0u;
    return !a || (n->fl & SQF_CUT) ? n->len : a;
}
/* (v0.92, decyzja użytkownika 2026-11-04: „mogę tylko skracać”) perkusja: nowa długość nuty i (sh: dotąd, nl: nowa). Krótsza od brzmienia
 * ucina, dłuższa rozciąga zanik (silnik: ctl.dlen); przejście przez długość brzmienia zatrzymuje się na niej (bez bitu: samo brzmienie) */
static void sq_drum_len(sqn_t *n, int32_t sh, int32_t nl, uint32_t a, uint32_t L)
{
    const int32_t A = (int32_t)(a < L ? a : L);
    if ((n->fl & SQF_CUT) && ((sh < A && nl >= A) || (sh > A && nl <= A)))
        nl = A;
    n->fl = (uint8_t)(nl != A ? n->fl | SQF_CUT : n->fl & ~SQF_CUT);
    n->len = (uint16_t)nl;
}

static int sq_held(uint32_t note, uint32_t keys) /* ta nuta jest pod trzymanym klawiszem */
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        if ((keys >> k & 1u) && key_note(k) == (int32_t)note)
            return 1;
    return 0;
}

/* (v0.66) SEQ 4 ścieżki (docs/funkcje/multi.md): w MULTI edytuje się nuty wybranej ścieżki (reszta = ghost w NOTES),
 * nowe nuty dostają jej numer; (v0.66.2) SOLO: ścieżka silnika (perkusja T1, syntezator ostatnia z T2-T4, sq_strk) */
static int32_t sq_utrk(void) { return mt_on ? mt_track(set.eng) : sq_strk(set.eng); }   /* (v0.66.2) SOLO: jedna */
static int sq_mine(const sqn_t *n)
{
    const int32_t t = sq_utrk();
    return t < 0 || SQN_TRK(n) == (uint32_t)t;
}
static uint8_t sq_tbits(void)
{
    const int32_t t = sq_utrk();
    return (uint8_t)(t > 0 ? (uint32_t)t << 4 : 0u);
}

static int sq_in_cell(const sqn_t *n)            /* nuta zaczyna się w komórce kursora */
{
    return n->vel && n->len && sq_mine(n) && n->t >= ui.sq_cur && n->t < ui.sq_cur + sq_grid();
}

static uint32_t sq_rlo(void) { return ui.sq_ra < ui.sq_re ? ui.sq_ra : ui.sq_re; }
static uint32_t sq_rhi(void) { return (ui.sq_ra > ui.sq_re ? ui.sq_ra : ui.sq_re) + sq_grid(); }

static int sq_tgt(uint32_t i)                    /* nuta i jest edytowana: w zakresie albo podświetlona */
{
    const sqn_t *n = &sqp.n[i];
    if (ui.sq_rng)
        return n->vel && n->len && sq_mine(n) && n->t >= sq_rlo() && n->t < sq_rhi();
    return (int32_t)i == ui.sq_note;
}

static int32_t sq_tgt_first(void)                /* pierwsza edytowana (wartości na karcie), -1 żadna */
{
    uint32_t i;
    if (!ui.sq_rng)
        return ui.sq_note;
    for (i = 0; i < SQ_NN; i++)
        if (sq_tgt(i))
            return (int32_t)i;
    return -1;
}

static int sq_cell_lt(int32_t a, int32_t b)      /* (v0.90, decyzja użytkownika 2026-11-03) kolejność w komórce jak do v0.87: od dołu (wysokość);
                                                  * ta sama wysokość kilka razy w komórce (jak ratchet): od lewej (czas) */
{
    const sqn_t *x = &sqp.n[a], *y = &sqp.n[b];
    return x->note != y->note ? x->note < y->note : x->t != y->t ? x->t > y->t : a < b;   /* (1.0.0) ta sama wysokość: od prawej */
}

static int32_t sq_cell_note(int32_t j)           /* j-ta nuta komórki kursora (od dołu), -1 brak */
{
    uint32_t i, k;
    int32_t got = -1;
    for (k = 0; k <= (uint32_t)j; k++) {         /* (mało nut w komórce: wybór po kolei) */
        int32_t best = -1;
        for (i = 0; i < SQ_NN; i++) {
            if (!sq_in_cell(&sqp.n[i]) || (got >= 0 && !sq_cell_lt(got, (int32_t)i)))
                continue;
            if (best < 0 || sq_cell_lt((int32_t)i, best))
                best = (int32_t)i;
        }
        if (best < 0)
            return -1;
        got = best;
    }
    return got;
}

static int32_t sq_cell_count(void)
{
    uint32_t i;
    int32_t c = 0;
    for (i = 0; i < SQ_NN; i++)
        c += sq_in_cell(&sqp.n[i]);
    return c;
}

static int32_t sq_cell_index(int32_t note)       /* które miejsce w komórce ma ta nuta, -1 */
{
    int32_t j, c = sq_cell_count();
    for (j = 0; j < c; j++)
        if (sq_cell_note(j) == note)
            return j;
    return -1;
}

static void sq_pick(uint32_t keys)               /* podświetlona nuta (jak w Melodee: jedna); trzymany klawisz: ta nuta */
{
    uint32_t i;
    (void)keys;
    for (i = 0; i < sizeof sq_sel / 4u; i++)
        sq_sel[i] = 0;
    if (ui.sq_note >= 0 && !sq_in_cell(&sqp.n[ui.sq_note]))
        ui.sq_note = -1;
    for (i = 0; i < SQ_NN; i++)                  /* podświetlona albo cały zakres */
        if (sq_tgt(i))
            sq_sel[i >> 5] |= 1u << (i & 31);
}

static int32_t sq_first_sel(void) { return ui.sq_note; }

static void sq_view_sel(void)                    /* widok w pionie pokazuje podświetloną nutę */
{
    int32_t nt;
    if (ui.sq_note < 0)
        return;
    nt = sqp.n[ui.sq_note].note;
    if (nt < ui.sq_low)
        ui.sq_low = nt;
    else if (nt > ui.sq_low + sq_rows() - 1)
        ui.sq_low = nt - sq_rows() + 1;
}

static void sq_cursor(int32_t to)                /* kursor na komórkę (w pętli), podświetla najniższą nutę w niej */
{
    int32_t L = (int32_t)sq_len(), g = (int32_t)sq_grid();
    to = ((to % L) + L) % L;
    ui.sq_cur = (uint32_t)(to / g * g);
    ui.sq_rng = 0;                               /* kursor dalej: zaznaczenie znika */
    ui.sq_moved = plat_ms();
    ui.sq_note = sq_cell_note(0);
    sq_pick(0);
    sq_view_sel();
    ui.redraw = 1;
}

static void sq_range(int32_t e)                  /* (v0.89) SEL + SELECT: zaznaczenie od kursora o kroki (w obie strony) */
{
    int32_t g = (int32_t)sq_grid(), L = (int32_t)sq_len();
    ui.sel_used = 1;
    if (!ui.sq_rng) {
        ui.sq_rng = 1;
        ui.sq_ra = ui.sq_re = ui.sq_cur;
    }
    ui.sq_re = (uint32_t)clamp((int32_t)ui.sq_re + e * g, 0, L > g ? L - g : 0);
    ui.sq_cur = ui.sq_re;                        /* kursor na końcu zaznaczenia */
    ui.sq_note = sq_cell_note(0);
    ui.sq_moved = plat_ms();
    sq_pick(0);
    ui.redraw = 1;
}

static void sq_vel_random(void)                  /* (v0.90: karta nuty, było SEL + K4) velocity edytowanych losowo (±20 na klik, jak randomize w FL) */
{
    static uint32_t r = 0x1234567u;
    uint32_t i;
    for (i = 0; i < SQ_NN; i++)
        if (sq_tgt(i)) {
            sq_u(i);
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            sqp.n[i].vel = (uint8_t)clamp((int32_t)sqp.n[i].vel + (int32_t)(r % 41u) - 20, 1, 127);
        }
    app_say("VELOCITY", "RANDOM");
}

static void sq_vert(int32_t e)                   /* K1: po nutach kroku (w prawo wyżej); dalej (albo pusty krok): widok o półton (v0.90: jak do v0.87) */
{
    int32_t c = sq_cell_count(), j = ui.sq_note >= 0 ? sq_cell_index(ui.sq_note) : -1;
    if (c && j >= 0 && j + e >= 0 && j + e < c) {
        ui.sq_note = sq_cell_note(j + e);
        sq_pick(0);
        sq_view_sel();
    } else {
        ui.sq_low = clamp(ui.sq_low + e, 0, 128 - sq_rows());
    }
    ui.redraw = 1;
}

/* (v0.90, decyzja użytkownika 2026-11-03) długość edytowanych nut: mode 0 = co 1/4 kroku GRID (K3), 1 = co tick (SEL + K3, dowolnie),
 * 2 = co cały krok GRID, do pełnego (SEL + ALGO; do v0.89 SEL + K3). Perkusja (v0.87): krótsza od brzmienia ucina (SQF_CUT), inaczej
 * za brzmieniem. (v0.90) Naprawa: dźwięk dłuższy niż wzór (talerz, długa stopa) nie dał się skrócić: start od długości wzoru. */
static void sq_len_edit(int32_t mode, int32_t d, uint32_t key)
{
    const uint32_t L = sq_len();
    const int32_t g = (int32_t)sq_grid();
    uint32_t i;
    if (sq_tgt_first() < 0)
        return;
    sq_um(key + ((uint32_t)(ui.sq_note + 1) << 4) + ((uint32_t)ui.sq_rng << 15) + ((uint32_t)mode << 16));
    sq_ac_new();
    for (i = 0; i < SQ_NN; i++) {
        sqn_t *n = &sqp.n[i];
        const uint32_t a = sq_auto(n->note);
        int32_t sh = (int32_t)sq_shown(n), nl;
        if (!sq_tgt(i))
            continue;
        if (sh > (int32_t)L)
            sh = (int32_t)L;
        if (mode == 2) {
            nl = d > 0 ? (sh + g * d) / g * g : (sh - 1 + g * (d + 1)) / g * g;
            if (nl < 1)                          /* krótsza od kroku zostaje: w lewo nie wydłuża */
                nl = sh < g ? sh : g;
        } else {
            nl = sh + (mode ? 1 : (g / 4 ? g / 4 : 1)) * d;
        }
        nl = clamp(nl, 1, (int32_t)L);
        sq_u(i);
        if (a && d < 0 && nl >= sh && (n->fl & SQF_CUT))   /* (v0.92) krócej już się nie da (krok): z powrotem do brzmienia (AUTO) */
            nl = (int32_t)(a < L ? a : L);
        if (a)
            sq_drum_len(n, sh, nl, a, L);
        else
            n->len = (uint16_t)nl;
        ui.sq_len = n->len;                      /* nowe nuty: ta długość (jak w FL) */
    }
    sq_changed();
}

static void sq_walk(int32_t e)                   /* (kroki i nuty akordu po kolei: K1 w v0.12.1) */
{
    int32_t g = (int32_t)sq_grid();
    while (e) {
        int32_t j = ui.sq_note >= 0 ? sq_cell_index(ui.sq_note) : -1, c = sq_cell_count();
        if (e > 0) {
            if (j >= 0 && j + 1 < c) {
                ui.sq_note = sq_cell_note(j + 1);
            } else {
                sq_cursor((int32_t)ui.sq_cur + g);
            }
            e--;
        } else {
            if (j > 0) {
                ui.sq_note = sq_cell_note(j - 1);
            } else {
                sq_cursor((int32_t)ui.sq_cur - g);
                c = sq_cell_count();
                ui.sq_note = c ? sq_cell_note(c - 1) : -1;   /* wstecz: ostatnia nuta poprzedniego kroku */
            }
            e++;
        }
    }
    ui.sq_moved = plat_ms();
    sq_pick(0);
    sq_view_sel();
    ui.redraw = 1;
}

static int32_t sq_find(uint32_t note)            /* nuta o tej wysokości w komórce kursora, -1 brak */
{
    uint32_t i, g = sq_grid();
    for (i = 0; i < SQ_NN; i++) {
        const sqn_t *n = &sqp.n[i];
        if (n->vel && n->len && sq_mine(n) && n->note == note && n->t >= ui.sq_cur && n->t < ui.sq_cur + g)
            return (int32_t)i;
    }
    return -1;
}

static int32_t sq_add(uint32_t note, uint32_t vel)   /* nowa nuta w kursorze (długość jak ostatnio edytowana), -1: brak miejsca */
{
    uint32_t i;
    for (i = 0; i < SQ_NN; i++)
        if (!sqp.n[i].vel) {
            sqn_t *n = &sqp.n[i];
            sq_u(i);
            n->len = (uint16_t)(ui.sq_len ? ui.sq_len : sq_grid());
            n->t = (uint16_t)ui.sq_cur;
            n->note = (uint8_t)note;
            n->fl = sq_tbits();
            n->chance = 100;
            sqp.x[i] = 0;
            n->vel = (uint8_t)(vel ? vel : (uint32_t)glob.g[G_SQVEL]);
            sq_changed();
            return (int32_t)i;
        }
    app_say("PATTERN FULL", "448 NOTES");
    return -1;
}

static void sq_toggle(uint32_t note, uint32_t vel)   /* SEL + klawisz w kursorze: usuwa tę nutę albo dodaje */
{
    int32_t i = sq_find(note);
    sq_um(0);
    if (i >= 0) {
        sq_u((uint32_t)i);
        sqp.n[i].vel = 0;
        sq_changed();
        app_say("NOTE", "DELETED");
    } else {
        ui.sq_note = sq_add(note, vel);
    }
    sq_pick(0);
    sq_view_sel();
}

static void sq_quantize(int all)                 /* nuty do gridu (all 1: wszystkie, 2: edytowane (karta), 0: z ostatniego nagrania) */
{
    uint32_t i, g = all || !sqp.recq ? sq_grid() : SQ_GRID[(sqp.recq - 1u) % 6u], L = sq_len();   /* (v0.92) nagranie: QUANTIZE z REC */
    for (i = 0; i < SQ_NN; i++) {
        sqn_t *n = &sqp.n[i];
        if (!n->vel || (!all && !(n->fl & SQF_NEW)) || (all == 1 && !sq_mine(n)) || (all == 2 && !sq_tgt(i)))   /* (wszystkie: wybranej ścieżki) */
            continue;
        if (all)
            sq_u(i);
        n->t = (uint16_t)((n->t + g / 2u) / g * g % L);
        n->fl &= (uint8_t)~SQF_NEW;
    }
    sq_changed();
}

static void sq_rec_end(void)                     /* koniec nagrywania na żywo: długości, kwantyzacja (FEEL SEL) */
{
    uint32_t i, any = 0;
    sq.rec = 0;
    rec_close();
    for (i = 0; i < SQ_NN; i++)
        any |= sqp.n[i].fl & SQF_NEW;
    if (!any)
        return;
    sq_um(0);                                    /* (v0.90) UNDO: całe nagranie jednym krokiem (przed nim te miejsca były puste) */
    for (i = 0; i < SQ_NN; i++)
        if (sqp.n[i].fl & SQF_NEW) {
            const sqn_t c = sqp.n[i];
            sqp.n[i].vel = 0;
            sq_u(i);
            sqp.n[i] = c;
        }
    if (sqp.recq) {
        sq_quantize(0);
    } else {
        for (i = 0; i < SQ_NN; i++)
            sqp.n[i].fl &= (uint8_t)~SQF_NEW;
        sq_changed();
    }
}

static void sq_play_key(void)                    /* PLAY: start / stop (od początku wzoru) */
{
    if (sq.play) {
        if (sq.rec)
            sq_rec_end();
        sq.req = 2;
        app_say("STOP", "");
    } else {
        static const uint16_t CL[4] = {SQ_PPQN, 2 * SQ_PPQN, SQ_BAR, 2 * SQ_BAR};
        sq.cin = (uint16_t)(rc_cin() & 1 ? CL[rc_cin() >> 1] : 0u);   /* (v0.92) odliczanie (tylko z REC) */
        sq.req = 1;
        app_say(sq.rec && sq.cin ? "COUNT IN" : "PLAY", sq.rec ? "RECORDING" : "");
    }
    ui.redraw = 1;
}

static void sq_rec_key(void)                     /* REC: nagrywanie (z PLAY na żywo, bez: wpis krokowy w NOTES) */
{
    if (sq.rec) {
        if (sq.play)
            sq_rec_end();
        sq.rec = 0;
        app_say("REC", "OFF");
    } else {
        sq.rec = 1;
        app_say("REC", sq.play ? "LIVE" : "STEP: KEYS IN NOTES");
    }
    ui.st_added = 0;
    ui.redraw = 1;
}

static void sq_set_pattern(int32_t p)            /* inny wzór: bieżący zapisany (gdy zmieniony), nowy wczytany */
{
    p = clamp(p, 0, SQ_NPAT - 1);
    if (p == sqg.pat)
        return;
    if (sq.rec && sq.play)
        sq_rec_end();
    if (sqp_dirty)
        sq_store();                              /* (zapis flasha: krótka przerwa w dźwięku, gdy gra) */
    sqg.pat = (uint8_t)p;
    sqg_dirty = 1;
    sq_dirty_at = plat_ms();
    sq_load(p);
    su_clear();                                  /* (v0.90) UNDO: tylko w obrębie wzoru */
    pt_rescan();
    ui.sq_cur = 0;
    ui.sq_rng = 0;
    sq_pick(0);
}

static int32_t swing_pct(int32_t v)              /* SWING na ekranie 0..100 % (w pamięci 50..75: druga nuta pary) */
{
    return (v - 50) * 4;
}

static int32_t sq_val(int32_t pg, int32_t k)     /* PATTERN / FEEL: wartość pod gałką */
{
    if (pg == SQ_PATTERN)
        return k == 0 ? sqg.pat + 1 : k == 1 ? (int32_t)(sq_len() / SQ_STEP) : k == 2 ? (sqp.bpm10 + 5) / 10 : sqp.swing;
    return k == 0 ? sqp.human : k == 1 ? sqp.vrand : k == 2 ? sqp.gate : sqp.click;
}

static int sq_is_view(void) { return ui.sq_pg == SQ_NOTES || ui.sq_pg == SQ_RACK; }
static int sq_in_set(void) { return ui.page == V_SEQ && !sq_is_view(); }   /* w ustawieniach SEQ (v0.90: sceny już nie tu) */

static uint32_t sq_hint_until;
#define NOTES_HINT "SELECT: STEP   ALGO: GRID   SEL: CARD   HOLD SEL: MORE"                   /* (v0.91) NOTES nie ma paska: podpowiedź w nagłówku przez 2,5 s po wejściu */
static void sq_open(uint32_t pg)                 /* strona sekcji SEQ */
{
    glob_commit();
    sq_hint_until = pg == SQ_NOTES && hints() ? plat_ms() + 2500u : 0;
    ui.sq_pg = (uint8_t)pg;
    if (pg == SQ_PATTERN || pg == SQ_FEEL || pg == SQ_OPT || pg == SQ_REC || pg == SQ_KEY)
        ui.sq_set = (uint8_t)pg;
    else if (pg == SQ_NOTES || pg == SQ_RACK)
        ui.sq_rk = (uint8_t)(pg == SQ_RACK);
    pt_rescan();                                 /* (v0.90) PATTERN: puste wzory od nowa */
    ui.page = V_SEQ;
    ui.knob = -1;
    ui.quick = 0;
    ui.browse = 0;
    sq_pick(0);
    ui.redraw = 1;
}
static void sq_view_back(void)                   /* z ustawień: widok, z którego przyszliśmy (RACK / NOTES) */
{
    sq_open(ENG_GRP[set.eng] == EG_DRUM && ui.sq_rk ? SQ_RACK : SQ_NOTES);
}
static void go_seq(void)                         /* SEQ krótko: wejście; w sekcji RACK <-> NOTES (bez perkusji nic); (v0.89) w ustawieniach: widok */
{
    const int drum = ENG_GRP[set.eng] == EG_DRUM;
    if (sq_in_set())
        sq_view_back();
    else if (ui.page != V_SEQ)
        sq_open(drum ? SQ_RACK : SQ_NOTES);
    else if (drum)
        sq_open(ui.sq_pg == SQ_RACK ? SQ_NOTES : SQ_RACK);
}
static void seq_hold(void)                       /* SEQ trzymany: ustawienia; w nich z powrotem */
{
    if (sq_in_set())
        sq_view_back();
    else
        sq_open(ui.sq_set ? ui.sq_set : SQ_PATTERN);
}
static void scn_open(void);
static void sq_set_turn(int32_t e)               /* SELECT w ustawieniach: PATTERN, FEEL, OPTIONS (v0.90: sceny pod HOME w MULTI) */
{
    if (e > 0 && ui.sq_pg == SQ_PATTERN)
        sq_open(SQ_FEEL);
    else if (e > 0 && ui.sq_pg == SQ_FEEL)
        sq_open(SQ_OPT);
    else if (e > 0 && ui.sq_pg == SQ_OPT)
        sq_open(SQ_REC);
    else if (e > 0 && ui.sq_pg == SQ_REC)        /* (v1.1.0) KEY za RECORD */
        sq_open(SQ_KEY);
    else if (e < 0 && ui.sq_pg == SQ_KEY)
        sq_open(SQ_REC);
    else if (e < 0 && ui.sq_pg == SQ_REC)
        sq_open(SQ_OPT);
    else if (e < 0 && ui.sq_pg == SQ_OPT)
        sq_open(SQ_FEEL);
    else if (e < 0 && ui.sq_pg == SQ_FEEL)
        sq_open(SQ_PATTERN);
}

static int sq_sel_trk(void)                      /* (v0.90) SEL + OP1-4 = ścieżka: MULTI, strony SEQ */
{
    return mt_ui() && (ui.page == V_SEQ || ui.page == V_SQN || ui.page == V_SQT);
}
static void sq_track_go(int32_t t)               /* (v0.66) OP1-4 w MULTI: ścieżka T1-T4 (jej nuty do edycji); (v0.90) SEL + OP, było SEQ + OP */
{
    char m[3] = {'T', (char)('1' + t), 0};
    const int32_t e = glob.g[G_MT0 + t];
    if (e >= NENG) {
        app_say(m, "EMPTY");
        return;
    }
    mt_focus_go(t);
    sq_open(ENG_GRP[e] == EG_DRUM ? SQ_RACK : SQ_NOTES);   /* perkusja w RACK, reszta w NOTES; karta nuty, narzędzia, ustawienia: zamknięte */
    ui.sq_rng = 0;
    ui.sq_note = sq_cell_note(0);
    sq_pick(0);
    app_say(m, ENG[e].name);
    ui.redraw = 1;
}

static void page_return(int32_t b)               /* powrót tam, skąd przyszliśmy (jak GLO): ustawienia ARP, lista scen */
{
    if (PANEL_PAGE(b) || b == V_GLO || b == V_HOME)
        ui.page = b;
    else if (b >= 0)
        show_page(b);
    else
        ui.page = V_HOME;
    ui.knob = -1;
    ui.redraw = 1;
}

static void go_arp(void)                         /* ARP trzymany: ustawienia (zawsze strona 1); drugi raz: z powrotem */
{
    glob_commit();
    if (ui.page == V_ARP) {
        page_return(ui.arp_back);
        return;
    }
    ui.arp_back = ui.page == V_SQT || ui.page == V_SQN ? V_SEQ : ui.page;
    ui.arp_more = 0;
    ui.page = V_ARP;
    ui.knob = -1;
    ui.quick = 0;
    ui.browse = 0;
    ui.redraw = 1;
}

static void sqg_changed(void)
{
    if (!sqp_dirty && !sqg_dirty)
        sq_dirty_at = plat_ms();
    sqg_dirty = 1;
    ui.redraw = 1;
}

static void arp_knob(uint32_t k, int32_t e)
{
    int32_t j = (int32_t)k + ui.arp_more * 4, v;
    const pdef_t *d = &ARDEF[j];
    if (j >= AR_NP)                              /* strona 3: tylko CHORD */
        return;
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    v = clamp((int32_t)sqg.a[j] + (d->fmt == F_LIST || d->hi - d->lo < 10 ? (e > 0 ? 1 : -1)
                                                                        : accel(EN_K1 + k, e, d->hi - d->lo)),
              d->lo, d->hi);
    if (v != sqg.a[j]) {
        sqg.a[j] = (uint8_t)v;
        sqg_changed();
    }
}

static void arp_sel(void)                        /* SEL w ustawieniach ARP: LATCH */
{
    sqg.latch ^= 1u;
    if (!sqg.latch)
        ev_push(EV_ARP_OFF);                     /* puszczone akordy przestają grać */
    app_say("LATCH", sqg.latch ? "ON" : "OFF");
    sqg_changed();
}

static void arp_toggle(void)                     /* ARP krótko: włącza / wyłącza (z jego ustawieniami), strona zostaje */
{
    sqg.arp_on ^= 1u;
    ev_push(EV_ARP_OFF);
    app_say("ARP", sqg.arp_on ? "ON" : "OFF");
    sqg_changed();
}

/* ---------------------------------------------------------------- RACK --- */
/* (v0.57, projekt zatwierdzony przez użytkownika 2026-10-29: docs/sekwencer.md „RACK”) SEQ perkusji jak Channel Rack
 * w FL: wiersz = dźwięk (16 białych klawiszy), krok = 1/16; te same nuty co NOTES (wiersz = nuta białego klawisza
 * w dowolnej oktawie). Białe klawisze = kroki 1-16 strony (ALGO: strona), SELECT = wiersz, gałki = krok (VEL, CHANCE,
 * RATCHET, NUDGE; SEL trzymany: cały wiersz), SEL + klawisz = wiersz tego dźwięku i podsłuch (REC: nagrywa), FX + klawisz =
 * MUTE, ARP + klawisz = SOLO, SEL krótko = TOOLS. Krok obejmuje nuty od -12 do +11 ticków od swojej kratki (NUDGE ±11). */
#define RK_T SQ_STEP                              /* krok: 1/16 */
#define RK_ROWS 8                                /* wierszy na ekranie */
static const uint8_t RK_NOTE[16] = {53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79};

static int rk_on(void) { return ui.page == V_SEQ && ui.sq_pg == SQ_RACK && ENG_GRP[set.eng] == EG_DRUM; }
static uint32_t rk_steps(void) { return sq_len() / RK_T; }

static void rk_name(int32_t row, char *b)        /* skrót dźwięku wiersza: 88 / 99 z silnika, MORPH / DUAL 1-8 (+: UPPER) */
{
    b[0] = 0;
    if (set.eng == ENG_88 || set.eng == ENG_99) {
        text_cat(b, (set.eng == ENG_88 ? D8_SN : D9_SN)[row]);
    } else {
        int_text(row % 8 + 1, b, 0);
        if (row >= 8)
            text_cat(b, "+");
    }
}

static int rk_in(const sqn_t *n, int32_t row, uint32_t st)   /* nuta wiersza w kroku st */
{
    const uint32_t L = sq_len();
    return n->vel && n->len && sq_mine(n) && n->t < L && rk_row_of(n->note) == row && (n->t + L + 12u - st * RK_T % L) % L < RK_T;
}
static int32_t rk_find(int32_t row, uint32_t st)
{
    uint32_t i;
    for (i = 0; i < SQ_NN; i++)
        if (rk_in(&sqp.n[i], row, st))
            return (int32_t)i;
    return -1;
}

static void rk_view(void)                        /* wybrany wiersz widoczny */
{
    if (ui.rk_row < ui.rk_top)
        ui.rk_top = ui.rk_row;
    if (ui.rk_row >= ui.rk_top + RK_ROWS)
        ui.rk_top = (uint8_t)(ui.rk_row - RK_ROWS + 1);
    ui.redraw = 1;
}

static void rk_goto(uint32_t pg)                 /* strona kroków: ten sam klawisz, na niepełnej ostatniej stronie najdalej jak się da */
{
    const uint32_t s = pg * 16u + ui.rk_step % 16u, ns = rk_steps();
    ui.rk_step = (uint16_t)(s < ns ? s : ns - 1u);
}

/* (v0.87, decyzja użytkownika 2026-10-31) światła białych klawiszy: nuty wybranego wiersza na tej stronie i krok grający */
static void rk_leds(void)
{
    static uint32_t at;
    const uint32_t now = plat_ms();
    rk_led_play = sq.play ? 1u << (sq.pos / RK_T % 16u) : 0u;
    if (now - at >= 8u) {                        /* (nuty: co 8 ms wystarczy) */
        const uint32_t L = sq_len(), ns = rk_steps(), pg = ui.rk_step % ns / 16u;
        uint32_t i, m = 0;
        at = now;
        for (i = 0; i < SQ_NN; i++) {
            const sqn_t *n = &sqp.n[i];
            if (n->vel && n->len && n->t < L && sq_mine(n) && rk_row_of(n->note) == ui.rk_row) {
                const uint32_t st = (n->t + 12u) % L / RK_T;
                if (st / 16u == pg)
                    m |= 1u << (st % 16u);
            }
        }
        rk_led_note = m;
    }
}
/* (v0.90, decyzja użytkownika 2026-11-03) NOTES: świecą klawisze nut kroku pod kursorem (podświetlona pełnym, reszta średnio);
 * SEL + taki klawisz usuwa swoją nutę (sq_toggle) */
static void sq_leds(void)
{
    static uint32_t at;
    const uint32_t now = plat_ms();
    uint32_t i, k, f = 0, m = 0;
    if (now - at < 8u)
        return;
    at = now;
    for (i = 0; i < SQ_NN; i++)
        if (sq_in_cell(&sqp.n[i]))
            for (k = 0; k < NKEYS; k++)
                if (key_note(k) == (int32_t)sqp.n[i].note) {
                    if ((int32_t)i == ui.sq_note)
                        f |= 1u << k;
                    else
                        m |= 1u << k;
                }
    sq_led_full = f;
    sq_led_mid = m & ~f;
}
static uint32_t rk_keys(uint32_t cols)           /* bity białych klawiszy (kroków) -> bity klawiszy FM1 */
{
    uint32_t c, m = 0;
    for (c = 0; c < 16u; c++)
        if (cols >> c & 1u)
            m |= 1u << (RK_NOTE[c] - 53u);
    return m;
}

static void rk_toggle(uint32_t col)              /* biały klawisz: krok strony włącza / wyłącza */
{
    const uint32_t st = ui.rk_step % rk_steps() / 16u * 16u + col;
    uint32_t i, any = 0;
    if (st >= rk_steps())
        return;
    ui.rk_step = (uint16_t)st;
    ui.sq_moved = plat_ms();
    sq_um(0);
    for (i = 0; i < SQ_NN; i++)
        if (rk_in(&sqp.n[i], ui.rk_row, st)) {
            sq_u(i);
            sqp.n[i].vel = 0;
            any = 1;
        }
    if (!any)
        for (i = 0; i < SQ_NN; i++)
            if (!sqp.n[i].vel) {
                sqn_t *n = &sqp.n[i];
                sq_u(i);
                n->t = (uint16_t)(st * RK_T);
                n->len = RK_T;
                n->note = RK_NOTE[ui.rk_row];
                n->fl = sq_tbits();
                n->chance = 100;
                sqp.x[i] = 0;
                n->vel = (uint8_t)glob.g[G_SQVEL];
                any = 1;
                break;
            }
    if (!any)
        app_say("PATTERN FULL", "448 NOTES");
    sq_changed();
}

static void rk_mute(int32_t row, int solo)       /* FX + klawisz: MUTE wiersza, ARP + klawisz: SOLO */
{
    uint16_t *m = solo ? &sqp.solo : &sqp.mute;
    char t[20], n[6];
    *m ^= (uint16_t)(1u << row);
    rk_name(row, n);
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, *m >> row & 1u ? " ON" : " OFF");
    app_say(solo ? "SOLO" : "MUTE", t);
    sq_changed();
}

/* (v0.92, decyzja użytkownika 2026-11-04: zamiast całego wiersza więcej dla kroku) SEL trzymany: K1 PITCH (±12 półtonów), K2 COND (obieg
 * wzoru: 1:2, 2:2, 1:4 ... 1ST), K3 FLAM (ciche przednie uderzenie), K4 LENGTH (co 1/4 kroku; krócej ucina, dłużej rozciąga zanik,
 * zatrzymanie na długości brzmienia = AUTO). Pary: VEL / PITCH, CHANCE / COND, RATCHET / FLAM, NUDGE / LENGTH. */
/* (v0.92, decyzja użytkownika 2026-11-04) kopia dźwięku perkusji: SEL + trzymany klawisz + drugi klawisz -> pytanie (V_CPY). Kopiuje
 * brzmienie i poziom; PAN, SEND (sprężyna) i CHOKE celu zostają. 88 / 99: tylko dźwięk tego samego rodzaju (stały model miejsca). */
static int cp_dm(void) { return set.eng == ENG_88 || set.eng == ENG_99; }
static void cp_name(int32_t snd, char *b)
{
    b[0] = 0;
    if (cp_dm())
        text_cat(b, (set.eng == ENG_88 ? D8_SN : D9_SN)[snd & 15]);
    else
        int_text(snd + 1, b, 0);
}
static void cp_ask(int32_t wa, int32_t wb)        /* wiersze (białe klawisze 0-15) */
{
    const int32_t a = cp_dm() ? wa : wa & 7, b = cp_dm() ? wb : wb & 7;
    if (wa < 0 || wb < 0)
        return;
    if (a == b) {
        app_say("SAME SOUND", "");
        return;
    }
    if (cp_dm() && (set.eng == ENG_88 ? &DM8 : &DM9)->model[a] != (set.eng == ENG_88 ? &DM8 : &DM9)->model[b]) {
        char t[16], n[4];
        t[0] = 0;
        cp_name(a, n);
        text_cat(t, n);
        text_cat(t, " > ");
        cp_name(b, n);
        text_cat(t, n);
        app_say(t, "OTHER KIND OF SOUND");
        return;
    }
    ui.cp_a = (int8_t)a;
    ui.cp_b = (int8_t)b;
    ui.cp_yes = 0;
    ui.cp_k = 0;
    if (ui.page != V_CPY)
        ui.cp_back = (int8_t)ui.page;
    ui.page = V_CPY;
    ui.redraw = 1;
}
static void cp_ok(void)
{
    const int32_t a = ui.cp_a, b = ui.cp_b;
    int32_t j;
    ui.page = ui.cp_back;
    ui.redraw = 1;
    if (!ui.cp_yes)
        return;
    if (set.eng == ENG_MORPH) {
        static const int16_t X[3] = {MP_PUNCH, MP_LEN, MP_FADE};
        for (j = 0; j < MPL; j++)
            set_param(b * MPL + j, MP[a * MPL + j]);
        for (j = 0; j < 3; j++)
            set_param(X[j] + b, MP[X[j] + a]);
    } else if (set.eng == ENG_DUAL) {
        for (j = 0; j < DUP; j++)
            if (j != DUP_PAN && j != DUP_SEND && j != DUP_CHOKE)
                set_param(b * DUP + j, DU[a * DUP + j]);
        set_param(DU_GATE + b, DU[DU_GATE + a]);
    } else {
        const dmeng_t *e = set.eng == ENG_88 ? &DM8 : &DM9;
        for (j = 0; j < DML; j++)
            if (j != DML_PAN && j != DML_CHOKE)
                set_param(b * DML + j, e->p[a * DML + j]);
        set_param(DM_LEN + b, e->p[DM_LEN + a]);
        set_param(DM_FADE + b, e->p[DM_FADE + a]);
    }
    {
        char t[16], n[4];
        t[0] = 0;
        cp_name(a, n);
        text_cat(t, n);
        text_cat(t, " > ");
        cp_name(b, n);
        text_cat(t, n);
        app_say(t, "COPIED");
    }
}

static void rk_knob(uint32_t k, int32_t e)       /* K1 VEL, K2 CHANCE, K3 RATCHET, K4 NUDGE; SEL trzymany: PITCH, COND, FLAM, LENGTH */
{
    const uint32_t L = sq_len();
    const int more = ui.sel_hold;
    const int32_t d = k == 0 && !more ? accel(EN_K1, e, 127) : k == 3 && more ? accel(EN_K4, e, (int32_t)L / 6) : e > 0 ? 1 : -1;
    uint32_t i, n = 0;
    if (more)
        ui.sel_used = 1;
    ui.sq_moved = plat_ms();
    sq_ac_new();
    sq_um(0x200u + k + ((uint32_t)more << 3) + ((uint32_t)ui.rk_row << 4) + ((uint32_t)ui.rk_step << 9));
    for (i = 0; i < SQ_NN; i++) {
        sqn_t *q = &sqp.n[i];
        if (!rk_in(q, ui.rk_row, ui.rk_step))
            continue;
        n++;
        sq_u(i);
        if (more) {
            const uint8_t x = sqp.x[i];
            if (k == 0) {
                sqp.x[i] = SQX_MAKE(clamp(SQX_PIT(x) + d, -12, 12), SQX_COND(x));
            } else if (k == 1) {
                sqp.x[i] = SQX_MAKE(SQX_PIT(x), clamp((int32_t)SQX_COND(x) + d, 0, NSQC - 1));
            } else if (k == 2) {
                q->fl = (uint8_t)(d > 0 ? q->fl | SQF_GLIDE : q->fl & ~SQF_GLIDE);   /* (perkusja: bit GLIDE = FLAM) */
            } else {
                const uint32_t a = sq_auto(q->note);
                int32_t sh = (int32_t)sq_shown(q);
                sh = sh > (int32_t)L ? (int32_t)L : sh;
                if (a)
                    sq_drum_len(q, sh, clamp(sh + 6 * d, 1, (int32_t)L), a, L);
                else
                    q->len = (uint16_t)clamp(sh + 6 * d, 1, (int32_t)L);
            }
            continue;
        }
        if (k == 0) {
            q->vel = (uint8_t)clamp((int32_t)q->vel + d, 1, 127);
        } else if (k == 1) {
            q->chance = (uint8_t)clamp((int32_t)q->chance + 5 * d, 0, 100);
        } else if (k == 2) {
            q->fl = (uint8_t)((q->fl & ~SQF_RAT) | (uint32_t)clamp((int32_t)SQN_RAT(q) - 1 + d, 0, 7) << 1);
        } else {                                 /* NUDGE: od kratki swojego kroku, ±11 ticków */
            const uint32_t c = (q->t + 12u) / RK_T * RK_T % L;
            const int32_t o = clamp((int32_t)((q->t + L + 12u - c) % L) - 12 + d, -11, 11);
            q->t = (uint16_t)(((int32_t)c + o + (int32_t)L) % (int32_t)L);
        }
    }
    if (!n) {
        app_say("EMPTY STEP", "PRESS A WHITE KEY");
        return;
    }
    sq_changed();
}

static void sq_knob(uint32_t k, int32_t e)
{
    uint32_t i, L = sq_len();
    int32_t d;
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    if (ui.sq_pg == SQ_RACK) {                   /* (v0.57) RACK: krok / wiersz */
        rk_knob(k, e);
        return;
    }
    if (ui.sq_pg == SQ_FEEL && ui.sel_hold && k == 0) {   /* (v0.57) FEEL: SEL + K1 = RANDOM ROLLS */
        char t[8];
        ui.sel_used = 1;
        sqp.rolls = (uint8_t)clamp((int32_t)sqp.rolls + accel(EN_K1, e, 100), 0, 100);
        int_text(sqp.rolls, t, 0);
        text_cat(t, "%");
        app_say("RANDOM ROLLS", t);
        sq_changed();
        return;
    }
    if (ui.sq_pg == SQ_KEY) {                    /* (v1.1.0) KEY, SCALE, SNAP wzoru; SNAP / inna skala przy SNAP: nuty od nowa */
        if (k < 3u) {
            const int32_t v = clamp((int32_t)sqp.rsv[k] + (e > 0 ? 1 : -1), KYDEF[k].lo, KYDEF[k].hi);
            if (v != sqp.rsv[k]) {
                if (sqp.rsv[SQ_SNP_] || k == SQ_SNP_)
                    ev_push(EV_ALL_OFF);     /* (puszczenie klawisza po zmianie SNAP trafiłoby w inną nutę) */
                sqp.rsv[k] = (uint8_t)v;
                sq_changed();
            }
        }
        return;
    }
    if (ui.sq_pg == SQ_REC) {                    /* (v0.92) RECORD; (v1.1.0) K1 BPM, K3 COUNT IN z długością */
        const int32_t v = clamp(rc_get((int32_t)k) + (k == 0 ? accel(EN_K1, e, 260) : e > 0 ? 1 : -1), RCDEF[k].lo, RCDEF[k].hi);
        if (k == 0) {
            if (v == rc_get(0))
                return;
            sqp.bpm10 = (uint16_t)(v * 10);
            sq_changed();
        } else if (k == 1) {
            sqp.click = (uint8_t)v;
            sq_changed();
        } else if (k == 3) {
            sqp.recq = (uint8_t)v;
            sq_changed();
        } else {
            const int32_t c = v ? 1 | (v - 1) << 1 : rc_cin() & 6;   /* OFF: długość zostaje */
            glob.g[G_SQUNIT] = (int16_t)(8 | c);
            glob_dirty = 1;
        }
        return;
    }
    if (ui.sq_pg == SQ_OPT) {                    /* (v0.88) OPTIONS: pola GLO, zapis przy wyjściu (glob_commit) */
        const int32_t g = SQ_OPTG[k];
        const pdef_t *pd = &GDEF[g < 0 ? 0 : g];
        if (g < 0)
            return;
        const int32_t v = clamp(glob.g[g] + (pd->hi - pd->lo < 20 ? (e > 0 ? 1 : -1) : accel(EN_K1 + k, e, pd->hi - pd->lo)), pd->lo, pd->hi);
        if (v != glob.g[g]) {
            glob.g[g] = (int16_t)v;
            glob_dirty = 1;
        }
        return;
    }
    if (ui.sq_pg != SQ_NOTES) {                  /* PATTERN / FEEL */
        int32_t pg = ui.sq_pg, v;
        const pdef_t *pd = &SQDEF[pg - 1][k];
        v = clamp(sq_val(pg, (int32_t)k) + (pd->hi - pd->lo < 20 ? (e > 0 ? 1 : -1) : accel(EN_K1 + k, e, pd->hi - pd->lo)),
                  pd->lo, pd->hi);
        if (v == sq_val(pg, (int32_t)k))
            return;
        if (pg == SQ_PATTERN) {
            if (k == 0) {
                sq_set_pattern(v - 1);
                return;
            }
            if (k == 1) {                        /* STEPS; (v0.87) SEL trzymany: co takt (16 kroków) */
                if (ui.sel_hold) {
                    ui.sel_used = 1;
                    v = e > 0 ? (sq_val(pg, 1) + 16) / 16 * 16 : (sq_val(pg, 1) - 1) / 16 * 16;
                    v = v < 1 ? 1 : v;
                }
                sq_steps_set(v);
                return;
            } else if (k == 2) {
                sqp.bpm10 = (uint16_t)(v * 10);
            } else {
                sqp.swing = (uint8_t)v;
            }
        } else {
            if (k == 0) sqp.human = (uint8_t)v;
            else if (k == 1) sqp.vrand = (uint8_t)v;
            else if (k == 2) sqp.gate = (uint8_t)v;
            else sqp.click = (uint8_t)v;
        }
        sq_changed();
        return;
    }
    if (ui.sel_hold && k < 2u) {                 /* (v0.89) SEL + K1: zoom w pionie, SEL + K2: w poziomie (ile taktów na ekranie) */
        sq_zoom(k == 0u, e);
        return;
    }
    if (ui.sel_hold)
        ui.sel_used = 1;
    if (ui.sel_hold && k == 3) {                 /* (v0.90) SEL + K4: UNDO (w lewo) / REDO (w prawo); było VEL RANDOM (teraz karta) */
        sq_undo(e);
        return;
    }
    if (k == 0) {                                /* K1: nuty kroku od dołu (ta sama wysokość: od prawej), dalej: widok; (1.0.0, decyzja
                                                  * użytkownika 2026-11-04) w lewo = w górę (do v0.92 w prawo; INVERTED = dawny kierunek) */
        sq_vert(glob.g[G_SQK1INV] ? e : -e);
        return;
    }
    if (sq_tgt_first() < 0) {                    /* pusty krok: PITCH wstawia nutę (ostatnia wysokość) */
        if (k != 1)
            return;
        ui.sq_note = sq_add((uint32_t)ui.sq_pitch, 0u);
        sq_pick(0);
        sq_view_sel();
        return;
    }
    if (k == 2) {                                /* K3 LENGTH co krok GRID (v0.91; było co 1/4 kroku); (v0.90) SEL + K3: dowolnie (co tick) */
        sq_len_edit(ui.sel_hold ? 1 : 2, ui.sel_hold ? accel(EN_K3, e, (int32_t)L) : e, 0x300u + k);
        return;
    }
    d = k == 3 ? accel(EN_K4, e, 127) : -e;      /* (1.0.0, decyzja użytkownika 2026-11-04) K2 w lewo = wyżej */
    sq_um(0x100u + k + ((uint32_t)(ui.sq_note + 1) << 4) + ((uint32_t)ui.sq_rng << 15));
    for (i = 0; i < SQ_NN; i++) {                /* podświetlona albo wszystkie z zakresu */
        sqn_t *n = &sqp.n[i];
        if (!sq_tgt(i))
            continue;
        sq_u(i);
        if (k == 1) {                            /* K2 PITCH */
            n->note = (uint8_t)clamp((int32_t)n->note + d, 0, 127);
            ui.sq_pitch = n->note;
        } else {                                 /* K4 VELOCITY */
            n->vel = (uint8_t)clamp((int32_t)n->vel + d, 1, 127);
        }
    }
    if (k == 1)
        sq_view_sel();
    sq_changed();
}

/* karta nuty (SEL na nucie, jak arkusz w Melodee): SELECT wiersz, gałka zmienia, SEL robi (GLIDE, DELETE), HOME zamyka */
/* (v0.90, decyzja użytkownika 2026-11-03) QUANTIZE (nuta albo zaznaczenie do GRID) i VEL RANDOM (było SEL + K4) przy DELETE */
enum { SN_MOVE, SN_GLIDE, SN_CHANCE, SN_RATCH, SN_QUANT, SN_VELR, SN_DUP, SN_DEL, NSN };
static const char *const SN_NAME[NSN] = {"MOVE", "GLIDE", "CHANCE", "RATCHET", "QUANTIZE", "VEL RANDOM", "DUPLICATE", "DELETE"};

static void sqn_value(int32_t row, char *t)     /* wartość wiersza karty */
{
    const sqn_t *q = &sqp.n[sq_tgt_first() >= 0 ? sq_tgt_first() : 0];
    char n[8];
    t[0] = 0;
    if (row == SN_MOVE) {
        int32_t off = (int32_t)(q->t % sq_grid());
        if (!off) {
            text_cat(t, "ON GRID");
        } else {
            text_cat(t, "+");
            int_text(off, n, 0);
            text_cat(t, n);
            text_cat(t, " TICKS");
        }
    } else if (row == SN_GLIDE) {
        text_cat(t, q->fl & SQF_GLIDE ? "ON" : "OFF");
    } else if (row == SN_CHANCE) {
        int_text(q->chance, t, 0);
        text_cat(t, "%");
    } else if (row == SN_RATCH) {
        t[0] = 'X';
        int_text((int32_t)SQN_RAT(q), t + 1, 0);
    } else if (row == SN_DUP) {
        text_cat(t, ui.sq_rng ? "AFTER RANGE" : "NEXT STEP");
    } else if (row == SN_QUANT) {
        text_cat(t, L_GRID[sqp.grid]);
    } else if (row == SN_VELR) {
        text_cat(t, "+-20");
    } else {
        text_cat(t, "SEL");
    }
}

static void sqn_turn(int32_t e)                  /* karta: gałka zmienia wartość wiersza (na wszystkich edytowanych) */
{
    uint32_t L = sq_len(), i;
    int32_t f = sq_tgt_first();
    if (f < 0 || ui.sqn_row > SN_RATCH)
        return;
    sq_um(0x500u + ui.sqn_row + ((uint32_t)(ui.sq_note + 1) << 4) + ((uint32_t)ui.sq_rng << 15));
    if (ui.sqn_row == SN_MOVE && ui.sq_rng) {   /* zakres jedzie razem z nutami (o krok, nie o tick) */
        int32_t g = (int32_t)sq_grid(), sh = (e > 0 ? 1 : -1) * g;
        for (i = 0; i < SQ_NN; i++)
            if (sq_tgt(i)) {
                sq_u(i);
                sqp.n[i].t = (uint16_t)(((int32_t)sqp.n[i].t + sh + 4 * (int32_t)L) % (int32_t)L);
            }
        ui.sq_ra = (uint32_t)(((int32_t)ui.sq_ra + sh + 4 * (int32_t)L) % (int32_t)L);
        ui.sq_re = (uint32_t)(((int32_t)ui.sq_re + sh + 4 * (int32_t)L) % (int32_t)L);
        ui.sq_cur = ui.sq_re;
        sq_pick(0);
        sq_changed();
        return;
    }
    for (i = 0; i < SQ_NN; i++) {
        sqn_t *q = &sqp.n[i];
        if (!sq_tgt(i))
            continue;
        sq_u(i);
        if (ui.sqn_row == SN_MOVE) {             /* tick po ticku (poza gridem); kursor idzie za nutą */
            q->t = (uint16_t)(((int32_t)q->t + e + 4 * (int32_t)L) % (int32_t)L);
            ui.sq_cur = q->t / sq_grid() * sq_grid();
        } else if (ui.sqn_row == SN_GLIDE) {
            q->fl = (uint8_t)(e > 0 ? q->fl | SQF_GLIDE : q->fl & ~SQF_GLIDE);
        } else if (ui.sqn_row == SN_CHANCE) {
            q->chance = (uint8_t)clamp((int32_t)q->chance + e * 5, 0, 100);
        } else {
            q->fl = (uint8_t)((q->fl & ~SQF_RAT) | (uint32_t)clamp((int32_t)SQN_RAT(q) - 1 + (e > 0 ? 1 : -1), 0, 7) << 1);
        }
    }
    sq_pick(0);
    sq_changed();
}

static void sqn_ok(void)                         /* karta: SEL (na wszystkich edytowanych) */
{
    sqn_t *q;
    int32_t f = sq_tgt_first();
    uint32_t i;
    if (f < 0)
        return;
    q = &sqp.n[f];
    sq_um(0);
    if (ui.sqn_row == SN_QUANT || ui.sqn_row == SN_VELR) {   /* (v0.90) nuta albo zaznaczenie; karta zostaje (VEL RANDOM: znowu) */
        if (ui.sqn_row == SN_QUANT) {
            sq_quantize(2);
            app_say("QUANTIZED", L_GRID[sqp.grid]);
        } else {
            sq_vel_random();
        }
        sq_pick(0);
        sq_changed();
        return;
    }
    if (ui.sq_rng && (ui.sqn_row == SN_GLIDE || ui.sqn_row == SN_DUP || ui.sqn_row == SN_DEL)) {
        uint32_t lo = sq_rlo(), w = sq_rhi() - lo, L = sq_len(), on = !(q->fl & SQF_GLIDE), j = 0, full = 0;
        if (ui.sqn_row == SN_DUP) {              /* zakres skopiowany tuż za siebie, zaznaczenie na kopii */
            uint32_t src[(SQ_NN + 31) / 32];     /* (v0.90) bity zamiast listy: 56 B na stosie, było 896 B stałego RAM (UNDO) */
            for (i = 0; i < SQ_NN; i++)
                src[i >> 5] = (i & 31u ? src[i >> 5] : 0u) | (uint32_t)sq_tgt(i) << (i & 31u);
            for (i = 0; i < SQ_NN; i++) {
                if (!(src[i >> 5] >> (i & 31u) & 1u))
                    continue;
                for (; j < SQ_NN && sqp.n[j].vel; j++)
                    ;
                if (j == SQ_NN) {
                    full = 1;
                    break;
                }
                sq_u(j);
                sqp.n[j] = sqp.n[i];
                sqp.x[j] = sqp.x[i];
                sqp.n[j].t = (uint16_t)((sqp.n[i].t + w) % L);
            }
            ui.sq_ra = (lo + w) % L;
            ui.sq_re = (ui.sq_ra + w - sq_grid()) % L;
            if (ui.sq_re < ui.sq_ra)             /* (przez koniec wzoru: zaznaczenie do końca) */
                ui.sq_re = L - sq_grid();
            ui.sq_cur = ui.sq_ra;
            app_say(full ? "PATTERN FULL" : "NOTES", full ? "448 NOTES" : "DUPLICATED");
        } else {
            for (i = 0; i < SQ_NN; i++)
                if (sq_tgt(i)) {
                    sq_u(i);
                    if (ui.sqn_row == SN_DEL)
                        sqp.n[i].vel = 0;
                    else
                        sqp.n[i].fl = (uint8_t)(on ? sqp.n[i].fl | SQF_GLIDE : sqp.n[i].fl & ~SQF_GLIDE);
                }
            if (ui.sqn_row == SN_DEL) {
                ui.sq_rng = 0;
                ui.sq_note = sq_cell_note(0);
                ui.page = V_SEQ;
                app_say("NOTES", "DELETED");
            }
        }
        ui.sq_note = sq_cell_note(0);
        sq_pick(0);
        sq_changed();
        return;
    }
    if (ui.sqn_row != SN_DUP)
        sq_u((uint32_t)f);
    if (ui.sqn_row == SN_GLIDE) {
        q->fl ^= SQF_GLIDE;
    } else if (ui.sqn_row == SN_DUP) {           /* kopia o krok dalej, kursor na niej */
        sqn_t c = *q;
        int32_t j;
        sq_cursor((int32_t)ui.sq_cur + (int32_t)sq_grid());
        c.t = (uint16_t)((c.t + sq_grid()) % sq_len());
        for (j = 0; j < SQ_NN && sqp.n[j].vel; j++)
            ;
        if (j == SQ_NN) {
            app_say("PATTERN FULL", "448 NOTES");
            return;
        }
        sq_u((uint32_t)j);
        sqp.n[j] = c;
        sqp.x[j] = sqp.x[f];
        ui.sq_note = j;
        app_say("NOTE", "DUPLICATED");
    } else if (ui.sqn_row == SN_DEL) {
        q->vel = 0;
        ui.sq_note = sq_cell_note(0);
        ui.page = V_SEQ;
        app_say("NOTE", "DELETED");
    } else {
        ui.page = V_SEQ;                         /* reszta wierszy: SEL zamyka kartę */
    }
    sq_pick(0);
    sq_changed();
}

static void sq_sel_short(void)                   /* SEL krótko: NOTES = GLIDE, PATTERN / RACK = narzędzia, FEEL = REC QUANTIZE */
{
    uint32_t i, all = 1;
    if (ui.sq_pg == SQ_OPT || ui.sq_pg == SQ_REC || ui.sq_pg == SQ_KEY)   /* (v0.88) OPTIONS, (v0.92) RECORD, (v1.1.0) KEY: same gałki */
        return;
    if (ui.sq_pg == SQ_PATTERN || ui.sq_pg == SQ_RACK) {
        ui.page = V_SQT;
        ui.sqt_cur = 0;
        ui.sqt_sure = 0;
        ui.sqt_from = (uint8_t)(sq_utrk() == 0 ? 1 : 0);   /* MOVE: z T1 (stare wzory), na T1 z T2 */
        ui.redraw = 1;
        return;
    }
    if (ui.sq_pg == SQ_FEEL) {
        sqp.recq = (uint8_t)(sqp.recq ? 0u : sqp.grid + 1u);   /* (v0.92) ON = grid wzoru; wartość: REC trzymany */
        app_say("REC QUANTIZE", L_RECQ[sqp.recq]);
        sq_changed();
        return;
    }
    (void)i;
    (void)all;
    if (sq_tgt_first() < 0) {                    /* pusty krok (i pusty zakres): jak wpisać */
        app_say("EMPTY STEP", "PLAY KEYS OR TURN K2");
        return;
    }
    ui.page = V_SQN;                             /* karta nuty */
    ui.sqn_row = 0;
    ui.knob = -1;
    ui.redraw = 1;
}

static void sqt_ok(void)                         /* okno narzędzi wzoru: SEL robi (CLEAR: drugi raz) */
{
    uint32_t i, L = sq_len();
    if (ui.sqt_cur == ST_ZINV || ui.sqt_cur == ST_K1INV) {   /* kierunek gałki: przełącza, okno zostaje */
        int32_t g = ui.sqt_cur == ST_ZINV ? G_SQZINV : G_SQK1INV;
        glob.g[g] = (int16_t)!glob.g[g];
        glob_dirty = 1;                          /* zapis przy wyjściu (glob_commit, jak GLO) */
        ui.redraw = 1;
        return;
    }
    if ((ui.sqt_cur == ST_CLEAR || ui.sqt_cur == ST_CLRALL) && !ui.sqt_sure) {
        ui.sqt_sure = 1;
        ui.redraw = 1;
        return;
    }
    sq_um(0);
    if (ui.sqt_cur == ST_QUANT) {
        sq_quantize(1);
        app_say("QUANTIZED", L_GRID[sqp.grid]);
    } else if (ui.sqt_cur == ST_DOUBLE) {
        uint32_t nsrc = SQ_NN;
        const uint32_t ns2 = L / SQ_STEP * 2u;
        if (ns2 > SQ_MAXBARS * 16u) {
            app_say("TOO LONG", "256 STEPS MAX");
            return;
        }
        sq.hold = 1;
        for (i = 0; i < nsrc; i++) {             /* kopia każdej nuty o długość wzoru dalej */
            uint32_t j;
            if (!sqp.n[i].vel || sqp.n[i].t >= L)
                continue;
            for (j = 0; j < SQ_NN && sqp.n[j].vel; j++)
                ;
            if (j == SQ_NN)
                break;
            sq_u(j);
            sqp.n[j] = sqp.n[i];
            sqp.x[j] = sqp.x[i];
            sqp.n[j].t = (uint16_t)(sqp.n[i].t + L);   /* (kopia: t >= L, nie kopiuje się drugi raz) */
        }
        sq_steps_set((int32_t)ns2);
        sq.hold = 0;
        app_say("DOUBLED", "");
        sq_changed();
    } else if (ui.sqt_cur == ST_MOVE) {          /* (v0.66.2) nuty ścieżki sqt_from na edytowaną (stare wzory: T1) */
        const int32_t to = sq_utrk() < 0 ? 0 : sq_utrk();
        char m[3] = {'T', (char)('1' + ui.sqt_from), 0};
        uint32_t moved = 0;
        if (ui.sqt_from == to) {
            app_say(m, "IS THIS TRACK");
            return;
        }
        sq.hold = 1;
        for (i = 0; i < SQ_NN; i++)
            if (sqp.n[i].vel && SQN_TRK(&sqp.n[i]) == ui.sqt_from) {
                sq_u(i);
                sqp.n[i].fl = (uint8_t)((sqp.n[i].fl & ~SQF_TRK) | (uint32_t)to << 4);
                moved++;
            }
        sq.hold = 0;
        app_say(m, moved ? "NOTES MOVED HERE" : "HAS NO NOTES");
        sq_changed();
    } else {                                     /* (v0.66.1) CLEAR: nuty wybranej ścieżki; CLEAR ALL TRACKS: wszystkie */
        const int all = ui.sqt_cur == ST_CLRALL;
        sq.hold = 1;
        for (i = 0; i < SQ_NN; i++)
            if (sqp.n[i].vel && (all || sq_mine(&sqp.n[i]))) {
                sq_u(i);
                sqp.n[i].vel = 0;
            }
        sq.hold = 0;
        app_say(all ? "ALL TRACKS" : mt_ui() ? "TRACK" : "PATTERN", "CLEARED");
        sq_changed();
    }
    ui.page = V_SEQ;
    sq_pick(0);
    ui.redraw = 1;
}

static void sq_keys(uint32_t pressed, uint32_t released, uint32_t keys)   /* klawisze na stronie NOTES */
{
    uint32_t k;
    if (ui.page != V_SEQ || ui.sq_pg != SQ_NOTES || sq.play)
        return;
    /* wpis (jak w Melodee): na pustym kroku klawisze wpisują nutę / akord, po puszczeniu kursor dalej; na kroku z nutami
     * tylko grają (REC: wpisują i tam; ta sama nuta w kursorze usuwa) */
    if (pressed && !ui.st_on) {
        ui.st_on = (uint8_t)(sq_cell_count() == 0 || sq.rec);
        sq_um(0);                                /* (v0.90) UNDO: akord jednym krokiem */
    }
    if (ui.st_on)
        for (k = 0; k < NKEYS; k++)
            if (pressed >> k & 1u) {
                int32_t note = key_note(k), i = sq_find((uint32_t)note);
                uint32_t v = key_vel(k);
                if (i >= 0 && !(ui.st_mine[i >> 5] >> (i & 31) & 1u)) {
                    sq_u((uint32_t)i);
                    sqp.n[i].vel = 0;            /* REC: ta sama nuta w kursorze usuwa */
                    sq_changed();
                } else if (i < 0 && (i = sq_add((uint32_t)note, v)) >= 0) {
                    ui.st_mine[i >> 5] |= 1u << (i & 31);
                    ui.st_added = 1;
                    ui.sq_pitch = (uint8_t)note;
                }
            }
    if (!keys && released) {
        for (k = 0; k < sizeof ui.st_mine / 4u; k++)
            ui.st_mine[k] = 0;
        if (ui.st_added)
            sq_cursor((int32_t)(ui.sq_cur + sq_grid()));
        ui.st_added = 0;
        ui.st_on = 0;
    }
    for (k = 0; k < NKEYS; k++)                  /* widok pokazuje graną nutę */
        if (pressed >> k & 1u) {
            int32_t note = key_note(k);
            if (note < ui.sq_low)
                ui.sq_low = note;
            else if (note > ui.sq_low + sq_rows() - 1)
                ui.sq_low = note - sq_rows() + 1;
        }
    ui.redraw = 1;
}

static void sq_autosave(uint32_t now)            /* wzór i ustawienia SEQ / ARP: w ciszy, 5 s po zmianie */
{
    if ((!sqp_dirty && !sqg_dirty) || sq.play || sq.rec || ar.nl || backup_hold || !out_quiet()
        || now - sq_dirty_at < AUTOSAVE_MS || now - ui.input_at < FLASH_IDLE_MS)
        return;
    if (sqp_dirty)
        sq_store();
    if (sqg_dirty)
        sqg_store();
}

static void go_glo(void)                 /* GLO: ustawienia globalne; drugi raz: z powrotem tam, skąd przyszliśmy */
{
    if (ui.page == V_GLO || ui.page == V_INFO || ui.page == V_RESET || ui.page == V_NOTE) {
        glob_commit();
        if (PANEL_PAGE(ui.glo_back))
            ui.page = ui.glo_back;
        else if (ui.glo_back >= 0)
            show_page(ui.glo_back);
        else
            ui.page = V_HOME;
        ui.knob = -1;
        ui.redraw = 1;
        return;
    }
    ui.glo_back = ui.page;
    ui.page = V_GLO;
    ui.glo_sec = -1;                         /* zawsze od listy sekcji */
    ui.knob = -1;
    ui.quick = 0;
    ui.redraw = 1;
}

static int32_t glo_count(int32_t sec)    /* pozycje sekcji (SYSTEM: działania) */
{
    int32_t n = 0;
    if (sec == GS_SYSTEM)
        return NSY;
    while (n < GS_N && GS_ITEM[sec][n] >= 0)
        n++;
    return n;
}

static int32_t glo_item(int32_t k)       /* pole GLO pod gałką K1..K4 na bieżącej stronie sekcji, -1: puste */
{
    int32_t i = ui.glo_page * 4 + k;
    return ui.page == V_GLO && ui.glo_sec >= 0 && ui.glo_sec != GS_SYSTEM && i < glo_count(ui.glo_sec)
           ? GS_ITEM[ui.glo_sec][i] : -1;
}

static void glo_set(int32_t g, int32_t v)   /* zmiana pola GLO (zapis przy wyjściu z GLO) */
{
    v = clamp(v, GDEF[g].lo, GDEF[g].hi);
    if (v == GV(g))
        return;
    GV(g) = (int16_t)v;
    glob_dirty = 1;
    glob_apply();
    if (g == G_CPU2) {
        if (!v)
            plat_cpu2_set(0);                /* od razu: jeden rdzeń */
        else if (!plat_cpu2_on())
            app_say("2ND CORE", "AFTER RESTART");   /* drugi rdzeń startuje tylko przy włączeniu */
    }
}

static void factory_reset(int32_t what)  /* 0 ustawienia GLO, 1 presety (USER, nadpisane fabryczne, banki DX7), 2 wszystko */
{
    int32_t b, e;
    if (what != 1) {
        for (b = 0; b < NGALL; b++)
            GV(b) = GDEF[b].def;
        gb_def(glob.gbc);                    /* (v0.91) krzywe PERFORM */
        glob_dirty = 1;
        glob_apply();
    }
    if (what != 0) {
        for (b = 0; b <= NBANK; b++) {
            uint32_t o = b < NBANK ? UOBJ[b] : OBJ_FACT;
            bk.magic = BANK_MAGIC;
            bk.used = 0;
            plat_store_save(o, &bk, bank_size(o));
            bank_read(o);
        }
        syx.magic = SYX_MAGIC;
        syx.used = syx.cz_used = 0;
        plat_store_save(OBJ_SYX, &syx, sizeof syx);
        fm1_dirty = 1;
        for (b = 0; b < NFO; b++)            /* efekty presetów: puste zapisy (bieżące: niżej, z brzmieniami) */
            fxo_create(b);
        fx_hash(fx_saved);
        for (b = 0; b < 4; b++)
            fx_saved[b] = ~fx_saved[b];      /* bieżące zapiszą się od nowa */
        fx_dirty = 1;
        xt_create();                         /* dodatki presetów (v0.38): puste, bieżące zapiszą się od nowa */
        xt_saved = ~xt_saved;
        xt_dirty = 1;
        for (e = 0; e < NENG; e++)           /* bieżące brzmienia: pierwszy preset fabryczny każdego silnika */
            if (what == 2 || set.preset[e] >= ID_USER) {
                int32_t keep = set.eng;
                set.eng = (uint8_t)e;
                load_preset(eng_first(e) + 1);
                set.eng = (uint8_t)keep;
            }
        settings_save();
    }
}

static void glo_sel(void)                /* SEL w GLO: wejście do sekcji, w SYSTEM działanie */
{
    if (ui.glo_sec < 0) {
        ui.glo_sec = (int8_t)ui.glo_cur;
        ui.glo_page = 0;
        ui.sy_cur = 0;
    } else if (ui.glo_sec == GS_SYSTEM) {
        if (ui.sy_cur == SY_INFO) {
            ui.page = V_INFO;
        } else if (ui.sy_cur == SY_HINTS) {
            glo_set(G_HINTS, !glob.g[G_HINTS]);
        } else if (ui.sy_cur == SY_RESET) {
            ui.page = V_RESET;
            ui.reset_opt = 0;
            ui.reset_sure = 0;
        } else {
            ui.page = V_NOTE;
            ui.note = ui.sy_cur;
        }
    }
    ui.knob = -1;
    ui.redraw = 1;
}

static void glo_select(int32_t e)        /* SELECT w GLO: sekcja na liście, strona w sekcji, działanie w SYSTEM */
{
    if (ui.glo_sec < 0) {
        ui.glo_cur = (uint8_t)((ui.glo_cur + e % NGS + NGS) % NGS);
    } else if (ui.glo_sec == GS_SYSTEM) {
        ui.sy_cur = (uint8_t)((ui.sy_cur + e % NSY + NSY) % NSY);
    } else {
        int32_t np = (glo_count(ui.glo_sec) + 3) / 4;
        ui.glo_page = (uint8_t)((ui.glo_page + e % np + np) % np);
        ui.knob = -1;
    }
    ui.redraw = 1;
}

static void quick_page(int32_t pg)       /* strona chwilowa z HOME (wraca po 2 s) */
{
    int32_t g = page_group(pg), keep = ui.last[set.eng][g];   /* nie zmienia, gdzie wraca sekcja */
    show_page(pg);
    ui.last[set.eng][g] = keep;
    ui.quick = 1;
    ui.qback = -1;                           /* (v0.65) wraca do HOME (gest: tam, skąd) */
}
static void quick_show(void) { quick_page(E->quick); }   /* gałka z HOME: strona szybka silnika */
static void algo_show(int32_t id)        /* (v0.64.3) ALGO z HOME: strona z parametrem pod ALGO (decyzja użytkownika
                                          * 2026-10-30, wszystkie silniki; było: strona szybka) — gałka, MORE albo SEL */
{
    int32_t p, k;
    for (p = 0; p < E->npages; p++)
        for (k = 0; k < 4; k++)
            if (E->knob(p, k) == id || (E->more && E->more(p, k) == id)) {
                quick_page(p);
                if (E->knob(p, k) != id && has_more(p))
                    ui.more_lock = 1;
                ui.knob = k;                 /* jej kolumna świeci jak przy kręceniu gałką */
                ui.knob_at = plat_ms();
                return;
            }
    for (p = 0; p < E->npages; p++)
        if (E->sel(p) == id) {
            quick_page(p);
            return;
        }
    quick_show();
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -3, 3);
    char t[12];
    if (o == set.octave)
        return;
    set.octave = (int8_t)o;
    set_dirty = 1;
    int_text(o, t, 1);
    if (ENG_GRP[set.eng] == EG_DRUM)         /* (v0.56) perkusja: półtony, układ klawiszy zostaje */
        text_cat(t, " SEMI");
    app_say(ENG_GRP[set.eng] == EG_DRUM ? "DRUM TUNE" : "OCTAVE", t);
}

/* gałka: krok przyspiesza przy szybkim kręceniu (duże zakresy) */
static int32_t accel(uint32_t k, int32_t e, int32_t span)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_at[k];
    ui.enc_at[k] = now;
    if (!glob.g[G_ACCEL])                    /* GLO KNOB ACCEL OFF: zawsze krok po kroku */
        return e;
    if (span >= 1000 && dt < 40u)
        return e * 10;
    if (span >= 100 && dt < 30u)
        return e * 3;
    return e;
}

static void gest_turn(uint32_t k, int32_t e);
/* (v0.47) PERFORM — warstwa jak SKIFF FX layer / Gross Beat (decyzja użytkownika 2026-10-28): FX trzymany + klawisz =
 * efekt, póki trzymasz klawisz (bez nuty); SEL = LATCH (klawisz włącza / wyłącza; trzymane zostają); gałki FILTER, CRUSH,
 * THROW, DEPTH (bez LATCH wracają po puszczeniu FX). Efekty odtwarzania: gra ostatni trzymany (puszczony: wraca
 * poprzedni); GATE, LPF, HPF, CRUSH, DUB nakładają się. DSP: ogs_perf.c. */
static const char *const PF_NAME[PF_N] = {"REPEAT 1/4", "REPEAT 1/4T", "REPEAT 1/8", "REPEAT 1/8T", "REPEAT 1/16",
                                          "REPEAT 1/16T", "REPEAT 1/32", "REVERSE", "ROLL DOWN", "HALF SPEED", "ROLL UP",
                                          "TAPE STOP", "FREEZE", "SCRATCH", "GATE", "REWIND", "LPF", "BUILD", "HPF",
                                          "OCT DOWN", "OCT UP", "CRUSH", "RANDOM", "VINYL", "DUB"};
static void pf_apply(void)               /* UI -> DSP: efekt odtwarzania (ostatni trzymany) i nakładane */
{
    uint32_t act = ui.pf_keys | ui.pf_lat, i, n = 0;
    int32_t pb = -1;
    for (i = 0; i < ui.pf_n; i++)            /* kolejność: tylko te, które grają */
        if (act >> ui.pf_ord[i] & 1u)
            ui.pf_ord[n++] = ui.pf_ord[i];
    ui.pf_n = (uint8_t)n;
    if (n)
        pb = ui.pf_ord[n - 1u];
    ogs_pf_pb = pb;
    ogs_pf_st = act & PF_STACK;
    ui.redraw = 1;
}
static void pf_macros(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        ogs_pf_mac[k] = (float)ui.pf_mac[k] * 0.01f;
}
static void pf_enter(void)
{
    if (!mt_ui())                            /* (v0.74, decyzja użytkownika 2026-10-30) PERFORM tylko w MULTI */
        return;
    ui.pf_back = ui.page;
    ui.page = V_PERF;
    ui.knob = -1;
    ui.redraw = 1;
}
static void pf_leave(void)               /* FX puszczony: strona sprzed PERFORM; bez LATCH gałki wracają */
{
    if (ui.page == V_PERF)
        ui.page = ui.pf_back;
    if (!ui.pf_latch) {
        ui.pf_mac[0] = ui.pf_mac[1] = ui.pf_mac[2] = 0;
        ui.pf_mac[3] = 100;
        pf_macros();
    }
    ui.knob = -1;
    ui.redraw = 1;
}
/* (v0.91, B13: decyzja użytkownika 2026-11-03 „globalnie”, klawisze moje) krzywe: w PERFORM OCT+ / OCT- = bank CURVES, krzywe 1-4 na białych
 * F3, G3, A3, B3; trzymany klawisz krzywej + K1 STEP, K2 VOLUME, K3 TIME, K4 SMOOTH; SEL = LATCH jak efekty */
static int32_t cv_of_key(uint32_t k) { return k == 0 ? 0 : k == 2 ? 1 : k == 4 ? 2 : k == 6 ? 3 : -1; }
static void cv_apply(void)
{
    int32_t c = -1, i;
    uint32_t held = 0;
    for (i = 0; i < 7; i += 2)
        if (ui.cv_held >> i & 1u)
            held |= 1u << cv_of_key((uint32_t)i);
    if (held)
        c = held >> ui.cv_last & 1u ? ui.cv_last : __builtin_ctz(held);
    else if (ui.cv_lat)
        c = ui.cv_lat >> ui.cv_last & 1u ? ui.cv_last : __builtin_ctz(ui.cv_lat);
    ogs_pf_cv = c;
    ui.redraw = 1;
}
static int32_t cv_edit(void)             /* krzywa ustawiana gałkami: trzymany klawisz krzywej */
{
    return ui.page == V_PERF && (ui.cv_held >> (ui.cv_last * 2u) & 1u) ? ui.cv_last : -1;
}
static void pf_key(uint32_t k, int down)
{
    uint32_t b = 1u << k, i, n = 0;
    if (k >= PF_N)
        return;
    if (!down && (ui.cv_held & b)) {         /* (v0.91) puszczony klawisz krzywej (bank już może być inny) */
        ui.cv_held &= ~b;
        cv_apply();
        if (!ui.pf_hold && !ui.pf_keys && !ui.cv_held && ui.page == V_PERF)
            pf_leave();
        return;
    }
    if (down && ui.pf_bank) {                /* (v0.91) bank CURVES */
        const int32_t c = cv_of_key(k);
        if (c < 0)
            return;
        ui.pf_used = 1;
        ui.cv_last = (uint8_t)c;
        if (ui.page != V_PERF)
            pf_enter();
        if (ui.pf_latch)
            ui.cv_lat ^= (uint8_t)(1u << c);
        ui.cv_held |= b;                     /* (trzymany: gałki ustawiają krzywą także przy LATCH) */
        cv_apply();
        return;
    }
    if (!down) {
        ui.pf_keys &= ~b;
        pf_apply();
        if (!ui.pf_hold && !ui.pf_keys && ui.page == V_PERF)   /* (v0.90) FX puszczony wcześniej: teraz jak puszczony FX */
            pf_leave();
        return;
    }
    ui.pf_used = 1;
    ui.pf_last = (uint8_t)k;
    if (ui.page != V_PERF)
        pf_enter();
    if (ui.pf_latch)
        ui.pf_lat ^= b;
    else
        ui.pf_keys |= b;
    if (!(PF_STACK & b)) {                   /* na koniec kolejki: gra ten */
        for (i = 0; i < ui.pf_n; i++)
            if (ui.pf_ord[i] != k)
                ui.pf_ord[n++] = ui.pf_ord[i];
        ui.pf_ord[n++] = (uint8_t)k;
        ui.pf_n = (uint8_t)n;
    }
    pf_apply();
}
static void pf_sel(void)                 /* SEL: LATCH (trzymane zostają zatrzaśnięte; OFF: wszystko zatrzaśnięte cichnie) */
{
    ui.pf_latch ^= 1u;
    if (ui.pf_latch)
        ui.pf_lat |= ui.pf_keys;
    else
        ui.pf_lat = 0;
    if (ui.pf_latch) {                       /* (v0.91) krzywe: trzymane zostają zatrzaśnięte */
        uint32_t i;
        for (i = 0; i < 7u; i += 2u)
            if (ui.cv_held >> i & 1u)
                ui.cv_lat |= (uint8_t)(1u << cv_of_key(i));
    } else {
        ui.cv_lat = 0;
    }
    cv_apply();
    app_say("LATCH", ui.pf_latch ? "ON" : "OFF");
    pf_apply();
}
/* (v0.90, decyzja użytkownika 2026-11-03) FX + trzymany klawisz efektu + K1-K3 = ustawienia tego efektu: K1 SYNC (BPM / KEY), K2 START,
 * K3 LENGTH (BEAT, BAR, 2 BARS, 4 BARS); bez klawisza gałki jak dotąd. FX puszczony przy trzymanym klawiszu: dalej, do puszczenia klawisza. */
static int32_t pf_edit(void)             /* efekt ustawiany gałkami: ostatni wciśnięty, póki trzymany; -1 żaden */
{
    return ui.page == V_PERF && (ui.pf_keys >> (ui.pf_last % PF_N) & 1u) ? (int32_t)(ui.pf_last % PF_N) : -1;
}
static int32_t pf_def_len(int32_t e) { return e == PF_LPF || e == PF_HPF || e == PF_BUILD ? 2 : 1; }   /* 1 BEAT, 2 BAR */
static void pf_cfg_knob(int32_t j, uint32_t k, int32_t e)
{
    uint32_t c = glob.pfs[j];
    int32_t s = (int32_t)(c >> 1 & 127u), q = (int32_t)(c >> 8 & 7u);
    if (k == 0 && (PF_HAS_SYNC >> j & 1u))
        c = (c & ~1u) | (e > 0 ? 1u : 0u);
    else if (k == 1 && (PF_HAS_RUN >> j & 1u))
        c = (c & ~(127u << 1)) | (uint32_t)clamp(s + accel(EN_K2, e, 100), 0, 99) << 1;
    else if (k == 2 && (PF_HAS_RUN >> j & 1u))
        c = (c & 0xFFu) | (uint32_t)clamp((q ? q : pf_def_len(j)) + (e > 0 ? 1 : -1), 1, 4) << 8;
    else
        return;
    if (c != glob.pfs[j]) {
        glob.pfs[j] = (uint16_t)c;
        ogs_pf_cfg[j] = (uint16_t)c;
        mt_gdirty = 1;                       /* (zapis GLO w ciszy, jak mikser MULTI: nie w trakcie grania) */
    }
}
static void pf_knob(uint32_t k, int32_t e)
{
    int32_t lo = k ? 0 : -100, v = clamp(ui.pf_mac[k] + accel(EN_K1 + k, e, k ? 100 : 200), lo, 100);
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.pf_used = 1;
    if (cv_edit() >= 0) {                    /* (v0.91) krzywa: K1 STEP, K2 VOLUME, K3 TIME, K4 SMOOTH */
        uint8_t *g = glob.gbc + cv_edit() * 32 + ui.cv_step * 2;
        if (k == 0)
            ui.cv_step = (uint8_t)clamp(ui.cv_step + e, 0, 15);
        else if (k == 1)
            g[0] = (uint8_t)clamp(g[0] + accel(EN_K2, e, 100), 0, 100);
        else if (k == 2)
            g[1] = (uint8_t)clamp(g[1] + e, 0, 15);
        else
            glob.gbc[PF_NCV * 32 + cv_edit()] = (uint8_t)clamp(glob.gbc[PF_NCV * 32 + cv_edit()] + accel(EN_K4, e, 100), 0, 100);
        mt_gdirty = 1;                       /* (zapis GLO w ciszy) */
        ui.redraw = 1;
        return;
    }
    if (pf_edit() >= 0) {
        pf_cfg_knob(pf_edit(), k, e);
        ui.redraw = 1;
        return;
    }
    if (v != ui.pf_mac[k]) {
        ui.pf_mac[k] = (int8_t)v;
        pf_macros();
    }
    ui.redraw = 1;
}
static void fx_press(void)               /* FX krótko (przy puszczeniu): strona FX, w niej MORE */
{
    if (ui.page == V_FX) {
        ui.fx_more = (uint8_t)((ui.fx_more + 1) % fx_npages(fx_cur()));
        ui.knob = -1;
        ui.redraw = 1;
    } else {
        go_fx();
    }
}

static void knob_turn(uint32_t k, int32_t e)
{
    int32_t id;
    if (ui.page == V_CPY)                    /* (v0.92) okno kopii: tylko SELECT / SEL / HOME */
        return;
    if (ui.page == V_SQT) {                  /* (v0.66.2) narzędzia wzoru: gałka = ścieżka MOVE HERE FROM */
        if (ui.sqt_cur == ST_MOVE) {
            ui.sqt_from = (uint8_t)clamp((int32_t)ui.sqt_from + (e > 0 ? 1 : -1), 0, MT_N - 1);
            ui.redraw = 1;
        }
        return;
    }
    if (ui.page == V_MOD) {                  /* (v0.65) strona MOD: ilości modulacji */
        mod_knob(k, e);
        return;
    }
    if (ui.gest && ui.page >= V_HOME) {      /* v0.35: LFO / ENV trzymany: modulacja */
        gest_turn(k, e);
        return;
    }
    if (ui.page == V_HOME && mt_ui()) {      /* (v0.82) MULTI: gałki z HOME = mikser wybranej ścieżki */
        mt_knob(k, e);
        return;
    }
    if (ui.page == V_FX) {
        fx_knob(k, e);
        return;
    }
    if (ui.page == V_SEQ) {
        sq_knob(k, e);
        return;
    }
    if (ui.page == V_ARP) {
        arp_knob(k, e);
        return;
    }
    if (ui.page == V_PERF) {
        pf_knob(k, e);
        return;
    }
    if (ui.page == V_SQN) {                  /* karta nuty: każda gałka zmienia wiersz */
        sqn_turn(e);
        return;
    }
    if (ui.page <= V_SAVE)                   /* w oknach gałki nic nie robią (wybór: SELECT) */
        return;
    if (ui.page == V_GLO) {
        int32_t g = glo_item((int32_t)k);
        if (algo_eng(g) >= 0 && algo_eng(g) < NENG) {   /* ALGO KNOB: DEFAULT, potem gałki stron silnika po kolei */
            static int16_t ids[128];
            int32_t en = algo_eng(g), n = algo_list(en, ids), at = -1, j;
            char t[24];
            for (j = 0; j < n; j++)
                if (ids[j] == glob.g[g])
                    at = j;
            at = clamp(at + (e > 0 ? 1 : -1), -1, n - 1);
            glo_set(g, at < 0 ? -1 : ids[at]);
            t[0] = 0;
            if (at >= 0) {
                text_cat(t, ENG[en].page_name[algo_page(en, ids[at])]);
                text_cat(t, " ");
                text_cat(t, ENG[en].def(ids[at])->name);
            }
            app_say(ENG[en].name, at < 0 ? "DEFAULT" : t);
        } else if (g >= 0)
            glo_set(g, GV(g) + accel(EN_K1 + k, e, GDEF[g].hi - GDEF[g].lo));
        ui.knob = (int32_t)k;
        ui.knob_at = plat_ms();
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_HOME)                   /* z ekranu głównego gałki od razu kręcą stronę szybką */
        quick_show();
    if (ui.quick)
        ui.quick_at = plat_ms();
    if (ui.sel_hold)
        ui.sel_used = 1;
    ui.browse = 0;
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    if (ui.page == PG_CHORD) {
        chord_turn(k, e);
        return;
    }
    if ((id = knob_id((int32_t)k)) < 0)
        return;
    set_param(id, param_step(id, accel(EN_K1 + k, e, E->def(id)->hi - E->def(id)->lo)));
}

/* (v1.1.0, P7) GLO > MIDI > CC IN: CC 21-24 = K1-K4 strony silnika (z HOME: strona szybka, jak gałki), wartość wprost:
 * 0-127 na cały zakres parametru. Inne ekrany: nic. */
static void cc_knob(uint32_t k, uint32_t v)
{
    int32_t id;
    const pdef_t *d;
    if (ui.page == V_HOME && !mt_ui())
        quick_show();
    if (ui.page < 0 || ui.page == PG_CHORD || ui.gest || (id = knob_id((int32_t)k)) < 0)
        return;
    if (ui.quick)
        ui.quick_at = plat_ms();
    d = E->def(id);
    set_param(id, d->lo + ((d->hi - d->lo) * (int32_t)v + 63) / 127);
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
}

static void algo_turn(int32_t e)         /* ALGO: parametr silnika (MOOG: CUTOFF, DX7: ALGORITHM) */
{
    char t[16];
    if (ui.page <= V_GLO && ui.page != V_SEQ && ui.page != V_ARP)   /* SEQ (poza NOTES) i ARP: ALGO jak zawsze */
        return;
    if (ui.page == V_HOME)
        algo_show(algo_id());
    if (ui.quick)
        ui.quick_at = plat_ms();
    ui.browse = 0;
    {
        int32_t id = algo_id();                  /* GLO ALGO KNOB albo parametr silnika */
        set_param(id, param_step(id, accel(EN_ALGO, e, E->def(id)->hi - E->def(id)->lo)));
        value_text(id, pget(id), t);
        app_say(E->def(id)->name, t);
    }
}

/* podpisane czarne klawisze FM1 (OP1..OP6, PITCH, GLOBAL, MONO, POLY): skróty z trzymanym SEL */
static const uint8_t LABEL_KEY[NLABEL] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22};   /* klawisz 0 = F3 */

static int32_t key_label(uint32_t key)
{
    int32_t l;
    for (l = 0; l < NLABEL && LABEL_KEY[l] != key; l++)
        ;
    return l;
}

static uint32_t label_avail(void)        /* klawisze, które przy trzymanym SEL coś robią */
{
    uint32_t b = 0;
    int32_t l;
    for (l = 0; l < NLABEL; l++)
        if (E->label_page(l, G_ALL, V_HOME) >= 0 || l == K_MONO || l == K_POLY)   /* (v0.64.4) stałe skróty */
            b |= 1u << LABEL_KEY[l];
    return b;
}

static void label_key(int32_t l)
{
    int32_t pg = E->label_page(l, G_ALL, ui.page);   /* (v0.64.4) z każdej strony tam, gdzie z HOME; (v0.64.5)
                                                     * strona podana tylko dla koła pod klawiszem (WAVE PITCH: LFO 1-4) */
    if (l == K_MONO || l == K_POLY) {
        char t[12];
        ui.browse = 0;
        set_param(E->mode, l == K_POLY);
        value_text(E->mode, pget(E->mode), t);
        app_say("MODE", t);
    } else if (pg >= 0) {
        show_page(pg);
    }
}

static int32_t drum_page_lane(void)      /* (v0.88) perkusja: dźwięk tej strony silnika (0..), -1: inna strona / silnik, KIT (cały zestaw) */
{
    if (ui.page < 0 || ui.page == PG_CHORD || ENG_GRP[set.eng] != EG_DRUM || ui.page == E->quick)
        return -1;
    return set.eng == ENG_MORPH ? mp_lane : set.eng == ENG_DUAL ? du_lane : dm_lane;
}
static uint32_t label_lit(void)          /* jasno: klawisz bieżącej strony (OP2 na OSC 2 / OP 2 ...); (v0.88) perkusja: klawisz dźwięku */
{
    int32_t l = ui.page >= 0 && ui.page != PG_CHORD ? E->page_label(ui.page) : -1;
    const int32_t d = drum_page_lane();
    return (l >= 0 ? 1u << LABEL_KEY[l] : 0u) | (d >= 0 ? 1u << (RK_NOTE[d] - 53u) : 0u);
}

/* SAVE: na INIT od razu nowy preset; na innym okno NADPISZ / NOWY (SELECT wybiera, SEL zatwierdza, HOME nie) */
static const char CHARSET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

static void name_start(int32_t id)       /* okno nazwy: propozycja = obecna nazwa albo USER nn */
{
    int32_t k;
    if (id < 0) {
        app_say("USER 128 / 128", "FULL");
        ui.page = ui.save_back;
        ui.redraw = 1;
        return;
    }
    ui.save_id = id;
    preset_name(id >= ID_USER && !rec_used(id) && CUR >= ID_SYX ? CUR : id, ui.name);   /* z banku: jego nazwa */
    for (k = 0; k < NAME_LEN; k++)
        if (!ui.name[k]) {
            for (; k < NAME_LEN; k++)
                ui.name[k] = ' ';
            break;
        }
    ui.name[NAME_LEN] = 0;
    ui.cur = 0;
    ui.page = V_NAME;
    ui.redraw = 1;
}

static void name_char(int32_t e)         /* SELECT: litera pod kursorem */
{
    int32_t n = (int32_t)sizeof CHARSET - 1, i;
    for (i = 0; i < n && CHARSET[i] != ui.name[ui.cur]; i++)
        ;
    if (i == n)
        i = 0;
    ui.name[ui.cur] = CHARSET[((i + e) % n + n) % n];
    ui.redraw = 1;
}

static void import_open(void)            /* bank przyszedł: okno IMPORT, propozycja = pierwsze wolne miejsce */
{
    int32_t b, nb = ui.imp_cz ? NCZB_USE : NSYX_USE;
    uint32_t u = ui.imp_cz ? syx.cz_used : syx.used;
    for (b = 0; b < nb && (u >> b & 1u); b++)
        ;
    ui.imp_slot = b < nb ? b : 0;
    if (ui.page > V_SAVE)
        ui.imp_back = ui.page == V_GLO ? V_HOME : ui.page;
    ui.page = V_IMPORT;
    syx_ready = 1;
    ui.redraw = 1;
}

static void ui_imp_cz(int cz) { ui.imp_cz = (uint8_t)cz; }

static void import_cz_ok(void)           /* bank CZ do miejsca: sektor = dwa banki (drugi przepisany z flasha) */
{
    uint32_t off = CZ_BASE + (uint32_t)ui.imp_slot / 2u * 0x1000u, i;
    const uint8_t *old = plat_data(off + ((uint32_t)ui.imp_slot & 1u ? 0u : 0x800u));
    int32_t c;
    char t[16];
    syx_ready = 0;
    if (ui.imp_slot & 1) {                   /* nowy w drugiej połowie */
        for (i = 0x800u; i-- > 0u;)
            syx_in[0x800u + i] = syx_in[i];
        for (i = 0; i < 0x800u; i++)
            syx_in[i] = old[i];
    } else {
        for (i = 0; i < 0x800u; i++)
            syx_in[0x800u + i] = old[i];
    }
    if (plat_data_erase(off) != 0 || plat_data_write(off, syx_in, 0x1000u) != 0) {
        app_say("CZ BANK", "WRITE FAILED");
        go_home();
        return;
    }
    for (c = 0; c < NAME_LEN; c++)
        syx.cz_name[ui.imp_slot][c] = syx_in_name[c];
    syx.cz_used |= 1u << ui.imp_slot;
    plat_store_save(OBJ_SYX, &syx, sizeof syx);
    syx_gen++;
    syx_in_name[0] = 0;
    cz_in_mask = 0;
    preset_go(ID_CZ + ui.imp_slot * 16);
    go_home();
    syx_bank_name(NSYX + ui.imp_slot, t);
    app_say("SAVED", t);
}

static int bank_store(int32_t slot)      /* bank DX7 z syx_in do miejsca slot (0-7) + nazwa; 0: błąd zapisu */
{
    uint32_t off = SYX_BASE + (uint32_t)slot * 0x1000u;
    int32_t c;
    if (plat_data_erase(off) != 0 || plat_data_write(off, syx_in + 5, 4096u) != 0)
        return 0;
    for (c = 0; c < NAME_LEN; c++)
        syx.name[slot][c] = syx_in_name[c];
    syx.used |= 1u << slot;
    plat_store_save(OBJ_SYX, &syx, sizeof syx);
    syx_gen++;
    fm1_dirty = 1;                           /* (v0.53) może to bank FM1 (albo coś go nadpisało) */
    syx_in_name[0] = 0;
    return 1;
}

static void import_ok(void)              /* SEL w oknie IMPORT: bank do miejsca, pierwsze brzmienie gra */
{
    char t[16];
    if (ui.imp_cz) {
        import_cz_ok();
        return;
    }
    syx_ready = 0;
    if (!bank_store(ui.imp_slot)) {
        app_say("DX7 BANK", "WRITE FAILED");
        go_home();
        return;
    }
    preset_go(syx_is_fm1(ui.imp_slot) ? ID_FM1 + (int32_t)(fm1_slot(0) == ui.imp_slot ? 0 : fm1_slot(1) == ui.imp_slot ? 32
                                                     : fm1_slot(2) == ui.imp_slot ? 64 : 96)
                                  : ID_SYX + ui.imp_slot * 32);
    go_home();
    syx_bank_name(ui.imp_slot, t);
    app_say("SAVED", t);
}

/* (v1.0.2, decyzja użytkownika 2026-11-04) ALL PRESETS: SEL trzymany na presecie USER + SEL w oknie DELETE? = usuwa go;
 * dalej gra następny z listy. Efekty i dodatki miejsca zostają we flashu: następny SAVE w to miejsce je nadpisuje */
static void user_del(void)
{
    const int32_t id = preset_now(), next = preset_walk(id, 1, -1);
    const uint32_t o = rec_obj(id);
    char t[NAME_LEN + 1];
    ui.allp_del = 0;
    ui.redraw = 1;
    preset_name(id, t);
    bank_read(o);
    bk.used &= ~(1u << rec_slot(id));
    if (plat_store_save(o, &bk, bank_size(o)) != 0) {
        bank_read(o);
        app_say("DELETE", "FAILED");
        return;
    }
    bank_read(o);
    if (ui.allp_back == id)
        ui.allp_back = next;                 /* (HOME nie wraca do usuniętego) */
    preset_go(next);
    app_say("DELETED", t);
}

static void save_ok(void)               /* SEL w oknach: zatwierdza (HOME anuluje) */
{
    if (ui.page == V_NAME) {
        ui.page = ui.save_back;
        ui.redraw = 1;
        save(ui.save_id, ui.name);
    } else if (ui.page == V_SAVE) {
        name_start(ui.save_new ? free_slot() : CUR);
    } else if (ui.page == V_ALLP || ui.page == V_ENGP) {
        go_home();
    } else if (ui.page == V_IMPORT) {
        import_ok();
    } else if (ui.page == V_CPY) {
        cp_ok();
    } else if (ui.page == V_RESET) {
        if (!ui.reset_sure) {
            ui.reset_sure = 1;               /* drugie potwierdzenie */
        } else {
            static const char *const W[3] = {"SETTINGS", "PRESETS", "EVERYTHING"};
            factory_reset(ui.reset_opt);
            ui.page = V_GLO;
            app_say("RESET", W[ui.reset_opt]);
        }
        ui.redraw = 1;
    } else if (ui.page == V_NOTE && ui.note == SY_UPDATE) {   /* (v1.1.0, P39) SEL drugi raz: tryb aktualizacji (jak REC przy włączaniu) */
        plat_uboot();
    } else if (ui.page == V_INFO || ui.page == V_NOTE) {
        ui.page = V_GLO;
        ui.redraw = 1;
    } else if (ui.page == V_FXL) {
        fxl_ok();
    } else if (ui.page == V_MTL) {
        mtl_ok();
    } else if (ui.page == V_SCN) {
        mt_scene_load();
    } else if (ui.page == V_SQT) {
        sqt_ok();
    } else if (ui.page == V_SQN) {
        sqn_ok();
    }
}

static void mt_scene_save(void);
static void save_key(void)
{
    if (ui.page == V_SCN) {                  /* (v0.86) lista scen: SAVE = zapis sceny pod kursorem */
        mt_scene_save();
        return;
    }
    if (ui.page == V_HOME && mt_ui() && glob.g[G_MT0 + mt_at_cur()] >= NENG)   /* (v0.85) pusta ścieżka: nie ma czego zapisać */
        return;
    if (ui.page <= V_SAVE && !PANEL_PAGE(ui.page))   /* w oknie SAVE nic nie robi: zatwierdza SEL */
        return;
    ui.save_back = ui.page == V_GLO ? V_HOME : ui.page;
    glob_commit();
    ui.knob = -1;
    ui.quick = 0;
    if (is_init(CUR) || CUR >= ID_SYX) {     /* INIT i brzmienia z banków: zawsze nowy USER */
        name_start(free_slot());
        return;
    }
    ui.page = V_SAVE;
    ui.save_new = 1;                         /* bezpieczniej: domyślnie NOWY */
    ui.redraw = 1;
}

static void home_key(void)               /* HOME: wraca; w ALL PRESETS / ENGINE do tego sprzed otwarcia */
{
    if (ui.page == V_ALLP && ui.allp_del) {   /* (v1.0.2) okno DELETE?: nie */
        ui.allp_del = 0;
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_CPY) {                  /* (v0.92) kopia dźwięku: NIE */
        ui.page = ui.cp_back;
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_FXL) {                  /* lista efektów: z powrotem, bez zmiany */
        ui.page = V_FX;
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_SQT || ui.page == V_SQN) {   /* narzędzia wzoru, karta nuty: z powrotem */
        ui.page = V_SEQ;
        ui.redraw = 1;
        return;
    }
    if (sq_in_set()) {                       /* (v0.88) ustawienia SEQ: widok (RACK / NOTES); (v0.90) lista scen: główny ekran (niżej) */
        sq_view_back();
        return;
    }
    if (ui.page == V_INFO || ui.page == V_RESET || ui.page == V_NOTE) {   /* okna SYSTEM: z powrotem do SYSTEM */
        ui.page = V_GLO;
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_GLO && ui.glo_sec >= 0) {   /* z sekcji: lista sekcji */
        ui.glo_cur = (uint8_t)ui.glo_sec;
        ui.glo_sec = -1;
        ui.knob = -1;
        ui.redraw = 1;
        return;
    }
    if (ui.page == V_ALLP && ui.allp_back != preset_now())
        preset_go(ui.allp_back);
    if (ui.page == V_MTL)                    /* (v0.86) lista silników: z powrotem, bez zmiany */
        mt_red = -1;
    if (ui.page == V_ENGP) {
        mt_red = -1;
        engine_switch(ui.engp_back);
    }
    if (ui.page == V_IMPORT) {
        syx_ready = 0;
        app_say(ui.imp_cz ? "CZ BANK" : "DX7 BANK", "CANCELLED");
        cz_in_mask = 0;
    }
    go_home();
}

static void sel_cycle(void)              /* SEL: opcja strony w pętli (FILTER: KEY TRACK OFF .. FULL, OFF ...) */
{
    if (ui.page == V_HOME && mt_ui()) {  /* (v0.82) MULTI: MUTE wybranej ścieżki; (v0.86) pusta: lista silników (jak pusty slot FX) */
        if (glob.g[G_MT0 + mt_at_cur()] >= NENG)
            mtl_open();
        else
            mt_mute();
        return;
    }
    if (ui.page == V_FX) {
        fx_mode();
        return;
    }
    if (ui.page == V_SEQ) {
        sq_sel_short();
        return;
    }
    if (ui.page == V_ARP) {
        arp_sel();
        return;
    }
    if (ui.page == V_PERF) {
        pf_sel();
        return;
    }
    int32_t id, v;
    const pdef_t *d;
    char t[24];
    if (ui.page == PG_CHORD) {
        chord_learn();
        return;
    }
    if (ui.page < 0 || (id = E->sel(ui.page)) < 0)
        return;
    d = E->def(id);
    v = pget(id);
    ui.browse = 0;
    set_param(id, v < d->hi ? v + 1 : d->lo);
    if (ui.quick)
        ui.quick_at = plat_ms();
    value_text(id, pget(id), t);
    app_say(d->name, t);
}

static void preset_turn(int32_t e, int all)   /* PRESETS: w silniku; all (HOME trzymany / okno): wszystkie */
{
    if (mt_ui())                             /* (v0.64) MULTI: bez ALL PRESETS (tylko silniki ścieżek) */
        all = 0;
    if (ui.page == V_HOME && mt_ui() && ui.sel_hold)   /* (v0.86) SEL trzymany + PRESETS: bez listy silników i MUTE */
        ui.sel_used = 1;
    if (ui.sel_hold)                         /* (v1.1.0, P33) SEL trzymany: efekty zostają (load_preset) */
        ui.sel_used = 1;
    keep_fx = (uint8_t)(ui.sel_hold != 0);
    if (ui.page == V_HOME && mt_ui() && glob.g[G_MT0 + mt_at_cur()] >= NENG)   /* (v0.85) pusta ścieżka: bez brzmienia */
        return;
    if (ui.page == V_ENGP) {                 /* okno ENGINE: PRESETS = brzmienie tego silnika (gra od razu, okno zostaje) */
        if (mt_ui() && (mt_red >= 0 || glob.g[G_MT0 + mt_at_cur()] >= NENG))
            return;                          /* (v0.85) MULTI: czerwony albo pusty: bez brzmienia */
        preset_go(preset_step(preset_now(), e, set.eng));
        ui.redraw = 1;
        return;
    }
    if (ui.page <= V_SAVE && ui.page != V_ALLP && !PANEL_PAGE(ui.page))   /* na stronie FX: presety z ich efektami */
        return;
    if (all && ui.page != V_ALLP) {
        ui.allp_back = preset_now();
        ui.page = V_ALLP;
        ui.allp_sel = ui.allp_del = 0;
    }
    preset_go(preset_step(preset_now(), e, ui.page == V_ALLP ? -1 : set.eng));
    if (ui.page != V_ALLP && !PANEL_PAGE(ui.page))
        go_home();
    ui.redraw = 1;
}


static void mt_enter(void)
{
    glob.g[G_MULTI] = 1;
    mt_fix();
    if (!mt_ui()) {                      /* (żadnej ścieżki: domyślne) */
        int32_t t;
        for (t = 0; t < MT_N; t++)
            glob.g[G_MT0 + t] = GDEF[G_MT0 + t].def;
        glob.g[G_MULTI] = 1;
        mt_fix();
    }
    mt_gdirty = 1;
    mt_focus_go(glob.g[G_MTFOC]);
    mt_ver++;
    glob_apply();                        /* (kolor MULTI) */
    ui.fx_slot = ui.fx_win = 0;
    go_home();                           /* (v0.85) HOME z oscyloskopem = główny ekran MULTI */
}
static void mt_leave(void)               /* wybrany silnik zostaje */
{
    glob.g[G_MULTI] = 0;
    mt_gdirty = 1;
    mt_ver++;
    glob_apply();                        /* (kolor SOLO) */
    ui.fx_slot = ui.fx_win = 0;
    go_home();
}
static void mode_toggle(void)            /* (v0.86, decyzja użytkownika 2026-10-30) HOME trzymany: SOLO <-> MULTI, napis przez chwilę */
{
    if (mt_ui())
        mt_leave();
    else
        mt_enter();
    ui.mode_until = plat_ms() + MODE_SHOW_MS;
    ui.redraw = 1;
    if (ann_on())
        ann_say(mt_ui() ? "MULTI" : "SOLO", 0, ANN_NOW);
}
static int mt_others(int32_t t)          /* inna ścieżka niż t gra */
{
    int32_t k;
    for (k = 0; k < MT_N; k++)
        if (k != t && glob.g[G_MT0 + k] < NENG)
            return 1;
    return 0;
}
static void mt_engine_set(int32_t t, int32_t e)   /* silnik ścieżki t (MT_NONE: pusta); nie może grać obok reszty: czerwony, ścieżka gra dalej */
{
    mt_red = -1;
    if (!mt_ok(t, e, glob.g + G_MT0)) {
        mt_red = (int8_t)e;
        return;
    }
    if (e == glob.g[G_MT0 + t])
        return;
    glob.g[G_MT0 + t] = (int16_t)e;
    glob.g[G_MTS0 + t] = 0;              /* (v0.82) inny silnik: wysyłki od MIX jego presetu */
    if (e < NENG) {
        mt_focus_go(t);
    } else if (glob.g[G_MTFOC] == t && mt_first() >= 0) {   /* wybrana zostaje pusta: gra inna, kursor zostaje */
        mt_focus_go(mt_first());
        mt_cur = (uint8_t)t;
    }
    mt_gdirty = 1;
    mt_ver++;
    ui.redraw = 1;
}
static void mt_why(int32_t t, int32_t e, char *o)   /* (v0.81) dlaczego e nie zagra na ścieżce t, np. „NOT WITH MINI” */
{
    int32_t k;
    o[0] = 0;
    for (k = 0; k < MT_N; k++) {
        const int32_t x = glob.g[G_MT0 + k];
        if (k == t || x >= NENG)
            continue;
        if (mt_code((uint32_t)x) == mt_code((uint32_t)e)) {
            char n[3] = {'T', (char)('1' + k), 0};
            text_cat(o, x == e ? "ALREADY ON " : "SAME CODE AS ");
            text_cat(o, x == e ? n : ENG[x].name);
            return;
        }
        if (mt_clash((uint32_t)e, (uint32_t)x)) {
            text_cat(o, "NOT WITH ");
            text_cat(o, ENG[x].name);
            return;
        }
    }
}
/* (v0.85, decyzja użytkownika 2026-10-30) okno ENGINE w MULTI (HOME na HOME), jak w SOLO, dla ścieżki pod kursorem: silniki ścieżki (T1 perkusja,
 * T2-T4 reszta), EMPTY, w kółko. (v0.86) tryb: HOME trzymany, sceny: SEQ trzymany */
#define MW_MAX (NENG + 1)
static int32_t mw_list(int32_t t, int16_t *o)
{
    int32_t n = 0, i;
    for (i = 0; i < NENG; i++)
        if (t > 0 || mt_drum(ENG_ORDER[i]))  /* (v0.91) T2-T4: wszystkie, też perkusja */
            o[n++] = ENG_ORDER[i];
    if (mt_others(t))
        o[n++] = MT_NONE;
    return n;
}
static int32_t mw_now(int32_t t) { return mt_red >= 0 ? mt_red : glob.g[G_MT0 + t]; }
static int32_t mw_find(const int16_t *o, int32_t n, int32_t x)
{
    int32_t i;
    for (i = 0; i < n && o[i] != x; i++)
        ;
    return i < n ? i : 0;
}
static const char *mw_name(int32_t x) { return x >= NENG ? "EMPTY" : ENG[x].name; }
static void mt_say(int32_t t)            /* wybrana ścieżka i jej silnik w stopce */
{
    char h[3] = {'T', (char)('1' + t), 0};
    app_say(h, mw_name(glob.g[G_MT0 + t]));
}
static void mt_engine(int32_t d)         /* (v0.85) ALGO na głównym ekranie MULTI: silnik ścieżki, po kolei; (v0.86) jak ALGO na slocie FX: tylko te, które zagrają */
{
    int16_t o[MW_MAX];
    const int32_t t = mt_at_cur(), n = mw_list(t, o);
    int32_t at = mw_find(o, n, glob.g[G_MT0 + t]), i;
    if (ui.sel_hold)                     /* SEL trzymany + ALGO: bez listy i MUTE */
        ui.sel_used = 1;
    for (i = 0; i < n; i++) {
        at = (at + (d > 0 ? 1 : n - 1)) % n;
        if (mt_ok(t, o[at], glob.g + G_MT0))
            break;
    }
    if (o[at] == glob.g[G_MT0 + t])      /* nic innego tu nie zagra */
        return;
    mt_engine_set(t, o[at]);
    ev_push(EV_ALL_OFF);
    mt_say(t);
}
static void mt_step(int32_t d)           /* (v0.64.2) SELECT na głównym ekranie MULTI: ścieżka; (v0.85) też pusta (ALGO daje jej silnik); (v0.86) jak slot FX, bez koła */
{
    const int32_t a = mt_at_cur(), t = clamp(a + (d > 0 ? 1 : -1), 0, MT_N - 1);
    if (ui.sel_hold)                     /* SELECT przy trzymanym SEL: bez listy */
        ui.sel_used = 1;
    if (t == a)
        return;
    mt_red = -1;
    mt_cur = (uint8_t)t;
    mt_focus_go(t);
    mt_say(t);
    ui.redraw = 1;
}
/* (v0.86, decyzja użytkownika 2026-10-30) lista silników ścieżki, jak lista efektów: SEL trzymany na głównym ekranie MULTI (pusta ścieżka: też SEL
 * krótko); SELECT kursor, ALGO grupy, SEL OK, HOME wraca bez zmiany; nic nie gra przed SEL. Czerwone: nie zagrają obok reszty, z powodem */
enum { MTL_ROWS = 7, MTL_HDR = 64, MTL_MAX = MW_MAX + NEG };
static int32_t mtl_rows(int32_t t, int8_t *r)    /* wiersze: EMPTY na górze, dalej nagłówek grupy (MTL_HDR + grupa) i jej silniki */
{
    int16_t o[MW_MAX];
    const int32_t n = mw_list(t, o);
    int32_t i, m = 0, g = -1;
    if (o[n - 1] >= NENG)
        r[m++] = (int8_t)MT_NONE;
    for (i = 0; i < n; i++)
        if (o[i] < NENG) {
            if (ENG_GRP[o[i]] != g) {
                g = ENG_GRP[o[i]];
                r[m++] = (int8_t)(MTL_HDR + g);
            }
            r[m++] = (int8_t)o[i];
        }
    return m;
}
static int32_t mtl_at(void)              /* silnik pod kursorem (ścieżki zmienione przez SysEx przy otwartej liście: ostatni wiersz) */
{
    int8_t r[MTL_MAX];
    const int32_t n = mtl_rows(mt_at_cur(), r);
    return r[ui.mtl_cur < n ? ui.mtl_cur : n - 1];
}
static void mtl_show(void)               /* przewija tak, żeby kursor (i nagłówek jego grupy) był widoczny */
{
    int8_t r[MTL_MAX];
    int32_t c = ui.mtl_cur, top;
    (void)mtl_rows(mt_at_cur(), r);
    top = c > 0 && r[c - 1] >= MTL_HDR ? c - 1 : c;
    if (top < ui.mtl_top)
        ui.mtl_top = (uint8_t)top;
    else if (c > ui.mtl_top + MTL_ROWS - 1)
        ui.mtl_top = (uint8_t)(c - (MTL_ROWS - 1));
    ui.redraw = 1;
}
static void mtl_open(void)
{
    int8_t r[MTL_MAX];
    const int32_t t = mt_at_cur(), n = mtl_rows(t, r);
    int32_t i;
    for (i = 0; i < n && r[i] != glob.g[G_MT0 + t]; i++)
        ;
    if (i == n)                              /* (nie powinno się zdarzyć: pierwszy silnik) */
        i = r[0] >= MTL_HDR ? 1 : 0;
    mt_red = -1;
    ui.mtl_cur = (uint8_t)i;
    ui.mtl_top = (uint8_t)clamp(i - 3, 0, n > MTL_ROWS ? n - MTL_ROWS : 0);
    mtl_show();
    ui.knob = -1;
    ui.page = V_MTL;
}
static void mtl_move(int32_t d)          /* SELECT: kursor po kolei, bez koła (jak lista efektów), nagłówki pomija */
{
    int8_t r[MTL_MAX];
    const int32_t n = mtl_rows(mt_at_cur(), r), s = d > 0 ? 1 : -1;
    int32_t c = (int32_t)ui.mtl_cur + s;
    if (c >= 0 && c < n && r[c] >= MTL_HDR)
        c += s;
    if (c >= 0 && c < n)
        ui.mtl_cur = (uint8_t)c;
    mt_red = -1;
    mtl_show();
}
static void mtl_group(int32_t e)         /* ALGO: pierwszy silnik następnej / poprzedniej grupy, w kółko (EMPTY jak grupa, jak lista efektów) */
{
    int8_t r[MTL_MAX], at[NEG + 1];
    const int32_t n = mtl_rows(mt_at_cur(), r);
    int32_t i, g = 0, ng = 0;
    for (i = 0; i < n; i++)
        if (i == 0 && r[0] < MTL_HDR)
            at[ng++] = 0;
        else if (r[i] >= MTL_HDR)
            at[ng++] = (int8_t)(i + 1);
    if (ng < 2) {                        /* (T1 bez EMPTY: jedna grupa) */
        mtl_move(e);
        return;
    }
    for (i = 0; i < ng; i++)
        if (at[i] <= ui.mtl_cur)
            g = i;
    ui.mtl_cur = (uint8_t)at[(g + (e > 0 ? 1 : ng - 1)) % ng];
    mt_red = -1;
    mtl_show();
}
static void mtl_ok(void)                 /* SEL w liście: ten silnik na ścieżce; czerwony: powód, lista zostaje */
{
    const int32_t t = mt_at_cur(), e = mtl_at();
    char r[24];
    if (e >= MTL_HDR)                    /* (j.w.: nagłówek) */
        return;
    if (!mt_ok(t, e, glob.g + G_MT0)) {
        mt_why(t, e, r);
        mt_red = (int8_t)e;
        app_say(ENG[e].name, r);
        return;
    }
    if (e != glob.g[G_MT0 + t]) {
        mt_engine_set(t, e);
        ev_push(EV_ALL_OFF);
    }
    go_home();
    mt_say(t);
}
static void mt_pan_text(int32_t v, char *t)  /* (v0.72) PAN: C, L25, R40 */
{
    t[0] = 0;
    if (!v) {
        text_cat(t, "C");
        return;
    }
    t[0] = v < 0 ? 'L' : 'R';
    int_text(v < 0 ? -v * 2 : v * 2, t + 1, 0);   /* (0..100 na ekranie) */
}
static int32_t mt_send_ui(int32_t t, uint32_t k)   /* (v0.82) wysyłka 0-100 na ekranie: pole albo MIX z presetu */
{
    const uint32_t v = mt_sf(t, k);
    const fx_slot_t *c = mt_fxe((uint32_t)glob.g[G_MT0 + t], k);
    return v ? (int32_t)v - 1 : c ? (c->p[3] * 100 + 63) / 127 : 100;
}
static void mt_send_set(int32_t t, uint32_t k, int32_t v)   /* v: 0-100; -1: jak MIX w presecie */
{
    const uint32_t sh = k * 7u;
    glob.g[G_MTS0 + t] = (int16_t)(((uint32_t)glob.g[G_MTS0 + t] & ~(127u << sh)) | (uint32_t)(v + 1) << sh);
    mt_gdirty = 1;
}
static void mt_knob(uint32_t k, int32_t e)   /* (v0.82) K1-K4 na HOME w MULTI: VOL, PAN, REVERB, DELAY wybranej ścieżki */
{
    static const char *const NM[4] = {" VOL", " PAN", " REVERB", " DELAY"};
    const int32_t t = mt_at_cur(), d = accel(EN_K1 + k, e, 100);
    char h[12], x[8];
    int32_t v;
    if (glob.g[G_MT0 + t] >= NENG)       /* (v0.85) pusta ścieżka */
        return;
    mt_red = -1;
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    if (ui.sel_hold)
        ui.sel_used = 1;
    if (k == 0u) {
        v = clamp(glob.g[G_MTV0 + t] + d, 0, 100);
        glob.g[G_MTV0 + t] = (int16_t)v;
        mt_gdirty = 1;
    } else if (k == 1u) {
        v = clamp(glob.g[G_MTP0 + t] + d, -50, 50);
        glob.g[G_MTP0 + t] = (int16_t)v;
        mt_gdirty = 1;
    } else {
        v = clamp(mt_send_ui(t, k - 2u) + d, 0, 100);
        mt_send_set(t, k - 2u, v);
    }
    h[0] = 'T';
    h[1] = (char)('1' + t);
    h[2] = 0;
    text_cat(h, NM[k]);
    if (k == 1u)
        mt_pan_text(v, x);
    else
        int_text(v, x, 0);
    app_say(h, x);
}
static void mt_mute(void)                /* (v0.82) SEL na HOME w MULTI: MUTE wybranej ścieżki */
{
    const int32_t t = mt_at_cur();
    char h[4] = {'T', (char)('1' + t), 0, 0};
    if (glob.g[G_MT0 + t] >= NENG)
        return;
    mt_red = -1;
    glob.g[G_MTMUTE] = (int16_t)(glob.g[G_MTMUTE] ^ (1 << t));
    mt_gdirty = 1;
    app_say(h, glob.g[G_MTMUTE] >> t & 1 ? "MUTE" : "ON");
    ui.redraw = 1;
}
/* (v0.73) sceny MULTI: zapis i wczytanie; (v0.86) lista scen; (v0.88) strona ustawień SEQ (SELECT scena, SEL wczytuje, SAVE zapisuje, HOME wraca) */
static uint8_t mt_scn_pend;              /* scena 1..SCN_N: brzmienia do wczytania, gdy wyjście przygaśnie (0: nic) */
static void mt_scene_presets(void)       /* app_frame: przerwanie zastosowało nowe ścieżki (wyjście było cicho) */
{
    const scn_t *c;
    int32_t k;
    if (!mt_scn_pend || *(volatile uint16_t *)&mt_seen != mt_ver)
        return;
    c = &glob.scn[mt_scn_pend - 1u];
    mt_scn_pend = 0;
    pend_preset = -1;
    for (k = 0; k < MT_N; k++) {             /* zapisane presety (load_preset wybiera ścieżkę silnika) */
        const int32_t e = glob.g[G_MT0 + k], id = c->pre[k];
        if (e < NENG && id < NALL && preset_eng(id) == e)
            load_preset(id);
    }
    /* (v0.82) wysyłki ze sceny (load_preset wraca je do MIX presetu) */
    {
        const uint16_t *ss = glob.scs[c - glob.scn];
        uint32_t off = 256u;                 /* (v0.88) MASTER FX przypięty / odpięty ze sceny (bity 14-15) */
        for (k = 0; k < MT_N; k++) {
            glob.g[G_MTS0 + k] = (int16_t)(ss[k] & 0x3FFFu);
            off |= ((uint32_t)ss[k] >> 14 & 1u) << k | ((uint32_t)ss[k] >> 15 & 1u) << (4 + k);
        }
        glob.g[G_MFXOFF] = (int16_t)off;
    }
    if (glob.g[G_MT0 + c->foc] < NENG)       /* wybrana ze sceny (load_preset przestawiał na każdą po kolei) */
        mt_focus_go(c->foc);
    ev_push(EV_ALL_OFF);
    ui.redraw = 1;
}
static void mt_scene_save(void)
{
    const int32_t mt_scn = ui.scn_cur;
    scn_t *c = &glob.scn[mt_scn];
    char h[12], t[12];
    int32_t k, ed = -1;
    c->ok = 1;
    c->foc = (uint8_t)glob.g[G_MTFOC];
    c->mute = (uint8_t)glob.g[G_MTMUTE];
    c->rsv = SCN_NUM;                        /* (v0.78) numeracja presetów */
    for (k = 0; k < MT_N; k++) {
        const int32_t e = glob.g[G_MT0 + k];
        c->eng[k] = (uint8_t)e;
        c->vol[k] = (uint8_t)glob.g[G_MTV0 + k];
        c->pan[k] = (int8_t)glob.g[G_MTP0 + k];
        c->pre[k] = (uint16_t)(e < NENG ? set.preset[e] : 0xFFFFu);
        glob.scs[mt_scn][k] = (uint16_t)((uint32_t)glob.g[G_MTS0 + k] | (mt_linked(k, 0) ? 0u : 1u << 14) | (mt_linked(k, 1) ? 0u : 1u << 15));
        if (e < NENG && set.edited[e] && ed < 0)
            ed = k;
    }
    glob.scm[mt_scn] = glob.mfx;             /* (v0.85) MASTER FX */
    glob_dirty = 1;                          /* (zapis w ciszy jak GLO MULTI) */
    mt_gdirty = 1;
    h[0] = 0;
    text_cat(h, "SCENE ");
    int_text(mt_scn + 1, t, 0);
    text_cat(h, t);
    if (ed >= 0) {                           /* niezapisane brzmienie: scena pamięta zapisany preset */
        t[0] = 'T';
        t[1] = (char)('1' + ed);
        t[2] = 0;
        text_cat(t, " NOT SAVED");
        app_say(h, t);
    } else {
        app_say(h, "SAVED");
    }
    ui.redraw = 1;
}
static void mt_scene_load(void)
{
    const int32_t mt_scn = ui.scn_cur;
    const scn_t *c = &glob.scn[mt_scn];
    int32_t k;
    if (!c->ok) {
        app_say("SCENE", "EMPTY");
        return;
    }
    for (k = 0; k < MT_N; k++) {
        glob.g[G_MT0 + k] = (int16_t)(c->eng[k] < NENG ? c->eng[k] : MT_NONE);
        glob.g[G_MTV0 + k] = (int16_t)clamp(c->vol[k], 0, 100);
        glob.g[G_MTP0 + k] = (int16_t)clamp(c->pan[k], -50, 50);
    }
    glob.g[G_MTMUTE] = (int16_t)(c->mute & 15u);
    glob.g[G_MTFOC] = (int16_t)c->foc;
    glob.g[G_MULTI] = 1;
    glob.mfx = glob.scm[mt_scn];             /* (v0.85) MASTER FX */
    mt_fix();
    if (!mt_ui()) {                          /* (żadnej poprawnej ścieżki) */
        app_say("SCENE", "EMPTY");
        return;
    }
    mt_gdirty = 1;
    mt_ver++;                                /* całe wyjście przygasa, ścieżki od nowa; brzmienia po przygaszeniu */
    mt_scn_pend = (uint8_t)(mt_scn + 1u);    /* (mt_scene_presets z app_frame) */
    mt_focus_go(glob.g[G_MTFOC]);
    ev_push(EV_ALL_OFF);
    {
        char h[12], t[8];
        h[0] = 0;
        text_cat(h, "SCENE ");
        int_text(mt_scn + 1, t, 0);
        text_cat(h, t);
        app_say(h, "LOADED");
    }
    go_home();
}
static void scn_open(void)               /* (v0.90) MULTI: HOME na głównym ekranie = lista scen (było: ustawienia SEQ za OPTIONS) */
{
    glob_commit();
    ui.page = V_SCN;
    ui.knob = -1;
    ui.quick = 0;
    ui.browse = 0;
    ui.redraw = 1;
}
static void engine_open(void)            /* HOME na HOME: okno ENGINE (SELECT wybiera, SEL ok, HOME wraca); (v0.90) w MULTI: lista scen
                                          * (decyzja użytkownika 2026-11-03; okno ENGINE w MULTI znika: silnik = ALGO albo SEL trzymany) */
{
    if (mt_ui()) {
        scn_open();
        return;
    }
    mt_red = -1;
    ui.engp_back = set.eng;
    ui.page = V_ENGP;
    ui.knob = -1;
    ui.redraw = 1;
}

/* SELECT w oknie ENGINE: silnik po kolei (każdy z własnym brzmieniem), gra od razu; ALGO (grp): pierwszy silnik
 * następnej / poprzedniej grupy */
static void engine_turn(int32_t e, int grp)
{
    int32_t at = eng_at(set.eng);
    if (grp) {                           /* ALGO: następna niepusta grupa, w kółko */
        int32_t g = ENG_GRP[set.eng], n;
        for (n = 0; n < NEG; n++) {
            g = (g + (e > 0 ? 1 : NEG - 1)) % NEG;
            for (at = 0; at < NENG && ENG_GRP[ENG_ORDER[at]] != g; at++)
                ;
            if (at < NENG)
                break;
        }
    } else {
        at = ((at + e) % NENG + NENG) % NENG;
    }
    engine_switch(ENG_ORDER[at]);
    ev_push(EV_ALL_OFF);
    ui.redraw = 1;
}

/* (v0.86, decyzja użytkownika 2026-10-30) EDIT trzymany + gałka = strona szybka silnika (FILTER, FM6: ALGO), w SOLO i MULTI; działa jak ENV / LFO niżej.
 * (v0.65, decyzje użytkownika 2026-10-30, gest.md „NOWY GEST”): ENV / LFO trzymany na stronie silnika albo HOME —
 * gałka = szybki dostęp do AMP ENV / LFO 1 (strona chwilowa, po puszczeniu i 2 s bez kręcenia powrót tam, skąd),
 * klawisz OP n = strona MOD n-tego źródła (zostaje: SELECT = strony celów, K1-K4 = ilość, zamyka jej przycisk albo
 * HOME). Bez kręcenia i klawisza przycisk działa przy puszczeniu jak dotąd (strona sekcji / MORE, dłużej: pierwsza). */
static uint32_t gest_keys(void)          /* klawisze OP, które mają źródło */
{
    uint32_t b = 0;
    int32_t l;
    if (ui.gest - 1u == B_EDIT)
        return 0;
    for (l = K_OP1; l <= K_OP6; l++)
        if (E->gest && E->gest((ui.gest_src & 0xF0) | l, -1, GEST_ASK, 0) >= 0)
            b |= 1u << LABEL_KEY[l];
    return b;
}

static int32_t env_page(void)            /* ENV + gałka: AMP ENV (PHASE: DCA 1), inaczej pierwsza strona ENV */
{
    static const char *const AMP[2] = {"AMP ENV", "DCA 1"};
    int32_t i, j;
    for (i = 0; i < grp_n(G_ENV); i++)
        for (j = 0; j < 2; j++) {
            const char *a = pg_name(grp_at(G_ENV, i)), *b = AMP[j];
            while (*a && *a == *b)
                a++, b++;
            if (!*a && !*b)
                return grp_at(G_ENV, i);
        }
    return grp_n(G_ENV) ? grp_at(G_ENV, 0) : -1;
}

static void gest_turn(uint32_t k, int32_t e)
{
    int32_t pg = ui.gest - 1u == B_EDIT ? E->quick : ui.gest - 1u == B_ENV ? env_page() : grp_n(G_LFO) ? grp_at(G_LFO, 0) : -1;
    uint8_t g = ui.gest;
    ui.gest_used = 1;
    if (pg < 0 || (ui.page == V_HOME && mt_ui() && glob.g[G_MT0 + mt_at_cur()] >= NENG))   /* (v0.86) pusta ścieżka: nie ma czym kręcić */
        return;
    if (ui.page != pg) {
        int32_t back = ui.quick ? ui.qback : ui.page;
        quick_page(pg);
        ui.qback = (int8_t)(back >= 0 ? back : -1);
    }
    ui.quick_at = plat_ms();
    ui.gest = 0;                             /* gałka jak zawsze na tej stronie */
    knob_turn(k, e);
    ui.gest = g;
}

/* strona MOD: cele źródła w kolejności stron silnika (parametry, dla których gest(src, id, ASK) >= 0), bez powtórzeń
 * (ta sama ilość w stałych ścieżkach, ten sam cel w matrycy WAVE / PHASE) */
#define MOD_MAX 48
static int16_t mod_ids[MOD_MAX], mod_keys[MOD_MAX];   /* cel: parametr i to, co go odróżnia (matryca: numer celu) */
static int32_t wt_dest(int32_t id);
static int32_t pd_dest(int32_t id);
static int32_t mod_key(uint32_t src, int32_t id)   /* to, co odróżnia cele (-1: nie cel) */
{
    int32_t r = E->gest(src, id, GEST_ASK, 0);
    if (r < 0)
        return -1;
    if (set.eng == ENG_WT || set.eng == ENG_PD)
        return set.eng == ENG_WT ? wt_dest(id) : pd_dest(id);
    return r ? r : 1000 + id;                    /* (v0.65) matryca na szkielecie: niepołączony = sam parametr */
}
static int32_t mod_skmx(void)            /* (v0.65) pierwszy parametr matrycy na szkielecie (-1: brak) */
{
    return set.eng == ENG_VCO ? V_MX : set.eng == ENG_SEM ? S_MX : set.eng == ENG_DCO || set.eng == ENG_ALPHA ? J_MX
         : set.eng == ENG_BIT ? B_MX : set.eng == ENG_WEST ? WS_MX : set.eng == ENG_MOOG ? P_MX : set.eng == ENG_DX7 ? X_MX
         : set.eng == ENG_PHYS ? PH_MX : set.eng == ENG_FORMANT ? FO_MX : -1;   /* (v0.71) MINI, FM6; (v0.78) PHYS; (v0.84) FORMANT */
}
static void mod_open(uint32_t src)
{
    const char *sn = "";
    int32_t keys[MOD_MAX], p, k, j, n = 0;
    if (!E->gest || E->gest(src, -1, GEST_ASK, &sn) < 0)
        return;
    for (p = 0; p < E->npages && n < MOD_MAX; p++)
        for (k = 0; k < 8 && n < MOD_MAX; k++) {
            int32_t id = k < 4 ? E->knob(p, k) : E->more ? E->more(p, k - 4) : -1, key, i;
            if (id < 0 || id >= E->np || (key = mod_key(src, id)) < 0)
                continue;
            for (i = 0; i < n && keys[i] != key; i++)
                ;
            if (i < n)
                continue;
            keys[n] = key;
            mod_keys[n] = (int16_t)key;
            mod_ids[n++] = (int16_t)id;
        }
    if (!n) {
        app_say(sn, "NO MOD");
        return;
    }
    if (ui.page != V_MOD) {
        j = ui.quick ? ui.qback : ui.page;   /* (ze strony chwilowej: tam, skąd na nią weszliśmy) */
        ui.mod_back = (int8_t)(j >= 0 ? j : -1);
    }
    ui.mod_src = (uint8_t)src;
    ui.mod_n = (uint8_t)n;
    ui.mod_pg = 0;
    ui.mod_btn = (uint8_t)(src & GEST_ENV ? B_ENV : B_LFO);
    ui.page = V_MOD;
    ui.quick = 0;
    ui.browse = 0;
    ui.knob = -1;
    ui.redraw = 1;
}
static int32_t wt_row(int32_t i);
static int32_t pd_row(int32_t i);
static int32_t mod_amt0(void)            /* matryca (WAVE, PHASE): ilość slotu 1 (zakres ilości), -1: stałe ścieżki */
{
    return set.eng == ENG_WT ? wt_row(0) + 2 : set.eng == ENG_PD ? pd_row(0) + 2 : -1;
}
static const char *mod_name(int32_t j)   /* nazwa celu: matryca — z listy DEST (A POS, B POS ...), stałe — ilość bez „> ” */
{
    int32_t a0 = mod_amt0(), a;
    const char *nm;
    if (a0 >= 0) {
        const pdef_t *dd = E->def(a0 - 1);      /* wiersz: SOURCE, DEST, AMOUNT, VIA */
        if (dd->list && mod_keys[j] >= dd->lo && mod_keys[j] <= dd->hi)
            return PLIST(dd)[mod_keys[j] - dd->lo];
        return E->def(mod_ids[j])->name;
    }
    a = E->gest(ui.mod_src, mod_ids[j], GEST_ASK, 0);
    if (mod_skmx() >= 0 && (a == 0 || (a >= mod_skmx() && a < mod_skmx() + 4 * SK_MXN))) {   /* (v0.65) cel matrycy */
        static char lb[4][16];                   /* (po jednym na kolumnę strony) */
        int32_t p, k, id = mod_ids[j];
        const char *pn = "", *s;
        char *o = lb[j & 3];
        for (p = 0; p < E->npages && !pn[0]; p++)
            for (k = 0; k < 4; k++)
                if (E->knob(p, k) == id)
                    pn = E->page_name[p];
        nm = E->def(id)->name;
        if (pn[0] == 'O' && pn[1] == 'S' && pn[2] == 'C' && pn[3] == ' ' && pn[4] && !pn[5]) {   /* OSC A: „A PW” */
            *o++ = pn[4];
            *o++ = ' ';
        } else if (nm[0] == 'O' && nm[1] == 'S' && nm[2] == 'C' && nm[3] == ' ' && nm[4] && !nm[5]) {   /* poziom: „A LEVEL” */
            *o++ = nm[4];
            nm = " LEVEL";
        }
        for (s = nm; *s && o < lb[j & 3] + 15; s++)
            *o++ = *s;
        *o = 0;
        return lb[j & 3];
    }
    nm = a >= 0 ? E->def(a)->name : E->def(mod_ids[j])->name;
    return nm[0] == '>' && nm[1] == ' ' ? nm + 2 : nm;
}
static void mod_close(void)              /* jej przycisk albo HOME: tam, skąd przyszliśmy */
{
    if (ui.mod_back >= 0)
        show_page(ui.mod_back);
    else
        go_home();
}
static void mod_select(int32_t e)        /* SELECT: strony celów (po 4) */
{
    int32_t np = (ui.mod_n + 3) / 4;
    ui.mod_pg = (uint8_t)(((int32_t)ui.mod_pg + (e > 0 ? 1 : np - 1)) % np);
    if (ui.gest)
        ui.gest_used = 1;
    ui.knob = -1;
    ui.redraw = 1;
}
static void mod_knob(uint32_t k, int32_t e)   /* K1-K4: ilość modulacji celu (matryca: połączenie w wolnym slocie) */
{
    const char *sn = "";
    char t[32], v[16];
    int32_t j = ui.mod_pg * 4 + (int32_t)k, id, a;
    if (ui.gest)
        ui.gest_used = 1;
    if (j >= ui.mod_n)
        return;
    id = mod_ids[j];
    a = E->gest(ui.mod_src, id, GEST_MAKE, &sn);
    if (a == GEST_FULL) {
        app_say(sn, "MOD FULL");
        return;
    }
    if (a < 0)
        return;
    set_param(a, pget(a) + accel(EN_K1 + k, e, E->def(a)->hi - E->def(a)->lo));
    if (pget(a) == 0 && E->gest(ui.mod_src, id, GEST_DROP, 0) >= 0)   /* do 0: połączenie znika */
        set.edited[set.eng] = (uint8_t)snap_differs(set.eng);
    ui.knob = (int32_t)k;
    ui.knob_at = plat_ms();
    ui.redraw = 1;
    t[0] = 0;
    text_cat(t, sn);
    text_cat(t, " > ");
    text_cat(t, mod_name(j));
    value_text(a, pget(a), v);
    app_say(t, v);
}

/* (v0.45) strona CHORD: K1 rodzaj, SEL = LEARN (trzymane klawisze FM1: najniższy = klawisz, reszta nad nim, do 6) */
static const pdef_t CH_DEF = {"TYPE", 0, NCHORD - 1, 0, F_LIST, LI(CHORD_NAME)};
static const char *const L_VOICE[] = {"CLOSE", "OPEN", "INV 1", "INV 2"};
static const pdef_t CV_DEF = {"VOICING", 0, 3, 0, F_LIST, LI(L_VOICE)};   /* (v1.1.0) K2 strony CHORD */
static void chord_set(const uint8_t *x)
{
    uint32_t i;
    for (i = 0; i < XT_N && i <= CHORD_LN; i++)
        xtc[set.eng][i] = x[i];
    xt_dirty = set_dirty = 1;
    set.edited[set.eng] = (uint8_t)snap_differs(set.eng);
    ui.redraw = 1;
}
static void chord_turn(uint32_t k, int32_t e)
{
    uint8_t x[CHORD_LN + 1];
    uint32_t i;
    if (k > 1u)
        return;
    for (i = 0; i <= CHORD_LN; i++)
        x[i] = xtc[set.eng][i];
    if (k == 1)                              /* (v1.1.0) K2: VOICING (bity 4-5) */
        x[0] = (uint8_t)((x[0] & 15u) | (uint32_t)clamp((int32_t)(x[0] >> 4 & 3u) + (e > 0 ? 1 : -1), 0, 3) << 4);
    else
        x[0] = (uint8_t)((x[0] & 0x30u) | (uint32_t)clamp((int32_t)(x[0] & 15u) + (e > 0 ? 1 : -1), 0, NCHORD - 1));
    chord_set(x);
}
static void chord_learn(void)
{
    uint32_t keys = touch_keys(), k, n = 0, lo = 32;
    uint8_t x[CHORD_LN + 1] = {(uint8_t)(CH_LEARN | (xtc[set.eng][0] & 0x30u))};   /* (v1.1.0) VOICING zostaje */
    for (k = 0; k < NKEYS; k++)
        if (keys >> k & 1u) {
            if (lo == 32u)
                lo = k;
            else if (n < CHORD_LN && k - lo < 32u)
                x[1u + n++] = (uint8_t)(k - lo);
        }
    if (!n) {
        app_say("CHORD", "HOLD KEYS + SEL");
        return;
    }
    chord_set(x);
    app_say("CHORD", "LEARNED");
}
static int sel_usable(void)              /* SEL coś tu robi: opcja strony / MORE, GLO, FX, okna (zatwierdza) */
{
    if (ui.sel_hold)
        return 0;                            /* trzymany i tak świeci */
    if (ui.page >= 0)
        return ui.page == PG_CHORD || E->sel(ui.page) >= 0;   /* (MORE nie jest pod SEL: tylko przełącznik strony) */
    return ui.page != V_HOME || mt_ui();     /* (v0.86) główny ekran MULTI: MUTE / lista */
}

static void input(void)
{
    static uint32_t eaten;                   /* klawisze zużyte jako skróty: ich puszczenie nie gasi nuty */
    static uint8_t knote[NKEYS];             /* nuta z chwili naciśnięcia: puszczenie gasi tę samą (zmiana oktawy w
                                              * trakcie nie zostawia wiszącej nuty ani nuty w ARP) */
    uint32_t keys, btn = plat_buttons(), k, avail = 0, shown, pr = 0, gk;
    int sel_shown;
    uint32_t chan = mt_ui() ? mt_ch(glob.g[G_MTFOC])   /* (v0.66.1) MULTI: MIDI OUT na kanale wybranej ścieżki (v1.0.1: z GLO) */
                  : glob.g[G_MIDICH] ? (uint32_t)glob.g[G_MIDICH] - 1u : 0u;
    int32_t e;
    touch_poll();
    keys = touch_keys();
    if (mt_ui() && ui.page != V_HOME && ui.page != V_ENGP && ui.page != V_MTL && ui.page != V_SCN)   /* (v0.85) poza głównym ekranem i oknami: kursor na wybranej (strony jej silnika) */
        mt_cur = (uint8_t)glob.g[G_MTFOC];
    ctl.dsemi = (int8_t)(set.octave + glob.g[G_TRANSP]);   /* (v0.56) perkusja: przestrojenie */
    if (mt_ui() && glob.g[G_MTFOC] >= 1 && glob.g[G_MTSYN] != glob.g[G_MTFOC]) {   /* (v0.66.2) SEQ w SOLO: ta ścieżka */
        glob.g[G_MTSYN] = glob.g[G_MTFOC];
        mt_gdirty = 1;
    }
    sq_drum = ENG_GRP[set.eng] == EG_DRUM;   /* (v0.57) SEQ: MUTE / SOLO wierszy, rolki rosną */
    if (ui.sq_pg == SQ_RACK && !sq_drum) {   /* (v0.57) silnik zmieniony na nie-perkusję: RACK -> NOTES */
        ui.sq_pg = SQ_NOTES;
        ui.redraw = 1;
    }
    if (ui.sel_hold && ui.page >= V_HOME)
        avail = label_avail();
    gk = ui.gest ? gest_keys() : 0u;
    for (k = 0; k < NKEYS; k++) {            /* klawisz 0 to F3 (MIDI 53) */
        uint32_t bit = 1u << k, v;
        int32_t note = key_note(k);
        if (keys & bit) {                    /* nacisk: najmocniej wciśnięty klawisz */
            uint32_t q = touch_pressure(k);
            pr = q > pr ? q : pr;
        }
        if (ui.cp_k == k + 1u && !(keys & bit))   /* (v0.92) kopia dźwięku: pierwszy klawisz puszczony */
            ui.cp_k = 0;
        if (ENG_GRP[set.eng] == EG_DRUM && ui.sel_hold && (keys & bit) && !(ui.keys & bit) && rk_row_of((uint32_t)note) >= 0
            && !(ui.page == V_SEQ && ui.sq_pg == SQ_NOTES)) {   /* (NOTES: SEL + klawisze = nuty w kursorze) */
            if (ui.cp_k && (keys >> (ui.cp_k - 1u) & 1u)) {   /* (v0.92, decyzja użytkownika 2026-11-04) SEL + trzymany + ten: kopia */
                eaten |= bit;
                ui.sel_used = 1;
                cp_ask(rk_row_of((uint32_t)key_note(ui.cp_k - 1u)), rk_row_of((uint32_t)note));
                continue;
            }
            ui.cp_k = (uint8_t)(k + 1u);         /* pierwszy: dalej jak zawsze (wybór dźwięku, RACK: wiersz i odsłuch) */
        }
        if (ui.sel_hold && sq_sel_trk() && (keys & bit) && !(ui.keys & bit)
            && key_label(k) >= K_OP1 && key_label(k) <= K_OP4) {   /* (v0.90) SEL + OP1-4 w SEQ (MULTI): ścieżka T1-T4 (było SEQ + OP) */
            eaten |= bit;
            ui.sel_used = 1;
            sq_track_go(key_label(k) - K_OP1);
            continue;
        }
        if ((bit & gk) && (keys & bit) && !(ui.keys & bit)) {   /* (v0.65) LFO / ENV trzymany + OP: strona MOD */
            eaten |= bit;
            ui.gest_src = (uint8_t)((ui.gest_src & 0xF0u) | (uint32_t)key_label(k));
            ui.gest_used = 1;
            mod_open(ui.gest_src);
            continue;
        }
        if (rk_on() && (keys & bit) && !(ui.keys & bit) && !(bit & avail)) {   /* (v0.57) RACK */
            const int32_t w = rk_row_of((uint32_t)note);   /* biały klawisz 0-15 (perkusja: bez oktawy) */
            if (ui.pf_hold || ui.arp_hold) {     /* FX / ARP trzymany + biały klawisz: MUTE / SOLO wiersza */
                eaten |= bit;
                if (w >= 0)
                    rk_mute(w, !ui.pf_hold);
                if (ui.pf_hold)
                    ui.pf_used = 1;
                else
                    ui.arp_done = 1;
                continue;
            }
            if (!ui.sel_hold) {                  /* klawisz: krok strony (czarny: nic) */
                eaten |= bit;
                if (w >= 0)
                    rk_toggle((uint32_t)w);
                continue;
            }
            if (w >= 0) {                        /* SEL + klawisz: wiersz tego dźwięku; dalej gra jak zwykle (REC: nagrywa) */
                ui.rk_row = (uint8_t)w;
                rk_view();
            }
            ui.sel_used = 1;
        }
        if (ui.pf_hold && mt_ui() && (keys & bit) && !(ui.keys & bit)) {   /* (v0.47) FX trzymany + klawisz: PERFORM,
                                                                          * bez nuty; (v0.74) tylko w MULTI */
            eaten |= bit;
            pf_key(k, 1);
            continue;
        }
        if (((ui.pf_keys | ui.cv_held) & bit) && !(keys & bit))   /* puszczony klawisz PERFORM (FX już może być puszczony) */
            pf_key(k, 0);
        if ((bit & avail) && (keys & bit) && !(ui.keys & bit)) {
            eaten |= bit;
            label_key(key_label(k));
            ui.sel_used = 1;
            continue;
        }
        if (ui.sel_hold && ui.page == V_SEQ && ui.sq_pg == SQ_NOTES && (keys & bit) && !(ui.keys & bit)) {
            eaten |= bit;                        /* NOTES: SEL trzymany + klawisz = nuta w kursorze (dodaj / usuń); (v0.90) w MULTI OP1-4: ścieżka (wyżej) */
            sq_toggle((uint32_t)note, key_vel(k));
            ui.sel_used = 1;
            continue;
        }
        if (ui.page == V_SEQ && ui.sq_pg == SQ_REC && !(sq.rec && sq.play) && (keys & bit) && !(ui.keys & bit)) {
            eaten |= bit;                        /* (v1.1.0) RECORD: klawisz = TAP TEMPO, bez nuty (nagrywanie na żywo: gra jak zawsze) */
            rc_tap();
            continue;
        }
        if (ENG_GRP[set.eng] == EG_DRUM && !rk_on() && (keys & bit) && !(ui.keys & bit)
            && (ui.sel_hold || ui.page >= V_HOME)) {   /* perkusja: SEL trzymany + biały klawisz = wybór dźwięku bez grania;
                                                          * (v0.90, decyzja użytkownika 2026-11-03) napis też przy graniu (HOME, strony) */
            int32_t L = set.eng == ENG_MORPH ? mp_select(note) : set.eng == ENG_DUAL ? du_select(note) : dm_select(note);
            if (L >= 0) {
                char t[12] = "SOUND ", n[4];
                int_text(L + 1, n, 0);
                text_cat(t, n);
                app_say(t, "SELECTED");
            }
            ui.redraw = 1;
            if (ui.sel_hold) {
                eaten |= bit;
                ui.sel_used = 1;
                continue;
            }
        }
        if (eaten & bit) {
            if (!(keys & bit))
                eaten &= ~bit;
            continue;
        }
        if ((keys & bit) && !(ui.keys & bit)) {
            v = key_vel(k);
            knote[k] = (uint8_t)note;
            if (glob.g[G_LOCAL])                 /* GLO LOCAL OFF: klawisze tylko do MIDI (dźwięk wraca z DAW) */
                ev_push(EV_ON(note, v));
            if (glob.g[G_MIDIOUT] && !sqg.arp_on)   /* z ARP na MIDI idą nuty arpeggiatora */
                plat_midi_send(0x09u | (0x90u | chan) << 8 | (uint32_t)note << 16 | v << 24, TO_USB);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            note = knote[k];
            if (glob.g[G_LOCAL])
                ev_push(EV_OFF(note));
            if (glob.g[G_MIDIOUT] && !sqg.arp_on)
                plat_midi_send(0x08u | (0x80u | chan) << 8 | (uint32_t)note << 16, TO_USB);
        }
    }
    sq_keys(keys & ~ui.keys & ~eaten, ui.keys & ~keys, keys & ~eaten);   /* NOTES: wpis krokowy, zaznaczenie */
    press_keys = (float)pr / 127.0f;
    if (ui.mode_until && (btn & ~ui.btn)) {  /* (v0.86) napis SOLO / MULTI: każdy przycisk (i gałka) go zamyka */
        ui.mode_until = 0;
        ui.redraw = 1;
    }
    {                                        /* SEL: w oknach zatwierdza od razu; na stronie przy puszczeniu (MORE) */
        uint32_t bit = 1u << B_SEL;
        if ((btn & bit) && !(ui.btn & bit)) {
            ui.browse = 0;
            if (ui.page == V_ALLP) {         /* (v1.0.2) ALL PRESETS: OK przy puszczeniu, trzymany = usuń USER; w oknie DELETE?: tak */
                if (ui.allp_del) {
                    user_del();
                } else {
                    ui.allp_sel = 1;
                    ui.sel_at = plat_ms();
                }
            } else if (ui.page <= V_SAVE && !PANEL_PAGE(ui.page)) {
                save_ok();
            } else if (ui.page == V_GLO) {
                glo_sel();
            } else {
                ui.sel_hold = 1;
                ui.sel_used = 0;
                ui.sel_at = plat_ms();
                ui.redraw = 1;
            }
        } else if (!(btn & bit) && (ui.btn & bit) && ui.allp_sel) {
            ui.allp_sel = 0;                 /* (v1.0.2) krótko (albo trzymany na nie-USER): OK; trzymany na USER: okno DELETE? */
            if (ui.page == V_ALLP)
                save_ok();
        } else if (!(btn & bit) && (ui.btn & bit) && ui.sel_hold) {
            ui.sel_hold = 0;
            ui.cp_k = 0;
            if (!ui.sel_used && plat_ms() - ui.sel_at < 500u)
                sel_cycle();
            ui.knob = -1;
            ui.redraw = 1;
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit) || k == B_SEL)
            continue;
        ui.browse = 0;                       /* każdy przycisk to akcja: SELECT z HOME już nie wraca sam */
        switch (k) {
        case B_OCTDN:                        /* w oknie nazwy: kursor, poza nim oktawa */
        case B_OCTUP:
            if (ui.page == V_NAME) {
                ui.cur = clamp(ui.cur + (k == B_OCTUP ? 1 : -1), 0, NAME_LEN - 1);
                ui.redraw = 1;
            } else if (ui.page == V_PERF) {  /* (v0.91) PERFORM: bank EFFECTS / CURVES */
                ui.pf_bank ^= 1u;
                app_say("PERFORM", ui.pf_bank ? "CURVES" : "EFFECTS");
            } else {
                octave_step(k == B_OCTUP ? 1 : -1);
            }
            break;
        case B_SAVE: save_key(); break;
        case B_EDIT:
        case B_ENV:
        case B_LFO:
            if ((ui.page >= V_HOME || ui.page == V_MOD) && !ui.sel_hold) {   /* gest; strona przy puszczeniu; (v0.86) też EDIT */
                ui.gest = (uint8_t)(k + 1u);
                ui.gest_used = 0;
                ui.gest_src = (uint8_t)((k == B_LFO ? GEST_LFO : GEST_ENV) | GEST_DEF);
                ui.gest_at = plat_ms();
                ui.redraw = 1;
                break;
            }
            if (ui.page >= V_HOME || PANEL_PAGE(ui.page) || ui.page == V_GLO || ui.page == V_MOD) {   /* też FX, SEQ ... */
                if (PANEL_PAGE(ui.page) || ui.page == V_GLO || ui.page == V_MOD) {
                    glob_commit();
                    ui.page = V_HOME;
                }
                goto_group(k == B_EDIT ? G_EDIT : k == B_ENV ? G_ENV : G_LFO);
                ui.grp_hold = (uint8_t)(k + 1u);   /* trzymany: po chwili pierwsza strona sekcji */
                ui.grp_done = 0;
                ui.grp_at = plat_ms();
            }
            break;
        case B_HOME:
            if (ui.page == V_HOME)           /* już na HOME: okno ENGINE przy puszczeniu */
                ui.home_pend = 1;
            else
                home_key();
            ui.home_used = 0;
            ui.home_hold = ui.page == V_HOME;    /* (v0.86) dalej trzymany: SOLO / MULTI (app_frame) */
            ui.home_at = plat_ms();
            break;
        case B_GLO:                          /* GLO: wejście; drugi raz: powrót do poprzedniej sekcji i strony */
            if (ui.page >= V_HOME || PANEL_PAGE(ui.page) || ui.page == V_GLO || ui.page == V_INFO || ui.page == V_RESET
                || ui.page == V_NOTE)
                go_glo();
            break;
        case B_FX:                           /* (v0.47) krótko: przy puszczeniu, trzymany: PERFORM; (v0.74) PERFORM tylko w MULTI —
                                              * w SOLO FX od razu (RACK: FX + klawisz = MUTE, dalej przy puszczeniu) */
            if (ui.page == V_FX || ui.page >= V_GLO || ui.page == V_SEQ || ui.page == V_ARP) {
                if (!mt_ui() && !rk_on()) {
                    fx_press();
                    break;
                }
                ui.pf_hold = 1;
                ui.pf_used = 0;
                ui.pf_at = plat_ms();
            }
            break;
        case B_SEQ:                          /* (v0.88) przy puszczeniu: go_seq; trzymany: seq_hold; (v0.66) trzymany + OP1-4 w MULTI = ścieżka */
            if (ui.page >= V_GLO || PANEL_PAGE(ui.page) || ui.page == V_SQT || ui.page == V_SQN || ui.page == V_SCN) {
                ui.grp_hold = (uint8_t)(k + 1u);
                ui.grp_done = 0;
                ui.grp_at = plat_ms();
            }
            break;
        case B_ARP:                          /* ARP: krótko ON / OFF (przy puszczeniu), trzymany: ustawienia */
            ui.arp_hold = 1;
            ui.arp_done = 0;
            ui.arp_at = plat_ms();
            break;
        case B_PLAY:                         /* NOTES (zatrzymany): krótko = od kursora, trzymany = od początku */
            if (!sq.play && ((ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) || ui.page == V_SQN)) {
                ui.play_hold = 1;
                ui.play_at = plat_ms();
            } else {
                sq_play_key();
            }
            break;
        case B_REC:                          /* (v0.92) krótko: REC jak dotąd (przy puszczeniu), trzymany 0,4 s: strona RECORD */
            ui.rec_hold = 1;
            ui.rec_at = plat_ms();
            break;
        default: app_say("SOON", ""); break;
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.input_at = plat_ms();                 /* GLO SCREEN OFF: każdy ruch budzi */
        if (ui.allp_del && ui.page == V_ALLP)    /* (v1.0.2) okno DELETE?: gałki stoją */
            continue;
        if (ui.mode_until) {
            ui.mode_until = 0;
            ui.redraw = 1;
        }
        if (btn >> B_HOME & 1u)                  /* (v0.86) HOME trzymany + gałka: bez SOLO / MULTI i okna ENGINE */
            ui.home_used = 1;
        if (ui.gest)                             /* (v0.86) LFO / ENV / EDIT + gałka: puszczenie bez przejścia do sekcji */
            ui.gest_used = 1;
        if (k == EN_SELECT) {                /* strony w sekcji; z HOME wszystkie */
            if (ui.page == V_HOME && mt_ui() && !ui.gest) {   /* (v0.64.2) MULTI: SELECT na HOME = ścieżka (decyzja
                                                               * użytkownika 2026-10-29; strony: EDIT / ENV / LFO) */
                mt_step(e);
                continue;
            }
            if (ui.page == V_ALLP) {         /* ALL PRESETS: SELECT po sekcjach (silnik, USER / FACTORY / FM1 ... ) */
                int32_t id = preset_now(), n;
                for (n = e < 0 ? -e : e; n > 0; n--)
                    id = sec_jump(id, e);
                preset_go(id);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_FX) {
                fx_select(e);
                continue;
            }
            if (ui.page == V_SEQ) {          /* SELECT: w lewo / w prawo po krokach; (v0.89) SEL + SELECT: zaznaczenie zakresu */
                if (ui.sq_pg == SQ_RACK && ui.sel_hold) {   /* (v0.88) RACK: SEL + SELECT = kursor lewo / prawo po krokach */
                    ui.sel_used = 1;
                    ui.rk_step = (uint16_t)clamp((int32_t)(ui.rk_step % rk_steps()) + (e > 0 ? 1 : -1), 0, (int32_t)rk_steps() - 1);
                    ui.sq_moved = plat_ms();
                    ui.redraw = 1;
                } else if (ui.sq_pg == SQ_RACK) {   /* (v0.57) RACK: wiersz (dźwięk) */
                    ui.rk_row = (uint8_t)clamp((int32_t)ui.rk_row + (e > 0 ? 1 : -1), 0, 15);
                    rk_view();
                } else if (ui.sq_pg == SQ_NOTES && ui.sel_hold)
                    sq_range(e);
                else if (ui.sq_pg == SQ_NOTES)
                    sq_cursor((int32_t)ui.sq_cur + e * (int32_t)sq_grid());
                else                             /* (v0.88) ustawienia SEQ: strony */
                    sq_set_turn(e);
                continue;
            }
            if (ui.page == V_SQN) {          /* karta nuty: wiersz */
                ui.sqn_row = (uint8_t)clamp((int32_t)ui.sqn_row + e, 0, NSN - 1);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_SQT) {
                ui.sqt_cur = (uint8_t)clamp((int32_t)ui.sqt_cur + (e > 0 ? 1 : -1), 0, NST - 1);
                ui.sqt_sure = 0;
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_ARP) {          /* SELECT: strona 1 / 2 / 3 (jak strony EDIT) */
                ui.arp_more = (uint8_t)clamp((int32_t)ui.arp_more + (e > 0 ? 1 : -1), 0, ARP_NPG - 1);
                ui.knob = -1;
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_FXL) {          /* lista efektów: EMPTY na górze */
                fxl_move(e);
                continue;
            }
            if (ui.page == V_MTL) {          /* (v0.86) lista silników */
                mtl_move(e);
                continue;
            }
            if (ui.page == V_SCN) {          /* (v0.86) lista scen */
                ui.scn_cur = (uint8_t)clamp((int32_t)ui.scn_cur + (e > 0 ? 1 : -1), 0, SCN_N - 1);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_ENGP) {         /* okno ENGINE: SELECT po kolei */
                engine_turn(e, 0);
                continue;
            }
            if (ui.page == V_MOD) {          /* (v0.65) strona MOD: strony celów */
                mod_select(e);
                continue;
            }
            if (ui.page == V_GLO) {          /* GLO: sekcja, strona sekcji, działanie SYSTEM */
                glo_select(e);
                continue;
            }
            if (ui.page == V_CPY) {          /* (v0.92) kopia dźwięku: TAK / NIE */
                if (e & 1)
                    ui.cp_yes ^= 1u;
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_RESET) {        /* FACTORY RESET: co (przed potwierdzeniem) */
                if (!ui.reset_sure)
                    ui.reset_opt = (uint8_t)((ui.reset_opt + e % 3 + 3) % 3);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_INFO || ui.page == V_NOTE)
                continue;
            if (ui.page == V_NAME) {         /* okno nazwy: SELECT zmienia literę */
                name_char(e);
                continue;
            }
            if (ui.page == V_SAVE) {         /* okno SAVE: SELECT przełącza NADPISZ / NOWY */
                ui.save_new ^= 1u;
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_IMPORT) {       /* okno IMPORT: miejsce 1-8 (DX7, CZ od v0.53 też) */
                int32_t nb = ui.imp_cz ? NCZB_USE : NSYX_USE;
                ui.imp_slot = (ui.imp_slot + e % nb + nb) % nb;
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_HOME) {
                show(G_ALL, 0);              /* z HOME zawsze od 1, w którą stronę by nie kręcić */
                ui.browse = 1;
            } else {
                uint8_t b = ui.browse;
                show(ui.grp, ui.idx + e);
                ui.browse = b;
            }
            ui.browse_until = ui.grp == G_ALL ? plat_ms() + QUICK_MS : 0u;
        } else if (k == EN_PRESET) {         /* jedyny wyjątek od SEL: HOME trzymany = poziom instrumentów */
            if (btn >> B_HOME & 1u)
                ui.home_used = 1;
            preset_turn(e, (btn >> B_HOME & 1u) != 0u);
        } else if (k == EN_ALGO) {
            if (ui.page == V_FX)             /* FX: ALGO wybiera efekt do slotu */
                fx_pick(e);
            else if (ui.page == V_FXL)       /* lista efektów: ALGO po grupach (v0.55) */
                fxl_group(e);
            else if (ui.page == V_MTL)       /* (v0.86) lista silników: ALGO po grupach */
                mtl_group(e);
            else if (ui.page == V_ENGP)      /* okno ENGINE: ALGO po grupach (VA, DIGI, DRUM) */
                engine_turn(e, 1);
            else if (ui.page == V_HOME && mt_ui())   /* (v0.85) MULTI: główny ekran, ALGO = silnik ścieżki */
                mt_engine(e);
            else if (ui.page == V_ALLP) {    /* ALL PRESETS: ALGO po silnikach (v0.37, jak po grupach w oknie ENGINE) */
                preset_go(eng_jump(preset_now(), e));
                ui.redraw = 1;
            }
            else if (ui.page == V_SEQ && ui.sel_hold) {   /* SEQ: SEL trzymany + ALGO = długość wzoru; (v0.88) co beat / krok / takt: OPTIONS */
                ui.sel_used = 1;
                if (ui.sq_pg == SQ_NOTES)        /* (v0.91, decyzja użytkownika 2026-11-03) NOTES: SEL + ALGO = długość wzoru co GRID
                                                  * (v0.90: długość nuty); RACK i ustawienia: wzór co BEAT */
                    sq_len_grid(e);
                else
                    sq_len_turn(e);
            } else if (ui.page == V_SEQ && ui.sq_pg == SQ_RACK) {   /* (v0.57) RACK: ALGO = strona kroków (po 16) */
                const int32_t np = (int32_t)(rk_steps() + 15u) / 16;
                const int32_t pg = clamp((int32_t)(ui.rk_step % rk_steps()) / 16 + (e > 0 ? 1 : -1), 0, np - 1);
                char t[16], n[6];
                rk_goto((uint32_t)pg);
                ui.sq_moved = plat_ms();
                int_text(pg * 16 + 1, t, 0);
                text_cat(t, "-");
                int_text(pg * 16 + 16, n, 0);
                text_cat(t, n);
                app_say("STEPS", t);
                ui.redraw = 1;
            } else if (ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) {   /* NOTES: ALGO = GRID (jak snap w FL) */
                int32_t g = clamp((int32_t)sqp.grid + (e > 0 ? 1 : -1), 0, 5);
                if (g != sqp.grid) {
                    sqp.grid = (uint8_t)g;
                    sq_changed();
                    sq_cursor((int32_t)ui.sq_cur);
                }
                app_say("GRID", L_GRID[sqp.grid]);
            } else {
                algo_turn(e);
            }
        } else {
            knob_turn(k - EN_K1, e);
        }
    }
    if (ui.gest && !(btn >> (ui.gest - 1u) & 1u)) {   /* v0.35: puszczony LFO / ENV bez gestu: jak dotąd */
        uint32_t b = ui.gest - 1u;
        int used = ui.gest_used, lng = plat_ms() - ui.gest_at >= 400u;
        ui.gest = 0;
        ui.redraw = 1;
        if (!used && ui.page == V_MOD) {     /* (v0.65) strona MOD: jej przycisk zamyka, drugi idzie do swojej sekcji */
            const uint32_t own = ui.mod_btn;
            mod_close();
            if (b == own)
                used = 1;
        }
        if (!used && ui.page >= V_HOME) {
            if (lng) {                       /* trzymany: pierwsza strona sekcji, bez MORE */
                show(b == B_ENV ? G_ENV : b == B_LFO ? G_LFO : G_EDIT, 0);
                ui.more_lock = 0;
            } else {
                goto_group(b == B_ENV ? G_ENV : b == B_LFO ? G_LFO : G_EDIT);
            }
        }
    }
    if (!(btn >> B_HOME & 1u) && (ui.btn >> B_HOME & 1u) && ui.home_pend) {
        ui.home_pend = 0;
        if (!ui.home_used && ui.page == V_HOME)
            engine_open();
    }
    if (ui.home_hold && (!(btn >> B_HOME & 1u) || ui.home_used)) {
        ui.home_hold = 0;
        ui.redraw = 1;
    }
    if (keys != ui.keys || btn != ui.btn) {
        ui.redraw = 1;
        ui.input_at = plat_ms();
    }
    ui.keys = keys;
    ui.btn = btn;
    /* skróty działają od razu, ale świecą dopiero po przytrzymaniu SEL (krótkie kliknięcie nie miga klawiszami) */
    shown = sel_lit() ? avail : 0u;
    sel_shown = shown != 0u;                 /* (v0.64.4) tryb MONO / POLY świeci tylko przy SEL, nie przy LFO / ENV */
    shown |= gk;                             /* LFO / ENV trzymany: klawisze źródeł przygaszone */
    if (ui.sel_hold && sq_sel_trk() && plat_ms() - ui.sel_at >= SEL_SHOW_MS)   /* (v0.90) SEL trzymany w SEQ (MULTI): OP1-4 = ścieżki z silnikiem */
        for (k = 0; k < MT_N; k++)
            if (glob.g[G_MT0 + k] < NENG)
                shown |= 1u << LABEL_KEY[K_OP1 + k];
    {                                            /* (v0.90) SEL trzymany 0,3 s: podpowiedzi (NOTES: pasek gałek, OP1-4) */
        static uint8_t selh_was;
        const uint8_t h = (uint8_t)(ui.sel_hold && plat_ms() - ui.sel_at >= SEL_SHOW_MS);
        if (h != selh_was) {
            selh_was = h;
            ui.redraw = 1;
        }
    }
    {
        static uint8_t shown_was;                /* podpowiedź strony: opis klawiszy, gdy świecą */
        if ((shown != 0u) != shown_was) {
            shown_was = shown != 0u;
            ui.redraw |= (ui.page >= 0 || ui.page == V_HOME) && hints();
        }
    }
    {
        static const uint8_t LED[3] = {B_EDIT, B_ENV, B_LFO};   /* świeci przycisk sekcji tej strony */
        uint32_t lit = label_lit();
        if (ui.gest && (ui.gest_src & 0xFu) != GEST_DEF)   /* wybrane źródło gestu: pełnym */
            lit |= 1u << LABEL_KEY[ui.gest_src & 0xFu];
        if (sel_shown)                           /* SEL trzymany: też bieżący tryb */
            lit |= 1u << LABEL_KEY[pget(E->mode) ? K_POLY : K_MONO];
        if (ui.sel_hold && sq_sel_trk() && plat_ms() - ui.sel_at >= SEL_SHOW_MS)   /* (v0.90) SEL trzymany w SEQ (MULTI): wybrana ścieżka pełnym */
            lit |= 1u << LABEL_KEY[K_OP1 + glob.g[G_MTFOC]];
        int gl = glob.g[G_LIGHTS], kl = glob.g[G_KEYL];
        uint32_t bl = btn
                    | (set.octave > 0 ? 1u << B_OCTUP : set.octave < 0 ? 1u << B_OCTDN : 0u)   /* oktawa poza 0 */
                    | (ui.page == V_HOME || ui.page == V_ALLP || ui.page == V_ENGP ? 1u << B_HOME
                       : ui.page == V_GLO || ui.page == V_INFO || ui.page == V_RESET || ui.page == V_NOTE ? 1u << B_GLO
                       : ui.page == V_FX || ui.page == V_FXL || ui.page == V_PERF ? 1u << B_FX
                       : ui.page == V_SCN ? 1u << B_HOME
                       : ui.page == V_SEQ || ui.page == V_SQT || ui.page == V_SQN ? 1u << B_SEQ : ui.page == V_ARP ? 0u
                       : ui.page == V_IMPORT ? 1u << B_SEL : ui.page == V_SAVE || ui.page == V_NAME ? 1u << B_SAVE
                       : ui.page == V_MOD ? 1u << ui.mod_btn
                       : ui.page <= V_SAVE ? 1u << B_HOME
                       : 1u << LED[page_group(ui.page)]);
        uint32_t kb = (kl == 3 ? 0u : keys & ~avail) | lit | (ui.page == V_PERF ? ui.pf_keys | ui.pf_lat : 0u), kd = 0, secmid = 0, kmid = 0;
        if (ui.pf_lat)                           /* (v0.47) zatrzaśnięty efekt PERFORM gra: FX świeci */
            bl |= 1u << B_FX;
        if (sel_usable())                        /* SEL świeci pełnym światłem tam, gdzie coś przełącza */
            bl |= 1u << B_SEL;
        if (ui.page >= 0 && !has_more(ui.page)) {   /* przycisk sekcji: pełny = jest MORE, przygaszony = brak */
            secmid = (1u << LED[page_group(ui.page)]) & ~btn;
            bl &= ~secmid;
        }
        if (sq.play)                             /* SEQ gra: PLAY (zielony); REC: nagrywanie */
            bl |= 1u << B_PLAY;
        if (sq.rec)
            bl |= 1u << B_REC;
        if (sqg.arp_on)                          /* ARP świeci, gdy gra (klik włącza / wyłącza) */
            bl |= 1u << B_ARP;
        if (kl == 1 || kl == 2)                  /* GLO KEY LIGHTS: przygaszone C albo wszystkie białe */
            for (k = 0; k < NKEYS; k++) {
                uint32_t pc = (53u + k) % 12u;
                if (kl == 1 ? pc == 0u : (0xAB5u >> pc & 1u))
                    kd |= 1u << k;
            }
        if (rk_on()) {                           /* (v0.87) RACK zamiast KEY LIGHTS: krok z nutą średnio, grający pusty najciemniej,
                                                  * grający z nutą pełnym światłem */
            rk_leds();
            kb |= rk_keys(rk_led_note & rk_led_play);
            kmid = rk_keys(rk_led_note & ~rk_led_play);
            kd = rk_keys(rk_led_play & ~rk_led_note);
        }
        if (ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) {   /* (v0.90) NOTES: nuty kroku pod kursorem */
            sq_leds();
            kb |= sq_led_full;
            kmid |= sq_led_mid;
            kd &= ~(sq_led_full | sq_led_mid);
        }
        {                                        /* jak plat_leds: PLAY świeci i przygasa swoją zieloną diodą */
            uint32_t play = 1u << B_PLAY, db = gl ? ((1u << NB) - 1u) & ~bl : 0u;
            bl = (bl & ~play) | (bl & play ? LED_PLAY_GREEN : 0u);
            db = (db & ~play) | (db & play ? LED_PLAY_GREEN : 0u);
            plat_lights(bl, kb, db, kd & ~kb, gl ? gl : kd ? 1 : 0);
        }
        {                                        /* FX przygaszony jaśniej, gdy któryś efekt gra */
            const fx_slot_t *fc = fxc.cur[set.eng];
            uint32_t fxon = 0;
            int32_t f;
            for (f = 0; f < FX_SLOTS; f++)
                fxon |= fc[f].mode;
            if (mt_ui())                         /* (v0.85) MASTER FX, gdy ktoś wysyła */
                fxon |= mt_swant;
            fxon = fxon && ui.page != V_FX ? 1u << B_FX : 0u;
            if (set.edited[set.eng])             /* brzmienie zmienione: SAVE delikatnie (do wciśnięcia) */
                fxon |= 1u << B_SAVE;
            fxon |= secmid;
            if (!sqg.arp_on && ui.page == V_ARP) /* ustawienia ARP, arp wyłączony: przygaszony */
                fxon |= 1u << B_ARP;
            plat_lights_mid(fxon, (shown & ~lit) | kmid); /* dostępne skróty przygaszone, tylko przy trzymanym SEL */
        }
    }
}

void app_input_only(void) { input(); }

#ifdef PLAT_WEB
#pragma push_macro("__attribute__")
#undef __attribute__
/* symulator: podwójny klik na gałce = wartość domyślna parametru (web/emu/app_worklet.js) */
__attribute__((used, visibility("default"))) void ogs_knob_reset(uint32_t role)
{
    int32_t id;
    if (role < (uint32_t)EN_K1 || role >= (uint32_t)NE || ui.page == V_HOME || ui.page <= V_SAVE)
        return;
    if (ui.page == V_GLO) {
        int32_t g = glo_item((int32_t)(role - EN_K1));
        if (g >= 0)
            knob_turn(role - EN_K1, GDEF[g].def - GV(g));
        return;
    }
    if (ui.quick)
        ui.quick_at = plat_ms();
    ui.browse = 0;
    if ((id = knob_id((int32_t)(role - EN_K1))) < 0)
        return;
    set_param(id, E->def(id)->def);
    ui.knob = (int32_t)(role - EN_K1);
    ui.knob_at = plat_ms();
}
#pragma pop_macro("__attribute__")
#endif

/* --------------------------------------------------------------- ekran --- */
static void mt_strip(void)               /* (v0.64.3) MULTI: górny pasek = ścieżki T1-T4 na każdym ekranie (wybrana
                                          * podświetlona, MUTE przekreślone, * = zmienione); bez komunikatów —
                                          * te w MULTI w stopce (decyzja użytkownika 2026-10-30) */
{
    int32_t t;
    for (t = 0; t < MT_N; t++) {
        const int32_t x = t * 60, e = glob.g[G_MT0 + t], cur = t == mt_at_cur(), mute = glob.g[G_MTMUTE] >> t & 1;
        const char *nm = e >= NENG ? "-" : ENG[e].name;
        char l[3] = {'T', (char)('1' + t), 0};
        if (cur)
            cv_rect(x + 2, 1, 56, 26, C_HI);
        cv_text(x + 6, 2, &FONT_XS, l, cur ? C_BLACK : C_GRAY);
        if (cur && e < NENG && set.edited[set.eng])
            cv_text(x + 54 - text_w(&FONT_XS, "*"), 2, &FONT_XS, "*", C_BLACK);
        cv_text(x + 6, 12, &FONT_XS, nm, cur ? C_BLACK : mute || e >= NENG ? C_DIM : C_WHITE);
        if (mute)
            cv_rect(x + 5, 17, text_w(&FONT_XS, nm) + 2, 1, cur ? C_BLACK : C_GRAY);
        if (e < NENG)                            /* (v0.85) VOL widać na każdym ekranie (dawne okno ścieżek) */
            cv_rect(x + 6, 24, glob.g[G_MTV0 + t] * 48 / 100, 2, cur ? C_BLACK : mute ? C_DIM : C_AMB);
    }
    cv_rect(0, 29, 240, 1, C_LINE);
}

static void header(void)
{
    cv_begin(240, 30, C_BLACK);
    if (mt_ui()) {
        mt_strip();
        cv_blit(0, 0);
        return;
    }
    if (plat_ms() < ui.msg_until) {
        int32_t x = cv_text(8, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(8, 6, &FONT_B, E->name, C_HI);
        char nm[24];
        preset_name(CUR, nm);
        cv_text(x + 8, 6, &FONT_B, nm, C_WHITE);
        if (set.edited[set.eng])
            cv_text(232 - text_w(&FONT_B, "*"), 6, &FONT_B, "*", C_HI);
    }
    cv_rect(0, 29, 240, 1, C_LINE);
    cv_blit(0, 0);
}

static void center(int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text((240 - text_w(f, s)) / 2, y, f, s, c);
}

/* ------------------------------------------------------------ wykresy --- */
/* krzywa w gv[] (jedna wartość na piksel), rysowana od nowa tylko przy zmianie parametru */
#define GX 8
#define GW 224
static float gv[GW];

static float hrand(uint32_t i)           /* stały „los” do rysunków: ten sam obraz przy każdym odświeżeniu */
{
    i *= 0x9E3779B1u;
    i ^= i >> 15;
    i *= 0x85EBCA77u;
    i ^= i >> 13;
    return (float)(int32_t)i * (1.0f / 2147483648.0f);
}

static void graph_plot(int32_t y, int32_t h, int bip, uint16_t c)   /* bip: -1..1 (środek), inaczej 0..1 */
{
    int32_t x, py = 0;
    for (x = 0; x < GW; x++) {
        float v = bip ? 0.5f - 0.5f * gv[x] : 1.0f - gv[x];
        int32_t yy = y + (int32_t)(fm_clampf(v, 0.0f, 1.0f) * (float)(h - 1) + 0.5f);
        if (x)
            cv_line(GX + x - 1, py, GX + x, yy, c);
        py = yy;
    }
}

static void graph_scale(float k)
{
    int32_t x;
    for (x = 0; x < GW; x++)
        gv[x] *= k;
}

/* (v0.90, decyzja użytkownika 2026-11-03: napisy nie mogą na siebie nachodzić) podpis wykresu w rogu na czarnym podkładzie: krzywa
 * rysowana wcześniej nie przechodzi przez litery (g_lblr: prawy róg) */
static void g_lbl(int32_t x, int32_t y, const char *s)
{
    cv_rect(x - 1, y, text_w(&FONT_XS, s) + 3, (int32_t)FONT_XS.h - 2, C_BLACK);
    cv_text(x, y, &FONT_XS, s, C_GRAY);
}
static void g_lblr(int32_t y, const char *s) { g_lbl(GX + GW - 4 - text_w(&FONT_XS, s), y, s); }

static void hz_label(float hz, int32_t y)   /* tempo LFO w rogu wykresu */
{
    char t[16], f[4];
    int32_t d = (int32_t)(hz * 10.0f + 0.5f);
    int_text(d / 10, t, 0);
    if (d < 100) {
        f[0] = '.';
        f[1] = (char)('0' + d % 10);
        f[2] = 0;
        text_cat(t, f);
    }
    text_cat(t, " HZ");
    g_lblr(y + 1, t);
}

#define BODY_H 182                       /* obszar stron: między górnym paskiem (32) a krótkim dolnym (214) */
#define GRAPH_Y 94                       /* wykres pod niższymi kolumnami */
#define GRAPH_H graph_h                  /* 76; przy podpowiedziach na stronach silników niższy (wyższy pasek) */
static int32_t graph_h = 76;
#define HINT_Y (BODY_H - 22)             /* podpowiedź na dole okna */
#define FOOT_TALL 28                     /* o tyle wyższy dolny pasek z podpowiedzią */
/* podpowiedzi na dolnym pasku (footer, hint_line): strona ustawia w trakcie rysowania; GLO > SYSTEM > HINTS OFF:
 * bez podpowiedzi obsługi (krótki pasek, strony dostają to miejsce), foot_keep: to nie podpowiedź, tylko stan
 * (CPU, SURE?) — zostaje zawsze */
static const char *foot_hint;
static char foot_buf[24];
static uint16_t foot_col;
static uint8_t foot_keep;
/* (v0.31) pierwszy efekt zszedł na 16 bitów (pełna pula efektów): komunikat na dolnym pasku przez chwilę, każda strona */
static uint32_t lo_note_until;
#define LO_NOTE "FX MEMORY FULL: RED FX = 16-BIT"
static int lo_note(void) { return lo_note_until != 0u; }
/* strona pod górnym paskiem na ekran: przy wysokim dolnym pasku (podpowiedź, od y 186) bez jego wierszy — inaczej każda
 * klatka najpierw zamazywała wiersz OCT / POLY / VOL / USB, a pasek rysował go od nowa (miganie na FM1, v0.30) */
static int foot_tall(void) { return (ui.page == V_HOME && hints()) || (foot_hint && (hints() || foot_keep)) || lo_note(); }
static void body_blit(void)
{
    uint32_t h = cv_h, top = foot_tall() ? 186u - 32u : (uint32_t)BODY_H;
#ifdef PLAT_WEB
    {                                    /* (v0.87, symulator: tools/screen_audit_test.mjs) coś narysowane pod dolnym paskiem */
        uint32_t x, y;
        au_hid = 0;
        for (y = top; y < h; y++)
            for (x = 0; x < cv_w; x++)
                if (cv_px[y * cv_w + x]) {
                    if (!au_hid++)
                        au_y = y;
                    break;
                }
    }
#endif
    if (cv_h > top)
        cv_h = top;
    cv_blit(0, 32);
    cv_h = h;
}
#include "ogs_moog_ui.c"
#include "ogs_dx7_ui.c"
#include "ogs_wt_ui.c"
#include "ogs_pd_ui.c"
#include "ogs_va_ui.c"
#include "ogs_sem_ui.c"
#include "ogs_dco_ui.c"
#include "ogs_bit_ui.c"
#include "ogs_west_ui.c"
#include "ogs_morph_ui.c"
#include "ogs_dm_ui.c"
#include "ogs_dual_ui.c"
#include "ogs_phys_ui.c"
#include "ogs_formant_ui.c"

/* silniki: strony sekcji (ALL = SELECT z HOME: ENV, LFO, EDIT) */
static const uint8_t MG_EDIT[] = {0, 1, 2, 3, 12, 4, 5, 6, 7, 13, 14}, MG_ENV[] = {8, 9}, MG_LFO[] = {10, 11},
                     MG_ALL[] = {8, 9, 10, 11, 0, 1, 2, 3, 12, 4, 5, 6, 7, 13, 14};   /* (v1.1.0) 14 TUNE na końcu: reszta bez przesunięcia */
static const uint8_t XG_EDIT[] = {0, 1, 2, 3, 4, 5, XP_OPLVL, XP_OPVOL, 6, 7, XP_UNI, XP_FILT, XP_VINT},   /* (v0.68) */
                     XG_ENV[] = {8, 9, 10, 11, 12, 13, 14, XP_FENV}, XG_LFO[] = {15, 16, 17},
                     XG_ALL[] = {8, 9, 10, 11, 12, 13, 14, XP_FENV, 15, 16, 17, 0, 1, 2, 3, 4, 5, XP_OPLVL, XP_OPVOL, 6, 7,
                                 XP_UNI, XP_FILT, XP_VINT};
static const uint8_t WG_EDIT[] = {0, 1, WP_WARP2, WP_UNI, 2, 3, WP_OSCL, 4, 5, 6, 7, WP_F2, WP_MACRO, WP_VINT},
                     WG_ENV[] = {8, 9, 10, WP_ENV4},
                     WG_LFO[] = {11, 12, WP_LFO3, WP_LFO4, WP_DRAW1, WP_DRAW2, 13, 14, 15, 16, 17, 18, 19, 20,
                                 WP_MOD9, WP_MOD9 + 1, WP_MOD9 + 2, WP_MOD9 + 3, WP_MOD9 + 4, WP_MOD9 + 5, WP_MOD9 + 6,
                                 WP_MOD9 + 7},
                     WG_ALL[] = {8, 9, 10, WP_ENV4, 11, 12, WP_LFO3, WP_LFO4, WP_DRAW1, WP_DRAW2, 13, 14, 15, 16, 17,
                                 18, 19, 20, WP_MOD9, WP_MOD9 + 1, WP_MOD9 + 2, WP_MOD9 + 3, WP_MOD9 + 4, WP_MOD9 + 5,
                                 WP_MOD9 + 6, WP_MOD9 + 7, 0, 1, WP_WARP2, WP_UNI, 2, 3, WP_OSCL, 4, 5, 6, 7, WP_F2,
                                 WP_MACRO, WP_VINT};
static const uint8_t ZG_EDIT[] = {0, 1, 17, 2, 3, 4, 5, 18}, ZG_ENV[] = {6, 7, 8, 9, 10}, ZG_LFO[] = {11, 12, 13, 14, 15, 16},
                     ZG_ALL[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 1, 17, 2, 3, 4, 5, 18};
static const uint8_t VG_EDIT[] = {0, 1, 2, 3, 4, 10}, VG_ENV[] = {5, 6}, VG_LFO[] = {7, 8, 9},
                     VG_ALL[] = {5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 10};
static const uint8_t SG_EDIT[] = {0, 1, 2, 3, 4, 10}, SG_ENV[] = {5, 6}, SG_LFO[] = {7, 8, 9},   /* (v0.67) 9 POLY MOD */
                     SG_ALL[] = {5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 10};
static const uint8_t JG_EDIT[] = {0, 1, 2, 3, 4, 9}, JG_ENV[] = {5, 6}, JG_LFO[] = {7, 8},
                     JG_ALL[] = {5, 6, 7, 8, 0, 1, 2, 3, 4, 9};
/* 8BIT / LPG (v0.45): kolejność jak wzorzec MINI — oscylatory, OSC LEVEL (+ łączenie), VOICE, kształtowanie, filtr */
static const uint8_t FG_EDIT[] = {0, 1, 2, 4, 3, 10}, FG_ENV[] = {7, 5, 6}, FG_LFO[] = {8, 9},   /* (v0.84) FORMANT */
                     FG_ALL[] = {7, 5, 6, 8, 9, 0, 1, 2, 4, 3, 10};
static const uint8_t HG_EDIT[] = {0, 1, 2, 3, 7}, HG_ENV[] = {4}, HG_LFO[] = {5, 6},   /* (v0.78) PHYS */
                     HG_ALL[] = {4, 5, 6, 0, 1, 2, 3, 7};
static const uint8_t BG_EDIT[] = {0, 1, 2, 4, 3, 5, 10}, BG_ENV[] = {6, 7}, BG_LFO[] = {8, 9},
                     BG_ALL[] = {6, 7, 8, 9, 0, 1, 2, 4, 3, 5, 10};
static const uint8_t QG_EDIT[] = {0, 1, 2, 3, 6, 4, 5, 10}, QG_ENV[] = {7, 11}, QG_LFO[] = {8, 9},   /* (v0.69) 11 ENV 2 */
                     QG_ALL[] = {7, 11, 8, 9, 0, 1, 2, 3, 6, 4, 5, 10};
static const engine_t ENG[NENG] = {
    {"MINI", P, NP, NPAGES, NPRESETS, PAGE_NAME, {MG_EDIT, MG_ENV, MG_LFO, MG_ALL}, {11, 2, 2, 15}, 7, P_MODE,
     P_CUT, {P_MODE, P_PRIO, -1}, 0, moog_reset, moog_load_preset, moog_preset_name, moog_note_on, moog_note_off,
     moog_all_off, moog_render, moog_knob, moog_sel, moog_more, moog_def, moog_vget, moog_vset, moog_text, moog_label_page, moog_page_label,
     moog_has_graph, moog_graph, moog_held_note, moog_scope_semi, moog_sounding, 0, 0, moog_gest, moog_moddepth},
    {"FM6", X, NX, NXPAGES, NDXP, XPAGE_NAME, {XG_EDIT, XG_ENV, XG_LFO, XG_ALL}, {13, 8, 3, 24}, XP_ALGO, X_MODE,
     X_ALG, {X_MODE, X_PRIO, X_VOICES}, 0, dx_reset, dx_load, dx_preset_name, dx_note_on, dx_note_off, dx_all_off,
     dx_render, dx_knob, dx_sel, dx_more, dx_def, dx_vget, dx_vset, dx_text, dx_label_page, dx_page_label,
     dx_has_graph, dx_graph, dx_held_note, dx_scope_semi, dx_sounding, 0, 0, dx_gest, dx_moddepth},
    {"WAVE", W, NW, NWPAGES, NWTP, WPAGE_NAME, {WG_EDIT, WG_ENV, WG_LFO, WG_ALL}, {14, 4, 22, 40}, 7, W_VMODE,
     W_APOS, {W_VMODE, W_VOICES, -1}, wt_init, wt_reset, wt_load, wt_preset_name, wt_note_on, wt_note_off, wt_all_off,
     wt_render, wt_knob, wt_sel, wt_more, wt_def, wt_vget, wt_vset, wt_text, wt_label_page, wt_page_label, wt_has_graph,
     wt_graph, wt_held_note, wt_scope_semi, wt_sounding, wt_graph_live, wt_step, wt_gest, wt_moddepth},
    {"PHASE", Z, NZ, NZPAGES, NZP, ZPAGE_NAME, {ZG_EDIT, ZG_ENV, ZG_LFO, ZG_ALL}, {8, 5, 6, 19}, 5, Z_VMODE,
     Z_DCW1, {Z_VMODE, Z_VOICES, -1}, pd_init, pd_reset, pd_load, pd_preset_name, pd_note_on, pd_note_off, pd_all_off,
     pd_render, pd_knob, pd_sel, pd_more, pd_def, pd_vget, pd_vset, pd_text, pd_label_page, pd_page_label, pd_has_graph,
     pd_graph, pd_held_note, pd_scope_semi, pd_sounding, pd_graph_live, 0, pd_gest, pd_moddepth},
    {"VCO", V, NV, NVPAGES, NVP, VPAGE_NAME, {VG_EDIT, VG_ENV, VG_LFO, VG_ALL}, {6, 2, 3, 11}, 4, V_MODE,
     V_CUT, {V_MODE, V_UNI, -1}, vc_init, vc_reset, vc_load, vc_preset_name, vc_note_on, vc_note_off, vc_all_off,
     vc_render, vc_knob, vc_sel, vc_more, vc_def, 0, 0, vc_text, vc_label_page, vc_page_label, vc_has_graph,
     vc_graph, sk_held_note, vc_scope_semi, sk_sounding, vc_graph_live, 0, vc_gest, vc_moddepth},
    {"SVF", S, NS, NSPAGES, NSP, SPAGE_NAME, {SG_EDIT, SG_ENV, SG_LFO, SG_ALL}, {6, 2, 3, 11}, 4, S_MODE,
     S_CUT, {S_MODE, S_UNI, -1}, sm_init, sm_reset, sm_load, sm_preset_name, sm_note_on, sm_note_off, sm_all_off,
     sm_render, sm_knob, sm_sel, sm_more, sm_def, 0, 0, sm_text, sm_label_page, sm_page_label, sm_has_graph,
     sm_graph, sk_held_note, sm_scope_semi, sk_sounding, sm_graph_live, 0, sm_gest, sm_moddepth},
    {"DCO", J, NJ, NJPAGES, NJP, JPAGE_NAME, {JG_EDIT, JG_ENV, JG_LFO, JG_ALL}, {6, 2, 2, 10}, 3, J_MODE,
     J_CUT, {J_MODE, J_UNI, -1}, jn_init, jn_reset, jn_load, jn_preset_name, jn_note_on, jn_note_off, jn_all_off,
     jn_render, jn_knob, jn_sel, jn_more, jn_def, 0, 0, jn_text, jn_label_page, jn_page_label, jn_has_graph,
     jn_graph, sk_held_note, jn_scope_semi, sk_sounding, jn_graph_live, 0, jn_gest, jn_moddepth},
    {"8BIT", B, NBT, NBPAGES, NBTP, BPAGE_NAME, {BG_EDIT, BG_ENV, BG_LFO, BG_ALL}, {7, 2, 2, 11}, 3, B_MODE,
     B_CRUSH, {B_MODE, B_UNI, -1}, bt_init, bt_reset, bt_load, bt_preset_name, bt_note_on, bt_note_off, bt_all_off,
     bt_render, bt_knob, bt_sel, bt_more, bt_def, 0, 0, bt_text, bt_label_page, bt_page_label, bt_has_graph,
     bt_graph, sk_held_note, bt_scope_semi, sk_sounding, bt_graph_live, 0, bt_gest, bt_moddepth},
    {"LPG", WSP, NWS, NWSPAGES, NWSP, WSPAGE_NAME, {QG_EDIT, QG_ENV, QG_LFO, QG_ALL}, {8, 2, 2, 12}, 4, WS_MODE,
     WS_FOLD, {WS_MODE, WS_UNI, -1}, ws_init, ws_reset, ws_load, ws_preset_name, ws_note_on, ws_note_off, ws_all_off,
     ws_render, ws_knob, ws_sel, ws_more, ws_def, 0, 0, ws_text, ws_label_page, ws_page_label, ws_has_graph,
     ws_graph, sk_held_note, ws_scope_semi, sk_sounding, ws_graph_live, 0, ws_gest, ws_moddepth},
    {"ALPHA", JA, NJ, NJPAGES, NAP, JPAGE_NAME, {JG_EDIT, JG_ENV, JG_LFO, JG_ALL}, {6, 2, 2, 10}, 3, J_MODE,   /* (v0.49: kod DCO) */
     J_CUT, {J_MODE, J_UNI, -1}, al_init, jn_reset, al_load, al_preset_name, al_note_on, al_note_off, jn_all_off,
     al_render, al_knob, jn_sel, al_more, al_def, 0, 0, al_text, jn_label_page, jn_page_label, jn_has_graph,
     al_graph, sk_held_note, al_scope_semi, sk_sounding, jn_graph_live, 0, al_gest, al_moddepth},
    {"MORPH", MP, NMPP, NMPPAGES, NMPK, MPPAGE_NAME, {MPG_EDIT, MPG_ENV, MPG_LFO, MPG_ALL}, {3, 1, 2, 6}, 4, MP_MODE,   /* (v0.50:
                                                                                            * perkusja, propozycja) */
     MP_MORPH, {MP_MODE, -1, -1}, mp_init, mp_reset, mp_load, mp_preset_name, mp_note_on, mp_note_off, mp_all_off,
     mp_render, mp_knob, mp_sel, mp_more, mp_def, 0, 0, mp_text, mp_label_page, mp_page_label, mp_has_graph,
     mp_graph, mp_held_note, mp_scope_semi, mp_sounding, mp_graph_live, 0, 0, 0, mp_note_cut, mp_sound_s},
    {"88", D8, NDMP, NDMPAGES, ND8K, DMPAGE_NAME, {DMG_EDIT, DMG_ENV, DMG_LFO, DMG_ALL}, {1, 1, 2, 4}, 2, DM_KMODE,   /* (v0.52:
                                                                                         * TR-808, propozycja) */
     DM_KDEC, {DM_KMODE, -1, -1}, d8_init, dm_reset, d8_load, d8_preset_name, d8_note_on, dm_note_off, dm_all_off,
     d8_render, dm_knob, dm_sel, dm_more, d8_def, 0, 0, dm_text, dm_label_page, dm_page_label, dm_has_graph,
     d8_graph, dm_held_note, d8_scope_semi, dm_sounding, dm_graph_live, 0, 0, 0, dm_note_cut, d8_sound_s},
    {"99", D9, NDMP, NDMPAGES, ND9K, DMPAGE_NAME, {DMG_EDIT, DMG_ENV, DMG_LFO, DMG_ALL}, {1, 1, 2, 4}, 2, DM_KMODE,   /* (v0.52:
                                                                                         * TR-909, propozycja) */
     DM_KDEC, {DM_KMODE, -1, -1}, d9_init, dm_reset, d9_load, d9_preset_name, d9_note_on, dm_note_off, dm_all_off,
     d9_render, dm_knob, dm_sel, dm_more, d9_def, 0, 0, dm_text, dm_label_page, dm_page_label, dm_has_graph,
     d9_graph, dm_held_note, d9_scope_semi, dm_sounding, dm_graph_live, 0, 0, 0, dm_note_cut, d9_sound_s},
    {"DUAL", DU, NDUP, NDUPAGES, NDUK, DUPAGE_NAME, {DUG_EDIT, DUG_ENV, DUG_LFO, DUG_ALL}, {3, 2, 2, 7}, 4, DU_KMODE,   /* (v0.54:
                                                                                          * Volca Drum, propozycja) */
     DU_KWGD, {DU_KMODE, -1, -1}, du_init, du_reset, du_load, du_preset_name, du_note_on, du_note_off, du_all_off,
     du_render, du_knob, du_sel, du_more, du_def, du_vget, du_vset, du_text, du_label_page, du_page_label, du_has_graph,
     du_graph, du_held_note, du_scope_semi, du_sounding, du_graph_live, 0, 0, 0, du_note_cut, du_sound_s},
    {"PHYS", PH, NPH, NPHPAGES, NPHP, PHPAGE_NAME, {HG_EDIT, HG_ENV, HG_LFO, HG_ALL}, {5, 1, 2, 8}, 1, PH_MODE,   /* (v0.78:
                                                                                          * propozycja) */
     PH_POS, {PH_MODE, PH_UNI, -1}, ph_init, ph_reset, ph_load, ph_preset_name, ph_note_on, ph_note_off, ph_all_off,
     ph_render, ph_knob, ph_sel, ph_more, ph_def, 0, 0, ph_text, ph_label_page, ph_page_label, ph_has_graph,
     ph_graph, sk_held_note, ph_scope_semi, sk_sounding, ph_graph_live, 0, ph_gest, ph_moddepth},
    {"FORMANT", FO, NFOR, NFOPAGES, NFOP, FOPAGE_NAME, {FG_EDIT, FG_ENV, FG_LFO, FG_ALL}, {6, 3, 2, 11}, 0, FO_MODE,   /* (v0.84:
                                                                                          * propozycja) */
     FO_VOWEL, {FO_MODE, FO_UNI, -1}, fo_init, fo_reset, fo_load, fo_preset_name, fo_note_on, fo_note_off, fo_all_off,
     fo_render, fo_knob, fo_sel, fo_more, fo_def, fo_vget, fo_vset, fo_text, fo_label_page, fo_page_label, fo_has_graph,
     fo_graph, sk_held_note, fo_scope_semi, sk_sounding, fo_graph_live, 0, fo_gest, fo_moddepth},
};

static void draw_graph(int32_t page)
{
    const int32_t y = GRAPH_Y, h = GRAPH_H;
    cv_rect(GX - 2, y - 2, GW + 4, h + 4, C_LINE);
    cv_rect(GX - 1, y - 1, GW + 2, h + 2, C_BLACK);
    E->graph(page);
}

/* oscyloskop: ok. 3 okresy granej nuty, zaczepiony na przejściu przez zero, auto-wzmocnienie do 4x */
#define SCOPE_Y (mt_ui() ? 30 : 22)       /* HOME bez górnego paska: oscyloskop u góry, nazwy na dole; (v0.64.3) MULTI:
                                          * pasek ścieżek 30 px jak na stronach, oscyloskop niższy */
#define SCOPE_H (mt_ui() ? 70 : 78)
#define HOME_NAME_Y (SCOPE_Y + SCOPE_H)   /* pas nazwy presetu (na nazwie silnika w tle) do stopki */
static uint16_t bg_lut[4];

static float glyph_a(const felucca_font_t *f, uint32_t gi, int32_t gx, int32_t gy)   /* poza komórką: 0 */
{
    return (float)font_a(f, gi, gx, gy);
}

static void bg_make(void)                /* nazwa silnika FONT_L powiększona z wygładzaniem (dwuliniowo) */
{
    const felucca_font_t *f = &FONT_L;
    const char *s = E->name;
    float sc = fm_minf(200.0f / (float)text_w(f, s), 4.2f), x0;   /* wysokość ograniczona niżej (zarys liter); (v0.90) 200 px, było 226: FORMANT od brzegu do brzegu */
    int32_t y0, i, top = f->h, bot = -1;
    uint32_t c;
    for (i = 0; s[i]; i++) {                 /* zarys liter (wiersze z tuszem), nie pudełko czcionki: środek optyczny */
        uint32_t gi = glyph(f, (uint8_t)s[i]);
        int32_t gx, gy;
        for (gy = 0; gy < (int32_t)f->h; gy++)
            for (gx = 0; gx < f->bw[gi]; gx++)
                if (glyph_a(f, gi, gx, gy) > 7.0f) {
                    top = gy < top ? gy : top;
                    bot = gy > bot ? gy : bot;
                }
    }
    if (bot < top) {
        top = 0;
        bot = f->h - 1;
    }
    sc = fm_minf(sc, 128.0f / (float)(bot - top + 1));   /* środek ekranu (120) i nad stopką (186) */
    x0 = 0.0f;
    y0 = (int32_t)(120.0f - (float)(bot - top + 1) * sc * 0.5f - (float)top * sc);   /* środek całego ekranu, z paskiem */
    for (i = 0; i < (int32_t)sizeof bg_a; i++)
        bg_a[i] = 0;
    x0 = (240.0f - (float)text_w(f, s) * sc) * 0.5f;
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s);
        int32_t bw = f->bw[gi], ox, oy, X0 = (int32_t)(x0 - (float)f->pad * sc);
        for (oy = 0; oy < (int32_t)((float)f->h * sc); oy++)
            for (ox = 0; ox < (int32_t)((float)bw * sc) + 1; ox++) {
                float sx = ((float)ox + 0.5f) / sc - 0.5f, sy = ((float)oy + 0.5f) / sc - 0.5f;
                int32_t ix = (int32_t)fm_floorf(sx), iy = (int32_t)fm_floorf(sy), px = X0 + ox, py = y0 + oy;
                float fx = sx - (float)ix, fy = sy - (float)iy, a;
                if (px < 0 || px >= 240 || py < 0 || py >= BG_H)
                    continue;
                a = (glyph_a(f, gi, ix, iy) * (1.0f - fx) + glyph_a(f, gi, ix + 1, iy) * fx) * (1.0f - fy)
                  + (glyph_a(f, gi, ix, iy + 1) * (1.0f - fx) + glyph_a(f, gi, ix + 1, iy + 1) * fx) * fy;
                a = fm_clampf((a * (1.0f / 15.0f) - 0.5f) * sc * 1.3f + 0.5f, 0.0f, 1.0f) * 3.0f + 0.5f;   /* ostra krawędź */
                {
                    uint32_t at = (uint32_t)(py * 240 + px), sh = (at & 3u) * 2u, q = (uint32_t)a;
                    if (q > (bg_a[at >> 2] >> sh & 3u))
                        bg_a[at >> 2] = (uint8_t)((bg_a[at >> 2] & ~(3u << sh)) | q << sh);
                }
            }
        x0 += (float)f->adv[gi] * sc;
    }
    if (ENG_GRP[set.eng] == EG_DRUM || mt_ui()) {   /* (v0.54) perkusja: nad nazwą małe „DRUMS” także na HOME (decyzja
                                              * użytkownika 2026-10-26: jak w oknie ENGINE, ten sam kolor co nazwa);
                                              * (v0.64) MULTI: ścieżka, np. „T2 BASS” */
        const felucca_font_t *m = &FONT_M;
        static const char *const MT_LBL[MT_N] = {"T1 DRUMS", "T2", "T3", "T4"};
        const char *d = mt_ui() ? MT_LBL[glob.g[G_MTFOC]] : "DRUMS";
        int32_t yd = (int32_t)((float)y0 + (float)top * sc) - (int32_t)m->h - 2, xd = (240 - text_w(m, d)) / 2;
        for (; *d; d++) {
            uint32_t gi = glyph(m, (uint8_t)*d);
            int32_t gx, gy;
            for (gy = 0; gy < (int32_t)m->h; gy++)
                for (gx = 0; gx < m->bw[gi]; gx++) {
                    const int32_t px = xd - m->pad + gx, py = yd + gy;
                    const uint32_t q = (uint32_t)(glyph_a(m, gi, gx, gy) * (3.0f / 15.0f) + 0.5f);
                    if (q && px >= 0 && px < 240 && py >= 0 && py < BG_H) {
                        uint32_t at = (uint32_t)(py * 240 + px), sh = (at & 3u) * 2u;
                        if (q > (bg_a[at >> 2] >> sh & 3u))
                            bg_a[at >> 2] = (uint8_t)((bg_a[at >> 2] & ~(3u << sh)) | q << sh);
                    }
                }
            xd += m->adv[gi];
        }
    }
    {                                        /* kolor: ciemna szarość z odrobiną odcienia motywu (GLO COLOR) */
        uint32_t hr = ((uint32_t)C_HI >> 11 & 31u) * 255u / 31u, hg = ((uint32_t)C_HI >> 5 & 63u) * 255u / 63u,
                 hb = ((uint32_t)C_HI & 31u) * 255u / 31u;
        /* ledwo jaśniejsza od czerni: wyświetlacz FM1 mocno rozjaśnia ciemne kolory (zdjęcie użytkownika), więc
         * najniższe stopnie RGB565: brzeg 1 stopień zieleni, środek 1/2/1 */
        static const uint16_t L[4] = {0, (1u << 5), (1u << 11) | (1u << 5) | 1u, (1u << 11) | (2u << 5) | 1u};
        (void)hr;
        (void)hg;
        (void)hb;
        for (c = 0; c < 4u; c++)
            bg_lut[c] = swap16(L[c]);
    }
}

static void bg_rows(int32_t y0)          /* wiersze tła od y0 na bieżące płótno (cv_begin już wyczyścił) */
{
    int32_t key = set.eng * 16 + glob.g[G_COLOR] + (mt_ui() ? (glob.g[G_MTFOC] + 1) * 1024 : 0), y, x;
    if (mt_ui() && glob.g[G_MT0 + mt_at_cur()] >= NENG)   /* (v0.85) pusta ścieżka: bez nazwy silnika */
        return;
    if (key != bg_for) {
        bg_make();
        bg_for = key;
    }
    for (y = 0; y < (int32_t)cv_h && y0 + y < BG_H; y++)
        for (x = 0; x < 240; x++) {
            uint32_t at = (uint32_t)((y0 + y) * 240 + x), a = bg_a[at >> 2] >> ((at & 3u) * 2u) & 3u;
            if (a)
                cv_px[y * 240 + x] = bg_lut[a];
        }
}

static void draw_scope(void)
{
    uint32_t w = scope_w, step, nn, lim, i, start;
    float hz = ctl.a4 * fm_exp2f((eng_scope_semi() - 69.0f) * (1.0f / 12.0f)), per = OGS_FS / hz, pk = 0.05f;
    int32_t x;
    step = (uint32_t)fm_clampf(3.0f * per / (float)GW, 1.0f, 8.0f);
    nn = GW * step;
    lim = (uint32_t)fm_minf(per + 2.0f, (float)(SC_LEN - nn - 64u));
    start = w - nn;
    for (i = 0; i < lim; i++) {
        uint32_t p = w - nn - i;
        if (scope[(p - 1u) & (SC_LEN - 1u)] < 0 && scope[p & (SC_LEN - 1u)] >= 0) {
            start = p;
            break;
        }
    }
    for (x = 0; x < GW; x++) {
        gv[x] = (float)scope[(start + (uint32_t)x * step) & (SC_LEN - 1u)] * (1.0f / 32767.0f);
        pk = fm_maxf(pk, fm_fabsf(gv[x]));
    }
    graph_scale(fm_minf(4.0f, 0.9f / pk));
    cv_begin(240, SCOPE_H, C_BLACK);
    bg_rows(SCOPE_Y);
    cv_rect(GX, SCOPE_H / 2, GW, 1, C_LINE);
    graph_plot(2, SCOPE_H - 4, 1, C_HI);
    cv_blit(0, SCOPE_Y);
}

static void mt_cols(int32_t y)           /* (v0.82) HOME w MULTI: K1-K4 = VOL, PAN, REVERB, DELAY wybranej ścieżki */
{
    static const char *const NM[4] = {"VOL", "PAN", "REVERB", "DELAY"};
    const int32_t t = mt_at_cur(), mute = glob.g[G_MTMUTE] >> t & 1;
    const uint32_t now = plat_ms();
    int32_t i;
    for (i = 0; i < 4; i++) {
        const int32_t x = 4 + i * 59, lit = ui.knob == i && now - ui.knob_at < 1500u;
        const int32_t v = i == 0 ? glob.g[G_MTV0 + t] : i == 1 ? glob.g[G_MTP0 + t] : mt_send_ui(t, (uint32_t)i - 2u);
        const int off = i >= 2 && (!mt_linked(t, (uint32_t)i - 2u) || !glob.mfx.s[i - 2].p[3]);   /* (v0.88) odpięty, MIX 0: ciemno */
        char s[8];
        if (i == 1) {
            mt_pan_text(v, s);
        } else if (i == 0 && mute) {
            s[0] = 0;
            text_cat(s, "MUTE");
        } else if (i >= 2 && !mt_linked(t, (uint32_t)i - 2u)) {
            s[0] = 0;
            text_cat(s, "OFF");
        } else {
            int_text(v, s, 0);
        }
        cv_text(x + (55 - text_w(&FONT_XS, NM[i])) / 2, y, &FONT_XS, NM[i], lit ? C_WHITE : C_GRAY);
        cv_text(x + (55 - text_w(&FONT_XS, s)) / 2, y + 11, &FONT_XS, s, off ? C_DIM : lit || !(i == 0 && mute) ? C_HI : C_GRAY);
        cv_rect(x + 4, y + 24, 47, 2, C_LINE);
        if (i == 1) {                        /* PAN: od środka */
            const int32_t w = (v < 0 ? -v : v) * 23 / 50;
            if (w)
                cv_rect(x + 27 - (v < 0 ? w : 0), y + 24, w + 1, 2, lit ? C_HI : C_AMB);
        } else {
            cv_rect(x + 4, y + 24, v * 47 / 100, 2, off ? C_DIM : lit ? C_HI : C_AMB);
        }
    }
}

/* (v1.1.0, akordy A; GLO > DISPLAY > CHORD NAME, domyślnie OFF) nazwa granego akordu na HOME (SOLO, pierwszy wiersz z prawej,
 * zamiast PRESET n / m): klasy dźwięków z nt_cnt (klawisze, MIDI, SEQ, CHORD), bas = najniższa; najprostszy pasujący kształt
 * na basie, inaczej na innej nucie akordu z „/BAS”; nieznany: bez napisu; znika 1 s po puszczeniu. Czcionki bez małych liter:
 * MIN / MAJ zamiast m / M, -5 / -9 / +9 zamiast b5 / b9 / #9 */
static const struct { uint16_t iv; char q[8]; } CHN[] = {   /* bit i: i półtonów nad prymą; najprostsze najpierw */
    {0x091, ""}, {0x089, "MIN"}, {0x049, "DIM"}, {0x111, "AUG"}, {0x0A1, "SUS4"}, {0x085, "SUS2"}, {0x081, "5"},
    {0x491, "7"}, {0x891, "MAJ7"}, {0x489, "MIN7"}, {0x449, "MIN7-5"}, {0x249, "DIM7"}, {0x4A1, "7SUS4"}, {0x291, "6"},
    {0x289, "MIN6"}, {0x889, "MINMAJ7"}, {0x095, "ADD9"}, {0x08D, "MINADD9"}, {0x495, "9"}, {0x895, "MAJ9"}, {0x48D, "MIN9"},
    {0x295, "6/9"}, {0x4B5, "11"}, {0x4AD, "MIN11"}, {0x695, "13"}, {0x8D1, "MAJ7+11"}, {0x493, "7-9"}, {0x499, "7+9"},
    {0x411, "7"}, {0x811, "MAJ7"}, {0x409, "MIN7"}, {0x415, "9"}, {0x40D, "MIN9"},
};
#define NCHN (uint32_t)(sizeof CHN / sizeof CHN[0])
static uint32_t ch_sig, ch_last;
static void chord_name_upd(uint32_t now)
{
    uint32_t n, pcs = 0, bass = 12u, sig, r, i, k, best = NCHN, root = 0;
    if (!GV(G_CHNAME) || mt_ui()) {
        ch_txt[0] = 0;
        return;
    }
    for (n = 0; n < 128u; n++)
        if (nt_cnt[n]) {
            if (bass == 12u)
                bass = n % 12u;
            pcs |= 1u << (n % 12u);
        }
    if (pcs & (pcs - 1u))
        ch_last = now;
    else if (ch_txt[0] && now - ch_last > 1000u) {   /* puszczony: napis jeszcze 1 s */
        ch_txt[0] = 0;
        ch_sig = 0;
        ui.redraw = 1;
    }
    sig = pcs | bass << 12;
    if (!(pcs & (pcs - 1u)) || sig == ch_sig)
        return;
    ch_sig = sig;
    for (i = 0; i < 12u; i++) {
        r = (bass + i) % 12u;
        if (pcs >> r & 1u)
            for (k = 0; k < best; k++)
                if (CHN[k].iv == ((pcs >> r | pcs << (12u - r)) & 0xFFFu)) {
                    best = k;
                    root = r;
                    break;
                }
        if (!i && best < NCHN)
            break;                               /* na basie: bez ukośnika */
    }
    ch_txt[0] = 0;
    if (best < NCHN) {
        text_cat(ch_txt, L_PC12[root]);
        text_cat(ch_txt, CHN[best].q);
        if (root != bass) {
            text_cat(ch_txt, "/");
            text_cat(ch_txt, L_PC12[bass]);
        }
    }
    ui.redraw = 1;
}

static void draw_home(void)            /* bez górnego paska: wiersz informacji, oscyloskop, na dole nazwa presetu na silniku */
{
    char t[24], n[8], name[24];
    int vis = glob.g[G_VHOME];
    int32_t i, pos = 0, all = 0, x;
    preset_name(CUR, name);
    t[0] = 0;                                /* PRESET n / m (SOLO: pierwszy wiersz z prawej, MULTI: pod nazwą) */
    if (CUR >= ID_FM1 && CUR < ID_XF) {      /* bank FM1: numer w nim */
        text_cat(t, "FM1  ");
        int_text(CUR - ID_FM1 + 1, n, 0);
        text_cat(t, n);
        text_cat(t, " / 128");
    } else if (CUR >= ID_CZ) {               /* brzmienie z banku CZ: nazwa banku i numer w nim */
        syx_bank_name(NSYX + (CUR - ID_CZ) / 16, t);
        text_cat(t, "  ");
        int_text((CUR - ID_CZ) % 16 + 1, n, 0);
        text_cat(t, n);
        text_cat(t, " / 16");
    } else if (CUR >= ID_SYX) {              /* brzmienie z banku: nazwa banku i numer w nim */
        syx_bank_name((CUR - ID_SYX) / 32, t);
        text_cat(t, "  ");
        int_text((CUR - ID_SYX) % 32 + 1, n, 0);
        text_cat(t, n);
        text_cat(t, " / 32");
    } else if (!is_init(CUR) && (CUR < ID_SYX || CUR >= ID_XF)) {
        for (i = 0; i < NALL; i = i + 1 == ID_SYX ? ID_XF : i + 1)   /* numer w sekcji silnika (v0.36): jego USER
                                                                   * albo fabryczne (bez INIT; v0.39: z efektami) */
            if (listed(i, set.eng) && !is_init(i)
                && (i >= ID_USER && i < ID_SYX) == (CUR >= ID_USER && CUR < ID_SYX)) {
                all++;
                if (i <= CUR)
                    pos++;
            }
        text_cat(t, CUR >= ID_USER ? "USER PRESET  " : "PRESET  ");
        int_text(pos, n, 0);
        text_cat(t, n);
        text_cat(t, " / ");
        int_text(all, n, 0);
        text_cat(t, n);
    } else {
        text_cat(t, "INIT");
    }
    if (mt_ui()) {                           /* (v0.64.3) MULTI: pasek ścieżek jak na stronach (PRESET n / m pod nazwą) */
        cv_begin(240, 30, C_BLACK);
        mt_strip();
        cv_blit(0, 0);
    } else {
        cv_begin(240, SCOPE_Y, C_BLACK);     /* pierwszy wiersz: silnik z lewej, PRESET n / m z prawej */
        bg_rows(0);
        if (plat_ms() < ui.msg_until) {      /* komunikaty (SAVED, ENGINE ...) w pierwszym wierszu */
            x = cv_text(8, 2, &FONT_B, ui.msg[0], C_WHITE);
            cv_text(x + 8, 2, &FONT_B, ui.msg[1], C_HI);
        } else {
            cv_text(8, 6, &FONT_XS, E->name, C_HI);
            if (ch_txt[0]) {                 /* (v1.1.0) CHORD NAME: grany akord zamiast PRESET n / m */
                x = 232 - text_w(&FONT_B, ch_txt);
                cv_text(x, 2, &FONT_B, ch_txt, C_WHITE);
            } else {
                x = 232 - text_w(&FONT_XS, t);
                cv_text(x, 6, &FONT_XS, t, C_GRAY);
            }
            if (set.edited[set.eng])         /* zmienione względem presetu (jak SAVE) */
                cv_text(x - 6 - text_w(&FONT_B, "*"), 2, &FONT_B, "*", C_HI);
        }
        cv_blit(0, 0);
    }
    if (vis) {
        draw_scope();
        ui.scope_at = plat_ms();
    } else {
        cv_begin(240, SCOPE_H, C_BLACK);
        bg_rows(SCOPE_Y);
        if (hints()) {
            center(SCOPE_H / 2 - 18, &FONT_XS, mt_ui() ? "PRESETS: BROWSE    SEL: MUTE" : "PRESETS: BROWSE    K1-K4: QUICK PAGE", C_GRAY);
            center(SCOPE_H / 2 + 2, &FONT_XS, "EDIT / ENV / LFO: PAGES", C_GRAY);
        }
        cv_blit(0, SCOPE_Y);
    }
    cv_begin(240, (hints() ? 186 : 214) - HOME_NAME_Y, C_BLACK);   /* nazwa presetu na środku nazwy silnika (środek
                                                                     * ekranu: 120), do dolnego paska */
    bg_rows(HOME_NAME_Y);
    if (mt_ui() && glob.g[G_MT0 + mt_at_cur()] >= NENG) {   /* (v0.85) pusta ścieżka: ALGO daje jej silnik */
        center(120 - HOME_NAME_Y - 16, &FONT_L, "EMPTY", C_DIM);
        center(120 - HOME_NAME_Y + 22, &FONT_XS, "SEL: CHOOSE AN ENGINE", C_GRAY);
    } else {
        center(120 - HOME_NAME_Y - 16, text_w(&FONT_L, name) <= 232 ? &FONT_L : &FONT_M, name, C_WHITE);
        if (mt_ui()) {                       /* (v0.64.3) MULTI: PRESET n / m pod nazwą (u góry pasek ścieżek) */
            if (!is_init(CUR))               /* (v0.85) INIT: bez szarego drugiego INIT */
                center(120 - HOME_NAME_Y + 22, &FONT_XS, t, C_GRAY);
            mt_cols(158 - HOME_NAME_Y);
        }
    }
    cv_blit(0, HOME_NAME_Y);
}

/* 4 kolumny = K1..K4, jak gałki na panelu; d[i] 0: puste pole; compact: niższe, pod nimi wykres */
static uint8_t col_red;                  /* draw_cols: wartości w tych kolumnach na czerwono (bit = kolumna) */
static int16_t col_y = -1, col_h = -1;   /* draw_cols: inne położenie / wysokość pól (strona FX) */
static int8_t col_mod[4];                /* v0.35 draw_cols: modulacja parametru (-100..100, 0 brak): pasek przy wartości */
#define C_MOD RGB(70, 170, 255)
static void draw_cols(const pdef_t *const *d, const int16_t *v, char (*txt)[16], int compact)
{
    uint32_t i, now = plat_ms();
    int32_t y = col_y >= 0 ? col_y : compact ? 30 : 36, bh = col_h >= 0 ? col_h : compact ? 56 : BODY_H - 36 - 10;
    int32_t barh = bh - 66;
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59, span, h;
        int lit = ui.knob == (int32_t)i && now - ui.knob_at < 1500u;
        const char *nm, *sp;
        char l1[12], l2[12];
        uint32_t j;
        if (!d[i] || d[i]->fmt == F_NONE) {  /* puste pole: gałka nic nie robi */
            cv_rect(x, y, 55, bh, C_LINE);
            cv_rect(x + 1, y + 1, 53, bh - 2, C_BLACK);
            continue;
        }
        span = d[i]->hi - d[i]->lo;
        nm = sp = d[i]->name;
        while (*sp && *sp != ' ')            /* nazwa w dwóch linijkach: "OSC1" / "WAVE" */
            sp++;
        for (j = 0; nm + j < sp && j < 11u; j++)
            l1[j] = nm[j];
        l1[j] = 0;
        for (j = 0; *sp && sp[1 + j] && j < 11u; j++)
            l2[j] = sp[1 + j];
        l2[j] = 0;
        cv_rect(x, y, 55, bh, lit ? C_DIM : C_LINE);
        cv_text(x + (55 - text_w(&FONT_XS, l1)) / 2, y + (compact ? 2 : 4), &FONT_XS, l1, lit ? C_WHITE : C_GRAY);
        cv_text(x + (55 - text_w(&FONT_XS, l2)) / 2, y + (compact ? 15 : 18), &FONT_XS, l2, lit ? C_WHITE : C_GRAY);
        {                                    /* wartość: za długa na FONT_S -> mniejsza czcionka (mieści się w polu) */
            const felucca_font_t *vf = text_w(&FONT_S, txt[i]) > 53 ? &FONT_XS : &FONT_S;
            cv_text(x + (55 - text_w(vf, txt[i])) / 2, y + (compact ? 33 : 40) + (vf == &FONT_XS ? 1 : 0), vf, txt[i],
                    col_red >> i & 1u ? RGB(255, 60, 50) : C_HI);
        }
        if (compact) {                       /* cienki pasek wartości na dole pola */
            h = span ? (v[i] - d[i]->lo) * 47 / span : 0;
            cv_rect(x + 4, y + bh - 4, 47, 2, C_BLACK);
            cv_rect(x + 4, y + bh - 4, h, 2, lit ? C_HI : C_AMB);
            if (col_mod[i]) {                /* modulacja: od wartości w prawo (+) / w lewo (-), nad paskiem */
                int32_t a = h + col_mod[i] * 47 / 100, lo = clamp(a < h ? a : h, 0, 47), hi = clamp(a < h ? h : a, 0, 47);
                cv_rect(x + 4 + lo, y + bh - 7, hi > lo ? hi - lo : 1, 2, C_MOD);
            }
        } else {
            h = span ? (v[i] - d[i]->lo) * barh / span : 0;
            cv_rect(x + 8, y + 62, 39, barh, C_BLACK);
            cv_rect(x + 8, y + 62 + barh - h, 39, h, lit ? C_HI : C_AMB);
            if (col_mod[i]) {                /* modulacja: pionowo przy prawej krawędzi, od wartości w górę / w dół */
                int32_t a = h + col_mod[i] * barh / 100, lo = clamp(a < h ? a : h, 0, barh), hi = clamp(a < h ? h : a, 0, barh);
                cv_rect(x + 49, y + 62 + barh - hi, 3, hi > lo ? hi - lo : 1, C_MOD);
            }
        }
    }
}

/* podpowiedź stron silników (EDIT / ENV / LFO): SELECT strony, przycisk sekcji MORE / powrót / pierwsza strona, SEL
 * trzymany = klawisze; po przytrzymaniu SEL (gdy świecą) — co robią klawisze */
static char page_buf[64];
static void page_hint(int more)
{
    const char *sec = ui.grp == G_ENV ? "ENV" : ui.grp == G_LFO ? "LFO" : "EDIT";
    int draw = set.eng == ENG_WT && (ui.page == WP_DRAW1 || ui.page == WP_DRAW2);
    page_buf[0] = 0;
    if (ui.gest) {                           /* (v0.65) LFO / ENV trzymany: gałki = AMP ENV / LFO 1, OP = strona MOD */
        text_cat(page_buf, "K1-K4: ");
        int32_t gp = ui.gest - 1u == B_LFO ? (grp_n(G_LFO) ? grp_at(G_LFO, 0) : -1) : ui.gest - 1u == B_ENV ? env_page() : E->quick;
        text_cat(page_buf, gp >= 0 ? pg_name(gp) : "-");   /* (v0.91) prawdziwa strona gestu (było zawsze AMP ENV / LFO 1) */
        if (gest_keys())
            text_cat(page_buf, "   OP: MOD PAGE");
    } else if (sel_lit()) {                  /* klawisze świecą: ich opis */
        int32_t l, n = 0, pg;
        char nm[12], nn[4];
        nm[0] = 0;
        for (l = K_OP1; l <= K_OP6; l++)     /* "OSC 1-4": wspólna nazwa stron klawiszy (bez numeru), różne: PAGES */
            if ((pg = E->label_page(l, G_ALL, ui.page)) >= 0) {   /* (koło: następna strona) */
                const char *s = E->page_name[pg];
                char w[12];
                uint32_t j;
                for (j = 0; s[j] && s[j] != ' ' && (s[j] < '0' || s[j] > '9') && j < 8u; j++)
                    w[j] = s[j];
                w[j] = 0;
                if (!n++)
                    text_cat(nm, w);
                else if (!text_eq(nm, w))
                    nm[0] = '*';
            }
        if (draw)
            text_cat(page_buf, "KNOBS: Q   ");
        if (n) {
            text_cat(page_buf, nm[0] == '*' ? "PAGES" : nm);
            text_cat(page_buf, " 1");
            if (n > 1) {
                int_text(n, nn, 0);
                text_cat(page_buf, "-");
                text_cat(page_buf, nn);
            }
            text_cat(page_buf, "   ");
        }
        if (E->label_page(K_PITCH, G_ALL, V_HOME) >= 0)
            text_cat(page_buf, "PITCH   ");
        if (E->label_page(K_GLOBAL, G_ALL, V_HOME) >= 0)
            text_cat(page_buf, "VOICE   ");
        text_cat(page_buf, "MONO / POLY");
    } else {
        if (more || has_more(ui.page)) {     /* (bez MORE: SELECT się nie mieści, a trzymanie sekcji to skrót) */
            text_cat(page_buf, "SELECT: PAGE   ");
            text_cat(page_buf, sec);
            text_cat(page_buf, more ? ": BACK   " : ": MORE   ");
        } else {
            text_cat(page_buf, "HOLD ");
            text_cat(page_buf, sec);
            text_cat(page_buf, ": PAGE 1   ");
        }
        text_cat(page_buf, draw ? "HOLD SEL: Q / KEYS" : "HOLD SEL: KEYS");
    }
    foot_hint = page_buf;
}

static void chord_graph(void)            /* klawiatura 3 oktawy od C: nuty akordu od C (klawisz biały) */
{
    static const uint8_t WK[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {1, 2, 4, 5, 6};
    const int32_t y = GRAPH_Y, h = GRAPH_H, ww = GW / 21;
    uint32_t m = chord_mask(xtc[set.eng], 60);   /* (od C) */
    int32_t o, i;
    for (o = 0; o < 3; o++)
        for (i = 0; i < 7; i++) {
            int32_t n = o * 12 + WK[i], x = GX + (o * 7 + i) * ww;
            cv_rect(x, y, ww - 1, h, !n ? C_WHITE : m >> n & 1u ? C_HI : C_LINE);
        }
    for (o = 0; o < 3; o++)
        for (i = 0; i < 5; i++) {
            int32_t n = o * 12 + BK[i], x = GX + (o * 7 + BX[i]) * ww - ww / 3;   /* BX: biały klawisz po prawej */
            cv_rect(x, y, ww * 2 / 3, h * 3 / 5, m >> n & 1u ? C_HI : C_BLACK);
        }
}

static void draw_page(void)
{
    const pdef_t *d[4];
    int16_t v[4];
    char t[24], txt[4][16];
    int32_t i, right = 232, id, x, tend;
    int vis, more = more_on();
    cv_begin(240, BODY_H, C_BLACK);
    if (ui.page == V_GLO && ui.glo_sec < 0) {   /* lista sekcji */
        cv_text(8, 4, &FONT_M, "GLOBAL", C_WHITE);
        for (i = 0; i < NGS; i++) {
            int on = i == ui.glo_cur, y = 34 + i * 24;
            if (on)
                cv_rect(4, y - 4, 232, 22, C_DIM);
            cv_text(12, y, &FONT_S, GS_NAME[i], on ? C_WHITE : C_GRAY);
            cv_text(232 - text_w(&FONT_XS, GS_HINT[i]), y + 2, &FONT_XS, GS_HINT[i], on ? C_HI : C_LINE);
        }
        body_blit();
        return;
    }
    if (ui.page == V_GLO && ui.glo_sec == GS_SYSTEM) {   /* SYSTEM: działania pod SEL */
        cv_text(8, 4, &FONT_M, "SYSTEM", C_WHITE);
        for (i = 0; i < NSY; i++) {
            int on = i == ui.sy_cur, y = 30 + i * 20;   /* sześć pozycji nad paskiem podpowiedzi (v1.1.0: + UPDATE MODE) */
            if (on)
                cv_rect(4, y - 2, 232, 20, C_DIM);
            cv_text(12, y, &FONT_S, SY_NAME[i], on ? C_WHITE : C_GRAY);
            if (i == SY_HINTS)
                cv_text(232 - text_w(&FONT_S, L_HINTS[glob.g[G_HINTS]]), y, &FONT_S, L_HINTS[glob.g[G_HINTS]], C_HI);
        }
        foot_hint = ui.sy_cur == SY_HINTS ? "SELECT: CHOOSE   SEL: ON / OFF   HOME: BACK"
                                          : "SELECT: CHOOSE   SEL: OPEN   HOME: BACK";
        body_blit();
        return;
    }
    if (ui.page == V_GLO) {                  /* sekcja: cztery pola na stronę, SELECT: strony */
        int32_t np = (glo_count(ui.glo_sec) + 3) / 4;
        cv_text(8, 4, &FONT_M, GS_NAME[ui.glo_sec], C_WHITE);
        if (np > 1) {
            char nn[8];
            int_text(ui.glo_page + 1, t, 0);
            text_cat(t, "/");
            int_text(np, nn, 0);
            text_cat(t, nn);
            cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
        }
        for (i = 0; i < 4; i++) {
            int32_t g = glo_item(i);
            d[i] = g >= 0 ? &GDEF[g] : 0;
            v[i] = g >= 0 ? GV(g) : 0;
            txt[i][0] = 0;
            if (g >= 0)
                def_text(&GDEF[g], -1, v[i], txt[i]);
        }
        draw_cols(d, v, txt, 0);
        body_blit();
        return;
    }
    vis = glob.g[G_VPAGES] && (ui.page == PG_CHORD || E->has_graph(ui.page));
    x = 8;
    if ((id = drum_page_lane()) >= 0) {      /* (v0.88, decyzja użytkownika 2026-11-02) perkusja: numer dźwięku przed tytułem */
        int_text(id + 1, t, 0);
        x = cv_text(x, 4, &FONT_M, t, C_HI) + 6;
    }
    x = cv_text(x, 4, &FONT_M, pg_name(ui.page), C_WHITE);
    if (more)
        x = cv_text(x + 6, 4, &FONT_M, "MORE", C_HI);
    else if (has_more(ui.page))              /* „+”: jest MORE (przycisk sekcji) */
        x = cv_text(x + 4, 4, &FONT_M, "+", C_DIM);
    tend = x + 8;                            /* (v0.90) koniec tytułu: opcja SEL nie wchodzi na niego */
    if (!ui.quick && !more) {                /* numer w sekcji: 3/8, 1/2; z HOME 1/12 */
        char nn[8];
        int_text(ui.idx + 1, t, 0);
        text_cat(t, "/");
        int_text(grp_n(ui.grp), nn, 0);
        text_cat(t, nn);
        right -= text_w(&FONT_XS, t);
        cv_text(right, 12, &FONT_XS, t, C_GRAY);
        right -= 8;
    }
    if (ui.page == PG_CHORD) {               /* CHORD: SEL = LEARN (trzymane klawisze) */
        x = right - text_w(&FONT_XS, "SEL") - text_w(&FONT_XS, "LEARN") - 4;
        x = cv_text(x, 12, &FONT_XS, "SEL", C_DIM);
        cv_text(x + 4, 12, &FONT_XS, "LEARN", C_HI);
    } else if (!more && (id = E->sel(ui.page)) >= 0) {   /* opcja pod SEL: "SEL KEY TRK 1/3", do prawej przed numerem */
        /* (v0.90, decyzje użytkownika 2026-11-03) za długie obok tytułu: najpierw mniejsza czcionka, nazwa opcji znika dopiero, gdy
         * i tak się nie mieści; „SEL” (przycisk) i wartość zostają zawsze */
        const char *nm = E->def(id)->name;
        const felucca_font_t *f = &FONT_XS;
        int32_t sp = 4, k;
        value_text(id, pget(id), t);
        for (k = 0; k < 3; k++) {
            f = k ? &FONT_T : &FONT_XS;
            sp = k ? 3 : 4;
            if (k == 2)
                nm = "";
            x = right - text_w(f, "SEL") - text_w(f, nm) - text_w(f, t) - (nm[0] ? 2 * sp : sp);
            if (x >= tend)
                break;
        }
        x = cv_text(x, f == &FONT_T ? 13 : 12, f, "SEL", C_DIM) + sp;
        if (nm[0])
            x = cv_text(x, f == &FONT_T ? 13 : 12, f, nm, C_GRAY) + sp;
        cv_text(x, f == &FONT_T ? 13 : 12, f, t, C_HI);
    }
    for (i = 0; i < 4; i++) {
        id = knob_id(i);
        d[i] = id >= 0 ? E->def(id) : 0;
        v[i] = id >= 0 ? (int16_t)pget(id) : 0;
        txt[i][0] = 0;
        if (id >= 0)
            value_text(id, v[i], txt[i]);
        col_mod[i] = (int8_t)(id >= 0 && E->moddepth ? E->moddepth(id) : 0);
    }
    if (ui.page == PG_CHORD) {               /* K1: rodzaj akordu */
        d[0] = &CH_DEF;
        v[0] = (int16_t)(xtc[set.eng][0] & 15u);
        def_text(&CH_DEF, -1, v[0], txt[0]);
        d[1] = &CV_DEF;                      /* (v1.1.0) K2: VOICING */
        v[1] = (int16_t)(xtc[set.eng][0] >> 4 & 3u);
        def_text(&CV_DEF, -1, v[1], txt[1]);
    }
    if (hints()) {                           /* wyższy pasek: niższe kolumny / wykres */
        page_hint(more);
        if (!vis)
            col_h = BODY_H - 36 - 10 - FOOT_TALL;
        graph_h = 76 - 20;
    }
    draw_cols(d, v, txt, vis);
    col_h = -1;
    for (i = 0; i < 4; i++)
        col_mod[i] = 0;
    if (vis && ui.page == PG_CHORD)
        chord_graph();
    else if (vis)
        draw_graph(ui.page);
    graph_h = 76;
    body_blit();
}

static void battery(int32_t x, int32_t y)   /* ikona baterii (plat_battery 0..3), USB: zasilanie z kabla */
{
    int b = plat_battery(), i;
    if (b == 4) {
        cv_text(x + 22 - text_w(&FONT_XS, "USB"), y, &FONT_XS, "USB", C_GRAY);
        return;
    }
    cv_rect(x, y + 2, 20, 10, C_GRAY);
    cv_rect(x + 1, y + 3, 18, 8, C_BLACK);
    cv_rect(x + 20, y + 5, 2, 4, C_GRAY);
    for (i = 0; i < 3 && b > i; i++)
        cv_rect(x + 2 + i * 6, y + 4, 5, 6, b <= 1 ? C_WHITE : C_HI);
}

/* podpowiedzi na dolnym pasku (decyzja użytkownika 2026-10-26: wszystkie podpowiedzi tam, jak na HOME); strona
 * ustawia foot_hint w trakcie rysowania, footer() go zużywa */

/* podpowiedź na dolnym pasku: części oddzielone trzema spacjami od lewej do prawej krawędzi (8 px), jak na HOME;
 * jedna część albo za długie: na środku */
static void hint_line(int32_t y, const char *s, uint16_t c)
{
    const char *p[6];
    char part[6][40];
    int32_t n = 0, sum = 0, i, x = 8, gap;
    while (*s && n < 6) {
        int32_t j = 0;
        while (*s == ' ')
            s++;
        p[n] = part[n];
        for (; *s && !(s[0] == ' ' && s[1] == ' ' && s[2] == ' '); s++)   /* za długa część: ucięta, reszta nie
                                                                            * staje się osobną (v0.37: "O    K") */
            if (j < 39)
                part[n][j++] = *s;
        part[n][j] = 0;
        if (j)
            sum += text_w(&FONT_XS, part[n++]);
    }
    gap = n > 1 ? (224 - sum) / (n - 1) : -1;
    if (gap < 8) {                               /* jedna część albo ciasno: całość na środku */
        char all[96];
        all[0] = 0;
        for (i = 0; i < n; i++) {
            if (i)
                text_cat(all, "   ");
            text_cat(all, p[i]);
        }
        center(y, &FONT_XS, all, c);
        return;
    }
    for (i = 0; i < n; i++) {
        if (i == n - 1)                          /* ostatnia równo do prawej krawędzi */
            x = 232 - text_w(&FONT_XS, p[i]);
        x = cv_text(x, y, &FONT_XS, p[i], c) + gap;
    }
}

static void footer(void)
{
    static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char t[16], o[6];
    int32_t note = eng_held_note(), x;
    int home = ui.page == V_HOME && hints() && !ui.mode_until, tall;
    if (!hints() && !foot_keep)              /* HINTS OFF: zostaje tylko stan (CPU, SURE?) */
        foot_hint = 0;
    tall = home || foot_hint || lo_note();   /* bez podpowiedzi: sam wiersz informacji */
    cv_begin(240, tall ? 54 : 26, C_BLACK);
    cv_rect(0, 0, 240, 1, C_LINE);
    if (mt_ui() && plat_ms() < ui.msg_until) {   /* (v0.64.3) MULTI: komunikaty tu (u góry zostaje pasek ścieżek) */
        x = cv_text(8, 4, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 4, &FONT_B, ui.msg[1], mt_red >= 0 ? RGB(255, 60, 50) : C_HI);   /* (v0.85) powód czerwonego silnika */
    } else {
        x = cv_text(8, 8, &FONT_XS, "OCT", C_GRAY);
        int_text(set.octave, t, 1);
        cv_text(x + 4, 8, &FONT_XS, t, C_HI);
        value_text(E->mode, pget(E->mode), t);
        cv_text(52, 8, &FONT_XS, t, C_HI);
        if (note >= 0) {                     /* grana nuta; bez niej: to, co zmienia granie (TRANSPOSE, LOCAL OFF) */
            t[0] = 0;
            text_cat(t, N[note % 12]);
            int_text(note / 12 - 1, o, 0);
            text_cat(t, o);
            cv_text(92, 8, &FONT_XS, t, C_WHITE);
        } else if (!glob.g[G_LOCAL]) {
            cv_text(92, 8, &FONT_XS, "LOCAL OFF", C_WHITE);
        } else if (glob.g[G_TRANSP]) {
            x = cv_text(92, 8, &FONT_XS, "TR", C_GRAY);
            int_text(glob.g[G_TRANSP], t, 1);
            cv_text(x + 4, 8, &FONT_XS, t, C_HI);
        }
        int_text((int32_t)vol_pct(), t, 0);
        text_cat(t, "%");
        x = cv_text(158, 8, &FONT_XS, "VOL", C_GRAY);
        cv_text(x + 4, 8, &FONT_XS, t, C_HI);
        battery(211, 7);
    }
    if (lo_note()) {
        hint_line(28, LO_NOTE, RGB(255, 60, 50));
    } else if (home && ui.home_hold) {       /* (v0.86) HOME trzymany: co robi */
        hint_line(28, mt_ui() ? "KEEP HOLDING: SOLO" : "KEEP HOLDING: MULTI   PRESETS: ALL", C_DIM);
    } else if (home && (ui.gest || sel_lit())) {   /* (v0.34.2) SEL trzymany: co robią; v0.35 gest */
        page_hint(0);                        /* klawisze, które świecą (jak na stronach silnika) */
        hint_line(28, foot_hint, C_DIM);
    } else if (home && mt_ui()) {            /* (v0.86) jak strona FX */
        hint_line(28, glob.g[G_MT0 + mt_at_cur()] >= NENG ? "SEL: LIST   ALGO: ENGINE   HOME: SCENES" : "SEL: MUTE   ALGO: ENGINE   HOME: SCENES", C_DIM);   /* (v0.90) sceny; HOLD SEL: przy SEL */
    } else if (home) {
        cv_text(8, 28, &FONT_XS, "HOME: ENGINE", C_DIM); /* podpowiedzi: te same marginesy co reszta (8 px) */
        cv_text(232 - text_w(&FONT_XS, "HOLD HOME: MULTI"), 28, &FONT_XS, "HOLD HOME: MULTI", C_DIM);
    } else if (foot_hint) {
        hint_line(28, foot_hint, foot_col ? foot_col : C_DIM);
    }
    foot_hint = 0;
    foot_col = 0;
    foot_keep = 0;
    cv_blit(0, tall ? 186 : 214);
}

static void draw_save(void)            /* okno SAVE: NADPISZ / NOWY */
{
    static const char *const OPT[2] = {"OVERWRITE", "NEW PRESET"};
    char t[24], n[8];
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "SAVE", C_WHITE);
    preset_name(CUR, t);
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    for (i = 0; i < 2; i++) {
        int on = ui.save_new == (uint32_t)i;
        const char *o = text_w(&FONT_B, OPT[i]) <= 216 ? OPT[i] : "NEW";   /* za długie: samo NEW */
        cv_rect(8, 34 + i * 40, 224, 34, on ? C_DIM : C_LINE);
        center(40 + i * 40, &FONT_B, o, on ? C_WHITE : C_GRAY);
    }
    t[0] = 0;                                /* ile miejsc USER zajętych (wspólne dla silników) */
    text_cat(t, "USER ");
    int_text(user_count(), n, 0);
    text_cat(t, n);
    text_cat(t, " / 128 USED");
    center(112, &FONT_XS, t, C_GRAY);
    foot_hint = "SELECT: CHOOSE   SEL: OK   HOME: CANCEL";
    body_blit();
}

static void draw_name(void)            /* okno nazwy: 12 kratek, kursor podkreślony */
{
    char c[2] = {0, 0}, t[24], n[8];
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "NAME", C_WHITE);
    t[0] = 0;
    if (ui.save_id >= ID_USER) {             /* które miejsce USER: "USER 05 / 128" */
        text_cat(t, "USER ");
        user_num(n, ui.save_id - ID_USER + 1);
        text_cat(t, n);
        text_cat(t, " / 128");
    } else {
        text_cat(t, "12 MAX");
    }
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    for (i = 0; i < NAME_LEN; i++) {
        int32_t x = 12 + i * 18;
        int on = i == ui.cur;
        cv_rect(x, 46, 16, 34, on ? C_DIM : C_LINE);
        c[0] = ui.name[i];
        cv_text(x + (16 - text_w(&FONT_M, c)) / 2, 51, &FONT_M, c, on ? C_WHITE : C_GRAY);
        if (on)
            cv_rect(x, 82, 16, 3, C_HI);
    }
    center(100, &FONT_XS, "SELECT: LETTER   OCT- / OCT+: MOVE", C_GRAY);
    foot_hint = "SEL: OK   HOME: CANCEL";
    body_blit();
}

/* okno ALL PRESETS: bieżący w środku, wokół sąsiedzi; silnik z lewej, sekcja bieżącego z prawej u góry.
 * v0.37 (użytkownik 2026-10-28): 7 wierszy (było 5), między sekcjami ich nazwy (nowy silnik: jego nazwa) */
enum { AP_ROWS = 7, AP_MID = 3 };
static void draw_allp(void)
{
    char t[24];
    int32_t row[AP_ROWS], r, a, x;           /* row >= 0: preset; ~id: nagłówek sekcji presetu id */
    cv_begin(240, BODY_H, C_BLACK);
    x = cv_text(8, 4, &FONT_M, "ALL PRESETS", C_WHITE);
    sec_label(CUR, t);                       /* v0.36: sekcja bieżącego (SELECT skacze po sekcjach) */
    if (232 - text_w(&FONT_XS, t) < x + 8 && ps_of(CUR) == PS_FM1) {   /* długa sekcja FM1: sama jej nazwa */
        t[0] = 0;
        text_cat(t, FM1_SEC[fm1_sec(CUR)].name);
    }
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);   /* v0.55: kolor motywu tylko nazwy silników */
    row[AP_MID] = CUR;
    for (r = AP_MID - 1, a = CUR; r >= 0; a = row[r--]) {   /* w górę: nagłówek nad pierwszym presetem sekcji */
        row[r] = preset_walk(a, -1, -1);
        if (sec_key(row[r]) != sec_key(a)) {
            row[r] = ~a;
            if (--r < 0)
                break;
            row[r] = preset_walk(a, -1, -1);
        }
    }
    for (r = AP_MID + 1, a = CUR; r < AP_ROWS; a = row[r++]) {   /* w dół */
        row[r] = preset_walk(a, 1, -1);
        if (sec_key(row[r]) != sec_key(a)) {
            row[r] = ~row[r];
            if (++r >= AP_ROWS)
                break;
            row[r] = ~row[r - 1];
        }
    }
    for (r = 0; r < AP_ROWS; r++) {
        int32_t y = 28 + r * 18, id = row[r];
        if (id < 0) {                        /* nagłówek: nazwa sekcji, kreska do prawej krawędzi */
            id = ~id;
            if (ps_of(id) == PS_INIT) {
                t[0] = 0;
                text_cat(t, ENG[preset_eng(id)].name);
            } else {
                sec_label(id, t);
            }
            x = cv_text(10, y + 1, &FONT_XS, t, ps_of(id) == PS_INIT ? C_HI : C_GRAY);   /* v0.55: sekcje szare */
            cv_rect(x + 6, y + 7, 226 - x, 1, C_LINE);
            continue;
        }
        if (r == AP_MID)
            cv_rect(4, y - 3, 232, 20, C_DIM);
        cv_text(10, y, &FONT_XS, ENG[preset_eng(id)].name, r != AP_MID ? C_LINE : C_HI);
        preset_name(id, t);
        cv_text(52, y, r != AP_MID ? &FONT_XS : &FONT_S, t, r != AP_MID ? C_GRAY : C_WHITE);
    }
    if (ui.allp_del) {                       /* (v1.0.2) okno DELETE?: na środku, nazwa presetu */
        cv_rect(20, 64, 200, 70, C_DIM);
        center(74, &FONT_M, "DELETE?", C_WHITE);
        preset_name(CUR, t);
        center(104, &FONT_S, t, C_HI);
    }
    foot_hint = ui.allp_del ? "SURE?  SEL: YES   HOME: NO"
              : CUR >= ID_USER && CUR < ID_SYX ? "SELECT: SECTION   SEL: OK   HOLD SEL: DELETE"
              : "SELECT: SECTION   ALGO: ENGINE   SEL: OK";
    foot_keep = ui.allp_del;                 /* pytanie przed kasowaniem: zawsze (jak FACTORY RESET) */
    foot_col = ui.allp_del ? C_WHITE : 0;
    body_blit();
}

static void big_text(const char *s, int32_t cy, float maxw, float maxh, uint16_t col)   /* FONT_L powiększony na płótno */
{
    const felucca_font_t *f = &FONT_L;
    float sc = fm_minf(maxw / (float)text_w(f, s), 6.0f), x0;
    int32_t i, top = f->h, bot = -1, y0;
    uint32_t cr = col >> 11, cg = (col >> 5) & 63u, cb = col & 31u;
    for (i = 0; s[i]; i++) {                 /* zarys liter: środek optyczny (jak tło HOME) */
        uint32_t gi = glyph(f, (uint8_t)s[i]);
        int32_t gx, gy;
        for (gy = 0; gy < (int32_t)f->h; gy++)
            for (gx = 0; gx < f->bw[gi]; gx++)
                if (glyph_a(f, gi, gx, gy) > 7.0f) {
                    top = gy < top ? gy : top;
                    bot = gy > bot ? gy : bot;
                }
    }
    if (bot < top) {
        top = 0;
        bot = f->h - 1;
    }
    sc = fm_minf(sc, maxh / (float)(bot - top + 1));
    x0 = (240.0f - (float)text_w(f, s) * sc) * 0.5f;
    y0 = (int32_t)((float)cy - (float)(bot - top + 1) * sc * 0.5f - (float)top * sc);
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s);
        int32_t bw = f->bw[gi], ox, oy, X0 = (int32_t)(x0 - (float)f->pad * sc);
        for (oy = (int32_t)((float)top * sc); oy <= (int32_t)((float)(bot + 1) * sc); oy++)
            for (ox = 0; ox < (int32_t)((float)bw * sc) + 1; ox++) {
                float sx = ((float)ox + 0.5f) / sc - 0.5f, sy = ((float)oy + 0.5f) / sc - 0.5f, fx, fy, a;
                int32_t ix = (int32_t)fm_floorf(sx), iy = (int32_t)fm_floorf(sy), px = X0 + ox, py = y0 + oy;
                uint32_t k, bg, A;
                if (px < 0 || px >= (int32_t)cv_w || py < 0 || py >= (int32_t)cv_h)
                    continue;
                fx = sx - (float)ix;
                fy = sy - (float)iy;
                a = (glyph_a(f, gi, ix, iy) * (1.0f - fx) + glyph_a(f, gi, ix + 1, iy) * fx) * (1.0f - fy)
                  + (glyph_a(f, gi, ix, iy + 1) * (1.0f - fx) + glyph_a(f, gi, ix + 1, iy + 1) * fx) * fy;
                A = (uint32_t)(fm_clampf((a * (1.0f / 15.0f) - 0.5f) * sc * 1.3f + 0.5f, 0.0f, 1.0f) * 32.0f);
                if (!A)
                    continue;
                k = (uint32_t)py * cv_w + (uint32_t)px;
                bg = swap16(cv_px[k]);
                cv_px[k] = swap16((uint16_t)(((cr * A + (bg >> 11) * (32u - A)) >> 5) << 11
                                  | ((cg * A + ((bg >> 5) & 63u) * (32u - A)) >> 5) << 5
                                  | ((cb * A + (bg & 31u) * (32u - A)) >> 5)));
            }
        x0 += (float)f->adv[gi] * sc;
    }
}


static void draw_mod(void)             /* (v0.65) strona MOD: 4 cele źródła, K1-K4 = ilość, SELECT = strony */
{
    static pdef_t md[4];
    const pdef_t *d[4];
    int16_t v[4];
    char txt[4][16], t[24], n[8];
    const char *sn = "";
    int32_t k, np = (ui.mod_n + 3) / 4, a0 = mod_amt0();   /* ilość w matrycy (-1: stałe ścieżki) */
    if (a0 < 0 && mod_skmx() >= 0)
        a0 = mod_skmx() + 2;                     /* (v0.65) matryca na szkielecie: zakres ilości slotu 1 */
    cv_begin(240, BODY_H, C_BLACK);
    E->gest(ui.mod_src, -1, GEST_ASK, &sn);
    k = cv_text(8, 4, &FONT_M, "MOD", C_WHITE);
    cv_text(k + 8, 4, &FONT_M, sn, C_HI);
    int_text(ui.mod_pg + 1, t, 0);
    text_cat(t, " / ");
    int_text(np, n, 0);
    text_cat(t, n);
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    for (k = 0; k < 4; k++) {
        int32_t j = ui.mod_pg * 4 + k, id, a, amt;
        d[k] = 0;
        v[k] = 0;
        txt[k][0] = 0;
        if (j >= ui.mod_n)
            continue;
        id = mod_ids[j];
        a = E->gest(ui.mod_src, id, GEST_ASK, 0);     /* stałe ścieżki: ich ilość; matryca: połączenie albo 0 */
        amt = a > 0 ? a : a0;                           /* (niepołączony w matrycy: zakres ilości, wartość 0) */
        if (amt < 0)
            continue;
        md[k] = *E->def(amt);
        md[k].name = mod_name(j);
        d[k] = &md[k];
        v[k] = (int16_t)(a > 0 ? pget(a) : 0);
        value_text(amt, v[k], txt[k]);
    }
    col_y = 34;
    draw_cols(d, v, txt, 0);
    col_y = -1;
    foot_hint = ui.mod_btn == B_ENV ? "SELECT: PAGE   K1-K4: AMOUNT   ENV: CLOSE" : "SELECT: PAGE   K1-K4: AMOUNT   LFO: CLOSE";
    body_blit();
}

static void draw_engp(void)            /* okno ENGINE: duża nazwa silnika (jak na HOME), pod nią jego brzmienie */
{
    char t[24], n[8];
    const int32_t at = eng_at(set.eng);
    const char *nx = ENG[ENG_ORDER[(at + 1) % NENG]].name;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, EG_NAME[ENG_GRP[set.eng]], C_WHITE);   /* v0.35: rodzaj silnika */
    int_text(at + 1, t, 0);                  /* miejsce wśród wszystkich silników */
    text_cat(t, " / ");
    int_text(NENG, n, 0);
    text_cat(t, n);
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    cv_text(8, 34, &FONT_XS, ENG[ENG_ORDER[(at + NENG - 1) % NENG]].name, C_DIM);   /* sąsiedzi: małe nazwy (v0.86: w kółko) */
    cv_text(232 - text_w(&FONT_XS, nx), 34, &FONT_XS, nx, C_DIM);
    big_text(E->name, 120 - 32, 220.0f, 60.0f, C_HI);   /* środek nazwy na środku ekranu (120), jak na HOME */
    if (ENG_GRP[set.eng] == EG_DRUM)          /* (v0.50) perkusja: nad nazwą małe „DRUMS” (decyzja użytkownika 2026-10-26) */
        center(120 - 32 - 52, &FONT_M, "DRUMS", C_HI);   /* (czcionka tej rodziny, wyraźna w małym rozmiarze) */
    preset_name(preset_now(), t);
    center(126, &FONT_S, t, C_WHITE);
    foot_hint = "SELECT: ENGINE   PRESETS: SOUND   SEL: OK";
    body_blit();
}
static void draw_mode(void)            /* (v0.86) HOME trzymany: tryb zmieniony, napis przez chwilę; MODE w kolorze motywu, nazwa w drugim odcieniu */
{
    const int m = mt_ui();
    const int32_t c = glob.g[G_COLOR];
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "MODE", m ? OGS_PAL_MT[c][1] : OGS_PAL[c][1]);   /* (v0.88) kolor tego trybu, jak nazwa */
    big_text(m ? "MULTI" : "SOLO", 120 - 32, 220.0f, 60.0f, m ? OGS_PAL_MT[c][1] : OGS_PAL[c][1]);
    center(126, &FONT_S, m ? "DRUMS + 3 TRACKS" : "ONE ENGINE + FX", C_WHITE);
    foot_hint = m ? "HOLD HOME: SOLO" : "HOLD HOME: MULTI";
    body_blit();
}
static void draw_mtl(void)             /* (v0.86) lista silników ścieżki: jak lista efektów; czerwone: nie zagrają obok reszty, z powodem */
{
    int8_t r[MTL_MAX];
    const int32_t t = mt_at_cur(), n = mtl_rows(t, r), cur = mtl_at();
    const char *g = cur >= NENG ? "CHOOSE ENGINE" : EG_NAME[ENG_GRP[cur]];
    char s[24] = {'T', (char)('1' + t), 0};
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, s, C_WHITE);
    cv_text(232 - text_w(&FONT_XS, g), 12, &FONT_XS, g, cur >= NENG ? C_GRAY : C_HI);   /* z prawej u góry: grupa kursora */
    for (i = 0; i < MTL_ROWS && ui.mtl_top + i < n; i++) {
        const int32_t w = ui.mtl_top + i, y = 28 + i * 18, e = r[w];
        const int on = w == ui.mtl_cur, red = e < MTL_HDR && !mt_ok(t, e, glob.g + G_MT0);
        const uint16_t col = red ? (on ? RGB(150, 0, 0) : RGB(255, 60, 50)) : on ? C_BLACK : C_WHITE;
        if (e >= MTL_HDR) {                  /* nagłówek grupy: nazwa w kolorze motywu, kreska do prawej */
            const int32_t x = cv_text(10, y + 1, &FONT_XS, EG_NAME[e - MTL_HDR], C_HI);
            cv_rect(x + 6, y + 7, 226 - x, 1, C_LINE);
            continue;
        }
        if (on)
            cv_rect(4, y - 2, 232, 18, C_HI);
        cv_text(12, y, &FONT_S, mw_name(e), col);
        s[0] = 0;
        if (red)
            mt_why(t, e, s);
        else if (e == glob.g[G_MT0 + t])
            text_cat(s, "HERE");
        cv_text(228 - text_w(&FONT_XS, s), y + 2, &FONT_XS, s, red ? col : on ? C_BLACK : C_GRAY);
    }
    foot_hint = "SELECT: ENGINE   ALGO: GROUP   SEL: OK";
    body_blit();
}
static void draw_scn(void)             /* (v0.86) lista scen MULTI (v0.90: HOME na głównym ekranie): numer, silniki ścieżek */
{
    char t[40], n[8];
    int32_t i, k;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "SCENES", C_WHITE);
    int_text(ui.scn_cur + 1, t, 0);
    text_cat(t, " / ");
    int_text(SCN_N, n, 0);
    text_cat(t, n);
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    for (i = 0; i < SCN_N; i++) {
        const scn_t *c = &glob.scn[i];
        const int on = i == ui.scn_cur;
        const int32_t y = 27 + i * 16;
        if (on)
            cv_rect(4, y - 1, 232, 16, C_HI);
        int_text(i + 1, n, 0);
        cv_text(12, y + 1, &FONT_XS, n, on ? C_BLACK : C_GRAY);
        t[0] = 0;
        for (k = 0; c->ok && k < MT_N; k++)
            if (c->eng[k] < NENG) {
                if (t[0])
                    text_cat(t, "  ");
                text_cat(t, ENG[c->eng[k]].name);
            }
        cv_text(30, y + 1, &FONT_XS, c->ok ? t : "EMPTY", on ? C_BLACK : c->ok ? C_WHITE : C_DIM);
    }
    foot_hint = "SELECT: SCENE   SEL: LOAD   SAVE: STORE";
    body_blit();
}

static void draw_import(void)          /* okno IMPORT: bank DX7 -> miejsce 1-32 */
{
    char t[24], n[16];
    int32_t c;
    int cz = ui.imp_cz;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, cz ? "CZ BANK" : "DX7 BANK", C_WHITE);
    t[0] = 0;
    if (cz) {                                /* CZ: ile brzmień przyszło (bez nazw) */
        int_text((int32_t)__builtin_popcount(cz_in_mask), t, 0);
        text_cat(t, " / 16 TONES");
    } else {
        for (c = 0; c < 10; c++)             /* pierwsze brzmienie banku */
            t[c] = syx_in[5 + 118 + c] >= ' ' && syx_in[5 + 118 + c] < 127 ? (char)syx_in[5 + 118 + c] : ' ';
        t[10] = 0;
    }
    cv_text(8, 34, &FONT_XS, syx_in_name[0] ? syx_in_name : cz ? "16 TONES" : "32 VOICES", C_GRAY);
    cv_text(232 - text_w(&FONT_XS, t), 34, &FONT_XS, t, C_GRAY);
    cv_rect(8, 54, 224, 40, C_DIM);
    t[0] = 0;
    text_cat(t, "TO BANK ");
    int_text(ui.imp_slot + 1, n, 0);
    text_cat(t, n);
    text_cat(t, cz ? " / 16" : " / 8");   /* (DX7: banki 1-8 od v0.19) */
    center(58, &FONT_B, t, C_WHITE);
    if ((cz ? syx.cz_used : syx.used) >> ui.imp_slot & 1u) {
        syx_bank_name(cz ? NSYX + ui.imp_slot : ui.imp_slot, n);
        t[0] = 0;
        text_cat(t, "REPLACES ");
        text_cat(t, n);
        center(102, &FONT_XS, t, C_HI);
    } else {
        center(102, &FONT_XS, "EMPTY", C_GRAY);
    }
    foot_hint = "SELECT: PLACE   SEL: OK   HOME: CANCEL";
    body_blit();
}

static uint16_t rgb_light(uint16_t c)   /* jaśniejszy odcień koloru motywu (zaznaczony slot): 40% do bieli */
{
    uint32_t r = c >> 11 & 31u, g = c >> 5 & 63u, b = c & 31u;
    r += (31u - r) * 2u / 5u;
    g += (63u - g) * 2u / 5u;
    b += (31u - b) * 2u / 5u;
    return (uint16_t)(r << 11 | g << 5 | b);
}

static float gr_show[4];                /* GR meter na ekranie: szybko w górę, w dół 1 dB na klatkę (bez migania) */

static void draw_gr(const fx_slot_t *c)  /* GR meter (jak Pro-C / OTT): COMPRESSOR jeden pasek, MULTIBAND trzy pasma */
{
    const int32_t y = 146, h = 8;
    int32_t i;
    for (i = 0; i < 4; i++) {
        float m = c->mode ? ogs_fx_meter[i] : 0.0f, a = fm_fabsf(m), b = fm_fabsf(gr_show[i]);
        gr_show[i] = a >= b || (m > 0.0f) != (gr_show[i] > 0.0f) ? m : gr_show[i] + (gr_show[i] > 0.0f ? -1.0f : 1.0f);
        if (fm_fabsf(gr_show[i]) < 0.1f)
            gr_show[i] = 0.0f;
    }
    if (c->type == FX_COMP) {                    /* ściszenie: od prawej w lewo, 0..24 dB */
        char t[16];
        int32_t w = (int32_t)(fm_minf(gr_show[0], 24.0f) * (176.0f / 24.0f));
        cv_text(8, y - 3, &FONT_XS, "GR", C_GRAY);
        cv_rect(28, y, 176, h, C_LINE);
        cv_rect(28 + 176 - w, y, w, h, C_HI);
        t[0] = 0;
        db_text(-gr_show[0], t);
        cv_text(232 - text_w(&FONT_XS, t), y - 3, &FONT_XS, t, gr_show[0] > 0.0f ? C_HI : C_GRAY);
        return;
    }
    for (i = 0; i < 3; i++) {                    /* MULTIBAND: od środka: w lewo ściszenie, w prawo podbicie (+-24 dB) */
        static const char *const B[3] = {"L", "M", "H"};
        int32_t x = 8 + i * 76, cx = x + 44, w = (int32_t)(fm_clampf(gr_show[1 + i], -24.0f, 24.0f) * (30.0f / 24.0f));
        cv_text(x, y - 3, &FONT_XS, B[i], C_GRAY);
        cv_rect(x + 12, y, 64, h, C_LINE);
        if (w < 0)
            cv_rect(cx + w, y, -w, h, C_HI);
        else
            cv_rect(cx, y, w, h, C_AMB);
        cv_rect(cx, y - 1, 1, h + 2, C_GRAY);
    }
}

/* (v0.92) EQ: krzywa (jak DRAW w WAVE) 20 Hz - 20 kHz, +-18 dB, punkty pasm 1-4 (kręcone jasne), pod nią widmo wyjścia na żywo
 * (ANALYZER: 32 pasma Goertzela na ostatnich 1024 próbkach oscyloskopu, okno trójkątne, w pętli ekranu — nie w dźwięku) */
#define EQG_N 56
static float eqg_x(float hz, int32_t w) { return fm_log2f(hz * (1.0f / 20.0f)) * (float)w * (1.0f / 9.9657843f); }
static void draw_eq(const fx_slot_t *c, int32_t y0, int32_t h)
{
    const uint8_t *x = fxc.x[set.eng] + FX_X_EQ;
    const int32_t X0 = 8, W = 224, cy = y0 + h / 2;
    const float dy = (float)(h / 2 - 2) / 18.0f;
    float hz[EQG_N], db[EQG_N];
    int32_t i, px = 0, py = 0, hot = -1;
    cv_rect(X0, y0, W, h, RGB(10, 10, 14));
    for (i = 0; i < 3; i++) {                    /* 100 Hz, 1 kHz, 10 kHz */
        static const float G[3] = {100.0f, 1000.0f, 10000.0f};
        cv_rect(X0 + (int32_t)eqg_x(G[i], W), y0, 1, h, C_LINE);
    }
    cv_rect(X0, cy, W, 1, C_LINE);
    if (!x[EQX_AN] && fx_on(c)) {                /* widmo: słupki od dołu, -60 .. 0 dB */
        const uint32_t w = scope_w, N = 1024u;
        for (i = 0; i < 32; i++) {
            const float f = 30.0f * fm_exp2f((float)i * (9.06f / 31.0f)), co = 2.0f * fm_cosf(6.2831853f * f / OGS_FS);
            float s1 = 0.0f, s2 = 0.0f, m, d;
            uint32_t k;
            int32_t bh, bx;
            for (k = 0; k < N; k++) {
                const float wv = 1.0f - fm_fabsf((float)k * (2.0f / (float)N) - 1.0f);
                const float s0 = (float)scope[(w - N + k) & (SC_LEN - 1u)] * (wv / 32767.0f) + co * s1 - s2;
                s2 = s1;
                s1 = s0;
            }
            m = s1 * s1 + s2 * s2 - co * s1 * s2;
            d = 10.0f * 0.30103f * fm_log2f(fm_maxf(m, 1e-12f) * (16.0f / ((float)N * (float)N)));
            bh = (int32_t)(fm_clampf((d + 60.0f) / 60.0f, 0.0f, 1.0f) * (float)(h - 2));
            bx = X0 + (int32_t)eqg_x(f, W) - 3;
            if (bh > 0)
                cv_rect(bx < X0 ? X0 : bx, y0 + h - 1 - bh, 6, bh, C_LINE);
        }
    }
    for (i = 0; i < EQG_N; i++)
        hz[i] = 20.0f * fm_exp2f((float)i * (9.9657843f / (float)(EQG_N - 1)));
    ogs_fx_eq_curve(c, fxc.x[set.eng], hz, db, EQG_N);
    for (i = 0; i < EQG_N; i++) {
        const int32_t qx = X0 + i * (W - 1) / (EQG_N - 1), qy = clamp(cy - (int32_t)(db[i] * dy), y0 + 1, y0 + h - 2);
        if (i)
            cv_line(px, py, qx, qy, C_HI);
        px = qx;
        py = qy;
    }
    if (ui.knob >= 0 && plat_ms() - ui.knob_at < 1500u) {   /* kręcone pasmo: z gałki (strony GAIN, FREQ, Q) */
        const int32_t s = fx_sj(c, ui.knob + ui.fx_more * 4);
        static const int8_t B[FX_NXE] = {2, 0, 2, 3, 0, 1, 2, 3, -1, -1, -1, -1, -1};
        hot = s < 0 ? -1 : s == 0 ? 0 : s == 1 || s == 2 ? 1 : s == 3 ? 3 : s >= FX_NP ? B[s - FX_NP] : -1;
    }
    for (i = 0; i < 4; i++) {                    /* punkty pasm */
        static const uint8_t GJ[4] = {0, 1, 0, 3};
        const float f = i == 1 ? 150.0f * fm_exp2f((float)c->p[2] * (5.3f / 127.0f))
                      : 20.0f * fm_exp2f((float)x[i == 0 ? EQX_F1 : i == 2 ? EQX_F3 : EQX_F4] * (9.9657843f / 127.0f));
        const float g = ((float)(i == 2 ? x[EQX_G3] : c->p[GJ[i]]) / 127.0f - 0.5f) * 24.0f;
        const int32_t bx = X0 + (int32_t)eqg_x(f, W), by = clamp(cy - (int32_t)(g * dy), y0 + 2, y0 + h - 3);
        cv_rect(bx - 2, by - 2, 5, 5, i == hot ? C_WHITE : C_GRAY);
        cv_rect(bx - 1, by - 1, 3, 3, i == hot ? C_HI : C_BLACK);
    }
}

static const char *fx_mt_why(const fx_slot_t *c)   /* (v0.83) MULTI: dlaczego włączony efekt nie gra (0: gra) */
{
    const int32_t t = glob.g[G_MTFOC];
    if (!mt_ui() || !c->mode || c->type >= FX_NTYPE)
        return 0;
    if (fx_is_mst(c))                    /* (v0.85) MASTER FX: ktoś wysyła, a ciężkie ścieżki zajęły pamięć */
        return mt_swant >> c->type & 1u && !ogs_fx_send_on(c == &glob.mfx.s[1]) ? "NO MEMORY" : 0;
    if (!(FX_TRACK_TYPES >> c->type & 1u))
        return "SOLO ONLY";
    return (mt_fxa[t] >> c->type & 1u) || (glob.g[G_MTMUTE] >> t & 1) ? 0 : "NO CPU";
}
static void draw_fx(void)              /* FX: łańcuch u góry (nazwy na tle motywu), pod nim gałki efektu (FX: MORE) */
{
    const fx_slot_t *c;
    const char *why;
    const int32_t nv = fx_view_n();
    pdef_t pd[4];
    const pdef_t *d[4];
    int16_t v[4];
    char txt[4][16], t[24], n[8];
    int32_t i, x, more = ui.fx_more, name_end, lo, mst;
    if (ui.fx_slot >= nv)                    /* (v0.85) MULTI: inny preset, mniej pozycji */
        ui.fx_slot = (uint8_t)(nv - 1);
    if (ui.fx_win + 3 >= nv)
        ui.fx_win = (uint8_t)(nv - 4);
    c = fx_cur();
    why = fx_mt_why(c);
    mst = fx_is_mst(c);
    cv_begin(240, BODY_H, C_BLACK);
    t[0] = 0;
    if (mst)                                 /* (v0.85) MASTER FX: M1, M2 */
        text_cat(t, "M");
    int_text(ui.fx_slot + 1, t + (mst ? 1 : 0), 0);   /* numer slotu przed nazwą */
    x = cv_text(8, 4, &FONT_M, t, mst ? C_HI : C_GRAY);
    lo = !mt_ui() && c->type < FX_NTYPE && (ogs_fx_lo() >> c->type & 1u);   /* (v0.31) gra w 16 bitach: nazwa czerwona */
    lo = lo || why;                          /* (v0.83) MULTI: nie gra: czerwona, z powodem */
    x = name_end = cv_text(x + 8, 4, &FONT_M, FX_NAME[c->type], lo ? RGB(255, 60, 50) : c->type == FX_NONE || fx_on(c) ? C_WHITE : C_GRAY);
    if (more >= fx_npages(c))                /* (TYPE zmienił liczbę stron) */
        more = ui.fx_more = 0;
    if (more)
        name_end = cv_text(x + 6, 4, &FONT_M, fx_pgname(c, more), C_HI);
    t[0] = 0;                                /* prawy górny róg: FX n / 6 (gdy jest miejsce; 16 bitów: 16-BIT), przed nim stan pod SEL */
    if (lo) {
        text_cat(t, why ? why : "16-BIT");
    } else if (!mst) {
        text_cat(t, "FX ");
        int_text(ui.fx_slot + 1, n, 0);
        text_cat(t, n);
        text_cat(t, " / ");
        int_text(nv, n, 0);
        text_cat(t, n);
    }
    x = 232 - text_w(&FONT_XS, t);
    if (x > name_end + 6)
        cv_text(x, 12, &FONT_XS, t, lo ? RGB(255, 60, 50) : C_GRAY);
    if (mst && !lo) {                        /* (v0.88) MASTER FX: T1-T4 przypięte (kolor) / odpięte (ciemne), wybrana biała */
        const uint32_t mk = c == &glob.mfx.s[1];
        char s[3] = {'T', '1', 0};
        for (i = 0; i < MT_N; i++)
            x -= text_w(&FONT_XS, "T1") + 4;
        for (i = 0, n[0] = 0, lo = x; i < MT_N; i++) {
            s[1] = (char)('1' + i);
            lo = cv_text(lo, 12, &FONT_XS, s, !mt_linked(i, mk) ? C_DIM : i == glob.g[G_MTFOC] ? C_WHITE : C_HI) + 4;
        }
        lo = 0;
    }
    if (!more && c->type != FX_NONE) {       /* stan pod SEL, jeśli nie wchodzi na nazwę */
        t[0] = 0;
        text_cat(t, FX_MODE[c->type][fx_on(c) ? c->mode : 0]);
        x -= 10 + text_w(&FONT_XS, t);
        if (x - 4 - text_w(&FONT_XS, "SEL") > name_end + 6) {
            cv_text(x, 12, &FONT_XS, t, fx_on(c) ? C_HI : C_GRAY);
            cv_text(x - 4 - text_w(&FONT_XS, "SEL"), 12, &FONT_XS, "SEL", C_DIM);
        }
    }
    for (i = 0; i < 4; i++) {                /* łańcuch: 4 z 6 widoczne (przewija się z kursorem) */
        const fx_slot_t *sc = fx_view(ui.fx_win + i);
        int32_t bx = 4 + i * 59, sl = ui.fx_win + i, on = sl == (int32_t)ui.fx_slot, ty = sc->type, ms = fx_is_mst(sc);
        int32_t play = ty != FX_NONE && fx_on(sc), off = on && ty != FX_NONE && !play;
        /* zaznaczony i gra: pełny kolor; zaznaczony i wyłączony: ramka koloru na ciemnym (widać, że nie gra); (v0.85) MASTER FX:
         * drugi odcień motywu i kreska pod spodem */
        const uint16_t *mp = OGS_PAL[glob.g[G_COLOR]];
        uint16_t bg = on ? rgb_light(ms ? mp[1] : C_HI) : play ? (ms ? mp[0] : C_AMB) : ty == FX_NONE ? C_BLACK : C_DIM;
        uint16_t fg = off ? C_WHITE : on || play ? C_BLACK : C_GRAY;
        cv_rect(bx, 30, 55, 30, ty == FX_NONE && !on ? C_LINE : bg);
        if (ty == FX_NONE && !on)
            cv_rect(bx + 1, 31, 53, 28, C_BLACK);
        if (off)
            cv_rect(bx + 2, 32, 51, 26, C_LINE);
        if (ty == FX_NONE) {                 /* pusty: jeden napis */
            cv_text(bx + (55 - text_w(&FONT_XS, "EMPTY")) / 2, 39, &FONT_XS, "EMPTY", on ? C_BLACK : C_LINE);
            continue;
        }
        if (ms)                              /* wspólna kreska pod slotami 1-2 */
            cv_rect(bx - (i && fx_is_mst(fx_view(sl - 1)) ? 4 : 0), 62, 55 + (i && fx_is_mst(fx_view(sl - 1)) ? 4 : 0), 3, C_WHITE);
        if (ty < FX_NTYPE && (mt_ui() ? fx_mt_why(sc) != 0 : (ogs_fx_lo() >> ty & 1u)))   /* 16 bitów: czerwony (ciemny na
                                                                                                * jasnym tle); (v0.83) MULTI: nie gra */
            fg = on || play ? RGB(150, 0, 0) : RGB(255, 60, 50);
        cv_text(bx + (55 - text_w(&FONT_XS, FX_SHORT[ty])) / 2, 33, &FONT_XS, FX_SHORT[ty], fg);
        cv_text(bx + (55 - text_w(&FONT_XS, FX_MODE[ty][fx_on(sc) ? sc->mode : 0])) / 2, 45, &FONT_XS, FX_MODE[ty][fx_on(sc) ? sc->mode : 0],
                off ? C_HI : on || play ? C_BLACK : C_LINE);
    }
    for (i = 0; i < 4; i++) {
        int32_t j = i + more * 4;
        txt[i][0] = 0;
        d[i] = 0;
        v[i] = 0;
        if (!fx_pname(c, j)[0])
            continue;
        pd[i].name = fx_pname(c, j);
        pd[i].lo = 0;
        pd[i].hi = j == fx_syncj(c) || j == fx_bendj(c) || eq_sw(c, j) || (c->type == FX_AWAH && fx_sj(c, j) >= AW_MODE && fx_sj(c, j) <= AW_TRIG) ? 1
                 : j == fx_typep(c->type) ? FX_NMODE[c->type] - 1 : c->type == FX_DELAY && j == FX_DL_STYLE ? FX_DL_NSTYLE - 1 : 127;
        pd[i].def = j == fx_syncj(c) ? c->type == FX_STUTTER : j == fx_bendj(c) ? 1 : c->type >= FX_NTYPE ? 0
                  : fx_sj(c, j) < FX_NP ? FXDEF[c->type][fx_sj(c, j)] : FXXDEF[fx_xoff(c->type) + fx_sj(c, j) - FX_NP];
        pd[i].fmt = F_NUM;
        pd[i].list = 0;
        d[i] = &pd[i];
        v[i] = j == fx_syncj(c) ? (int16_t)fx_sync_on(c->type) : j == fx_bendj(c) ? (int16_t)!c->p[j] : *fx_par(c, j);
        fx_text(c, j, txt[i]);
    }
    if (c->type == FX_NONE) {
        center(104, &FONT_S, "EMPTY SLOT", C_GRAY);
    } else {
        int gr = c->type == FX_COMP || c->type == FX_MBAND, eqg = c->type == FX_EQ, tall = !hints() && plat_cpu_pct() < 80u;
        col_red = (c->type == FX_DELAY && !more && (!mt_ui() || ogs_fx_send_on(1)) && ogs_fx_delay_red(c))     /* TIME poza pełną jakością: czerwony */
               || (c->type == FX_STUTTER && !more && ogs_fx_stutter_red(c)) ? 1u : 0u;   /* (v0.43) DIV też */
        col_y = 68;
        col_h = eqg ? 56 : gr ? 74 : 86;         /* COMPRESSOR, MULTIBAND: pod gałkami GR meter; (v0.92) EQ: krzywa */
        if (tall && !eqg)                        /* HINTS OFF: krótki pasek, wyższe pola (EQ: wyższa krzywa) */
            col_h += FOOT_TALL;
        draw_cols(d, v, txt, 0);
        col_y = col_h = -1;
        col_red = 0;
        if (gr)
            draw_gr(c);
        if (eqg)
            draw_eq(c, 68 + 56 + 2, 28 + (tall ? FOOT_TALL : 0));
    }
    if (plat_cpu_pct() >= 80u) {             /* ciężki łańcuch: ostrzeżenie zamiast podpowiedzi (8 slotów) */
        t[0] = 0;
        text_cat(t, "CPU ");
        int_text((int32_t)plat_cpu_pct(), n, 0);
        text_cat(t, n);
        text_cat(t, "%  HEAVY CHAIN");
        foot_buf[0] = 0;
        text_cat(foot_buf, t);
        foot_hint = foot_buf;
        foot_col = RGB(255, 60, 50);
        foot_keep = 1;
    } else {
        foot_hint = ui.sel_hold && !mst ? "SEL+SELECT: MOVE FX   HOLD LONGER: LIST"   /* (v0.92) */
                  : c->type == FX_NONE ? "SEL: CHOOSE AN EFFECT   HOME: EXIT"
                  : !more && mst ? "SEL: TRACK ON / OFF   HOLD SEL: ORDER"
                  : !more && c->type == FX_EQ ? "SEL: ON / OFF   SEL+KNOB: Q   FX: MORE"   /* (v0.91) */
                  : !more ? "SEL: ON / OFF   HOLD SEL: LIST   FX: MORE"
                          : more + 1 < fx_npages(c) ? "FX: NEXT PAGE   HOME: EXIT" : "FX: BACK   HOME: EXIT";
    }
    body_blit();
}

static void draw_fxl(void)             /* lista efektów do slotu: SELECT wybiera, SEL ok (z łańcucha: zamiana), HOME wraca */
{
    char t[24], n[8];
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    t[0] = 0;
    text_cat(t, "SLOT ");
    int_text(ui.fx_slot + 1, n, 0);
    text_cat(t, n);
    cv_text(8, 4, &FONT_M, t, C_WHITE);
    i = fx_grp(ui.fxl_cur);                  /* z prawej u góry: grupa kursora (jak sekcja w ALL PRESETS) */
    cv_text(232 - text_w(&FONT_XS, i < 0 ? "CHOOSE EFFECT" : FX_GRP[i].name), 12, &FONT_XS,
            i < 0 ? "CHOOSE EFFECT" : FX_GRP[i].name, i < 0 ? C_GRAY : C_HI);
    for (i = 0; i < FXL_ROWS; i++) {
        int32_t w = ui.fxl_top + i, y = 28 + i * 18, r, g, at, ty, x;
        int on;
        for (r = 0; r <= FX_NTYPE && fxl_row((uint32_t)r) < w; r++)
            ;
        if (r > FX_NTYPE)
            break;
        g = fx_grp((uint32_t)r);
        if (fxl_row((uint32_t)r) > w) {      /* nagłówek grupy: nazwa w kolorze motywu, kreska do prawej */
            x = cv_text(10, y + 1, &FONT_XS, FX_GRP[g].name, C_HI);
            cv_rect(x + 6, y + 7, 226 - x, 1, C_LINE);
            continue;
        }
        ty = FX_ORDER[r];
        on = r == ui.fxl_cur;
        if (on)
            cv_rect(4, y - 2, 232, 18, C_HI);
        cv_text(12, y, &FONT_S, FX_NAME[ty], on ? C_BLACK : C_WHITE);
        at = fxl_slot_of((uint32_t)ty);
        if (mt_ui() && fx_hide((uint32_t)ty)) {   /* (v0.85) MULTI: wybór = idź do slotu MASTER FX */
            cv_text(228 - text_w(&FONT_XS, "MASTER"), y + 2, &FONT_XS, "MASTER", on ? C_BLACK : C_GRAY);
        } else if (at >= 0) {                /* już w łańcuchu: w którym slocie (wybór = zamiana) */
            const int32_t vat = fx_view_of(at);
            t[0] = 0;
            text_cat(t, vat == ui.fx_slot ? "HERE" : "SLOT ");
            if (vat != ui.fx_slot) {
                int_text(vat + 1, n, 0);
                text_cat(t, n);
            }
            cv_text(228 - text_w(&FONT_XS, t), y + 2, &FONT_XS, t, on ? C_BLACK : C_GRAY);
        }
    }
    foot_hint = "SELECT: EFFECT   ALGO: GROUP   SEL: OK";
    body_blit();
}

/* ------------------------------------------------------- SEQ: ekran --- */
static const char *const NOTE_NM[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static void note_name(int32_t n, char *t)        /* 60 = C4 */
{
    char o[6];
    t[0] = 0;
    text_cat(t, NOTE_NM[n % 12]);
    int_text(n / 12 - 1, o, 0);
    text_cat(t, o);
}

static uint16_t rgb_mix(uint16_t a, uint16_t b, int32_t k)   /* a -> b, k 0..64 */
{
    int32_t ra = a >> 11 & 31, ga = a >> 5 & 63, ba = a & 31, rb = b >> 11 & 31, gb = b >> 5 & 63, bb = b & 31;
    return (uint16_t)((ra + (rb - ra) * k / 64) << 11 | (ga + (gb - ga) * k / 64) << 5 | (ba + (bb - ba) * k / 64));
}

static void sq_title(const char *name, const char *right)   /* wiersz nazwy strony SEQ / ARP */
{
    int32_t x = cv_text(8, 4, &FONT_M, name, C_WHITE);
    (void)x;
    if (right)
        cv_text(232 - text_w(&FONT_XS, right), 12, &FONT_XS, right, C_GRAY);
}

static void sq_state(char *t)                    /* PLAY / REC / STOP i wzór */
{
    char n[8];
    t[0] = 0;
    text_cat(t, sq.rec ? (sq.play ? "REC  " : "STEP REC  ") : sq.play ? "PLAY  " : "");
    text_cat(t, "P");
    int_text(sqg.pat + 1, n, 0);
    if (sqg.pat < 9)
        text_cat(t, "0");
    text_cat(t, n);
}

#define RX 22                                    /* piano roll (cały ekran, jak w Melodee): oś czasu od x */
#define RW 216
#define RROWS sq_rows()                          /* zoom w pionie: 8 / 13 / 18 / 26 rzędów (22 / 14 / 10 / 7 px) */
#define RH sq_rh()

static uint32_t sq_bar0(void) { return ui.sq_cur / sq_span() * sq_span(); }   /* początek widoku */
static int sq_bar_seen(uint32_t i) { return i * SQ_BAR < sq_bar0() + sq_span() && (i + 1u) * SQ_BAR > sq_bar0(); }   /* takt i na ekranie */
static int32_t rx(uint32_t t, uint32_t bar0) { return RX + (int32_t)((t - bar0) * RW / sq_span()); }

static void note_bar(int32_t x0, int32_t x1, int32_t y, int32_t h, uint16_t c)   /* nuta: zaokrąglone rogi */
{
    if (x1 - x0 < 3) {
        cv_rect(x0, y, x1 - x0 > 0 ? x1 - x0 : 1, h, c);
        return;
    }
    cv_rect(x0 + 1, y, x1 - x0 - 2, h, c);
    cv_rect(x0, y + 1, 1, h - 2, c);
    cv_rect(x1 - 1, y + 1, 1, h - 2, c);
}

static int sel_long(void) { return ui.sel_hold && plat_ms() - ui.sel_at >= SEL_SHOW_MS; }   /* (v0.90) SEL trzymany: podpowiedzi */

static void notes_top_mt(void)                   /* (v0.66) NOTES w MULTI: ścieżki T1-T4 (wybrana jasna, MUTE przekreślona), pod
                                                  * nimi stan, wzór, krok, tempo i takty albo komunikat */
{
    char t[32], n[8];
    uint32_t bar0 = ui.sq_cur / SQ_BAR, i, nb = sqp.bars;
    int32_t k, x, bw;
    for (k = 0; k < MT_N; k++) {
        const int32_t xt = k * 60, e = glob.g[G_MT0 + k], cur = k == glob.g[G_MTFOC], mute = glob.g[G_MTMUTE] >> k & 1;
        char l[16] = {'T', (char)('1' + k), ' ', 0};
        text_cat(l, e >= NENG ? "-" : ENG[e].name);
        if (cur)
            cv_rect(xt + 2, 1, 56, 13, C_HI);
        cv_text(xt + 6, 2, &FONT_XS, l, cur ? C_BLACK : mute || e >= NENG ? C_DIM : C_WHITE);
        if (mute)
            cv_rect(xt + 5, 8, text_w(&FONT_XS, l) + 2, 1, cur ? C_BLACK : C_GRAY);
    }
    if (sel_long() && plat_ms() >= ui.msg_until) {   /* (v0.90) SEL trzymany: OP1-4, SELECT, ALGO */
        cv_text(8, 16, &FONT_XS, "OP1-4: TRACK   SELECT: RANGE   ALGO: PATTERN", C_HI);
        return;
    }
    if (plat_ms() < sq_hint_until && plat_ms() >= ui.msg_until) {   /* (v0.91) po wejściu: podpowiedź */
        cv_text(8, 16, &FONT_XS, NOTES_HINT, C_DIM);
        return;
    }
    if (plat_ms() < ui.msg_until) {              /* komunikat w drugim wierszu (pasek ścieżek zostaje) */
        x = cv_text(8, 16, &FONT_XS, ui.msg[0], C_WHITE);
        cv_text(x + 6, 16, &FONT_XS, ui.msg[1], C_HI);
        return;
    }
    sq_state(t);
    text_cat(t, "   ");
    int_text((int32_t)(ui.sq_cur % SQ_BAR / sq_grid()) + 1, n, 0);
    text_cat(t, n);
    text_cat(t, "/");
    int_text((int32_t)(SQ_BAR / sq_grid()), n, 0);
    text_cat(t, n);
    text_cat(t, "   ");
    int_text((sqp.bpm10 + 5) / 10, n, 0);
    text_cat(t, n);
    text_cat(t, " BPM");
    x = cv_text(8, 16, &FONT_XS, t, sq.rec ? RGB(255, 70, 60) : C_GRAY) + 10;
    bw = nb > 8u ? 4 : 6;                        /* takty: kreski, bieżący jasny, grający biały */
    for (i = 0; i < nb && x + bw < 232; i++, x += bw + 2) {
        uint16_t col = sq_bar_seen(i) ? C_HI : C_DIM;
        if (sq.play && i == sq.pos / SQ_BAR)
            col = i == bar0 ? C_WHITE : C_GRAY;
        cv_rect(x, 19, bw, 5, col);
    }
}

static void notes_top(void)                      /* górny pasek NOTES: wzór, stan, takty, grid, tempo */
{
    char t[24], n[8];
    uint32_t bar0 = ui.sq_cur / SQ_BAR, i, nb = sqp.bars;
    int32_t x, bw;
    cv_begin(240, 30, C_BLACK);
    if (mt_ui()) {                               /* (v0.66) MULTI: ścieżki jak na każdym ekranie, pod nimi reszta */
        notes_top_mt();
        cv_rect(0, 29, 240, 1, C_LINE);
        cv_blit(0, 0);
        return;
    }
    if (plat_ms() < ui.msg_until) {              /* komunikat (GLIDE ON, GRID ...) jak w zwykłym nagłówku */
        x = cv_text(8, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else if (sel_long()) {                     /* (v0.90) SEL trzymany: SELECT i ALGO (gałki: pasek na dole) */
        cv_text(8, 9, &FONT_XS, "SELECT: RANGE     ALGO: PATTERN x GRID", C_HI);
    } else if (plat_ms() < sq_hint_until) {      /* (v0.91) po wejściu: podpowiedź (NOTES nie ma paska) */
        cv_text(8, 9, &FONT_XS, NOTES_HINT, C_DIM);
    } else {
        t[0] = 'P';
        t[1] = 0;
        if (sqg.pat < 9)
            text_cat(t, "0");
        int_text(sqg.pat + 1, n, 0);
        text_cat(t, n);
        x = cv_text(8, 6, &FONT_B, t, C_HI);
        if (sq.rec || sq.play)
            x = cv_text(x + 6, 6, &FONT_B, sq.rec ? (sq.play ? "REC" : "STEP") : "PLAY",
                        sq.rec ? RGB(255, 70, 60) : C_WHITE);
        t[0] = 0;                                /* prawy róg: krok (SELECT) w takcie / ile, tempo */
        int_text((int32_t)(ui.sq_cur % SQ_BAR / sq_grid()) + 1, n, 0);
        text_cat(t, n);
        text_cat(t, "/");
        int_text((int32_t)(SQ_BAR / sq_grid()), n, 0);
        text_cat(t, n);
        {
            int32_t xr = 232 - text_w(&FONT_XS, "BPM"), xb;
            int_text((sqp.bpm10 + 5) / 10, n, 0);
            cv_text(xr, 8, &FONT_XS, "BPM", C_DIM);
            xb = xr - 4 - text_w(&FONT_B, n);
            cv_text(xb, 6, &FONT_B, n, C_GRAY);
            cv_text(xb - 10 - text_w(&FONT_B, t), 6, &FONT_B, t, C_WHITE);
        }
        bw = nb > 8u ? 4 : 8;                    /* takty: kreski, bieżący jasny, grający biały */
        x = sq.rec || sq.play ? 108 : 52;
        for (i = 0; i < nb && x + bw < 140; i++, x += bw + 2) {
            uint16_t c = sq_bar_seen(i) ? C_HI : C_DIM;
            if (sq.play && i == sq.pos / SQ_BAR)
                c = i == bar0 ? C_WHITE : C_GRAY;
            cv_rect(x, 12, bw, 5, c);
        }
    }
    cv_rect(0, 29, 240, 1, C_LINE);
    cv_blit(0, 0);
}

static void notes_chips(void)                    /* dół NOTES: 4 chipy = 4 gałki: NOTE (wybór), PITCH, LEN, VEL */
{
    static const char *const NM[4] = {"", "", "LEN", "VEL"};
    static const char *const SH[4] = {"ZOOM V", "ZOOM H", "FINE LEN", "UNDO"};
    int32_t sel = ui.sq_note, k, c = sq_cell_count();
    char v[4][16], n[8];
    sq_ac_new();
    cv_begin(240, 26, C_BLACK);
    if (sel_long()) {                            /* (v0.90, decyzja użytkownika 2026-11-03) SEL trzymany: co robią gałki */
        for (k = 0; k < 4; k++) {
            const int32_t bx = 2 + k * 60;
            note_bar(bx, bx + 56, 2, 22, C_LINE);
            cv_text(bx + (56 - text_w(&FONT_XS, SH[k])) / 2, 7, &FONT_XS, SH[k], C_HI);
        }
        cv_blit(0, 214);
        return;
    }
    v[0][0] = v[1][0] = v[2][0] = v[3][0] = 0;
    if (sel < 0) {
        text_cat(v[1], "+ NOTE");                /* K2 na pustym kroku wstawia nutę */
    } else {
        const sqn_t *q = &sqp.n[sel];
        const uint32_t sl = sq_shown(q);         /* (v0.87) perkusja: ile naprawdę trwa */
        note_name(q->note, v[1]);
        int_text((int32_t)(sl / sq_grid()), v[2], 0);   /* w krokach gridu */
        if (sl % sq_grid()) {
            text_cat(v[2], ".");
            int_text((int32_t)(sl % sq_grid() * 10u / sq_grid()), n, 0);
            text_cat(v[2], n);
        }
        int_text(q->vel, v[3], 0);
    }
    if (c > 1 && sel >= 0) {                     /* K1: która nuta akordu */
        text_cat(v[0], "NOTE ");
        int_text(sq_cell_index(sel) + 1, n, 0);
        text_cat(v[0], n);
        text_cat(v[0], "/");
        int_text(c, n, 0);
        text_cat(v[0], n);
    } else {
        text_cat(v[0], "UP / DN");
    }
    for (k = 0; k < 4; k++) {
        int32_t bx = 2 + k * 60, lit = ui.knob == k && plat_ms() - ui.knob_at < 1500u, x;
        uint16_t bg = lit ? C_HI : C_LINE, fg = lit ? C_BLACK : C_HI;
        if (!v[k][0])
            continue;
        note_bar(bx, bx + 56, 2, 22, bg);
        if (NM[k][0]) {                          /* nazwa mała, wartość duża */
            x = cv_text(bx + 5, 7, &FONT_XS, NM[k], lit ? C_BLACK : C_GRAY);
            cv_text(x + 4, 5, &FONT_B, v[k], fg);
        } else if (k == 0) {                     /* nawigacja: mniejszym, szarym */
            cv_text(bx + (56 - text_w(&FONT_XS, v[k])) / 2, 7, &FONT_XS, v[k], lit ? C_BLACK : C_GRAY);
        } else {
            cv_text(bx + (56 - text_w(&FONT_B, v[k])) / 2, 5, &FONT_B, v[k], sel < 0 && !lit ? C_GRAY : fg);
        }
    }
    cv_blit(0, 214);
}

static void draw_sqn(void)                       /* karta nuty na piano rollu: SELECT wiersz, gałka zmienia, SEL robi */
{
    char t[24];
    int32_t i, y0 = 5, x0 = 30, w = 180, h = NSN * 18 + 28;   /* (v0.90) 8 wierszy po 18 px */
    cv_rect(x0 - 2, y0 - 2, w + 4, h + 4, C_HI);
    cv_rect(x0, y0, w, h, RGB(12, 12, 15));
    if (ui.sq_rng) {                             /* zakres: ile nut */
        uint32_t c = 0;
        for (i = 0; i < SQ_NN; i++)
            c += (uint32_t)sq_tgt((uint32_t)i);
        int_text((int32_t)c, t, 0);
        text_cat(t, " NOTES");
        cv_text(x0 + 10, y0 + 5, &FONT_B, t, C_WHITE);
        cv_text(x0 + w - 10 - text_w(&FONT_XS, "SELECTION"), y0 + 6, &FONT_XS, "SELECTION", C_GRAY);
    } else if (ui.sq_note >= 0) {
        note_name(sqp.n[ui.sq_note].note, t);
        cv_text(x0 + 10, y0 + 5, &FONT_B, t, C_WHITE);
        cv_text(x0 + w - 10 - text_w(&FONT_XS, "NOTE"), y0 + 6, &FONT_XS, "NOTE", C_GRAY);
    }
    for (i = 0; i < NSN; i++) {
        int32_t y = y0 + 25 + i * 18, on = i == ui.sqn_row;
        if (on)
            note_bar(x0 + 4, x0 + w - 4, y - 2, 17, C_HI);
        cv_text(x0 + 10, y, &FONT_S, SN_NAME[i], on ? C_BLACK : i == SN_DEL ? RGB(255, 90, 80) : C_WHITE);
        sqn_value(i, t);
        if (i != SN_DEL)
            cv_text(x0 + w - 10 - text_w(&FONT_XS, t), y + 1, &FONT_XS, t, on ? C_BLACK : C_HI);
    }
}

static void draw_notes_page(void)                /* NOTES: piano roll na całą wysokość (rzędy 14 px), takt na szerokość */
{
    char t[8];
    uint32_t L = sq_len(), V = sq_span(), bar0 = sq_bar0(), g = sq_grid(), i, r, pass;
    int32_t low = ui.sq_low, cx = rx(ui.sq_cur, bar0), cw = (int32_t)(g * RW / V), rows = RROWS, rh = RH;
    int dense = cw < 5;                          /* gęsto: grid tylko na ćwierćnutach */
    const uint32_t sc = sq_scale();              /* (v1.1.0) SEQ > KEY: wiersze nut w skali jaśniejsze, tonika najjaśniej */
    sq_ac_new();
    for (r = 0; r < rows; r++) {                /* klawiatura z lewej, tło wierszy: czarne klawisze ciemniejsze */
        int32_t note = low + rows - 1 - (int32_t)r, pc = note % 12, y = (int32_t)r * rh;
        int blk = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        cv_rect(0, y, 20, rh - 1, blk ? RGB(14, 14, 17) : RGB(150, 152, 158));
        cv_rect(RX, y, RW, rh, sc >> pc & 1u ? (pc == sqp.rsv[SQ_KEY_] % 12 ? RGB(48, 38, 76) : RGB(28, 31, 48))
                               : blk ? RGB(8, 8, 10) : RGB(20, 21, 25));
        if (pc == 0) {                           /* C: podpis na klawiszu (gdy się mieści) i mocniejsza linia */
            note_name(note, t);
            if (rh >= 10)
                cv_text(2, y + (rh - 12) / 2, &FONT_T, t, C_BLACK);
            cv_rect(RX, y + rh - 1, RW, 1, C_DIM);
        }
    }
    for (i = 0; i < V; i += g) {                 /* grid; ćwierćnuty mocniej, takty najmocniej */
        int32_t gx = RX + (int32_t)(i * RW / V);
        if (dense && i % SQ_PPQN)
            continue;
        cv_rect(gx, 0, 1, rows * rh, i % SQ_BAR == 0u && i ? C_GRAY : i % SQ_PPQN == 0u ? C_DIM : C_LINE);
    }
    if (L < bar0 + V) {                          /* (v0.87) za końcem wzoru (długość w krokach): pusto */
        const int32_t xe = L > bar0 ? rx(L, bar0) : RX;
        cv_rect(xe, 0, RX + RW - xe, rows * rh, C_BLACK);
    }
    if (ui.sq_rng) {                             /* zaznaczenie (SEL + SELECT): pasy nad i pod zakresem */
        uint32_t lo = sq_rlo(), hi = sq_rhi();
        int32_t xa = lo <= bar0 ? RX : lo >= bar0 + V ? RX + RW : rx(lo, bar0);
        int32_t xb = hi <= bar0 ? RX : hi >= bar0 + V ? RX + RW : rx(hi, bar0);
        if (xb > xa) {
            cv_rect(xa, 0, xb - xa, 2, C_HI);
            cv_rect(xa, rows * rh - 2, xb - xa, 2, C_HI);
        }
    }
    /* (v0.66) MULTI: najpierw nuty innych ścieżek (ghost), na nich wybranej. (v0.92, użytkownik 2026-11-04: krótka nuta wewnątrz
     * długiej była niewidoczna) nuty wybranej od najdłuższych do najkrótszych (pass 1-3), zaznaczone na samym wierzchu (pass 4);
     * nuta na innej ma czarne szczeliny przy końcach */
    for (pass = 0; pass < 5u; pass++)
    for (i = 0; i < SQ_NN; i++) {                /* nuty: jasność = velocity, zaznaczone białe (jak w Melodee) */
        const sqn_t *q = &sqp.n[i];
        int32_t row, x0, x1, y;
        uint16_t c;
        uint32_t sl;
        if (!q->vel || q->t >= L || q->t >= bar0 + V || (uint32_t)sq_mine(q) != (pass ? 1u : 0u))
            continue;
        sl = pass ? sq_shown(q) : q->len;        /* (v0.87) perkusja: ile naprawdę trwa */
        if (pass && (SQ_ISSEL(i) ? 4u : sl > 4u * g ? 1u : sl > g ? 2u : 3u) != pass)
            continue;
        if (q->t + (sl ? sl : 1u) <= bar0)
            continue;
        row = low + rows - 1 - q->note;
        x0 = q->t < bar0 ? RX : rx(q->t, bar0) + 1;
        x1 = q->t + sl >= bar0 + V ? RX + RW : rx(q->t + sl, bar0);
        if (L < bar0 + V && q->t + sl >= L)      /* (nie wchodzi w puste za końcem wzoru) */
            x1 = rx(L, bar0);
        if (x1 < x0 + 3)
            x1 = x0 + 3;
        if (!pass) {                             /* ghost (bez edycji); perkusja: znacznik uderzenia u góry */
            if (!SQN_TRK(q))                     /* (T1 = perkusja) */
                cv_rect(x0, 3, 2, 4, RGB(92, 94, 106));
            else if (row >= 0 && row < rows)
                note_bar(x0, x1, row * rh + 1, rh - (rh > 8 ? 3 : 2), RGB(46, 48, 58));
            continue;
        }
        c = SQ_ISSEL(i) ? C_WHITE : rgb_mix(C_DIM, C_HI, 20 + q->vel * 44 / 127);
        if (q->chance < 100u)                    /* szansa < 100 %: przygaszona */
            c = rgb_mix(C_BLACK, c, 20 + q->chance * 44 / 100);
        if (row < 0 || row >= rows) {           /* poza widokiem: znacznik na brzegu */
            cv_rect(x0, row < 0 ? 0 : rows * rh - 3, x1 - x0, 3, c);
            continue;
        }
        y = row * rh + 1;
        if (pass > 1u) {                         /* (v0.92) szczeliny: widać ją na dłuższej nucie */
            cv_rect(x0 - 1, y, 1, rh - (rh > 8 ? 3 : 2), C_BLACK);
            cv_rect(x1, y, 1, rh - (rh > 8 ? 3 : 2), C_BLACK);
        }
        note_bar(x0, x1, y, rh - (rh > 8 ? 3 : 2), c);
        if (SQN_RAT(q) > 1u) {                   /* ratchet: podział nuty */
            uint32_t k, rr = SQN_RAT(q);
            for (k = 1; k < rr; k++)
                cv_rect(x0 + (x1 - x0) * (int32_t)k / (int32_t)rr, y + 2, 2, rh - 7, C_BLACK);
        }
        if (q->fl & SQF_GLIDE) {                 /* glide: linia do następnej nuty (jak slide w FL) */
            uint32_t d = next_start(q->t, SQN_TRK(q)), j, nt = q->note;
            int32_t xe, r2;
            for (j = 0; j < SQ_NN; j++)
                if (sqp.n[j].vel && sqp.n[j].len && (q->t + d) % L == sqp.n[j].t && sq_mine(&sqp.n[j])) {
                    nt = sqp.n[j].note;
                    break;
                }
            r2 = low + rows - 1 - (int32_t)nt;
            if (q->t + d < L && q->t + d < bar0 + V && r2 >= 0 && r2 < rows) {   /* następna nuta na ekranie: do niej */
                xe = rx(q->t + d, bar0);
                cv_line(x1 - 1, y + rh / 2 - 1, xe, r2 * rh + rh / 2, C_WHITE);
                cv_line(x1 - 1, y + rh / 2, xe, r2 * rh + rh / 2 + 1, C_WHITE);
            } else {                             /* poza ekranem / w następnym obiegu: krótka kreska w jej stronę */
                int32_t ym = y + rh / 2, dy = r2 < row ? -rh / 2 - 2 : r2 > row ? rh / 2 + 2 : 0;
                xe = x1 - 1 + 8 < RX + RW ? x1 - 1 + 8 : RX + RW - 1;
                cv_line(x1 - 1, ym - 1, xe, ym - 1 + dy, C_WHITE);
                cv_line(x1 - 1, ym, xe, ym + dy, C_WHITE);
            }
        }
    }
    cv_rect(cx, 0, cw, 2, C_WHITE);              /* kursor: obrys komórki (jak w Melodee) */
    cv_rect(cx, rows * rh - 2, cw, 2, C_WHITE);
    cv_rect(cx, 0, 2, rows * rh, C_WHITE);
    cv_rect(cx + cw - 2, 0, 2, rows * rh, C_WHITE);
    if (sq.play && sq.pos >= bar0 && sq.pos < bar0 + V)   /* odtwarzanie */
        cv_rect(rx(sq.pos, bar0), 0, 2, rows * rh, sq.rec ? RGB(255, 70, 60) : C_HI);
}

static void draw_rack(void)                      /* (v0.57) RACK: 8 wierszy x 16 kroków strony, pod spodem wartości kroku */
{
    static const char *const KN[8] = {"VEL ", "CHANCE ", "RATCHET ", "NUDGE ", "PITCH ", "COND ", "FLAM ", "LEN "};
    const uint32_t L = sq_len(), ns = rk_steps(), st = ui.rk_step % ns, pg0 = st / 16u * 16u;
    const uint32_t play = sq.play ? sq.pos / RK_T : 0xFFFFu;
    const int32_t sel = rk_find(ui.rk_row, st);
    char t[32], n[8];
    int32_t r, c, k;
    sq_state(t);
    text_cat(t, "  STEPS ");
    int_text((int32_t)pg0 + 1, n, 0);
    text_cat(t, n);
    text_cat(t, "-");
    int_text((int32_t)(pg0 + 16u < ns ? pg0 + 16u : ns), n, 0);
    text_cat(t, n);
    text_cat(t, " / ");
    int_text((int32_t)ns, n, 0);
    text_cat(t, n);
    sq_title("RACK", t);
    for (r = 0; r < RK_ROWS; r++) {
        const int32_t row = ui.rk_top + r, y = 28 + r * 14;
        const int on = row == ui.rk_row, off = (sqp.mute >> row & 1u) || (sqp.solo && !(sqp.solo >> row & 1u));
        uint16_t lc = off ? RGB(80, 80, 86) : sqp.solo >> row & 1u ? C_HI : on ? C_WHITE : C_GRAY;
        if (on)
            cv_rect(0, y, 31, 13, RGB(36, 37, 44));
        rk_name(row, n);
        cv_text(3, y + 1, &FONT_T, n, lc);
        if (sqp.mute >> row & 1u)
            cv_text(31 - text_w(&FONT_T, "M"), y + 1, &FONT_T, "M", RGB(255, 90, 80));
        else if (sqp.solo >> row & 1u)
            cv_text(31 - text_w(&FONT_T, "S"), y + 1, &FONT_T, "S", C_HI);
        for (c = 0; c < 16; c++) {
            const uint32_t s = pg0 + (uint32_t)c;
            const int32_t x = 32 + c * 13, i = s < ns ? rk_find(row, s) : -1;
            uint16_t bg = s >= ns ? C_BLACK : s == play ? RGB(70, 72, 82) : c % 4 == 0 ? RGB(40, 41, 48) : RGB(24, 25, 30);
            cv_rect(x + 1, y + 1, 11, 12, bg);
            if (i >= 0) {                        /* krok: jasność = VEL, CHANCE < 100 przygaszony, NUDGE przesuwa, RATCHET liczba */
                const sqn_t *q = &sqp.n[i];
                const int32_t o = (int32_t)((q->t + L + 12u - s * RK_T % L) % L) - 12, nx = o * 6 / 12;
                uint16_t col = rgb_mix(C_DIM, C_HI, 20 + q->vel * 44 / 127);
                if (q->chance < 100u)
                    col = rgb_mix(C_BLACK, col, 20 + q->chance * 44 / 100);
                if (off)
                    col = rgb_mix(C_BLACK, col, 24);
                cv_rect(x + 2 + (nx < -1 ? -1 : nx > 1 ? 1 : nx), y + 2, 9, 10, col);
                if (SQN_RAT(q) > 1u) {
                    int_text((int32_t)SQN_RAT(q), n, 0);
                    cv_text(x + 4, y + 1, &FONT_T, n, C_BLACK);
                }
                if (SQX_PIT(sqp.x[i]))           /* (v0.92) PITCH: kreska u góry (wyżej) / u dołu (niżej); FLAM: lewy brzeg; COND: kropka */
                    cv_rect(x + 3, SQX_PIT(sqp.x[i]) > 0 ? y + 2 : y + 10, 7, 2, C_WHITE);
                if (q->fl & SQF_GLIDE)
                    cv_rect(x + 2, y + 2, 2, 10, C_BLACK);
                if (SQX_COND(sqp.x[i]))
                    cv_rect(x + 9, y + 9, 2, 2, C_BLACK);
            }
            if (on && s == st) {                 /* wybrany krok: obrys */
                cv_rect(x, y, 13, 1, C_WHITE);
                cv_rect(x, y + 13, 13, 1, C_WHITE);
                cv_rect(x, y, 1, 14, C_WHITE);
                cv_rect(x + 12, y, 1, 14, C_WHITE);
            }
        }
    }
    for (k = 0; k < 4; k++) {                    /* pod gałkami: wartości kroku (SEL trzymany: PITCH, COND, FLAM, LEN); (v0.92) na środku kolumny */
        const sqn_t *q = sel >= 0 ? &sqp.n[sel] : 0;
        t[0] = 0;
        text_cat(t, KN[k + (ui.sel_hold ? 4 : 0)]);
        if (!q) {
            text_cat(t, "-");
        } else if (ui.sel_hold) {
            const uint8_t x = sqp.x[sel];
            if (k == 0) {
                int_text(SQX_PIT(x), n, 1);
                text_cat(t, n);
            } else if (k == 1) {
                text_cat(t, L_SQCOND[SQX_COND(x)]);
            } else if (k == 2) {
                text_cat(t, q->fl & SQF_GLIDE ? "ON" : "OFF");
            } else if (!(q->fl & SQF_CUT) && sq_auto(q->note)) {
                text_cat(t, "AUTO");
            } else {
                const uint32_t sl = sq_shown(q);
                int_text((int32_t)(sl / RK_T), n, 0);
                text_cat(t, n);
                if (sl % RK_T) {
                    text_cat(t, ".");
                    int_text((int32_t)(sl % RK_T * 10u / RK_T), n, 0);
                    text_cat(t, n);
                }
            }
        } else if (k == 0) {
            int_text(q->vel, n, 0);
            text_cat(t, n);
        } else if (k == 1) {
            int_text(q->chance, n, 0);
            text_cat(t, n);
            text_cat(t, "%");
        } else if (k == 2) {
            int_text((int32_t)SQN_RAT(q), n, 0);
            text_cat(t, n);
        } else {
            int_text((int32_t)((q->t + L + 12u - st * RK_T % L) % L) - 12, n, 1);
            text_cat(t, n);
        }
        cv_text(k * 60 + (60 - text_w(&FONT_T, t)) / 2, 142, &FONT_T, t, ui.knob == k ? C_HI : ui.sel_hold ? C_WHITE : C_GRAY);
    }
    foot_hint = ui.sel_hold ? "SELECT: STEP   ALGO: PAT LEN   KEY+KEY: COPY" : "FX+KEY: MUTE   ARP+KEY: SOLO   SEL: TOOLS";   /* (v0.91) SEL + ALGO */
}

/* (v0.90, decyzja użytkownika 2026-11-03: wybór wzoru był brzydki; funkcje bez zmian) PATTERN: u góry 16 wzorów (wybrany jasny, pusty
 * przygaszony), pod nimi podgląd całego wzoru (nuty wybranej ścieżki jasne, reszta szara, takty, odtwarzanie), na dole K1-K4:
 * PATTERN, STEPS, BPM, SWING. Puste wzory we flashu: sprawdzane raz na wejście / zmianę wzoru (pt_scan). */
static uint16_t pt_full;                         /* bit = wzór z nutami */
static uint8_t pt_scan;
static void pt_rescan(void) { pt_scan = 0; }
static void draw_pattern(void)
{
    static const char *const KN[4] = {"PATTERN", "STEPS", "BPM", "SWING"};
    const uint32_t L = sq_len();
    char t[24], n[8];
    int32_t i, k, lo = 127, hi = 0, used = 0;
    if (!pt_scan) {
        pt_full = 0;
        for (i = 0; i < SQ_NPAT; i++) {
            const sqpat_t *p = (const sqpat_t *)(const void *)plat_data(sq_off(i));
            uint32_t j;
            if (p && p->magic == SQ_MAGIC)
                for (j = 0; j < SQ_NN; j++)
                    if (p->n[j].vel && p->n[j].vel <= 127u) {
                        pt_full |= (uint16_t)(1u << i);
                        break;
                    }
        }
        pt_scan = 1;
    }
    for (i = 0; i < SQ_NN; i++)                  /* bieżący z RAM (może być niezapisany) */
        if (sqp.n[i].vel && sqp.n[i].len && sqp.n[i].t < L) {
            used++;
            lo = sqp.n[i].note < lo ? sqp.n[i].note : lo;
            hi = sqp.n[i].note > hi ? sqp.n[i].note : hi;
        }
    sq_state(t);
    sq_title("PATTERN", sq.play || sq.rec ? t : 0);
    for (i = 0; i < SQ_NPAT; i++) {              /* 16 wzorów; (v0.92, decyzja użytkownika 2026-11-04) pod podglądem */
        const int32_t x = 8 + i * 14, cur = i == sqg.pat, full = cur ? used > 0 : pt_full >> i & 1, y = 89;
        int_text(i + 1, n, 0);
        if (cur)
            note_bar(x, x + 13, y, 20, C_HI);
        else
            cv_rect(x, y, 13, 20, full ? C_LINE : RGB(14, 14, 17));
        cv_text(x + (13 - text_w(&FONT_T, n)) / 2, y + 3, &FONT_T, n, cur ? C_BLACK : full ? C_WHITE : C_DIM);
        if (full)
            cv_rect(x + 5, y + 16, 3, 2, cur ? C_BLACK : C_HI);
    }
    {                                            /* podgląd: cały wzór na szerokość, wysokości od najniższej do najwyższej nuty */
        const int32_t px = 8, py = 30, pw = 224, ph = 54;
        int32_t span = hi - lo + 1, pass;
        uint32_t b;
        cv_rect(px, py, pw, ph, RGB(14, 14, 17));
        for (b = SQ_PPQN; b < L; b += SQ_PPQN)   /* ćwierćnuty, takty mocniej */
            cv_rect(px + (int32_t)(b * (uint32_t)pw / L), py, 1, ph, b % SQ_BAR ? RGB(24, 25, 30) : C_LINE);
        if (!used) {
            center(py + ph / 2 - 6, &FONT_XS, "EMPTY PATTERN", C_DIM);
        } else {
            if (span < 12) {                     /* mało wysokości: na środku, nie rozciągane */
                lo -= (12 - span) / 2;
                span = 12;
            }
            for (pass = 0; pass < 2; pass++)     /* inne ścieżki pod spodem, wybrana na wierzchu */
                for (i = 0; i < SQ_NN; i++) {
                    const sqn_t *q = &sqp.n[i];
                    int32_t x0, x1, y;
                    if (!q->vel || !q->len || q->t >= L || sq_mine(q) != pass)
                        continue;
                    x0 = px + (int32_t)(q->t * (uint32_t)pw / L);
                    x1 = px + (int32_t)((q->t + q->len < L ? q->t + q->len : L) * (uint32_t)pw / L);
                    y = py + ph - 3 - (q->note - lo) * (ph - 6) / span;
                    cv_rect(x0, y - 1, x1 - x0 > 2 ? x1 - x0 - 1 : 2, 3,
                            pass ? rgb_mix(C_DIM, C_HI, 20 + q->vel * 44 / 127) : RGB(70, 72, 82));
                }
        }
        if (sq.play && L)
            cv_rect(px + (int32_t)(sq.pos % L * (uint32_t)pw / L), py, 2, ph, C_WHITE);
    }
    for (k = 0; k < 4; k++) {                    /* K1-K4 */
        const int32_t bx = 4 + k * 59, v = sq_val(SQ_PATTERN, k), lit = ui.knob == k && plat_ms() - ui.knob_at < 1500u;
        t[0] = 0;
        if (k == 0 && v < 10)
            text_cat(t, "0");
        int_text(k == 3 ? swing_pct(v) : v, n, 0);
        text_cat(t, n);
        if (k == 3)
            text_cat(t, "%");
        note_bar(bx, bx + 55, 114, 38, lit ? C_DIM : C_LINE);
        cv_text(bx + (55 - text_w(&FONT_XS, KN[k])) / 2, 117, &FONT_XS, KN[k], lit ? C_WHITE : C_GRAY);
        cv_text(bx + (55 - text_w(&FONT_S, t)) / 2, 132, &FONT_S, t, C_HI);
    }
    foot_hint = "SELECT: PAGE   SEL: TOOLS   SEQ: BACK";
}

static void draw_seq(void)
{
    cv_begin(240, BODY_H, C_BLACK);
    if (ui.sq_pg == SQ_NOTES) {
        draw_notes_page();
    } else if (ui.sq_pg == SQ_RACK) {
        draw_rack();
    } else if (ui.sq_pg == SQ_PATTERN) {         /* (v0.90) PATTERN: wzory, podgląd, wartości */
        draw_pattern();
    } else if (ui.sq_pg == SQ_REC) {             /* (v0.92) RECORD (REC trzymany) */
        const pdef_t *d[4];
        int16_t v[4];
        char txt[4][16], t[24];
        int32_t k;
        sq_state(t);
        sq_title(SQ_PGNAME[SQ_REC], t);
        for (k = 0; k < 4; k++) {
            d[k] = &RCDEF[k];
            v[k] = (int16_t)rc_get(k);
            txt[k][0] = 0;
            def_text(d[k], -1, v[k], txt[k]);
        }
        col_y = 34;
        col_h = hints() ? 120 : 120 + FOOT_TALL;
        draw_cols(d, v, txt, 0);
        col_y = col_h = -1;
        foot_hint = "KEYS: TAP   REC: ARM   PLAY: START   SELECT: PAGE";   /* (v1.1.0) klawisze = TAP TEMPO */
    } else if (ui.sq_pg == SQ_KEY) {             /* (v1.1.0) KEY */
        const pdef_t *d[4];
        int16_t v[4];
        char txt[4][16], t[24];
        int32_t k;
        sq_state(t);
        sq_title(SQ_PGNAME[SQ_KEY], t);
        for (k = 0; k < 4; k++) {
            d[k] = k < 3 ? &KYDEF[k] : 0;
            v[k] = (int16_t)(k < 3 ? sqp.rsv[k] : 0);
            txt[k][0] = 0;
            if (k < 3)
                def_text(d[k], -1, v[k], txt[k]);
        }
        col_y = 34;
        col_h = hints() ? 120 : 120 + FOOT_TALL;
        draw_cols(d, v, txt, 0);
        col_y = col_h = -1;
        foot_hint = "SELECT: PAGE   SEQ / HOME: BACK";
    } else if (ui.sq_pg == SQ_OPT) {             /* (v0.88) OPTIONS; (v0.89) zwykła strona ustawień */
        const pdef_t *d[4];
        int16_t v[4];
        char txt[4][16], t[24];
        int32_t k;
        sq_state(t);
        sq_title(SQ_PGNAME[SQ_OPT], t);
        for (k = 0; k < 4; k++) {
            const int32_t g = SQ_OPTG[k];
            d[k] = g < 0 ? 0 : &GDEF[g];
            v[k] = g < 0 ? 0 : glob.g[g];
            txt[k][0] = 0;
            if (g >= 0)
                def_text(d[k], -1, v[k], txt[k]);
        }
        col_y = 34;
        col_h = hints() ? 120 : 120 + FOOT_TALL;
        draw_cols(d, v, txt, 0);
        col_y = col_h = -1;
        foot_hint = "SELECT: PAGE   SEQ / HOME: BACK";
    } else {
        const pdef_t *d[4];
        int16_t v[4];
        char txt[4][16], t[24], n[8];
        int32_t k, pg = ui.sq_pg, used = 0, i;
        for (i = 0; i < SQ_NN; i++)
            used += sqp.n[i].vel != 0;
        sq_state(t);
        if (pg == SQ_PATTERN) {
            text_cat(t, "  NOTES ");
            int_text(used, n, 0);
            text_cat(t, n);
        } else {
            if (sqp.rolls) {                     /* (v0.57) SEL + K1: losowe rolki */
                text_cat(t, "  ROLLS ");
                int_text(sqp.rolls, n, 0);
                text_cat(t, n);
                text_cat(t, "%");
            }
            text_cat(t, "  REC Q ");
            text_cat(t, L_RECQ[sqp.recq % 7u]);
        }
        sq_title(SQ_PGNAME[pg], t);
        for (k = 0; k < 4; k++) {
            int32_t x = sq_val(pg, k);
            d[k] = &SQDEF[pg - 1][k];
            v[k] = (int16_t)x;
            if (d[k]->fmt == F_LIST) {
                txt[k][0] = 0;
                text_cat(txt[k], PLIST(d[k])[x]);
            } else {
                int_text(pg == SQ_PATTERN && k == 3 ? swing_pct(x) : x, txt[k], 0);
                if ((pg == SQ_PATTERN && k == 3) || (pg == SQ_FEEL && k != 3))
                    text_cat(txt[k], "%");
            }
        }
        col_y = 34;
        col_h = hints() ? 120 : 120 + FOOT_TALL;   /* HINTS OFF: krótki pasek, wyższe pola */
        draw_cols(d, v, txt, 0);
        col_y = col_h = -1;
        foot_hint = pg == SQ_PATTERN ? "SELECT: PAGE   SEL: TOOLS   SEQ: BACK"
                                    : "SEL: REC QUANT   SEL+K1: ROLLS   SEQ: BACK";
    }
    if (mt_ui() && ui.sel_hold && ui.sq_pg != SQ_NOTES)   /* (v0.90) SEL trzymany w MULTI: klawisze OP = ścieżki */
        foot_hint = ui.sq_pg == SQ_RACK ? "SEL+OP1-4: TRACK   SELECT: STEP   KNOB: ROW" : "SEL+OP1-4: TRACK T1-T4";
    body_blit();
}

static void draw_arp(void)                       /* ARP: 4 gałki (MORE: 4 następne, 3: CHORD), pod nimi ostatnie kroki */
{
    static const char *const CHW[3] = {"STAB: WHOLE CHORD EACH STEP", "SPLIT: CHORD NOTES ONE BY ONE",
                                       "OFF: ONLY THE KEYS YOU HOLD"};
    static const char *const WHAT[ARM_N] = {"LOW TO HIGH", "HIGH TO LOW", "UP, THEN DOWN", "IN THE ORDER YOU PLAYED",
                                            "RANDOM NOTE EACH STEP", "WHOLE CHORD EACH STEP",
                                            "LOOP OF 8 RANDOM STEPS (MUTATE)"};
    const pdef_t *d[4];
    int16_t v[4];
    char txt[4][16], t[24];
    int32_t k, more = ui.arp_more, lo = 127, hi = 0;
    cv_begin(240, BODY_H, C_BLACK);
    k = cv_text(8, 4, &FONT_M, "ARP", C_WHITE);
    if (more)
        k = cv_text(k + 6, 4, &FONT_M, "MORE", C_HI);
    cv_text(k + 8, 4, &FONT_M, sqg.arp_on ? "ON" : "OFF", sqg.arp_on ? C_HI : C_GRAY);
    t[0] = 0;
    if (sqg.latch)
        text_cat(t, "LATCH  ");
    text_cat(t, sq.play ? "SYNC  " : sq.ext ? "MIDI  " : "");
    text_cat(t, more == 2 ? "3 / 3" : more ? "2 / 3" : "1 / 3");
    cv_text(232 - text_w(&FONT_XS, t), 12, &FONT_XS, t, C_GRAY);
    for (k = 0; k < 4; k++) {
        int32_t j = k + more * 4, x = j < AR_NP ? sqg.a[j] : 0;
        d[k] = j < AR_NP ? &ARDEF[j] : 0;
        v[k] = (int16_t)x;
        txt[k][0] = 0;
        if (j >= AR_NP) {
        } else if (ARDEF[j].fmt == F_LIST) {
            text_cat(txt[k], PLIST(&ARDEF[j])[x]);
        } else {
            int_text(j == AR_SWING ? swing_pct(x) : x, txt[k], 0);
            if (j >= AR_GATE)
                text_cat(txt[k], "%");
        }
    }
    col_y = 34;
    col_h = 56;
    draw_cols(d, v, txt, 1);
    col_y = col_h = -1;
    for (k = 0; k < 16; k++) {                   /* ostatnie kroki: wysokość nuty (pauza: kropka) */
        int32_t x = ar.hist[k];
        if (x) {
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
        }
    }
    for (k = 0; k < 16; k++) {
        int32_t s = (int32_t)((ar.hist_w + (uint32_t)k) & 15u), x = ar.hist[s], bx = 8 + k * 14, h;
        int last = k == 15;
        if (!x) {
            cv_rect(bx + 4, 134, 4, 2, C_LINE);
            continue;
        }
        h = hi > lo ? 8 + (x - lo) * 34 / (hi - lo) : 24;
        cv_rect(bx, 136 - h, 12, h, last ? rgb_light(C_HI) : C_AMB);
    }
    if (more == 2)                               /* CHORD: co robi; brzmienie bez akordu: gdzie go włączyć */
        center(140, &FONT_XS, chord_at(60) == 1u ? "NO CHORD IN THIS SOUND (EDIT: CHORD)" : CHW[sqg.a[AR_CHORD] % 3u],
               C_HI);
    else
        center(140, &FONT_XS, !sqg.arp_on ? "ARP: TURN THE ARPEGGIATOR ON"
                                     : !ar.nl ? "HOLD KEYS: THEY PLAY ONE BY ONE"
                                     : WHAT[sqg.a[AR_MODE]], sqg.arp_on ? C_HI : C_GRAY);
    foot_hint = "SELECT: PAGE   ARP: MORE   SEL: LATCH";   /* (v0.91) CHORD pod SELECT */
    body_blit();
}

static const char *const L_SYNC[2] = {"BPM", "KEY"}, *const L_LEN[5] = {"", "BEAT", "BAR", "2 BARS", "4 BARS"};   /* PERFORM: ustawienia klawisza (ogs_lists) */
static void draw_perf(void)              /* PERFORM: 4 gałki, klawiatura (trzymane / zatrzaśnięte), nazwa efektu; (v0.47.2)
                                          * bez pamięci: klawisze, które nie grają, ciemnoczerwone + powód */
{
    static const char *const WHY[3] = {"", "DELAY IN FX CHAIN", "FX CHAIN FULL"};
    static const pdef_t PD[4] = {{"FILTER", -100, 100, 0, F_SIGNED, 0}, {"CRUSH", 0, 100, 0, F_NUM, 0},
                                 {"THROW", 0, 100, 0, F_NUM, 0}, {"DEPTH", 0, 100, 100, F_NUM, 0}};
    static const uint8_t WK[15] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24};   /* białe od F3 */
    static const uint8_t BK[10] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22}, BX[10] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13};
    const pdef_t *d[4];
    int16_t v[4];
    char txt[4][16], why[48];
    int32_t k, x;
    uint32_t act = ui.pf_keys | ui.pf_lat, mem = ogs_pf_mem < 3u ? ogs_pf_mem : 0u, off = mem ? PF_NEEDMEM : 0u;
    const char *line = act ? PF_NAME[ui.pf_last % PF_N] : "HOLD FX + KEY: REPEAT, TAPE, FILTER ...";
    uint16_t lc = act && ogs_pf_red && ui.pf_last <= PF_R32 ? RGB(255, 60, 50) : act ? C_HI : C_GRAY;
    static const pdef_t PC[4] = {{"SYNC", 0, 1, 0, F_LIST, LI(L_SYNC)}, {"START", 0, 99, 0, F_NUM, 0}, {"LENGTH", 1, 4, 1, F_LIST, LI(L_LEN)},
                                 {"", 0, 0, 0, F_NONE, 0}};
    int32_t ed = pf_edit();
    const int32_t ce = cv_edit();
    cv_begin(240, BODY_H, C_BLACK);
    x = cv_text(8, 4, &FONT_M, "PERFORM", C_WHITE);
    if (ui.pf_bank)                          /* (v0.91) bank krzywych */
        x = cv_text(x + 6, 4, &FONT_M, "CURVES", C_HI);
    if (ui.pf_latch)
        cv_text(x + 6, 4, &FONT_M, "LATCH", C_HI);
    if (ogs_fx_beat >= 0.0f)
        cv_text(232 - text_w(&FONT_XS, "1/16 GRID"), 12, &FONT_XS, "1/16 GRID", C_GRAY);
    if (ce >= 0) {                           /* (v0.91) trzymany klawisz krzywej: STEP, VOLUME, TIME, SMOOTH */
        static const pdef_t PV[4] = {{"STEP", 1, 16, 1, F_NUM, 0}, {"VOLUME", 0, 100, 100, F_NUM, 0}, {"TIME", 0, 15, 0, F_NUM, 0},
                                     {"SMOOTH", 0, 100, 20, F_NUM, 0}};
        const uint8_t *g = glob.gbc + ce * 32 + ui.cv_step * 2;
        for (k = 0; k < 4; k++) {
            d[k] = &PV[k];
            v[k] = (int16_t)(k == 0 ? ui.cv_step + 1 : k == 1 ? g[0] : k == 2 ? g[1] : glob.gbc[PF_NCV * 32 + ce]);
            txt[k][0] = 0;
            if (k == 2 && !v[k])
                text_cat(txt[k], "LIVE");
            else if (k == 2) {
                text_cat(txt[k], "-");
                int_text(v[k], txt[k] + 1, 0);
                text_cat(txt[k], "/16");
            } else {
                int_text(v[k], txt[k], 0);
                if (k)
                    text_cat(txt[k], "%");
            }
        }
        col_y = 34;
        col_h = 56;
        draw_cols(d, v, txt, 1);
        col_y = col_h = -1;
        ed = -2;                             /* (gałki makr niżej: nie) */
    } else if (ed >= 0) {                    /* (v0.90) trzymany klawisz efektu: jego SYNC, START, LENGTH (nie ma: puste pole) */
        const uint32_t c = glob.pfs[ed], q = c >> 8 & 7u;
        for (k = 0; k < 4; k++) {
            const int on = k == 0 ? (int)(PF_HAS_SYNC >> ed & 1u) : k < 3 ? (int)(PF_HAS_RUN >> ed & 1u) : 0;
            d[k] = on ? &PC[k] : 0;
            v[k] = (int16_t)(k == 0 ? (int32_t)(c & 1u) : k == 1 ? (int32_t)(c >> 1 & 127u) : (int32_t)(q ? q : (uint32_t)pf_def_len(ed)));
            txt[k][0] = 0;
            if (on && k == 1) {
                int_text(v[k], txt[k], 0);
                text_cat(txt[k], "%");
            } else if (on) {
                text_cat(txt[k], k == 0 ? L_SYNC[v[k] & 1] : L_LEN[v[k]]);
            }
        }
        col_y = 34;
        col_h = 56;
        draw_cols(d, v, txt, 1);
        col_y = col_h = -1;
    } else
    for (k = 0; k < 4; k++) {
        d[k] = &PD[k];
        v[k] = ui.pf_mac[k];
        txt[k][0] = 0;
        if (k == 2 && off) {                 /* THROW: echo DUB bez pamięci */
            text_cat(txt[k], "NO MEMORY");
        } else if (k == 0 && !v[k]) {
            text_cat(txt[k], "OFF");
        } else if (k == 0) {
            text_cat(txt[k], v[k] < 0 ? "LP " : "HP ");
            int_text(v[k] < 0 ? -v[k] : v[k], txt[k] + 3, 0);
        } else {
            int_text(v[k], txt[k], 0);
            text_cat(txt[k], "%");
        }
    }
    if (ed == -1) {
        col_y = 34;
        col_h = 56;
        col_red = off ? 1u << 2 : 0u;
        draw_cols(d, v, txt, 1);
        col_y = col_h = -1;
        col_red = 0;
    }
    if (ui.pf_bank) {                        /* (v0.91) krzywe: 4 klawisze, pod nimi 16 kroków krzywej (ustawianej / grającej / ostatniej) */
        const int32_t c = ce >= 0 ? ce : ogs_pf_cv >= 0 ? ogs_pf_cv : ui.cv_last, ps = ogs_pf_cv == c ? ogs_pf_cstep : -1;
        const uint8_t *g = glob.gbc + c * 32;
        char nm[12] = "CURVE 1";
        for (k = 0; k < PF_NCV; k++) {
            const uint32_t on = ui.cv_held >> (k * 2) & 1u, lat = ui.cv_lat >> k & 1u;
            nm[6] = (char)('1' + k);
            cv_rect(8 + k * 57, 96, 53, 14, on ? C_WHITE : lat ? C_HI : k == c ? C_DIM : C_LINE);
            cv_text(8 + k * 57 + (53 - text_w(&FONT_XS, nm)) / 2, 99, &FONT_XS, nm, on || lat ? C_BLACK : C_GRAY);
        }
        for (k = 0; k < 16; k++) {
            const int32_t h = g[k * 2] * 30 / 100, bx = 8 + k * 14;
            cv_rect(bx, 114, 12, 32, k == ps ? C_LINE : RGB(10, 10, 14));
            if (h)
                cv_rect(bx, 146 - h, 12, h, k == ps ? C_WHITE : C_HI);
            if (g[k * 2 + 1])                /* CZAS: kreska u góry (dłuższa = dalej wstecz) */
                cv_rect(bx + 1, 114, 1 + g[k * 2 + 1] * 10 / 15, 2, C_WHITE);
            if (ce >= 0 && k == ui.cv_step)
                cv_rect(bx, 148, 12, 2, C_WHITE);
        }
        line = ogs_pf_cv >= 0 ? "CURVE PLAYING" : "HOLD FX + F3 G3 A3 B3: CURVE 1-4";
        lc = ogs_pf_cv >= 0 ? C_HI : C_GRAY;
    } else
    for (k = 0; k < 15; k++) {               /* klawiatura F3..F5: biały = trzymany, kolor = zatrzaśnięty */
        uint32_t b = 1u << WK[k];
        cv_rect(8 + k * 15, 98, 14, 36, ui.pf_keys & b ? C_WHITE : ui.pf_lat & b ? C_HI
                                        : off & b ? rgb_mix(C_BLACK, RGB(255, 60, 50), 18) : C_LINE);
    }
    for (k = 0; k < 10; k++) {
        uint32_t b = 1u << BK[k];
        cv_rect(8 + BX[k] * 15 - 5, 98, 10, 21, ui.pf_keys & b ? C_WHITE : ui.pf_lat & b ? C_HI
                                                : off & b ? rgb_mix(C_BLACK, RGB(255, 60, 50), 30) : C_DIM);
    }
    if (mem && (!act || off >> (ui.pf_last % PF_N) & 1u)) {   /* bez pamięci: dlaczego (klawisz, który nie gra: jego nazwa) */
        why[0] = 0;
        text_cat(why, act ? PF_NAME[ui.pf_last % PF_N] : WHY[mem]);
        text_cat(why, act ? ": NO MEMORY (" : ": ONLY GATE LPF HPF CRUSH");
        if (act) {
            text_cat(why, WHY[mem]);
            text_cat(why, ")");
        }
        line = why;
        lc = RGB(255, 60, 50);
    }
    center(140, &FONT_XS, line, lc);
    foot_hint = ed >= 0 ? (PF_HAS_SYNC >> ed & 1u ? "KEY HELD: KNOBS SET THIS EFFECT" : "THIS EFFECT HAS NO SETTINGS")
                        : "SEL: LATCH   HOLD KEY + KNOB: EFFECT SETUP";
    body_blit();
}

static void draw_sqt(void)                       /* narzędzia wzoru: SELECT wybiera, SEL robi, HOME wraca */
{
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    sq_title("TOOLS", "PATTERN");
    for (i = 0; i < NST; i++) {
        int32_t y = 28 + i * 16, on = i == ui.sqt_cur;   /* (v0.87) 7 wierszy + napis nad wyższym paskiem podpowiedzi */
        if (on)
            cv_rect(4, y - 1, 232, 16, C_HI);
        cv_text(12, y, &FONT_S, i == ST_CLEAR && mt_ui() ? "CLEAR TRACK" : ST_NAME[i], on ? C_BLACK : C_WHITE);
        if (i == ST_MOVE) {                      /* (v0.66.2) z której ścieżki (gałka) */
            char m[3] = {'T', (char)('1' + ui.sqt_from), 0};
            cv_text(228 - text_w(&FONT_S, m), y, &FONT_S, m, on ? C_BLACK : C_HI);
        }
        if (i >= ST_ZINV) {                      /* kierunek gałki: wartość po prawej */
            const char *v = L_INV[glob.g[i == ST_ZINV ? G_SQZINV : G_SQK1INV]];
            cv_text(228 - text_w(&FONT_S, v), y, &FONT_S, v, on ? C_BLACK : C_HI);
        }
    }
    if (ui.sqt_sure)
        center(28 + NST * 16, &FONT_XS, ui.sqt_cur == ST_CLEAR && mt_ui() ? "SEL AGAIN: CLEAR THIS TRACK"
                                       : "SEL AGAIN: CLEAR ALL NOTES", RGB(255, 70, 60));
    foot_hint = ui.sqt_cur == ST_MOVE ? "KNOB: FROM TRACK   SEL: MOVE   HOME: BACK" : "SELECT: CHOOSE   SEL: DO IT   HOME: BACK";
    body_blit();
}

static void info_row(int32_t y, const char *k, const char *v)
{
    cv_text(12, y, &FONT_XS, k, C_GRAY);
    cv_text(228 - text_w(&FONT_XS, v), y, &FONT_XS, v, C_HI);
}

static void draw_info(void)            /* SYSTEM > INFO: wersja, procesor, drugi rdzeń, presety */
{
    static const char *const WHY[6] = {"ON", "MISREAD", "LATE", "BARRED", "FAULT", "NO ANSWER"};
    char t[24], n[12];
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "INFO", C_WHITE);
    info_row(30, "FIRMWARE", APP_NAME " " OGS_VERSION);
    int_text((int32_t)plat_cpu_pct(), t, 0);
    text_cat(t, "%   PEAK ");
    int_text((int32_t)cpu_peak, n, 0);
    text_cat(t, n);
    text_cat(t, "%");
    info_row(46, "CPU", t);
    info_row(62, "2ND CORE", plat_cpu2_on() ? "ON" : !glob.g[G_CPU2] ? "OFF" : WHY[plat_cpu2_gave_up() % 6]);
    int_text((int32_t)plat_xruns(), t, 0);
    info_row(78, "AUDIO DROPOUTS", t);
    int_text(user_count(), t, 0);
    text_cat(t, " / 128");
    info_row(94, "USER PRESETS", t);
    int_text((int32_t)__builtin_popcount(syx.used), t, 0);
    text_cat(t, " / 8   CZ ");
    int_text((int32_t)__builtin_popcount(syx.cz_used), n, 0);
    text_cat(t, n);
    text_cat(t, " / 16");
    info_row(110, "DX7 BANKS", t);
    int_text(plat_battery_raw(), t, 0);      /* surowy odczyt (progi kresek: 531 / 561 / 591) */
    text_cat(t, plat_battery() == 4 ? "   USB" : "");
    info_row(126, "BATTERY", t);
    foot_hint = "SEL / HOME: BACK";
    body_blit();
}

static void draw_reset(void)           /* SYSTEM > FACTORY RESET: co, potem drugie potwierdzenie */
{
    static const char *const OPT[3] = {"SETTINGS", "PRESETS", "EVERYTHING"};
    static const char *const WHAT[3] = {"GLOBAL SETTINGS TO DEFAULT", "DELETES USER PRESETS + DX7 BANKS",
                                        "SETTINGS + PRESETS + SOUNDS"};
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "FACTORY RESET", C_WHITE);
    for (i = 0; i < 3; i++) {
        int on = i == ui.reset_opt;
        if (ui.reset_sure && !on)
            continue;
        cv_rect(8, 32 + i * 28, 224, 24, on ? C_DIM : C_LINE);
        center(36 + i * 28, &FONT_S, OPT[i], on ? C_WHITE : C_GRAY);
    }
    center(116, &FONT_XS, WHAT[ui.reset_opt], C_HI);
    foot_hint = ui.reset_sure ? "SURE?  SEL: YES   HOME: NO" : "SELECT: CHOOSE   SEL: OK   HOME: BACK";
    foot_keep = ui.reset_sure;               /* pytanie przed kasowaniem: zawsze */
    foot_col = ui.reset_sure ? C_WHITE : 0;
    body_blit();
}

/* (v0.92) kopia dźwięku perkusji: pytanie, SELECT TAK / NIE, SEL zatwierdza, HOME = NIE */
static void draw_cpy(void)
{
    char t[24], n[8];
    int32_t i;
    cv_begin(240, BODY_H, C_BLACK);
    cv_text(8, 4, &FONT_M, "COPY SOUND", C_WHITE);
    t[0] = 0;
    cp_name(ui.cp_a, n);
    text_cat(t, n);
    text_cat(t, "  >  ");
    cp_name(ui.cp_b, n);
    text_cat(t, n);
    center(34, &FONT_B, t, C_HI);
    t[0] = 0;
    text_cat(t, "SOUND ");
    text_cat(t, n);
    text_cat(t, " WILL BE REPLACED");
    center(60, &FONT_XS, t, C_GRAY);
    center(74, &FONT_XS, "(PAN, SEND AND CHOKE STAY)", C_DIM);
    for (i = 0; i < 2; i++) {                    /* TAK / NIE */
        const int on = (i == 0) == (ui.cp_yes != 0);
        const int32_t x = 24 + i * 100;
        if (on)
            note_bar(x, x + 92, 94, 26, C_HI);
        else
            cv_rect(x, 94, 92, 26, C_LINE);
        cv_text(x + (92 - text_w(&FONT_S, i ? "NO" : "YES")) / 2, 99, &FONT_S, i ? "NO" : "YES", on ? C_BLACK : C_GRAY);
    }
    foot_hint = "SELECT: YES / NO   SEL: OK   HOME: NO";
    foot_keep = 1;
    body_blit();
}

static void draw_note(void)            /* SYSTEM > BACKUP / CALIBRATION: jak to zrobić */
{
    cv_begin(240, BODY_H, C_BLACK);
    if (ui.note == SY_BACKUP) {
        cv_text(8, 4, &FONT_M, "BACKUP", C_WHITE);
        center(36, &FONT_XS, "PRESETS AND SETTINGS", C_GRAY);
        center(52, &FONT_XS, "TO AND FROM THE COMPUTER (.SYX STAY THERE)", C_GRAY);
        center(76, &FONT_XS, "CONNECT USB, THEN ON THE COMPUTER:", C_HI);
        center(92, &FONT_XS, "OG SYNTH MANAGER > BACKUP", C_WHITE);
    } else if (ui.note == SY_UPDATE) {       /* (v1.1.0, P39) tryb aktualizacji chipu bez REC przy włączaniu */
        cv_text(8, 4, &FONT_M, "UPDATE MODE", C_WHITE);
        center(36, &FONT_XS, "FOR M-UPGRADE AND OTHER INSTALLERS", C_GRAY);
        center(52, &FONT_XS, "(THE SAME AS REC AT POWER-ON)", C_GRAY);
        center(76, &FONT_XS, "THE SCREEN GOES DARK", C_HI);
        center(92, &FONT_XS, "TO LEAVE: TURN THE FM-1 OFF AND ON", C_WHITE);
    } else {
        cv_text(8, 4, &FONT_M, "CALIBRATION", C_WHITE);
        center(36, &FONT_XS, "KEYS, KNOBS AND BUTTONS", C_GRAY);
        center(60, &FONT_XS, "TURN THE FM-1 OFF, THEN HOLD", C_HI);
        center(76, &FONT_XS, "OCT-  AND  OCT+", C_WHITE);
        center(92, &FONT_XS, "WHILE TURNING IT ON", C_HI);
    }
    foot_hint = ui.note == SY_UPDATE ? "SEL: UPDATE MODE   HOME: BACK" : "SEL / HOME: BACK";
    body_blit();
}

static void draw(void)
{
    if ((ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) || ui.page == V_SQN) {   /* NOTES: cały ekran; karta nuty na nim */
        notes_top();
        cv_begin(240, BODY_H, C_BLACK);
        draw_notes_page();
        if (ui.page == V_SQN)
            draw_sqn();
        cv_blit(0, 32);
        notes_chips();
        return;
    }
    if (ui.page != V_HOME)                   /* HOME bez górnego paska (wszystko w pierwszym wierszu i na dole) */
        header();
    if (ui.page == V_NAME)
        draw_name();
    else if (ui.page == V_SAVE)
        draw_save();
    else if (ui.page == V_ALLP)
        draw_allp();
    else if (ui.page == V_IMPORT)
        draw_import();
    else if (ui.page == V_ENGP)
        draw_engp();
    else if (ui.page == V_MOD)
        draw_mod();
    else if (ui.page == V_INFO)
        draw_info();
    else if (ui.page == V_RESET)
        draw_reset();
    else if (ui.page == V_NOTE)
        draw_note();
    else if (ui.page == V_FX)
        draw_fx();
    else if (ui.page == V_FXL)
        draw_fxl();
    else if (ui.page == V_MTL)
        draw_mtl();
    else if (ui.page == V_SCN)
        draw_scn();
    else if (ui.page == V_SEQ)
        draw_seq();
    else if (ui.page == V_ARP)
        draw_arp();
    else if (ui.page == V_PERF)
        draw_perf();
    else if (ui.page == V_SQT)
        draw_sqt();
    else if (ui.page == V_CPY)
        draw_cpy();
    else if (ui.page == V_HOME && ui.mode_until) {   /* (v0.86) napis SOLO / MULTI */
        header();
        draw_mode();
    } else if (ui.page == V_HOME)
        draw_home();
    else
        draw_page();
    footer();
}

static void speak(void)
{
    if (ann_on() && ann_where()) {
        char t[64], nm[24];
        t[0] = 0;
        text_cat(t, "OG SYNTH ");
        text_cat(t, E->name);
        text_cat(t, ". ");
        preset_name(CUR, nm);
        text_cat(t, nm);
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(plat_ms());
}

/* ekran startowy: logo OG Synth (app/ogs_logo.h, tools/make_logo.py) i wersja, białe na czarnym. Platforma rysuje go
 * zaraz po włączeniu ekranu (APP_SPLASH); zostaje do SPLASH_MS od startu albo do pierwszego dotyku panelu / klawisza
 * (dźwięk i klawisze działają od razu). Symulator: tak samo (app_ui_init rysuje go sam). */
#include "ogs_logo.h"
#define SPLASH_MS 4500u                      /* v0.19: dłużej (było 2 s), co najmniej SPLASH_MIN od app_ui_init */
#define SPLASH_MIN 3000u

void app_splash(void)
{
    const int32_t h = OGS_LOGO_H + 28, ox = (240 - OGS_LOGO_W) / 2;
    int32_t x, y;
    char t[16] = "V";
    lcd_fill(0, 0, 240, 240, RGB(0, 0, 0));
    cv_begin(240, (uint32_t)h, RGB(0, 0, 0));
    {                                        /* (v0.41) serie (ogs_logo.h): dekodowane w pętli rysowania */
        uint32_t at = 0, left = 0, v = 0;
        for (y = 0; y < OGS_LOGO_H; y++)
            for (x = 0; x < OGS_LOGO_W; x++) {
                if (!left) {
                    uint32_t b = at < sizeof OGS_LOGO ? OGS_LOGO[at++] : 0u;
                    v = b & 0x80u ? b >> 3 & 15u : 0u;
                    left = (b & 0x80u ? b & 7u : b) + 1u;
                }
                left--;
                if (v)
                    cv_pset(ox + x, y, RGB(v * 17u, v * 17u, v * 17u));
            }
    }
    text_cat(t, OGS_VERSION);
    cv_text((240 - text_w(&FONT_XS, t)) / 2, OGS_LOGO_H + 12, &FONT_XS, t, RGB(255, 255, 255));
    cv_blit(0, (uint32_t)(240 - h) / 2);
}

void app_ui_init(void)
{
    touch_on(1);
    glob_apply();                            /* krzywe klawiszy po touch_on */
    if (!splash_until) {                     /* pierwszy raz: logo jeszcze chwilę */
        uint32_t now = plat_ms();
        splash_until = now + SPLASH_MIN > SPLASH_MS ? now + SPLASH_MIN : SPLASH_MS;
        splash_on = 1;
        ui.input_at = 0;
        app_splash();
    } else {
        lcd_fill(0, 0, 240, 240, C_BLACK);
    }
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, lit;
    static int32_t last_note = -2;
    static uint8_t was_dirty;
    uint32_t now, pct;
    int32_t note;
    input();
    mt_scene_presets();                      /* (v0.73) scena MULTI: brzmienia po przygaszeniu */
    {                                        /* (v0.88) perkusja: inny dźwięk (też MIDI): numer, gałki i wykres strony od razu */
        static int32_t lane = -1;
        const int32_t d = drum_page_lane();
        if (d != lane) {
            lane = d;
            ui.redraw = 1;
        }
    }
    /* czas PO wejściu: gałka zapisuje quick_at / sel_at... plat_ms() z obsługi; wzięty wcześniej now bywał o 1 ms
     * mniejszy, now - quick_at przekręcał się na ogromną liczbę i strona szybka na mgnienie wracała do HOME (v0.30) */
    now = plat_ms();
    pct = vol_pct();                         /* to samo, co na ekranie (inaczej VOL zostawał na 1 %) */
    speak();
    if (splash_on) {                         /* logo: do SPLASH_MS albo pierwszego dotyku */
        if (now < splash_until && !ui.input_at)
            return;
        splash_on = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        ui.redraw = 1;
    }
    {                                        /* GLO SCREEN OFF: podświetlenie gaśnie po czasie bez dotyku panelu */
        uint32_t off = SCROFF_S[glob.g[G_SCROFF]] * 1000u;
        int dark = off && now - ui.input_at >= off;
        if (dark != ui.screen_off) {
            ui.screen_off = (uint8_t)dark;
#ifndef PLAT_HOST
            fm1_lcd_backlight(!dark);
#endif
            ui.redraw = 1;
        }
    }
    out_unmute();                            /* zapisy flasha z poprzedniej klatki zrobione: dźwięk wraca płynnie */
    if (ui.page == V_HOME || ch_txt[0])      /* (v1.1.0) CHORD NAME na HOME */
        chord_name_upd(plat_ms());
    {                                        /* (v1.1.0, P7) CC IN z przerwania */
        uint32_t q;
        for (q = 0; q < 4u; q++)
            if (cc_req[q]) {
                const uint32_t v = cc_req[q] & 0x7Fu;
                cc_req[q] = 0;
                cc_knob(q, v);
            }
    }
    if (pend_preset >= 0 && (dk_quiet || !eng_sounding() || preset_eng(pend_preset) != set.eng)) {
        int32_t id = pend_preset;            /* preset czekał na przygaszenie: teraz (cisza, bez trzasku) */
        preset_go(id);
        ui.redraw = 1;
    }
    {
        static uint32_t dk_seen;
        if (dk_done != dk_seen) {            /* przełącznik wszedł przy zerze: gwiazdka SAVE od nowa */
            dk_seen = dk_done;
            set.edited[set.eng] = (uint8_t)snap_differs(set.eng);
            ui.redraw = 1;
        }
    }
    if (pc_req >= 0) {                       /* Program Change: n-ty preset bieżącego silnika (0 = INIT) */
        int32_t id = eng_first(set.eng), n = pc_req;
        pc_req = -1;
        while (n-- > 0)
            id = preset_step(id, 1, set.eng);
        if (id != preset_now()) {
            preset_go(id);
            ui.redraw = 1;
        }
    }
    if (ui.allp_sel && now - ui.sel_at >= SEL_EQ_MS && CUR >= ID_USER && CUR < ID_SYX && ui.page == V_ALLP) {
        ui.allp_sel = 0;                     /* (v1.0.2) ALL PRESETS: SEL trzymany 0,8 s na USER = okno DELETE? */
        ui.allp_del = 1;
        ui.redraw = 1;
    }
    if (ui.page == V_FX && ui.sel_hold && !ui.sel_used
        && now - ui.sel_at >= SEL_EQ_MS) {   /* SEL trzymany: lista efektów; (v0.92, decyzja użytkownika 2026-11-04) wszędzie 0,8 s jak na EQ
                                              * (SEL + SELECT przesuwa efekt) */
        ui.sel_used = 1;
        fxl_open();
    }
    if (ui.page == V_HOME && mt_ui() && ui.sel_hold && !ui.sel_used && now - ui.sel_at >= SEL_SHOW_MS) {   /* (v0.86) główny ekran MULTI: lista silników */
        ui.sel_used = 1;
        mtl_open();
    }
    if (ui.home_hold && ui.page == V_HOME && now - ui.home_at >= HOME_HOLD_MS) {   /* (v0.86) HOME trzymany: SOLO / MULTI */
        ui.home_hold = 0;
        ui.home_used = 1;                    /* (bez okna ENGINE przy puszczeniu) */
        mode_toggle();
    }
    if (ui.mode_until && now >= ui.mode_until) {
        ui.mode_until = 0;
        ui.redraw = 1;
    }
    {                                        /* (v0.31) efekty w 16 bitach: zmiana = nowa klatka, pierwszy = komunikat 3 s */
        static uint32_t lo_seen;
        uint32_t lo = mt_ui() || *(volatile uint16_t *)&mt_seen != mt_ver ? 0u : ogs_fx_lo();   /* (v0.82) MULTI: wysyłki zawsze w 16 bitach, bez komunikatu;
                                                                                                * (v0.86) wyjście z MULTI: bez chwili ze starym układem */
        if (lo != lo_seen) {
            if (lo && !lo_seen)
                lo_note_until = (now + 3000u) | 1u;
            lo_seen = lo;
            ui.redraw = 1;
        }
        if (lo_note_until && (int32_t)(now - lo_note_until) >= 0) {
            lo_note_until = 0;
            ui.redraw = 1;
        }
    }
    if (ui.page == V_FX) {                   /* ostrzeżenie CPU: zmiana stanu = nowa klatka */
        static uint8_t heavy;
        uint8_t h = plat_cpu_pct() >= 80u;
        if (h != heavy) {
            heavy = h;
            ui.redraw = 1;
        }
    }
    if (ui.rec_hold) {                       /* (v0.92) REC: puszczony przed 400 ms = nagrywanie, trzymany = RECORD (drugi raz: z powrotem) */
        int up = !(plat_buttons() >> B_REC & 1u);
        if (up || now - ui.rec_at >= 400u) {
            ui.rec_hold = 0;
            if (up)
                sq_rec_key();
            else if (ui.page == V_SEQ && ui.sq_pg == SQ_REC)
                sq_view_back();
            else
                sq_open(SQ_REC);
        }
    }
    if (ui.play_hold) {                      /* PLAY w NOTES: puszczony przed 400 ms = od kursora */
        int up = !(plat_buttons() >> B_PLAY & 1u);
        if (up || now - ui.play_at >= 400u) {
            ui.play_hold = 0;
            sq.from = up ? ui.sq_cur : 0u;
            sq_play_key();
            if (!up)
                app_say("PLAY", "FROM START");
        }
    }
    if (ui.pf_hold) {                        /* (v0.47) FX: trzymany 300 ms = PERFORM; krótko (puszczenie) = FX / MORE */
        if (!(plat_buttons() >> B_FX & 1u)) {
            ui.pf_hold = 0;
            if (ui.page == V_PERF && !ui.pf_keys && !ui.cv_held)   /* (v0.90) klawisz efektu trzymany: strona zostaje (gałki ustawiają efekt) */
                pf_leave();
            else if (!ui.pf_used)
                fx_press();
        } else if (ui.page != V_PERF && now - ui.pf_at >= 300u && !rk_on()) {   /* (v0.57) RACK: FX + klawisz = MUTE */
            pf_enter();
        }
    }
    if (ui.arp_hold) {                       /* ARP: trzymany 400 ms = ustawienia / powrót; krótko = ON / OFF */
        if (!(plat_buttons() >> B_ARP & 1u)) {
            ui.arp_hold = 0;
            if (ui.arp_done) {
            } else if (rk_on() && now - ui.arp_at >= 400u) {   /* (v0.57) RACK: trzymany bez klawisza = ustawienia (przy
                                                              * puszczeniu: ARP + klawisz = SOLO) */
                go_arp();
            } else if (ui.page == V_ARP) {   /* w ustawieniach ARP: MORE (jak w każdej sekcji; ON / OFF poza nimi); (v0.91, decyzja
                                              * użytkownika 2026-11-03) tylko 1 <-> MORE, CHORD pod SELECT */
                ui.arp_more = (uint8_t)(ui.arp_more == 0u ? 1u : 0u);
                ui.knob = -1;
                ui.redraw = 1;
            } else {
                arp_toggle();
            }
        } else if (!ui.arp_done && now - ui.arp_at >= 400u && !rk_on()) {
            ui.arp_done = 1;
            if (ui.page >= V_GLO || PANEL_PAGE(ui.page) || ui.page == V_SQT || ui.page == V_SQN)
                go_arp();
        }
    }
    if (ui.grp_hold) {                       /* EDIT / ENV / LFO trzymany: pierwsza strona sekcji */
        uint32_t b = ui.grp_hold - 1u;
        if (!(plat_buttons() >> b & 1u)) {
            ui.grp_hold = 0;
            if (b == B_SEQ && !ui.grp_done)
                go_seq();
        } else if (!ui.grp_done && now - ui.grp_at >= 400u) {
            ui.grp_done = 1;
            if (b == B_SEQ)                  /* (v0.88) ustawienia sekwencera; w MULTI w nich też sceny */
                seq_hold();
            else {
                show(b == B_EDIT ? G_EDIT : b == B_ENV ? G_ENV : G_LFO, 0);
                ui.more_lock = 0;            /* trzymany: pierwsza strona bez MORE (krótki nacisk już przełączył) */
            }
        }
    }
    if (ui.page == V_SEQ && sq.play) {       /* odtwarzanie: linia w piano rollu (co 1/32), widok za nią po 2 s */
        static uint32_t at;
        uint32_t p = sq.pos / 12u;
        if (p != at) {
            at = p;
            if (ui.sq_pg == SQ_NOTES && now - ui.sq_moved > 2000u && sq.pos / sq_span() != ui.sq_cur / sq_span()) {
                ui.sq_cur = sq.pos / sq_span() * sq_span();   /* widok: takty, które grają */
                sq_pick(0);
                ui.redraw = 1;
            } else if (ui.sq_pg == SQ_RACK) {   /* (v0.57) RACK: strona kroków za odtwarzaniem */
                if (sq.pos / RK_T / 16u != ui.rk_step % rk_steps() / 16u) {   /* (v0.87) od razu, bez czekania 2 s */
                    rk_goto(sq.pos / RK_T / 16u);
                    ui.redraw = 1;
                } else {
                    ui.redraw_roll = 1;
                }
            } else if (ui.sq_pg == SQ_NOTES) {
                static uint32_t bar;
                if (sq.pos / SQ_BAR != bar) {     /* nowy takt: pasek u góry też */
                    bar = sq.pos / SQ_BAR;
                    ui.redraw = 1;
                } else {
                    ui.redraw_roll = 1;          /* sama linia odtwarzania: tylko roll */
                }
            } else {
                ui.redraw = 1;
            }
        }
    }
    if (ui.page == V_ARP) {                  /* ARP: kroki na wykresie */
        static uint8_t w;
        if (ar.hist_w != w) {
            w = ar.hist_w;
            ui.redraw_roll = 1;                  /* kroki ARP: sama strona (bez paska i stopki) */
        }
    }
    (void)out_quiet();                       /* co klatkę: kiedy ostatnio coś grało */
    if (ui.page == V_FX && fx_cur()->type == FX_EQ && fx_on(fx_cur()) && !fxc.x[set.eng][FX_X_EQ + EQX_AN]) {   /* (v0.92) widmo EQ: 10 / s */
        static uint32_t eq_at;
        if (now - eq_at >= 100u) {
            eq_at = now;
            ui.redraw = 1;
        }
    }
    if (ui.page == V_FX && (fx_cur()->type == FX_COMP || fx_cur()->type == FX_MBAND)) {   /* GR meter: 15 / s */
        static uint32_t gr_at;
        if (now - gr_at >= 66u && ((fx_cur()->mode && fm_fabsf(ogs_fx_meter[0]) + fm_fabsf(ogs_fx_meter[1]) + fm_fabsf(ogs_fx_meter[2])
                                    + fm_fabsf(ogs_fx_meter[3]) > 0.05f) || gr_show[0] != 0.0f || gr_show[1] != 0.0f
                                    || gr_show[2] != 0.0f || gr_show[3] != 0.0f)) {
            gr_at = now;
            ui.redraw = 1;
        }
    }
    {                                        /* wykres na żywo (FILTER WAVE / PHASE): 10 / s, gdy gra, i raz po */
        static uint32_t live_at;
        static uint8_t was_live;
        int live = ui.page >= 0 && ui.page < E->npages && E->graph_live && E->graph_live(ui.page)
                   && glob.g[G_VPAGES] && eng_sounding();
        if ((live && now - live_at >= 100u) || (!live && was_live)) {
            live_at = now;
            ui.redraw = 1;
        }
        was_live = (uint8_t)live;
    }
    sq_autosave(now);
    if (ui.page == V_INFO && now - ui.knob_at >= 500u) {   /* INFO: liczby na żywo */
        ui.knob_at = now;
        ui.redraw = 1;
    }
    if (plat_cpu_pct() > cpu_peak)
        cpu_peak = plat_cpu_pct();
    note = eng_held_note();
    {                                        /* bateria / USB: stopka sama (bez czekania na dotyk) */
        static int last_bat = -2;
        int b = plat_battery();
        if (b != last_bat) {
            last_bat = b;
            ui.redraw = 1;
        }
    }
    if (sq_hint_until && now >= sq_hint_until) {
        sq_hint_until = 0;
        ui.redraw = 1;
    }
    if (pct != last_pct || note != last_note || (ui.msg_until && now >= ui.msg_until)) {
        last_pct = pct;
        last_note = note;
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        ui.redraw = 1;
    }
    if (ui.quick && !ui.gest && now - ui.quick_at >= QUICK_MS && ui.qback >= 0)
        show_page(ui.qback);                 /* (v0.65) strona chwilowa z gestu LFO / ENV: wraca tam, skąd */
    else if ((ui.quick && !ui.gest && now - ui.quick_at >= QUICK_MS)   /* strona szybka: wraca do HOME */
             || (ui.browse && now >= ui.browse_until))   /* SELECT z HOME bez akcji: też */
        go_home();
    if (set_dirty && !was_dirty)
        ui.dirty_at = now;
    was_dirty = set_dirty;
    if (backup_back) {
        backup_back = 0;
        go_home();
        app_say("RESTORED", "FROM USB");
    }
    if (set_dirty && !backup_hold && ui.page >= V_GLO && out_quiet() && now - ui.dirty_at >= AUTOSAVE_MS
        && now - ui.input_at >= FLASH_IDLE_MS) {
        settings_save();                     /* preset, oktawa, silnik, brzmienie: same, w ciszy (zapis flasha) */
        was_dirty = 0;
    }
    if (mt_gdirty && !backup_hold && ui.page >= V_GLO && out_quiet() && now - ui.input_at >= FLASH_IDLE_MS) {
        mt_gdirty = 0;                       /* (v0.64) MULTI: ścieżki, głośności, MUTE — jak brzmienia, w ciszy */
        glob_dirty = 1;
        glob_commit();
    }
    scope_on = (ui.page == V_HOME && glob.g[G_VHOME] && !ui.mode_until)   /* (v0.92) też widmo EQ */
            || (ui.page == V_FX && fx_cur()->type == FX_EQ && !fxc.x[set.eng][FX_X_EQ + EQX_AN]);
    if (scope_on && ui.page == V_HOME && !ui.redraw && now - ui.scope_at >= 40u) {   /* oscyloskop: 25 klatek/s, tylko gdy gra;
                                                         * (v1.1.0) tylko HOME: przy EQ bufor dla widma, bez rysowania na FX */
        if (eng_sounding())
            ui.scope_tail = 4;
        if (ui.scope_tail) {
            ui.scope_tail--;
            draw_scope();
            ui.scope_at = now;
        }
    }
    if (ui.knob >= 0 && now - ui.knob_at < 1500u)
        lit = 1;
    else if (lit) {                          /* podświetlenie gałki gaśnie */
        lit = 0;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        ui.redraw_roll = 0;
        draw();
    } else if (ui.redraw_roll) {
        ui.redraw_roll = 0;
        if (ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) {
            cv_begin(240, BODY_H, C_BLACK);
            draw_notes_page();
            cv_blit(0, 32);
        } else if (ui.page == V_SEQ && ui.sq_pg == SQ_RACK) {
            draw_seq();
        } else if (ui.page == V_ARP) {
            draw_arp();
        }
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "preset"))
        *got = CUR;
    else if (!strcmp(what, "eng"))
        *got = set.eng;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "held"))
        *got = eng_held_note() >= 0;
    else if (!strcmp(what, "vis"))
        *got = glob.g[G_VHOME] | glob.g[G_VPAGES] << 1;
    else if (what[0] == 'p' && what[1] >= '0' && what[1] <= '9')   /* pN: parametr N bieżącego silnika */
        *got = E->p[atoi(what + 1) % E->np];
    else
        return 0;
    return 1;
}
#endif

/* (v1.1.0) listy wartosci parametrow (ogs_lists.h): pdef_t.list = numer w tej tabeli */
const char *const *const ogs_lists[LI_N] = {
    [LI_L_ONOFF] = L_ONOFF,
    [LI_L_MIDICH] = L_MIDICH,
    [LI_L_CPU2] = L_CPU2,
    [LI_L_LIGHTS] = L_LIGHTS,
    [LI_L_KEYL] = L_KEYL,
    [LI_L_SCROFF] = L_SCROFF,
    [LI_L_COLOR] = L_COLOR,
    [LI_L_TOPKEYS] = L_TOPKEYS,
    [LI_L_CLKIN] = L_CLKIN,
    [LI_L_INV] = L_INV,
    [LI_L_HINTS] = L_HINTS,
    [LI_L_MTMST] = L_MTMST,
    [LI_L_MTCTL] = L_MTCTL,
    [LI_L_TUNEK] = L_TUNEK,
    [LI_L_CLICK] = L_CLICK,
    [LI_L_CINL] = L_CINL,
    [LI_L_RECQ] = L_RECQ,
    [LI_L_PC12] = L_PC12,
    [LI_L_SCALE] = L_SCALE,
    [LI_L_VOICE] = L_VOICE,
    [LI_CHORD_NAME] = CHORD_NAME,
    [LI_L_SYNC] = L_SYNC,
    [LI_L_LEN] = L_LEN,
    [LI_LB_WAVE] = LB_WAVE,
    [LI_LB_COMB] = LB_COMB,
    [LI_LB_MODE] = LB_MODE,
    [LI_LB_KTRK] = LB_KTRK,
    [LI_LB_LW] = LB_LW,
    [LI_LB_FT] = LB_FT,
    [LI_LB_LM] = LB_LM,
    [LI_LB_GT] = LB_GT,
    [LI_LB_MXS] = LB_MXS,
    [LI_LJ_MODE] = LJ_MODE,
    [LI_LJ_CHO] = LJ_CHO,
    [LI_LJ_LW] = LJ_LW,
    [LI_LJ_SUB] = LJ_SUB,
    [LI_LJ_MODEL] = LJ_MODEL,
    [LI_LJ_VCA] = LJ_VCA,
    [LI_LJ_LM] = LJ_LM,
    [LI_LA_SAW] = LA_SAW,
    [LI_LJ_MXS] = LJ_MXS,
    [LI_LDM_CHOKE] = LDM_CHOKE,
    [LI_LDM_HATS] = LDM_HATS,
    [LI_LDM_MODE] = LDM_MODE,
    [LI_LDU_SRC] = LDU_SRC,
    [LI_LDU_MOD] = LDU_MOD,
    [LI_LDU_EG] = LDU_EG,
    [LI_LDU_CHOKE] = LDU_CHOKE,
    [LI_LDU_UP] = LDU_UP,
    [LI_LDU_WG] = LDU_WG,
    [LI_LDU_MODE] = LDU_MODE,
    [LI_LX_MXS] = LX_MXS,
    [LI_LX_CURVE] = LX_CURVE,
    [LI_LX_MODE] = LX_MODE,
    [LI_LX_WAVE] = LX_WAVE,
    [LI_LX_LFW] = LX_LFW,
    [LI_LX_VMODE] = LX_VMODE,
    [LI_LX_PRIO] = LX_PRIO,
    [LI_LX_GMODE] = LX_GMODE,
    [LI_LX_FTYPE] = LX_FTYPE,
    [LI_LX_KTRK] = LX_KTRK,
    [LI_LX_VIEW] = LX_VIEW,
    [LI_LX_VIEW_1] = LX_VIEW + 1,
    [LI_LX_OPS] = LX_OPS,
    [LI_LX_CSRC] = LX_CSRC,
    [LI_LX_KTYPE] = LX_KTYPE,
    [LI_LFM_SINGER] = LFM_SINGER,
    [LI_LFM_SRC] = LFM_SRC,
    [LI_LFM_MODE] = LFM_MODE,
    [LI_LFM_LW] = LFM_LW,
    [LI_LFM_LM] = LFM_LM,
    [LI_LFM_CONS] = LFM_CONS,
    [LI_LFM_MXS] = LFM_MXS,
    [LI_LM_WAVE] = LM_WAVE,
    [LI_LM_FLT] = LM_FLT,
    [LI_LM_PM] = LM_PM,
    [LI_LM_NENV] = LM_NENV,
    [LI_LM_CHOKE] = LM_CHOKE,
    [LI_LM_MODE] = LM_MODE,
    [LI_LM_UPPER] = LM_UPPER,
    [LI_L_WAVE] = L_WAVE,
    [LI_L_RANGE] = L_RANGE,
    [LI_L_RANGE4] = L_RANGE4,
    [LI_L_MODE] = L_MODE,
    [LI_L_PRIO] = L_PRIO,
    [LI_L_RTRG] = L_RTRG,
    [LI_L_DTYPE] = L_DTYPE,
    [LI_L_FTYPE] = L_FTYPE,
    [LI_L_MSRC] = L_MSRC,
    [LI_L_KTRK] = L_KTRK,
    [LI_L_NOISE] = L_NOISE,
    [LI_L_CHORUS] = L_CHORUS,
    [LI_L_LWAVE] = L_LWAVE,
    [LI_L_MDEST] = L_MDEST,
    [LI_L_MXS] = L_MXS,
    [LI_LZ_W] = LZ_W,
    [LI_LZ_W2] = LZ_W2,
    [LI_LW_LSH] = LW_LSH,
    [LI_LZ_SRC] = LZ_SRC,
    [LI_LZ_DST] = LZ_DST,
    [LI_LZ_VIA] = LZ_VIA,
    [LI_LZ_KF] = LZ_KF,
    [LI_LW_VM] = LW_VM,
    [LI_LW_DT] = LW_DT,
    [LI_LZ_FT] = LZ_FT,
    [LI_LZ_PT] = LZ_PT,
    [LI_LW_LM] = LW_LM,
    [LI_LZ_EM] = LZ_EM,
    [LI_LZ_SUS] = LZ_SUS,
    [LI_LPH_EXC] = LPH_EXC,
    [LI_LPH_MODEL] = LPH_MODEL,
    [LI_LPH_MODE] = LPH_MODE,
    [LI_LPH_LW] = LPH_LW,
    [LI_LPH_LM] = LPH_LM,
    [LI_LPH_BOW] = LPH_BOW,
    [LI_LPH_MXS] = LPH_MXS,
    [LI_LS_AW] = LS_AW,
    [LI_LS_BW] = LS_BW,
    [LI_LS_MODE] = LS_MODE,
    [LI_LS_LW] = LS_LW,
    [LI_LS_LM] = LS_LM,
    [LI_LS_LT] = LS_LT,
    [LI_LS_KTRK] = LS_KTRK,
    [LI_LV_PMD] = LV_PMD,
    [LI_LS_MXS] = LS_MXS,
    [LI_L_ARMODE] = L_ARMODE,
    [LI_L_GRID] = L_GRID,
    [LI_L_ARCHORD] = L_ARCHORD,
    [LI_LV_AW] = LV_AW,
    [LI_LV_BW] = LV_BW,
    [LI_LV_MODE] = LV_MODE,
    [LI_LV_KTRK] = LV_KTRK,
    [LI_LV_LW] = LV_LW,
    [LI_LV_REV] = LV_REV,
    [LI_LV_LM] = LV_LM,
    [LI_LV_LT] = LV_LT,
    [LI_LV_MXS] = LV_MXS,
    [LI_LW_WAVE] = LW_WAVE,
    [LI_LW_LPG] = LW_LPG,
    [LI_LWS_MODE] = LWS_MODE,
    [LI_LWS_LW] = LWS_LW,
    [LI_LWS_LM] = LWS_LM,
    [LI_LWS_RANGE] = LWS_RANGE,
    [LI_LWS_ESRC] = LWS_ESRC,
    [LI_LWS_MXS] = LWS_MXS,
    [LI_LW_TAB] = LW_TAB,
    [LI_LW_SSH] = LW_SSH,
    [LI_LW_SOCT] = LW_SOCT,
    [LI_LW_SRC] = LW_SRC,
    [LI_LW_DST] = LW_DST,
    [LI_LW_VIA] = LW_VIA,
    [LI_LW_WM] = LW_WM,
    [LI_LW_NT] = LW_NT,
    [LI_LW_PH] = LW_PH,
    [LI_LW_FT] = LW_FT,
    [LI_LW_STK] = LW_STK,
    [LI_LW_UM] = LW_UM,
    [LI_LW_F2T] = LW_F2T,
    [LI_LW_DSM] = LW_DSM,
};
