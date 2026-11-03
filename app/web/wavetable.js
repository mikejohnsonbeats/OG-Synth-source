// SPDX-License-Identifier: MPL-2.0
// OGS: tablica .wav (Serum, Vital, …) -> tablica USER silnika WAVE na FM1 (OG SYNTH MANAGER i symulator).
// Zwykły skrypt (działa też z pliku, bez serwera): globalThis.ogsWave = { parse, frames, sysex }.
// Ramka FM1: 256 próbek, harmoniczne 1..127 (reszta i tak odpada w silniku), do 63 ramek równo z całej tablicy.
// SysEx (bez F0 / F7): 7D 4F 47 53 20 nr ramek nazwa | 22 nr ramka dane (256 x 2 x 7 bitów) | 23 nr.
(function () {
  const FR = 256, H = 127, MAXF = 63;

  // .wav -> { x: Float32Array (pierwszy kanał), size: długość ramki z pliku (Serum: znacznik "clm "), 0 nieznana }
  function parse(buf) {
    const dv = new DataView(buf), u8 = new Uint8Array(buf), tag = (o) => String.fromCharCode(...u8.subarray(o, o + 4));
    if (buf.byteLength < 12 || tag(0) !== "RIFF" || tag(8) !== "WAVE") throw new Error("to nie jest plik WAV");
    let fmt = null, data = null, size = 0;
    for (let o = 12; o + 8 <= buf.byteLength;) {
      const id = tag(o), n = dv.getUint32(o + 4, true), b = o + 8;
      if (id === "fmt ") {
        let f = dv.getUint16(b, true);
        if (f === 0xFFFE && n >= 26) f = dv.getUint16(b + 24, true);   // WAVE_FORMAT_EXTENSIBLE: podformat
        fmt = { f, ch: dv.getUint16(b + 2, true), bits: dv.getUint16(b + 14, true) };
      } else if (id === "data") {
        data = [b, Math.min(n, buf.byteLength - b)];
      } else if (id === "clm ") {                               // Serum: "<!>2048 ..."
        const m = /<!>(\d+)/.exec(String.fromCharCode(...u8.subarray(b, b + Math.min(n, 32))));
        if (m) size = parseInt(m[1], 10);
      }
      o = b + n + (n & 1);
    }
    if (!fmt || !data) throw new Error("brak danych w pliku WAV");
    const bps = fmt.bits / 8, step = bps * fmt.ch, len = Math.floor(data[1] / step), x = new Float32Array(len);
    for (let i = 0; i < len; i++) {
      const o = data[0] + i * step;
      if (fmt.f === 3 && fmt.bits === 32) x[i] = dv.getFloat32(o, true);
      else if (fmt.f === 3 && fmt.bits === 64) x[i] = dv.getFloat64(o, true);
      else if (fmt.bits === 16) x[i] = dv.getInt16(o, true) / 32768;
      else if (fmt.bits === 24) x[i] = ((dv.getUint8(o) | dv.getUint8(o + 1) << 8 | dv.getInt8(o + 2) << 16)) / 8388608;
      else if (fmt.bits === 32) x[i] = dv.getInt32(o, true) / 2147483648;
      else if (fmt.bits === 8) x[i] = (dv.getUint8(o) - 128) / 128;
      else throw new Error(`nieobsługiwany format WAV (${fmt.bits} bit)`);
    }
    return { x, size };
  }

  // próbki -> do 63 ramek po 256 (Int16Array każda), bez DC, wspólna skala
  function frames(w) {
    const x = w.x;
    let S = w.size > 0 ? w.size : x.length % 2048 === 0 ? 2048 : x.length < 2048 ? x.length : x.length % 256 === 0 ? 256 : 2048;
    if (S < 8) throw new Error("za krótki plik");
    const N = Math.max(1, Math.floor(x.length / S)), M = Math.min(N, MAXF), out = [];
    const hm = Math.min(H, Math.floor(S / 2) - 1);
    let pk = 0;
    for (let j = 0; j < M; j++) {
      const at = (M === 1 ? 0 : Math.round(j * (N - 1) / (M - 1))) * S, a = new Float64Array(hm + 1), b = new Float64Array(hm + 1);
      for (let h = 1; h <= hm; h++) {                          // DFT: tylko harmoniczne 1..127
        let sa = 0, sb = 0;
        const w0 = 2 * Math.PI * h / S;
        for (let n = 0; n < S; n++) { const v = x[at + n] || 0; sa += v * Math.cos(w0 * n); sb += v * Math.sin(w0 * n); }
        a[h] = 2 * sa / S; b[h] = 2 * sb / S;
      }
      const y = new Float32Array(FR);
      for (let i = 0; i < FR; i++) {
        let v = 0;
        for (let h = 1; h <= hm; h++) { const t = 2 * Math.PI * h * i / FR; v += a[h] * Math.cos(t) + b[h] * Math.sin(t); }
        y[i] = v; pk = Math.max(pk, Math.abs(v));
      }
      out.push(y);
    }
    const k = pk > 0 ? 0.95 / pk : 0;
    // ostrzeżenie: plik wygląda na zwykłe nagranie, a nie tablicę (brak znacznika Serum, długość nie dzieli się przez
    // typowy rozmiar ramki 256 / 2048, albo bardzo długi)
    let warn = "";
    if (x.length > 4000000) warn = `Plik jest bardzo długi (${(x.length / 44100).toFixed(0)} s) — tablica to zwykle kilka sekund.`;
    else if (w.size <= 0 && x.length > 4096 && x.length % 256 !== 0)
      warn = "Plik nie ma znacznika tablicy (Serum) i jego długość nie dzieli się przez 256 ani 2048 — to wygląda na zwykłe nagranie, nie tablicę falową.";
    return { frames: out.map((y) => Int16Array.from(y, (v) => Math.round(v * k * 32767))), total: N, size: S, warn };
  }

  // ramki -> komunikaty SysEx (Uint8Array bez F0 / F7) dla miejsca slot (0..2)
  function sysex(fr, slot, name) {
    const nm = (name || "").toUpperCase().replace(/[^ A-Z0-9-]/g, " ").slice(0, 12).trimEnd();
    const msgs = [Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x20, slot, fr.length, ...[...nm].map((c) => c.charCodeAt(0))])];
    fr.forEach((f, j) => {
      const d = new Uint8Array(7 + 2 * FR);
      d.set([0x7D, 0x4F, 0x47, 0x53, 0x22, slot, j]);
      for (let i = 0; i < FR; i++) { const q = (f[i] >> 2) & 0x3FFF; d[7 + 2 * i] = q & 0x7F; d[8 + 2 * i] = q >> 7; }
      msgs.push(d);
    });
    msgs.push(Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x23, slot]));
    return msgs;
  }

  /* (v0.74) edytor tablic (OG SYNTH MANAGER): ramka = Float32Array(256) -1..1 */
  // widmo ramki: a[h], b[h] (cos, sin) dla h 1..127
  function spectrum(f) {
    const a = new Float64Array(H + 1), b = new Float64Array(H + 1);
    for (let h = 1; h <= H; h++) {
      let sa = 0, sb = 0;
      for (let n = 0; n < FR; n++) { const t = 2 * Math.PI * h * n / FR; sa += f[n] * Math.cos(t); sb += f[n] * Math.sin(t); }
      a[h] = 2 * sa / FR; b[h] = 2 * sb / FR;
    }
    return { a, b };
  }
  // widmo -> ramka (bez DC, harmoniczne 1..127)
  function synth(sp) {
    const y = new Float32Array(FR);
    for (let n = 0; n < FR; n++) {
      let v = 0;
      for (let h = 1; h <= H; h++) if (sp.a[h] || sp.b[h]) { const t = 2 * Math.PI * h * n / FR; v += sp.a[h] * Math.cos(t) + sp.b[h] * Math.sin(t); }
      y[n] = v;
    }
    return y;
  }
  // amplitudy harmonicznych 1..n (do słupków)
  function amps(f, n = 32) {
    const sp = spectrum(f), out = new Float32Array(n);
    for (let h = 1; h <= n; h++) out[h - 1] = Math.hypot(sp.a[h], sp.b[h]);
    return out;
  }
  // harmoniczna h (1..) na amplitudę m: faza zostaje (cisza: sinus)
  function setAmp(f, h, m) {
    const sp = spectrum(f), cur = Math.hypot(sp.a[h], sp.b[h]);
    if (cur > 1e-9) { sp.a[h] *= m / cur; sp.b[h] *= m / cur; } else { sp.a[h] = 0; sp.b[h] = m; }
    return synth(sp);
  }
  // n ramek pośrednich między fa i fb: "x" = krzyżowo (próbki), "s" = amplitudy harmonicznych (fazy z fa)
  function morph(fa, fb, n, mode) {
    const out = [];
    const A = mode === "s" ? spectrum(fa) : null, B = mode === "s" ? spectrum(fb) : null;
    for (let k = 1; k <= n; k++) {
      const t = k / (n + 1);
      if (mode !== "s") { out.push(Float32Array.from(fa, (v, i) => v + (fb[i] - v) * t)); continue; }
      const sp = { a: new Float64Array(H + 1), b: new Float64Array(H + 1) };
      for (let h = 1; h <= H; h++) {
        const ma = Math.hypot(A.a[h], A.b[h]), mb = Math.hypot(B.a[h], B.b[h]), m = ma + (mb - ma) * t;
        const ph = ma > 1e-9 ? Math.atan2(A.a[h], A.b[h]) : mb > 1e-9 ? Math.atan2(B.a[h], B.b[h]) : 0;
        sp.a[h] = m * Math.sin(ph); sp.b[h] = m * Math.cos(ph);
      }
      out.push(synth(sp));
    }
    return out;
  }
  // ramki edytora -> Int16 do FM1 (bez DC, pasmo 1..127 jak w silniku, wspólna skala 0,95)
  function toFm1(fr) {
    const ys = fr.map((f) => synth(spectrum(f)));
    let pk = 0;
    ys.forEach((y) => y.forEach((v) => { pk = Math.max(pk, Math.abs(v)); }));
    const k = pk > 0 ? 0.95 / pk : 0;
    return ys.map((y) => Int16Array.from(y, (v) => Math.round(v * k * 32767)));
  }
  // ramki -> .wav 16 bit mono 44,1 kHz ze znacznikiem Serum "clm " <!>256 (parse() czyta go z powrotem)
  function wav(fr) {
    const n = fr.length * FR, clm = "<!>256 10000000 wavetable (OG Synth)", cl = clm.length + (clm.length & 1);
    const buf = new ArrayBuffer(44 + 8 + cl + n * 2), dv = new DataView(buf);
    const w4 = (o, s) => { for (let i = 0; i < s.length; i++) dv.setUint8(o + i, s.charCodeAt(i)); };
    w4(0, "RIFF"); dv.setUint32(4, buf.byteLength - 8, true); w4(8, "WAVE");
    w4(12, "fmt "); dv.setUint32(16, 16, true); dv.setUint16(20, 1, true); dv.setUint16(22, 1, true);
    dv.setUint32(24, 44100, true); dv.setUint32(28, 88200, true); dv.setUint16(32, 2, true); dv.setUint16(34, 16, true);
    w4(36, "clm "); dv.setUint32(40, clm.length, true); w4(44, clm);
    const d = 44 + cl;
    w4(d, "data"); dv.setUint32(d + 4, n * 2, true);
    let o = d + 8;
    fr.forEach((f) => f.forEach((v) => { dv.setInt16(o, Math.max(-32768, Math.min(32767, Math.round(v * 32767))), true); o += 2; }));
    return buf;
  }

  globalThis.ogsWave = { parse, frames, sysex, spectrum, synth, amps, setAmp, morph, toFm1, wav, FR, MAXF };
})();
