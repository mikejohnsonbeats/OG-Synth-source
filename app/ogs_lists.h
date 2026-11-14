/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Mike Johnson Beats */
/* (v1.1.0) listy wartosci parametrow: pdef_t.list = numer listy w ogs_lists[] (0 = brak), parametr 12 B zamiast 16
 * (~5 KB flasha). Tabela na koncu app.c (po wszystkich listach). Nowa lista: dopisac tu i w ogs_lists[]. */
#pragma once
#define LI(x) LI_##x
enum {
    LI_0,
    LI_L_ONOFF, LI_L_MIDICH, LI_L_CPU2, LI_L_LIGHTS, LI_L_KEYL, LI_L_SCROFF, LI_L_COLOR, LI_L_TOPKEYS, LI_L_CLKIN,
    LI_L_INV, LI_L_HINTS, LI_L_MTMST, LI_L_MTCTL, LI_L_TUNEK, LI_L_CLICK, LI_L_CINL, LI_L_RECQ, LI_L_PC12, LI_L_SCALE, LI_L_VOICE, LI_CHORD_NAME, LI_L_SYNC, LI_L_LEN,
    LI_LB_WAVE, LI_LB_COMB, LI_LB_MODE, LI_LB_KTRK, LI_LB_LW, LI_LB_FT, LI_LB_LM, LI_LB_GT, LI_LB_MXS, LI_LJ_MODE,
    LI_LJ_CHO, LI_LJ_LW, LI_LJ_SUB, LI_LJ_MODEL, LI_LJ_VCA, LI_LJ_LM, LI_LA_SAW, LI_LJ_MXS, LI_LDM_HATS,
    LI_LDM_MODE, LI_LDU_SRC, LI_LDU_MOD, LI_LDU_EG, LI_LDU_UP, LI_LDU_WG, LI_LDU_MODE, LI_LX_MXS,
    LI_LX_CURVE, LI_LX_MODE, LI_LX_WAVE, LI_LX_LFW, LI_LX_VMODE, LI_LX_PRIO, LI_LX_GMODE, LI_LX_FTYPE, LI_LX_KTRK,
    LI_LX_VIEW, LI_LX_VIEW_1, LI_LX_OPS, LI_LX_CSRC, LI_LX_KTYPE, LI_LFM_SINGER, LI_LFM_SRC, LI_LFM_MODE,
    LI_LFM_LW, LI_LFM_LM, LI_LFM_CONS, LI_LFM_MXS, LI_LM_WAVE, LI_LM_FLT, LI_LM_PM, LI_LM_NENV, LI_LM_CHOKE,
    LI_LM_MODE, LI_LM_UPPER, LI_L_WAVE, LI_L_RANGE, LI_L_RANGE4, LI_L_MODE, LI_L_PRIO, LI_L_RTRG, LI_L_DTYPE, LI_L_FTYPE,
    LI_L_MSRC, LI_L_KTRK, LI_L_NOISE, LI_L_CHORUS, LI_L_LWAVE, LI_L_MDEST, LI_L_MXS, LI_LZ_W, LI_LZ_W2, LI_LW_LSH,
    LI_LZ_SRC, LI_LZ_DST, LI_LZ_VIA, LI_LZ_KF, LI_LW_VM, LI_LW_DT, LI_LZ_FT, LI_LZ_PT, LI_LW_LM, LI_LZ_EM, LI_LZ_SUS,
    LI_LPH_EXC, LI_LPH_MODEL, LI_LPH_MODE, LI_LPH_LW, LI_LPH_LM, LI_LPH_BOW, LI_LPH_MXS, LI_LS_AW, LI_LS_BW,
    LI_LS_MODE, LI_LS_LW, LI_LS_LM, LI_LS_LT, LI_LS_KTRK, LI_LV_PMD, LI_LS_MXS, LI_L_ARMODE, LI_L_GRID, LI_L_ARCHORD, LI_L_ARKEYS,
    LI_LV_AW, LI_LV_BW, LI_LV_MODE, LI_LV_KTRK, LI_LV_LW, LI_LV_REV, LI_LV_LM, LI_LV_LT, LI_LV_MXS, LI_LW_WAVE,
    LI_LW_LPG, LI_LWS_MODE, LI_LWS_LW, LI_LWS_LM, LI_LWS_RANGE, LI_LWS_ESRC, LI_LWS_MXS, LI_LW_TAB, LI_LW_SSH,
    LI_LW_SOCT, LI_LW_SRC, LI_LW_DST, LI_LW_VIA, LI_LW_WM, LI_LW_NT, LI_LW_PH, LI_LW_FT, LI_LW_STK, LI_LW_UM,
    LI_LW_F2T, LI_LW_DSM,
    LI_L_PICK,                                   /* (v1.3.0) GLO MIDI KNOB PICKUP */
    LI_L_FXKEEP,                                 /* (1.5.0) GLO AUDIO FX SEND */
    LI_L_SCNSQ,                                  /* (1.5.1) SEQ OPTIONS SCENE */
    LI_L_GLOUT,                                  /* (1.8.0) GLO MIDI GLIDE OUT */
    LI_N
};
extern const char *const *const ogs_lists[LI_N];
#define PLIST(d) (ogs_lists[(d)->list])          /* lista wartosci parametru d (F_LIST) */
