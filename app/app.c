/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Mike Johnson Beats; based on the fm1-base template, Copyright (C) 2026 Charles Vestal */
#include "ogs_app_01.c"
typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -3 .. +3 (v0.21; do v0.20: -2 .. +2) */
    uint8_t eng;                         /* bieżący silnik */
    uint8_t rsv[2];                      /* [0] numeracja presetów (18; od 1.6.0 nieczytana), (2.0.1) 19: [1] = DRUM TUNE (DTUNE) */
    uint16_t preset[NENG_S];             /* ostatnio wczytany preset każdego silnika (nazwa na ekranie) */
    uint8_t edited[NENG_S];              /* brzmienie zmienione względem presetu */
    uint16_t np[NENG_S];                 /* ile parametrów zapisano (silnik urósł: reszta domyślna) */
    int16_t p[SET_P];
    int16_t pw[WX_N];                    /* v0.19: WAVE ponad swoje 128 miejsc (parametry 128..) */
    int16_t px[SET_PX];                  /* v0.22: silniki od VCO (ENG_POFF >= SET_P) */
} settings_t;
_Static_assert(sizeof(settings_t) <= PLAT_STORE_MAX, "ustawienia: jeden obiekt");
static settings_t set;
#include "ogs_app_02.c"
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static float bl[APP_BLOCK], br[APP_BLOCK];
    const engine_t *e;
    static float hx[2], hy[2], eq_g;         /* SPEAKER EQ: górnoprzepustowy ok. 150 Hz (mały głośnik nie charczy) */
    float gain = mt_void ? 0.0f : (float)master_q12 / 4096.0f * 7500000.0f;   /* (v1.1.2) pusta scena: cisza (gain_now wygładza) */
    uint32_t i, done = 0, w = scope_w;
    int sc = scope_on && !mt_void, eq = glob.g[G_SPKEQ], fading, want;
    float pk = 0.0f;
    const uint32_t bt0 = plat_ticks24();     /* (2.0.0) czas bloku: ogs_blk_t (koniec funkcji) */
    if (mt_seen != mt_ver || mt_xs < 0.0f || mx_hold) {   /* (v0.64) MULTI: zmiana trybu / ścieżek po przygaszeniu wyjścia;
                                                           * (v1.4.0) mx_hold: cicho, aż UI zapisze bliźniaki */
        if (((!mt_on && eng_play >= NENG) || mt_xg <= 0.0f) && !mx_hold) {   /* (start: od razu) */
            mt_seen = mt_ver;
            mt_apply();
            mt_xs = 1.0f;
        } else {
            mt_xs = -1.0f;
        }
    }
    if (rsm_apply()) {                        /* (2.0) SAMPLE REC: pamięć nagrania z puli efektów / z powrotem — pula od nowa */
        if (mt_on)
            mt_ver++;
        else
            ogs_fx_arena_give();
    }
    i = set.eng;                             /* zmiana silnika: nowy do wspólnego RAM, zanim cokolwiek zagra */
    if (i >= NENG)                           /* (ustawienia w trakcie wczytywania) */
        i = eng_play < NENG ? eng_play : ENG_MOOG;
    if (mt_on) {                             /* MULTI: wybrana ścieżka gra jako „silnik”, bez wymiany RAM */
        mt_focus_in();
        ly_sync();                           /* (1.8.0) KEYS LAYER: ścieżki z warstwą (po zmianie wybranej) */
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
    ctl.beat = ogs_fx_beat;                  /* (1.8.0, pkt 23) LFO SYNC silników: takt (ogs_lfo_lock) */
    ctl.blk++;
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
            ogs_fx_late = glob.mst.pp & (MS_PP_REV | MS_PP_DL);   /* (2.0) PRZED: mokre REVERB / DELAY dochodzi za MASTER */
            mt_sends(bl, br, c);             /* (v0.76) wspólne REVERB / DELAY (efekty presetów ścieżek) */
            mt_master(bl, br, c);            /* (2.0) MASTER: suma -> wysyłki PO -> [sloty bez perkusji -> + perkusja] -> PERFORM PO ->
                                              * MASTER -> wysyłki PRZED -> PERFORM PRZED */
            ogs_fx_sends_late(bl, br, c);
            if (mt_pfpre)
                ogs_pf_render(bl, br, c);
            if (ogs_pf_pb < 0 && !ogs_pf_st) /* PERFORM zmienia drogę, gdy nic nie gra (bez skoku w dźwięku) */
                mt_pfpre = (uint8_t)(glob.mst.pp >> 2 & 1u);
            ogs_fx_master(mt_mst >= 0 ? (uint32_t)mt_mst : (uint32_t)glob.g[G_MTMST], bl, br, c);   /* (v0.77) master; (2.0)
                                              * LIMIT z GLO zostaje na końcu, COMP przeszedł do MASTER */
            if (mt_ft >= 0)
                ogs_fx_track_meter(fx_mview ? MT_M : (uint32_t)mt_ft);   /* (v0.83) GR meter strony FX: wybrana ścieżka; (2.0) MASTER */
        } else {
            ogs_fx_render(fx_play(), fx_xplay(), bl, br, c);   /* efekty globalne za silnikiem; (v0.74) bez PERFORM */
        }
        if (OUT_NEW)                         /* (1.7.1, użytkownik 2026-11-10: „chcę mieć jak najczystsze brzmienie naszych silników”)
                                              * wszystkie silniki, SOLO i MULTI (za MASTER): limiter -2 dB z wyprzedzeniem, za nim
                                              * wyjście liniowe (było: tanh od v0.1.0 — głośne akordy „harczały”, FM6 najbardziej) */
            ogs_fx_limit(bl, br, c, lim_th());   /* (2.0) próg z GLO */
        if (rsm.st == RSM_ARM || rsm.st == RSM_REC)   /* (2.0) SAMPLE REC: to, co gra (przed metronomem i SPEAKER EQ); SOLO: nie
                                              * SAMPLE grający nagranie (nagrałby siebie) */
            rsm_take(bl, br, c, !mt_on || mt_seen == mt_ver, mt_on || eng_play != ENG_SMP || SMP[SMP_SLOT] != SA_REC);
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
            for (i = 0; i < c; i++) {            /* (1.7.1) wyjście liniowe (szczyt z limitera -2 dB; ±1 tylko na wypadek klik / EQ) */
                float l = OUT_NEW ? fm_clampf(bl[i], -1.0f, 1.0f) : OUT_LIN ? bl[i] : fm_tanhf(bl[i]);
                float r = OUT_NEW ? fm_clampf(br[i], -1.0f, 1.0f) : OUT_LIN ? br[i] : fm_tanhf(br[i]), m;
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
    mx_at(-1);                               /* (v1.4.0) brzmienia bliźniaków na swoje miejsca: UI widzi wybraną ścieżkę */
    if (want && dk_g <= 0.0f) {              /* przy zerze: zmiany z UI wchodzą, UI może wczytać preset */
        dk_apply();
        dk_quiet = dk_quiet_req;
    } else {
        dk_quiet = 0;
    }
    scope_w = w;
    out_lvl = pk;
#ifdef PLAT_WEB
    ogs_blk_t = plat_ticks24() - bt0;        /* (2.0.0) czas tego bloku dla silników (FM1 i symulator: n = APP_BLOCK) */
    if (!ogs_blk_t)
        ogs_blk_t = web_bpct * OGS_TPCT;     /* testy: udawany czas bloku (ogs_bpct), gdy zegar stoi */
#else
    ogs_blk_t = plat_ticks24() - bt0;        /* (2.0.0) czas tego bloku dla silników (FM1: n = APP_BLOCK zawsze) */
#endif
}
#include "ogs_app_03.c"
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
#include "ogs_app_04.c"
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
#include "ogs_app_05.c"
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
    uint8_t lr_cur, lr_on;               /* (v1.3.0) MIDI LEARN: wiersz listy; nauka trwa (czeka na kontroler) */
    uint32_t scope_at;                   /* ostatnia klatka oscyloskopu HOME */
    uint32_t browse_until;               /* SELECT z HOME: do kiedy czeka na akcję */
    uint32_t dirty_at;                   /* ostatnia zmiana do zapisania samemu */
    uint8_t browse;                      /* SELECT z HOME bez żadnej akcji: po browse_until wraca do HOME */
    uint8_t sel_hold, sel_used;          /* SEL trzymany (skróty, MORE, silnik, ALL); użyty: puszczenie nie
                                          * przełącza opcji strony */
    uint32_t sel_at;                     /* kiedy SEL naciśnięty: opcję przełącza tylko krótkie kliknięcie */
    int8_t glo_sec;                      /* GLO: -1 lista sekcji, albo GS_* */
    uint8_t mac_e;                       /* (1.5.0) GLO > ALGO KNOB: edytowany silnik (ALGO zmienia) */
    uint8_t glo_cur, glo_page, sy_cur;   /* kursor listy sekcji, strona w sekcji, kursor SYSTEM */
    uint8_t reset_opt, reset_sure;       /* FACTORY RESET: co (ustawienia / presety / wszystko), drugie potwierdzenie */
    uint8_t note;                        /* okno V_NOTE: SY_BACKUP albo SY_CALIB */
    uint8_t fx_slot, fx_win;             /* FX: bieżący slot 0..7, pierwszy z czterech widocznych w łańcuchu */
    uint8_t fx_m;                        /* (2.0) MULTI: strona MASTER (sloty glob.mst; HOME: z powrotem na stronę FX) */
    uint8_t fx_more;                     /* FX: strona gałek efektu (0, MORE, dalej kolejne; FX przełącza) */
    uint8_t pf_hold, pf_used, pf_latch, pf_last;   /* (v0.47) PERFORM: FX trzymany, klawisz użyty (puszczenie FX bez
                                                    * strony FX), LATCH, ostatni klawisz (nazwa na ekranie) */
    uint32_t pf_at, pf_keys, pf_lat;     /* FX wciśnięty kiedy; klawisze trzymane / zatrzaśnięte (bit = klawisz = PF_*) */
    int32_t pf_back;                     /* strona sprzed PERFORM */
    int8_t pf_mac[4];                    /* gałki: FILTER -100..100, CRUSH, THROW, DEPTH 0..100 */
    uint8_t pf_ord[PF_N], pf_n;          /* kolejność wciśnięć efektów odtwarzania (gra ostatni trzymany) */
    uint8_t pf_bank, cv_last, cv_lat, cv_step;   /* (v0.91) PERFORM: OCT = bank CURVES; krzywa ostatnia, zatrzaśnięte (bit), krok edycji */
    uint8_t pf_stay;                     /* (1.8.0, P73) PERFORM: 0 zamknięty, 1 otwarty FX (stary gest: puszczony FX zamyka), 2 został
                                          * otwarty (FX puszczony bez klawisza i gałki): do FX krótko, HOME, przycisku strony */
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
    uint8_t sq_duph;                     /* (1.8.0, pkt 2C) karta nuty > DUPLICATE: 1 HERE (w tym samym miejscu), 0 NEXT STEP (bajt wyrównania) */
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
    uint8_t sqt_src;                     /* (1.6.0, P62) COPY PATTERN / TRACK FROM: wzór, z którego (gałka) */
    int16_t sq_note;                     /* NOTES: podświetlona nuta (jedna, jak w Melodee), -1 pusty krok */
    int16_t gp_par;                      /* (glide) edycja skoków: nuta (rodzic) */
    uint8_t gp_on;                       /* (glide) NOTES: edycja skoków nuty gp_par (karta nuty > GLIDE); (1.8.0, pkt 8) 2: tryb MOVE skoku */
    uint8_t sq_rng;                      /* NOTES: zaznaczony zakres (SEL + SELECT, jak w FL): edycja i karta na wszystkich */
    uint16_t gp_mv;                      /* (1.8.0, pkt 8) tryb MOVE skoku: jego miejsce (bajty wyrównania) */
    uint32_t sq_ra, sq_re;               /* zakres: komórka startu i końca (tick w gridzie, w obie strony) */
    uint8_t play_hold;                   /* PLAY w NOTES: krótko = od kursora, trzymany = od początku */
    uint8_t rec_hold;                    /* (v0.92) REC: krótko = nagrywanie, trzymany = strona RECORD */
    uint8_t sq_selp;                     /* (1.7.2) wysokość ostatnio podświetlonej nuty + 1 (0: jeszcze żadna; w wolnym bajcie) */
    uint8_t mv_on;                       /* (1.8.0) NOTES: tryb MOVE (karta > MOVE): SELECT przesuwa nutę / zaznaczenie o GRID */
    uint32_t rec_at;
    uint32_t play_at;
    uint32_t play_T;                     /* (1.8.2) PLAY w NOTES: tick zegara SEQ przy naciśnięciu (zegar MIDI: start od tej chwili) */
    uint8_t sq_pitch;                   /* wysokość nowej nuty z K2 (ostatnio wpisana / zmieniona) */
    uint8_t sqn_row;                     /* karta nuty: wiersz */
    uint8_t st_on;                       /* klawisze wpisują (pusty krok albo REC) do puszczenia wszystkich */
    uint8_t sq_zv, sq_zh;                /* NOTES: zoom w pionie (SEL + K1: ile rzędów), w poziomie (SEL + K2: takty) */
    uint8_t cp_k, cp_yes;                /* (v0.92) kopia dźwięku: klawisz trzymany z SEL (+ 1), wybór TAK */
    int8_t cp_a, cp_b, cp_back;          /* skąd, dokąd (dźwięk), strona sprzed okna */
    uint8_t redraw_roll;                 /* odtwarzanie: tylko piano roll (bez paska i chipów: mniej przesyłania) */
    uint8_t redraw;
} ui;                                     /* (1.8.0, miejsce) bez inicjatora: .bss zamiast .data; pola niezerowe: ui_boot() */
#include "ogs_app_06.c"
OGS_UI void app_say(const char *a, const char *b)
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
#include "ogs_app_07.c"
OGS_UI static void input(void)
{
    static uint32_t eaten;                   /* klawisze zużyte jako skróty: ich puszczenie nie gasi nuty */
    static uint8_t knote[NKEYS];             /* nuta z chwili naciśnięcia: puszczenie gasi tę samą (zmiana oktawy w
                                              * trakcie nie zostawia wiszącej nuty ani nuty w ARP) */
    uint32_t keys, btn = plat_buttons(), k, avail = 0, shown, pr = 0, gk;
    int sel_shown;
    uint32_t chan = mt_ui() ? mt_ch(glob.g[G_MTFOC])   /* (v0.66.1) MULTI: MIDI OUT na kanale wybranej ścieżki (v1.0.1: z GLO) */
                  : glob.g[G_MIDICH] ? (uint32_t)glob.g[G_MIDICH] - 1u : 0u;
    int32_t e;
    ko_in(btn, chan);                        /* (1.5.1) CONTROLS OUT: przyciski, MASTER */
    for (k = 0; k < NB; k++)                 /* (v1.3.0) MIDI LEARN: przyciski z kontrolera; kliknięcie = jedna klatka (kolejne: co drugą) */
        if (lr_pc[k] != lr_ps[k] && !(ui.btn >> k & 1u)) {
            lr_ps[k]++;
            if (!(ko_bm >> k & 1u))
                btn |= 1u << k;
        }
    btn |= lr_hold & ~ko_bm;
    lr_frame(btn);
    touch_poll();
    keys = touch_keys();
    if (mt_ui() && ui.page != V_HOME && ui.page != V_ENGP && ui.page != V_MTL && ui.page != V_SCN)   /* (v0.85) poza głównym ekranem i oknami: kursor na wybranej (strony jej silnika) */
        mt_cur = (uint8_t)glob.g[G_MTFOC];
    ctl.dsemi = (int8_t)(DTUNE + glob.g[G_TRANSP]);   /* (v0.56) perkusja: przestrojenie; (2.0.1) DRUM TUNE, nie oktawa klawiatury */
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
            && !(ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) && !sel_keys_own()) {   /* (NOTES: SEL + klawisze = nuty w kursorze) */
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
        if (ui.grp_hold == B_SEQ + 1u && mt_ui() && (keys & bit) && !(ui.keys & bit)
            && key_label(k) >= K_OP1 && key_label(k) <= K_OP4) {   /* (1.6.0, K65 / P63) SEQ trzymany + OP1-4: MUTE T1-T4 (bez nuty;
                                                                    * SEQ nie otwiera już ustawień ani strony przy puszczeniu) */
            eaten |= bit;
            ui.grp_done = 1;
            mt_mute(key_label(k) - K_OP1);
            continue;
        }
        if (ui.arp_hold && mt_ui() && (keys & bit) && !(ui.keys & bit)
            && key_label(k) >= K_OP1 && key_label(k) <= K_OP4) {   /* (1.8.0, P77) ARP trzymany + OP1-4: KEYS LAYER T1-T4 (bez nuty; ARP
                                                                    * nie przełącza się i nie otwiera ustawień) */
            eaten |= bit;
            ui.arp_done = 1;
            mt_layer(key_label(k) - K_OP1);
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
            if (!ui.sel_hold) {                  /* klawisz: krok strony; (v1.3.1, decyzja użytkownika 2026-11-06) czarny z nadrukiem:
                                                  * strona perkusji od razu (jak SEL + klawisz, koło tak samo) */
                eaten |= bit;
                if (w >= 0)
                    rk_toggle((uint32_t)w);
                else if (label_pages() & bit)
                    label_key(key_label(k));
                continue;
            }
            if (w >= 0) {                        /* SEL + klawisz: wiersz tego dźwięku; dalej gra jak zwykle (REC: nagrywa) */
                ui.rk_row = (uint8_t)w;
                rk_view();
            }
            ui.sel_used = 1;
        }
        if ((ui.pf_hold || ui.page == V_PERF) && mt_ui() && (keys & bit) && !(ui.keys & bit)) {   /* (v0.47) FX trzymany + klawisz:
                                                                          * PERFORM, bez nuty; (v0.74) tylko w MULTI; (1.8.0) na ekranie
                                                                          * PERFORM klawisze zawsze = efekty (też bez FX: otwarty, P73) */
            eaten |= bit;
            pf_key(k, 1);
            continue;
        }
        if (((ui.pf_keys | ui.cv_held) & bit) && !(keys & bit))   /* puszczony klawisz PERFORM (FX już może być puszczony) */
            pf_key(k, 0);
        /* (v1.4.2, decyzja użytkownika 2026-11-07) perkusja: F#5 jak OP1-OP6 — SEL + F#5 = ENV, bez SEL gra; w RACK od razu strona
         * (label_pages wyżej). Do v1.4.1: bez SEL = ENV na każdej stronie */
        if ((bit & avail) && (keys & bit) && !(ui.keys & bit)) {
            eaten |= bit;
            label_key(key_label(k));
            ui.sel_used = 1;
            continue;
        }
        if (ui.sel_hold && ui.page == V_SEQ && ui.sq_pg == SQ_NOTES && !ui.gp_on && (keys & bit) && !(ui.keys & bit)) {
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
        if (ENG_GRP[set.eng] == EG_DRUM && !rk_on() && (keys & bit) && !(ui.keys & bit) && !sel_keys_own()
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
            if (glob.g[G_LOCAL])                 /* GLO LOCAL OFF: klawisze tylko do MIDI (dźwięk wraca z DAW); (1.8.0) EV_KEY: KEYS LAYER */
                ev_push(EV_ON(note, v) | EV_KEY(53u + k));
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
            if (ui.page == V_ALLP || ui.page == V_SCN || ui.page == V_LRN || ui.page == V_ENGP) {   /* (v1.0.2) ALL PRESETS: OK przy
                                                            * puszczeniu, trzymany = usuń USER; w oknie DELETE?: tak; (v1.1.2) lista scen tak
                                                            * samo; (v1.3.0) MIDI LEARN; (1.8.0, pkt 35) okno ENGINE: OK przy puszczeniu
                                                            * (SEL trzymany + gałka: efekty zostają, fx_keep) */
                if (ui.allp_del) {
                    if (ui.page == V_SCN)
                        scn_del();
                    else if (ui.allp_del == 2)   /* (2.0.2) brzmienie z banku DX7 / CZ */
                        voice_del();
                    else
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
            const int ok = ui.allp_sel == 1; /* (1.8.0, pkt 35) 2: SEL + gałka (efekty zostały): puszczenie nic nie robi */
            ui.allp_sel = 0;                 /* (v1.0.2) krótko (albo trzymany na nie-USER): OK; trzymany na USER: okno DELETE? */
            if (ok && (ui.page == V_ALLP || ui.page == V_SCN || ui.page == V_LRN || ui.page == V_ENGP))
                save_ok();
        } else if (!(btn & bit) && (ui.btn & bit) && ui.sel_hold) {
            ui.sel_hold = 0;
            ui.cp_k = 0;
            if (st_frz) {                        /* (1.8.0) STRETCH: po puszczeniu SEL zaznaczenie znowu = nuty w zakresie */
                st_frz = 0;
                sq_pick(0);
            }
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
                || ui.page == V_NOTE || ui.page == V_LRN)
                go_glo();
            break;
        case B_FX:                           /* (v0.47) krótko: przy puszczeniu, trzymany: PERFORM; (v0.74) PERFORM tylko w MULTI —
                                              * w SOLO FX od razu (RACK: FX + klawisz = MUTE, dalej przy puszczeniu) */
            if (ui.page == V_FX || ui.page >= V_GLO || ui.page == V_SEQ || ui.page == V_ARP || ui.page == V_PERF) {   /* (1.8.0)
                                                                     * PERFORM, który został otwarty: FX krótko = wyjście (app_frame) */
                if (!mt_ui() && !rk_on()) {
                    fx_press();
                    break;
                }
                ui.pf_hold = 1;
                ui.pf_used = 0;
                ui.pf_at = plat_ms();
            }
            break;
        case B_SEQ:                          /* (v0.88) przy puszczeniu: go_seq; trzymany 0,7 s: seq_hold; (1.6.0) trzymany + OP1-4 w MULTI =
                                              * MUTE ścieżki (v0.66-v0.89: ścieżka, od v0.90 SEL + OP) */
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
        case B_PLAY:                         /* NOTES (zatrzymany): krótko = od kursora, trzymany = od początku; (1.8.2) kursor na
                                              * początku: od razu, przy naciśnięciu (oba gesty dają to samo; było: przy puszczeniu) */
            if (!sq.play && ui.sq_cur && ((ui.page == V_SEQ && sq_roll()) || ui.page == V_SQN)) {
                ui.play_hold = 1;
                ui.play_at = plat_ms();
                ui.play_T = sq.T;
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
    if (ui.pf_stay == 2u && (btn & ~ui.btn & ~(1u << B_FX)))   /* (1.8.0) otwarty PERFORM: FX + inny przycisk (SEL, OCT ...) to nie
                                                               * kliknięcie FX (puszczony nie zamyka) */
        ui.pf_used = 1;
    for (k = 0; k < NE; k++) {
        const int32_t m = (int8_t)(lr_ec[k] - lr_es[k]), s = lr_stp[k];   /* (v1.3.0) MIDI LEARN: enkodery; kroki z gałek ze stopem bez
                                                                      * przyspieszenia */
        lr_es[k] = (uint8_t)(lr_es[k] + m);
        lr_stp[k] = 0;
        if (s)
            ui.enc_at[k] = 0;
        if ((e = ko_enc((int)k) + m + s) == 0)   /* (1.5.1) ko_enc: plat_enc + obroty dla CONTROLS OUT */
            continue;
        ui.input_at = plat_ms();                 /* GLO SCREEN OFF: każdy ruch budzi */
        if (ui.pf_stay == 2u)                    /* (1.8.0) otwarty PERFORM: FX + gałka to nie kliknięcie FX */
            ui.pf_used = 1;
        if (ui.allp_del && (ui.page == V_ALLP || ui.page == V_SCN))   /* (v1.0.2) okno DELETE?: gałki stoją */
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
                keep_fx = (uint8_t)fx_keep();    /* (1.8.0, pkt 35) SEL trzymany: z obecnymi efektami */
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
                } else if (ap_mvl()) {       /* (1.8.0) AUTO, tryb MOVE: SELECT o GRID, SEL + SELECT o krok */
                    ap_move(e);
                } else if (mv_live()) {      /* (1.8.0) tryb MOVE: SELECT o GRID, SEL + SELECT o tick */
                    if (ui.sel_hold)
                        ui.sel_used = 1;
                    sq_move(e, ui.sel_hold);
                } else if (gp_live()) {      /* (glide) edycja skoków: kursor po nucie (SEL + SELECT: nic); (1.8.0, pkt 8) tryb MOVE
                                              * skoku: SELECT o GRID, SEL + SELECT o tick */
                    if (ui.sel_hold)
                        ui.sel_used = 1;
                    if (ui.gp_on == 2)
                        gp_move(e, ui.sel_hold);
                    else if (!ui.sel_hold)
                        gp_cursor(e);
                } else if (sq_roll() && ui.sel_hold)
                    sq_range(e);
                else if (sq_roll())
                    sq_cursor((int32_t)ui.sq_cur + e * (int32_t)sq_grid());
                else                             /* (v0.88) ustawienia SEQ: strony */
                    sq_set_turn(e);
                continue;
            }
            if (ui.page == V_SQN) {          /* karta nuty: wiersz */
                ui.sqn_row = (uint8_t)clamp((int32_t)ui.sqn_row + e, 0, ui.sq_pg == SQ_AUTO ? 3 : ui.gp_on ? GP_NC - 1 : NSN - 1);   /* (1.6.0)
                                                                                                            * AUTO: 4; (glide) karta skoku: 3 */
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
            if (ui.page == V_LRN) {          /* (v1.3.0) MIDI LEARN: wiersz */
                if (!ui.lr_on)
                    ui.lr_cur = (uint8_t)clamp((int32_t)ui.lr_cur + (e > 0 ? 1 : -1), 0, LR_N - 1);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_NAME) {         /* okno nazwy: (1.7.2, decyzja użytkownika 2026-11-13) SELECT = kursor (było: litera;
                                              * OCT- / OCT+ dalej też kursor), K1 = litera */
                ui.cur = clamp(ui.cur + (e > 0 ? 1 : -1), 0, NAME_LEN - 1);
                ui.redraw = 1;
                continue;
            }
            if (ui.page == V_SAVE) {         /* okno SAVE: SELECT przełącza NADPISZ / NOWY; (1.6.0) FM6: w kółko z SEND AS DX7 */
                const uint32_t nn = save_n();
                ui.save_new = (uint8_t)((ui.save_new + (e > 0 ? 1u : nn - 1u)) % nn);
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
            else if (ui.page == V_HOME && mt_ui() && (ui.sel_hold || glob.g[G_MT0 + mt_at_cur()] >= NENG)) {   /* (v0.85) MULTI: główny
                                                                     * ekran, silnik ścieżki; (1.5.0, decyzja użytkownika 2026-11-07)
                                                                     * SEL + ALGO, samo ALGO jak w SOLO (pusta ścieżka: samo ALGO też) */
                ui.sel_used = 1;
                mt_engine(e);
            }
            else if (ui.page == V_ALLP) {    /* ALL PRESETS: ALGO po silnikach (v0.37, jak po grupach w oknie ENGINE) */
                keep_fx = (uint8_t)fx_keep();    /* (1.8.0, pkt 35) SEL trzymany: z obecnymi efektami */
                preset_go(eng_jump(preset_now(), e));
                ui.redraw = 1;
            }
            else if (ui.page == V_SEQ && ui.sel_hold) {   /* SEQ: SEL trzymany + ALGO = długość wzoru; (v0.88) co beat / krok / takt: OPTIONS */
                ui.sel_used = 1;
                if (sq_roll())                   /* (v0.91, decyzja użytkownika 2026-11-03) NOTES: SEL + ALGO = długość wzoru co GRID
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
            } else if (ui.page == V_SEQ && sq_roll()) {   /* NOTES (i AUTO): ALGO = GRID (jak snap w FL); (1.7.2) kolejność 1/1, 1/2,
                                                       * 1/4 ... 1/32 (numery 7, 6, 0 ... 5), w prawo drobniej */
                const int32_t r = clamp((sqp.grid >= 6u ? 7 - (int32_t)sqp.grid : (int32_t)sqp.grid + 2) + (e > 0 ? 1 : -1), 0, 7);
                int32_t g = r < 2 ? 7 - r : r - 2;
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
    if (GV(G_CTLOUT))                        /* (1.5.1) CONTROLS OUT: gałki ruszone na FM1 */
        ko_knobs(chan, btn);
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
        const uint8_t h = (uint8_t)(((ui.sel_hold || ui.allp_sel) && plat_ms() - ui.sel_at >= SEL_SHOW_MS) | arp_lay() << 1);   /* (1.8.0)
                                                                                     * też okna ALL PRESETS / ENGINE (podpowiedź SEL + gałka);
                                                                                     * ARP trzymany: KEYS LAYER */
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
        if (arp_lay())                           /* (1.8.0, P77) ARP trzymany: OP1-4 ścieżek z silnikiem (KEYS LAYER ON: pełnym) */
            for (k = 0; k < MT_N; k++)
                if (glob.g[G_MT0 + k] < NENG)
                    *(MT_LAY(k) ? &lit : &shown) |= 1u << LABEL_KEY[K_OP1 + k];
        int gl = glob.g[G_LIGHTS], kl = glob.g[G_KEYL];
        uint32_t bl = btn
                    | (oct_shown() > 0 ? 1u << B_OCTUP : oct_shown() < 0 ? 1u << B_OCTDN : 0u)   /* oktawa poza 0 (perkusja: DRUM TUNE) */
                    | (ui.page == V_HOME || ui.page == V_ALLP || ui.page == V_ENGP ? 1u << B_HOME
                       : ui.page == V_GLO || ui.page == V_INFO || ui.page == V_RESET || ui.page == V_NOTE || ui.page == V_LRN ? 1u << B_GLO
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
        if (sqg.a[AR_KLIGHT] && sq_scale() && ENG_GRP[set.eng] != EG_DRUM && ui.page != V_PERF
            && !(ui.page == V_SEQ && ui.sq_pg == SQ_NOTES)) {   /* (v1.2.0) ARP > KEY > LIGHTS (zamiast GLO KEY LIGHTS): klawisze
                                                                 * skali przygaszone, tonika jaśniej; RACK / NOTES / PERFORM: swoje */
            kd = 0;
            for (k = 0; k < NKEYS; k++) {
                const uint32_t pc = (uint32_t)key_note(k) % 12u;
                if (pc == sqg.a[AR_KEY])
                    kmid |= 1u << k;
                else if (sq_scale() >> pc & 1u)
                    kd |= 1u << k;
            }
        }
        if (rk_on()) {                           /* (v0.87) RACK zamiast KEY LIGHTS: krok z nutą średnio, grający pusty najciemniej,
                                                  * grający z nutą pełnym światłem */
            rk_leds();
            kb |= rk_keys(rk_led_note & rk_led_play);
            kmid = rk_keys(rk_led_note & ~rk_led_play);
            kd = rk_keys(rk_led_play & ~rk_led_note) | label_pages();   /* (v1.3.1) klawisze stron perkusji: przygaszone */
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
            if (ui.page == V_SEQ || ui.page == V_SQT || ui.page == V_SQN ? sqp_dirty || sqg_dirty : EDT(set.eng))
                fxon |= 1u << B_SAVE;            /* brzmienie zmienione: SAVE delikatnie (do wciśnięcia); (1.8.0) na stronach SEQ:
                                                  * wzór niezapisany (SAVE zapisuje wzór) */
            fxon |= secmid;
            if (!sqg.arp_on && (ui.page == V_ARP || sqg.latch))   /* ustawienia ARP, arp wyłączony: przygaszony; (1.5.0) też LATCH
                                                                * bez ARP (nuty wiszą) */
                fxon |= 1u << B_ARP;
            plat_lights_mid(fxon, (shown & ~lit) | kmid); /* dostępne skróty przygaszone, tylko przy trzymanym SEL */
        }
    }
}
#include "ogs_app_08.c"
void app_input_only(void) { input(); }
#include "ogs_app_09.c"
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
#include "ogs_app_10.c"
void app_ui_init(void)
{
    touch_on(1);
    if (!splash_until) {                     /* pierwszy raz: logo jeszcze chwilę */
        uint32_t now = plat_ms();
        splash_until = now + SPLASH_MIN > SPLASH_MS ? now + SPLASH_MIN : SPLASH_MS;
        splash_on = 1;
        ui.input_at = 0;
        cv_bgc[0] = 0;                       /* (2.0.2) logo na czarnym jak przy starcie (fm1-base: przed app_init), także z tłem motywu */
        app_splash();
    } else {
        lcd_fill(0, 0, 240, 240, C_BLACK);
    }
    glob_apply();                            /* krzywe klawiszy po touch_on; (2.0.2) po logo: tło motywu od pierwszej strony */
    ui.redraw = 1;
}
#include "ogs_app_11.c"
OGS_UI void app_frame(void)
{
    static uint32_t last_pct, lit;
    static int32_t last_note = -2;
    static uint8_t was_dirty;
    uint32_t now, pct;
    int32_t note;
    input();
    if (rsm.fresh && set.eng == ENG_SMP) {   /* (2.0) nowe nagranie: SAMPLE gra je (K1 = REC) — też po powrocie z innego silnika */
        rsm.fresh = 0;
        set_param(SMP_SLOT, SA_REC);
        EDT(set.eng) = (uint8_t)snap_differs(set.eng);
    }
    ap_frame();                              /* (1.6.0) AUTO: koniec dotyku gałki, SAVE po zmianach z linii */
    if (mx_lv && mt_xg <= 0.0f) {            /* (v1.4.0) do SOLO: wyjście cicho, bliźniaki do flasha, potem pula dla łańcucha */
        mx_store();
        mx_lv = 0;
        mt_ver++;
        mx_hold = 0;
    }
    mx_sync();                               /* (v1.4.0) bliźniaki: przed brzmieniami sceny */
    sg_frame();                              /* (1.8.0, P75 / P76) SONG / kolejka wzoru (ogs_auto.c) */
    mt_scene_presets();                      /* (v0.73) scena MULTI: brzmienia po przygaszeniu */
    if (set.eng == ENG_WT)                   /* (2.0, Shrub A) L2-L4 DRAW pojawiły się / zniknęły */
        wt_pages_fix();
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
    {                                        /* GLO SCREEN OFF: ekran czarny po czasie bez dotyku panelu */
        uint32_t off = SCROFF_S[glob.g[G_SCROFF]] * 1000u;
        int dark = off && now - ui.input_at >= off;
        if (dark != ui.screen_off) {
            ui.screen_off = (uint8_t)dark;
#ifndef PLAT_HOST
            /* (1.6.4, skrzynka #13, odtworzone na FM1) podświetlenie zostaje włączone: zgaszone (PA2) przy małym poborze
             * prądu FM1 wyłączał głośnik, diody i klawiaturę aż do wyłączenia zasilania (procesor i USB działały dalej).
             * Teraz jedno wypełnienie na czarno, potem lcd.c nic nie rysuje (lcd_dark); po przebudzeniu cały ekran od nowa
             * (jak po logo) */
            if (dark) {
                lcd_fill(0, 0, 240, 240, C_BLACK);
                lcd_dark = 1;
            } else {
                lcd_dark = 0;
                lcd_fill(0, 0, 240, 240, C_BLACK);
            }
#endif
            ui.redraw = 1;
        }
    }
    out_unmute();                            /* zapisy flasha z poprzedniej klatki zrobione: dźwięk wraca płynnie */
    if (ui.page == V_HOME || ch_txt[0])      /* (v1.1.0) CHORD NAME na HOME */
        chord_name_upd(plat_ms());
    if (pend_preset >= 0 && (dk_quiet || !eng_sounding() || preset_eng(pend_preset) != set.eng)) {
        int32_t id = pend_preset;            /* preset czekał na przygaszenie: teraz (cisza, bez trzasku) */
        preset_go(id);
        ui.redraw = 1;
    }
    {
        static uint32_t dk_seen;
        if (dk_done != dk_seen) {            /* przełącznik wszedł przy zerze: gwiazdka SAVE od nowa */
            dk_seen = dk_done;
            EDT(set.eng) = (uint8_t)snap_differs(set.eng);
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
    if (ui.allp_sel == 1 && now - ui.sel_at >= SEL_EQ_MS     /* (1.8.0, pkt 35) po SEL + gałce (2) nigdy */
        && (ui.page == V_ALLP ? (CUR >= ID_USER && CUR < ID_SYX) || bank_of(CUR) >= 0 : ui.page == V_SCN && glob.scn[ui.scn_cur].ok)) {
                                             /* (v1.1.2) też scena; (2.0.2) też brzmienie z banku DX7 / CZ */
        ui.allp_sel = 0;                     /* (v1.0.2) ALL PRESETS: SEL trzymany 0,8 s na USER = okno DELETE? */
        ui.allp_del = (uint8_t)(ui.page == V_ALLP && bank_of(CUR) >= 0 ? 2 : 1);
        ui.redraw = 1;
    }
    if (ui.allp_sel && ui.page == V_LRN && !ui.lr_on && now - ui.sel_at >= SEL_EQ_MS) {   /* (v1.3.0) MIDI LEARN: SEL trzymany = wyczyść */
        ui.allp_sel = 0;
        lr_clear();
    }
    if (ui.page == V_FX && ui.sel_hold && !ui.sel_used
        && now - ui.sel_at >= SEL_EQ_MS) {   /* SEL trzymany: lista efektów; (v0.92, decyzja użytkownika 2026-11-04) wszędzie 0,8 s jak na EQ
                                              * (SEL + SELECT przesuwa efekt) */
        ui.sel_used = 1;
        fxl_open();
    }
    if (ui.page == V_HOME && mt_ui() && ui.sel_hold && !ui.sel_used && now - ui.sel_at >= SEL_EQ_MS) {   /* (v0.86) główny ekran MULTI: lista
                                                                     * silników; (1.5.0) po 0,8 s jak lista efektów: SEL + ALGO = silnik */
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
        int up = !(btn_now() >> B_REC & 1u);
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
        int up = !(btn_now() >> B_PLAY & 1u);
        if (up || now - ui.play_at >= 400u) {
            ui.play_hold = 0;
            sq.from = up ? ui.sq_cur : 0u;
            sq.late = (uint16_t)(sq.ext ? sq.T - ui.play_T : 0u);   /* (1.8.2) zegar MIDI: wzór liczony od naciśnięcia (było: od
                                              * puszczenia / 0,4 s — ok. 150 ms za DAW); nuty z tej chwili grają (chase), odliczanie krótsze */
            sq_play_key();
            if (!up)
                app_say("PLAY", "FROM START");
        }
    }
    if (ui.pf_stay && ui.page != V_PERF)     /* (1.8.0) z PERFORM wyszedł przycisk strony (EDIT, GLO, SEQ, SELECT ...; też przy trzymanym FX) */
        pf_leave();
    if (ui.page == V_PERF) {                 /* (1.8.0) PERFORM nad stroną chwilową / przeglądaniem (2 s bez kręcenia): czas liczy się od
                                              * wyjścia z PERFORM (było: wracała do HOME w trakcie i zamykała PERFORM, który został) */
        ui.quick_at = now;
        ui.browse_until = now + QUICK_MS;
    }
    if (ui.pf_hold) {                        /* (v0.47) FX: trzymany 300 ms = PERFORM; krótko (puszczenie) = FX / MORE */
        if (!(btn_now() >> B_FX & 1u)) {
            ui.pf_hold = 0;
            if (ui.page == V_PERF) {
                /* (1.8.0, P73: skrzynka #15 „Loud Difficulty”, decyzje użytkownika 2026-11-13) FX trzymany i puszczony bez klawisza efektu
                 * i gałki: PERFORM zostaje otwarty (klawisze = efekty bez FX) do FX krótko (bez niczego innego), HOME albo przycisku
                 * strony; stary gest FX + klawisz / gałka: puszczony FX zamyka jak dotąd ((v0.90) klawisz efektu trzymany bez LATCH,
                 * klawisz krzywej: przy jego puszczeniu); w otwartym FX + klawisz nie zamyka */
                if (ui.pf_stay == 1u && !ui.pf_used)
                    ui.pf_stay = 2;
                else if (ui.pf_stay == 2u ? !ui.pf_used && now - ui.pf_at < 300u : !(ui.pf_latch ? 0u : ui.pf_keys) && !ui.cv_held)
                    pf_leave();
                ui.redraw = 1;
            } else if (!ui.pf_used)
                fx_press();
        } else if (ui.page != V_PERF && now - ui.pf_at >= 300u && !rk_on()) {   /* (v0.57) RACK: FX + klawisz = MUTE */
            pf_enter();
        }
    }
    if (ui.arp_hold) {                       /* ARP: trzymany 400 ms = ustawienia / powrót; krótko = ON / OFF */
        if (!(btn_now() >> B_ARP & 1u)) {
            ui.arp_hold = 0;
            if (ui.arp_done) {
            } else if (rk_on() && now - ui.arp_at >= 400u) {   /* (v0.57) RACK: trzymany bez klawisza = ustawienia (przy
                                                              * puszczeniu: ARP + klawisz = SOLO) */
                go_arp();
            } else {                         /* (v1.2.0, decyzja użytkownika 2026-11-06) także w ustawieniach ARP: ON / OFF (strony:
                                              * SELECT; v0.91 - v1.1.2 tam 1 <-> MORE) */
                arp_toggle();
            }
        } else if (!ui.arp_done && now - ui.arp_at >= (mt_ui() ? HOME_HOLD_MS : 400u) && !rk_on()) {   /* (1.8.0) MULTI: 0,7 s, jak SEQ
                                                                                         * (czas na ARP + OP1-4 = KEYS LAYER) */
            ui.arp_done = 1;
            if (ui.page >= V_GLO || PANEL_PAGE(ui.page) || ui.page == V_SQT || ui.page == V_SQN)
                go_arp();
        }
    }
    if (ui.grp_hold) {                       /* EDIT / ENV / LFO trzymany: pierwsza strona sekcji */
        uint32_t b = ui.grp_hold - 1u;
        if (!(btn_now() >> b & 1u)) {
            ui.grp_hold = 0;
            if (b == B_SEQ && !ui.grp_done)
                go_seq();
        } else if (!ui.grp_done && now - ui.grp_at >= (b == B_SEQ ? HOME_HOLD_MS : 400u)) {   /* (1.6.0, P63) SEQ: 0,7 s jak HOME
                                                                                         * (czas na SEQ + OP1-4 = MUTE) */
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
            if (sq_roll() && !ui.gp_on && !ui.mv_on && !ap_mv && now - ui.sq_moved > 2000u && sq.pos / sq_span() != ui.sq_cur / sq_span()) {
                if (ui.sq_note >= 0)             /* (1.8.0, pkt 31) jak ruch kursora: nuta najbliższa wysokością (było: wybór znikał,
                                                  * a następny K2 wpisywał nową nutę); edycja glide i MOVE: widok stoi */
                    ui.sq_selp = (uint8_t)(sqp.n[ui.sq_note].note + 1u);
                ui.sq_cur = sq.pos / sq_span() * sq_span();   /* widok: takty, które grają */
                if (!ui.sq_rng)
                    ui.sq_note = sq_cell_near(ui.sq_selp);
                sq_pick(0);
                ui.redraw = 1;
            } else if (ui.sq_pg == SQ_RACK) {   /* (v0.57) RACK: strona kroków za odtwarzaniem */
                if (sq.pos / RK_T / 16u != ui.rk_step % rk_steps() / 16u) {   /* (v0.87) od razu, bez czekania 2 s */
                    rk_goto(sq.pos / RK_T / 16u);
                    ui.redraw = 1;
                } else {
                    ui.redraw_roll = 1;
                }
            } else if (sq_roll()) {
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
    if (ui.page == V_FX && fx_cur()->type == FX_EQ && fx_on(fx_cur()) && !fx_xb(fx_cur())[FX_X_EQ + EQX_AN]) {   /* (v0.92) widmo EQ: 10 / s */
        static uint32_t eq_at;
        if (now - eq_at >= (anim_fast() ? 32u : 100u)) {   /* (2.0.2) przy zapasie CPU ~31 / s (było 10 / s) */
            eq_at = now;
            anim_kick();
        }
    }
    if (ui.page == V_FX && (fx_cur()->type == FX_COMP || fx_cur()->type == FX_MBAND)) {   /* GR meter: 15 / s */
        static uint32_t gr_at;
        if (now - gr_at >= (anim_fast() ? 32u : 66u) && ((fx_cur()->mode && fm_fabsf(ogs_fx_meter[0]) + fm_fabsf(ogs_fx_meter[1]) + fm_fabsf(ogs_fx_meter[2])
                                    + fm_fabsf(ogs_fx_meter[3]) > 0.05f) || gr_show[0] != 0.0f || gr_show[1] != 0.0f
                                    || gr_show[2] != 0.0f || gr_show[3] != 0.0f)) {
            gr_at = now;
            anim_kick();
        }
    }
    {                                        /* wykres na żywo (FILTER WAVE / PHASE): 10 / s, gdy gra, i raz po */
        static uint32_t live_at;
        static uint8_t was_live;
        int live = ui.page >= 0 && ui.page < E->npages && (E == &ENG[ENG_WT] ? wt_graph_live(ui.page) : E == &ENG[ENG_PD] && pd_graph_live(ui.page))
                   && glob.g[G_VPAGES] && eng_sounding();
        if ((live && now - live_at >= (anim_fast() ? 32u : 100u)) || (!live && was_live)) {   /* (2.0.2) ~31 / s przy zapasie */
            live_at = now;
            if (live)
                anim_kick();
            else
                ui.redraw = 1;                   /* raz po: całe */
        }
        was_live = (uint8_t)live;
    }
    {                                        /* (v1.2.3, decyzja użytkownika 2026-11-06) wyjście z NOTES (i karty nuty): oktawa sprzed
                                              * wejścia — zmiany z K2 (sq_oct_follow) i z OCT w NOTES tylko na czas edycji; perkusja:
                                              * OCT = strój, zostaje */
        static int8_t nt_oct = -128;
        const int in = (ui.page == V_SEQ && ui.sq_pg == SQ_NOTES) || ui.page == V_SQN;
        if (in && nt_oct == -128) {
            nt_oct = set.octave;
        } else if (!in && nt_oct != -128) {
            if (ENG_GRP[set.eng] != EG_DRUM)
                octave_step(nt_oct - set.octave);
            nt_oct = -128;
        }
    }
    sq_autosave(now);
    if (ui.page == V_INFO && now - ui.knob_at >= 500u) {   /* INFO: liczby na żywo */
        ui.knob_at = now;
        ui.redraw = 1;
    }
    if (plat_cpu_pct() > cpu_peak)
        cpu_peak = plat_cpu_pct();
#if OGS_BENCH || OGS_FRAMES
    {                                        /* (2.0) pomiar (SysEx 0x10): najdłuższa przerwa między klatkami (płynność ekranu po
                                              * kompresji czcionek), 0,1 ms; build pomiarowy (tools/ui_klatki_hw.py) */
        static uint32_t at;
        const uint32_t t = plat_ticks24(), d = (t - at) / (PLAT_TICKS_PER_US * 100u);
        if (at && d > ui_fmax)
            ui_fmax = (uint16_t)(d > 16383u ? 16383u : d);
        at = t;
    }
#endif
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
    /* (1.8.0, pkt 4: „po ponownym uruchomieniu ma się otwierać ostatnio używany ...”) zapis sam także na stronach FX / SEQ / ARP /
     * PERFORM / MOD, karcie nuty i TOOLS (do 1.7.1 tylko HOME, GLO i strony silnika: wyłączenie FM1 na innej stronie gubiło zmiany);
     * dalej nie w oknach przeglądania (ALL PRESETS, ENGINE, SAVE, nazwa, listy), żeby podgląd nie został „ostatnio używanym” */
    const int work = ui.page >= V_GLO || PANEL_PAGE(ui.page) || ui.page == V_SQN || ui.page == V_SQT || ui.page == V_MOD;
    if (set_dirty && !backup_hold && work && out_quiet() && now - ui.dirty_at >= AUTOSAVE_MS
        && now - ui.input_at >= FLASH_IDLE_MS) {
        settings_save();                     /* preset, oktawa, silnik, brzmienie: same, w ciszy (zapis flasha) */
        was_dirty = 0;
    }
    if ((mt_gdirty || (ui.page == V_GLO && (glob_dirty || mac_bd))) && !backup_hold && work && out_quiet()
        && now - ui.input_at >= FLASH_IDLE_MS) {   /* (1.8.0) też GLO i ALGO KNOB, gdy zostajesz w GLO (wcześniej: przy wyjściu z GLO) */
        if (mt_gdirty)
            glob_dirty = 1;
        mt_gdirty = 0;                       /* (v0.64) MULTI: ścieżki, głośności, MUTE — jak brzmienia, w ciszy */
        glob_commit();
    }
    {   /* (2.0.0) silnik / preset / tryb zmienione spoza panelu (SysEx, Manager, Program Change): pełne przerysowanie. Wcześniej
         * co 40 ms rysował się tylko pas oscyloskopu z tłem NOWEGO silnika, a nagłówek, nazwa presetu i stopka zostawały po starym
         * (zdjęcie użytkownika 2026-11-19: ALPHA / RAVE STAB, pod spodem „DRUMS” i uderzenia MORPH) */
        static uint32_t drawn_sig = 0xFFFFFFFFu;
        const uint32_t now_sig = (uint32_t)set.eng | (uint32_t)CUR << 8 | (uint32_t)mt_ui() << 24;
        if (now_sig != drawn_sig) {
            drawn_sig = now_sig;
            ui.redraw = 1;
        }
    }
    scope_on = (ui.page == V_HOME && glob.g[G_VHOME] && !ui.mode_until)   /* (v0.92) też widmo EQ */
            || (ui.page == V_FX && fx_cur()->type == FX_EQ && !fx_xb(fx_cur())[FX_X_EQ + EQX_AN]);
    if (scope_on && ui.page == V_HOME && !ui.redraw && now - ui.scope_at >= (anim_fast() ? 16u : 40u)) {   /* oscyloskop: (2.0.2) przy zapasie CPU co klatkę (~62 / s), inaczej co trzecią (~21 / s), tylko gdy gra;
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
        anim_body = ui.redraw == 2u;         /* (2.0.2) odświeżenie animacji: bez pasków i szczeliny */
        ui.redraw = 0;
        ui.redraw_roll = 0;
        if (ui.page != V_HOME && !anim_body) {             /* (2.0.2, użytkownik 2026-11-24: oscyloskop „przechodzi przez szczelinę”, jak serduszka
                                              * Bambo) wiersze 30-31 między górnym paskiem (0-29) a stroną (od 32) rysuje tylko HOME
                                              * (oscyloskop): na innych stronach czyste, w kolorze tła stron (płótno 2 wierszy, gfx.c) */
            cv_begin(240, 2, C_BLACK);
            cv_blit(0, 30);
        }
        draw();
        anim_body = 0;
    } else if (ui.redraw_roll) {
        ui.redraw_roll = 0;
        if (ui.page == V_SEQ && sq_roll()) {
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
#include "ogs_app_12.c"
#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "octave"))
        *got = oct_shown();
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
#include "ogs_app_13.c"
