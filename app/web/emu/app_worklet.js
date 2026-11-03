// OGS: MIDI z przeglądarki (Web MIDI) do symulatora, jak przez USB (PORT_USB = 0); SysEx z pliku .syx (app_sysex);
// banki DX7 z obszaru danych flasha przechowuje strona (localStorage), bo symulator czyści ten obszar przy starcie.

// wiadomość "load" (z bankami od strony) jest potrzebna przed startem aplikacji: zapamiętujemy ją po drodze
const register = globalThis.registerProcessor;
globalThis.registerProcessor = (name, cls) => register(name, class extends cls {
  constructor(...a) {
    super(...a);
    const h = this.port.onmessage;
    this.port.onmessage = (e) => { if (e.data && e.data.type === "load") globalThis.mjbLoad = e.data; return h(e); };
  }
});

globalThis.fm1App = {
  load(fm1) {                                                  // przed startem: banki DX7 (i za nimi CZ) z przeglądarki do flasha
    const d = globalThis.mjbLoad && globalThis.mjbLoad.mjbSyx;
    if (d && fm1.ex.ogs_syx_area) {
      const n = Math.min(d.length, fm1.ex.ogs_syx_size());
      new Uint8Array(fm1.mem.buffer, fm1.ex.ogs_syx_area(), n).set(d.subarray(0, n));
      if (fm1.ex.ogs_cz_area && d.length > n) {
        const c = Math.min(d.length - n, fm1.ex.ogs_cz_size());
        new Uint8Array(fm1.mem.buffer, fm1.ex.ogs_cz_area(), c).set(d.subarray(n, n + c));
      }
    }
    fm1.mjbGen = fm1.ex.ogs_syx_gen ? fm1.ex.ogs_syx_gen() : 0;
  },
  process(fm1) {                                               // bank zapisany: cały obszar do przeglądarki
    if (!fm1.ex.ogs_syx_gen || fm1.ex.ogs_syx_gen() === fm1.mjbGen) return;
    fm1.mjbGen = fm1.ex.ogs_syx_gen();
    const dx = new Uint8Array(fm1.mem.buffer, fm1.ex.ogs_syx_area(), fm1.ex.ogs_syx_size());
    const cz = fm1.ex.ogs_cz_area ? new Uint8Array(fm1.mem.buffer, fm1.ex.ogs_cz_area(), fm1.ex.ogs_cz_size()) : new Uint8Array(0);
    const all = new Uint8Array(dx.length + cz.length);
    all.set(dx); all.set(cz, dx.length);
    fm1.port.postMessage({ type: "app", mjbSyx: all });
  },
  message(fm1, m) {
    if (m.knobReset !== undefined) { if (fm1.ex.ogs_knob_reset) fm1.ex.ogs_knob_reset(m.knobReset); return; }   // silnik OGS: wartość domyślna parametru
    if (m.sysex) {                                             // ramki SysEx (bez F0 / F7) po kolei, jak z USB
      for (const f of m.sysex) {
        new Uint8Array(fm1.mem.buffer, fm1.ex.ogs_sysex_buf(), f.length).set(f);
        fm1.ex.ogs_sysex(f.length);
      }
      return;
    }
    if (!m.midi) return;
    const [st, d1 = 0, d2 = 0] = m.midi;
    const cin = st >= 0xF0 ? 0xF : st >> 4;                    // USB-MIDI: bajt 0 = kabel 0 / CIN
    fm1.ex.web_midi_in(0, (cin | st << 8 | d1 << 16 | d2 << 24) >>> 0);
  },
};
