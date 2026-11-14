/* OG Synth — paczka presetów USER (OG SYNTH MANAGER): ramki SysEx do FM1 i pętla wgrywania. TEN SAM KOD wykonuje Manager
 * (app/web/manager.html, przez Web MIDI) i test tools/v180_pack_test.mjs (przez symulator) — test sprawdza dokładnie to, co
 * Manager wyśle. Dane presetów: paczka.js (window.OGS_PACZKA, tools/paczka/eksport.mjs).
 *
 * Komendy firmware 1.8.0 (app/app.c: app_sysex, pk_set):
 *   F0 7D 4F 47 53 26 nr dane(<=512) F7   kawałek nr całego brzmienia presetu (półbajty, młodszy pierwszy; nr 0 = nowy preset)
 *                                          -> F0 7D 4F 47 53 24 26 nr F7 (jak kawałki banku 01). OG Synth starszy niż 1.8.0
 *                                          nie odpowiada (sprawdzenie wersji).
 *   F0 7D 4F 47 53 27 00 F7               brzmienie z kawałków jako NOWY preset USER w pierwszym wolnym miejscu (zajętych nie
 *                                          nadpisuje), potem gra jak wczytany z PRESETS -> F0 7D 4F 47 53 25 nr st F7:
 *                                          st 0 = zapisany w USER nr + 1 (nr 0-127), 1 = USER pełne (nic nie zapisane),
 *                                          2 = zła suma / niepełne (kawałki jeszcze raz), 3 = złe dane, 4 = FM1 w trybie MULTI,
 *                                          5 = zapis do pamięci się nie udał. Powtórzone 27 bez nowych kawałków (spóźniona
 *                                          odpowiedź) = ta sama odpowiedź, nic drugi raz.
 *   F0 7D 4F 47 53 10 00 F7               pomiar (każda wersja OG Synth) -> 11 ...: tylko do sprawdzenia wersji (FM1 jest, ale
 *                                          starszy) po kawałku bez odpowiedzi.
 * Brzmienie (379 B przed półbajtami): rekord presetu jak we flashu (232 B: nazwa 12, silnik, FF, np, parametry po 1 B, „szerokie”
 * po 2 B: OGS_PACZKA.wide, reszta zera), 8 slotów efektów po 10 B (rodzaj, tryb, gałki 1-8; pusty: 28 i zera), 47 gałek ponad 8
 * (domyślne FXXDEF z app.c, na miejscach efektów łańcucha ich gałki), 16 bajtów dodatków, FNV-1a 32 (młodszy bajt pierwszy) z całej
 * reszty. FM1 gubi ok. 0,4 % ramek z komputera (odpowiedzi nie giną): krok bez odpowiedzi idzie jeszcze raz (do 3 razy). */
(function (g) {
  "use strict";
  const H = [0xF0, 0x7D, 0x4F, 0x47, 0x53];
  const PUSTY = 28;                                          // pusty slot efektów (FX_NONE)
  const NAME = 12, REC = 232, NP_MAX = REC - NAME - 3, FXN = 8, FXB = 10, NX = 47, XT = 16, LEN = REC + FXN * FXB + NX + XT;
  const KAW = 512;                                           // kawałek (jak bank DX7 01): 2 na preset
  // gałki ponad 8 (app.c FXXDEF) i miejsce każdego efektu z nimi (fx_xoff / fx_nx): COMPRESSOR, MULTIBAND, DELAY, REVERB, AUTO WAH, EQ
  const XDEF = [127, 0, 0, 0, 64, 92, 79, 92, 56, 127, 41, 44, 63, 127, 39, 44, 52, 127, 41, 44, 20, 20, 102, 0, 50, 30, 30, 77, 64, 110,
    0, 0, 80, 127, 64, 33, 89, 105, 29, 38, 38, 29, 0, 0, 0, 0, 0];
  const XOFF = { 5: [0, 4], 17: [4, 16], 6: [20, 4], 3: [24, 6], 18: [30, 4], 4: [34, 13] };
  const ST = ["", "pelne", "suma", "dane", "multi", "zapis"];   // status odpowiedzi 25 (st) -> stop wgraj()
  const ascii = (n) => [...String(n).toUpperCase().replace(/[^ A-Z0-9-]/g, " ").padEnd(NAME).slice(0, NAME)].map((c) => c.charCodeAt(0));
  function fnv(b, n) {                                       // FNV-1a 32 jak app.c fnv()
    let h = 0x811C9DC5;
    for (let i = 0; i < n; i++) h = Math.imul(h ^ b[i], 16777619) >>> 0;
    return h >>> 0;
  }
  const wideOf = (opt) => (opt && opt.wide) || (g.OGS_PACZKA && g.OGS_PACZKA.wide) || {};

  /* co w presecie nie pasuje do formatu (pusta lista = całe brzmienie da się wysłać) */
  function braki(pr, wide) {
    const b = [], w = (wide || wideOf())[pr.ei] || [];
    if (!/^[ A-Z0-9-]{1,12}$/.test(pr.n) || !pr.n.trim()) b.push("nazwa");
    if (!(pr.ei >= 0 && pr.ei < 16)) b.push("silnik");
    if (!Array.isArray(pr.p) || pr.p.length + w.filter((id) => id < pr.p.length).length > NP_MAX
        || pr.p.some((v, id) => !Number.isInteger(v) || (w.includes(id) ? v < -32768 || v > 32767 : v < -128 || v > 127))) b.push("parametry");
    if (!Array.isArray(pr.fx) || pr.fx.length !== FXN || pr.fx.some((f) => f[0] === PUSTY ? f.length !== 1
        : !(f[0] >= 0 && f[0] < PUSTY) || !(f[1] >= 0 && f[1] < 16) || f[2].length !== 8 || f[2].some((v) => !(v >= 0 && v <= 255))
          || f[3].length !== (XOFF[f[0]] ? XOFF[f[0]][1] : 0) || f[3].some((v) => !(v >= 0 && v <= 127)))) b.push("efekty");
    if (!Array.isArray(pr.xt) || pr.xt.length !== XT || pr.xt.some((v) => !(v >= 0 && v <= 255))) b.push("dodatki");
    return b;
  }

  /* całe brzmienie presetu: 379 B (z sumą) jak w pk_set */
  function stan(pr, wide) {
    const d = new Uint8Array(LEN + 4), w = (wide || wideOf())[pr.ei] || [];
    d.set(ascii(pr.n), 0);
    d[NAME] = pr.ei;
    d[NAME + 1] = 0xFF;
    d[NAME + 2] = pr.p.length;
    let at = NAME + 3;
    pr.p.forEach((v, id) => { d[at++] = v & 0xFF; if (w.includes(id)) d[at++] = v >> 8 & 0xFF; });
    const x = XDEF.slice();
    pr.fx.forEach((f, k) => {
      const o = REC + k * FXB;
      if (f[0] === PUSTY) { d[o] = PUSTY; return; }
      d[o] = f[0]; d[o + 1] = f[1]; d.set(f[2], o + 2);
      if (XOFF[f[0]]) f[3].forEach((v, j) => { x[XOFF[f[0]][0] + j] = v; });
    });
    d.set(x, REC + FXN * FXB);
    d.set(pr.xt, REC + FXN * FXB + NX);
    const h = fnv(d, LEN);
    for (let i = 0; i < 4; i++) d[LEN + i] = h >>> (8 * i) & 0xFF;
    return d;
  }

  /* kroki jednego presetu: m ramka (F0..F7), a początek odpowiedzi od bajtu 5 (24 26 nr / 25), ms czekanie */
  function kroki(pr, wide) {
    const d = stan(pr, wide), q = [];
    for (const b of d) q.push(b & 15, b >> 4);
    const k = [];
    for (let nr = 0; nr * KAW < q.length; nr++)
      k.push({ m: [...H, 0x26, nr, ...q.slice(nr * KAW, (nr + 1) * KAW), 0xF7], a: [0x24, 0x26, nr], ms: 1500 });
    k.push({ m: [...H, 0x27, 0x00, 0xF7], a: [0x25], ms: 5000 });   // (00: zapas; FM1 bierze ramki od 6 B) zapis do flasha trwa
    return k;
  }

  /* krok z odpowiedzią: do 3 razy; io.wait(a, ms) przed send (odpowiedź nie umknie) */
  async function krok(io, k, cb, pr) {
    for (let t = 0; t < 3; t++) {
      if (t && cb.retry) cb.retry(pr, k, t);
      const w = io.wait(k.a, k.ms);
      io.send(k.m, k);
      const r = await w;
      if (r) return r;
    }
    return null;
  }
  async function starszy(io) {                               // OG Synth jest (pomiar 10 -> 11), ale bez komendy 26
    for (let t = 0; t < 2; t++) {
      const w = io.wait([0x11], 1500);
      io.send([...H, 0x10, 0x00, 0xF7], null);
      if (await w) return true;
    }
    return false;
  }

  /* wgrywanie presetów po kolei. io: send(ramka, krok), wait(a, ms) -> Promise<odpowiedź F0..F7, której bajty od 5. zaczynają się
   * od a | null>, pause(ms). cb: start(pr, i), ok(pr, nr, i), retry(pr, krok | null = cały preset, próba). Wynik: { wynik: [{ pr, nr }],
   * stop, pr: na którym stanęło }; stop: "" (wszystkie) | "pelne" (USER pełne: ten i dalsze nie) | "wersja" (OG Synth starszy niż
   * 1.8.0: nic nie wysłane poza próbą) | "cisza" (FM1 nie odpowiada) | "multi" (FM1 w trybie MULTI) | "suma" (3 razy zła suma) |
   * "dane" (FM1 nie przyjął danych) | "zapis" (zapis do pamięci FM1 się nie udał) | "braki" (preset nie pasuje do formatu: nic). */
  async function wgraj(lista, io, cb = {}, opt = {}) {
    const wynik = [], wide = wideOf(opt);
    const z = lista.find((pr) => braki(pr, wide).length);
    if (z) return { wynik, stop: "braki", pr: z };
    for (const pr of lista) {
      if (cb.start) cb.start(pr, wynik.length);
      let r = null;
      for (let p = 0; p < 3 && !r; p++) {                     // cały preset jeszcze raz, gdy FM1 odpowie „zła suma” (st 2)
        if (p && cb.retry) cb.retry(pr, null, p);
        for (const k of kroki(pr, wide)) {
          r = await krok(io, k, cb, pr);
          if (!r) {
            const ver = !wynik.length && !p && k.m[5] === 0x26 && k.m[6] === 0 && await starszy(io);
            return { wynik, stop: ver ? "wersja" : "cisza", pr };
          }
        }
        if (r[7] === 2) r = null;
      }
      if (!r) return { wynik, stop: "suma", pr };
      if (r[7]) return { wynik, stop: ST[r[7]] || "zapis", pr };
      wynik.push({ pr, nr: r[6] });
      if (cb.ok) cb.ok(pr, r[6], wynik.length);
    }
    return { wynik, stop: "", pr: null };
  }

  g.ogsPaczka = { kroki, stan, braki, wgraj, fnv, PUSTY, LEN, PELNE: true };
})(typeof window !== "undefined" ? window : globalThis);
