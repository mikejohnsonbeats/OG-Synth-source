// OGS: wejście MIDI symulatora — np. prawdziwe FM1 po USB steruje symulatorem; wczytywanie .syx DX7 (brzmienie albo
// bank 32 brzmień, tymi samymi ramkami co OG SYNTH MANAGER do FM1) i Casio CZ (brzmienie do PHASE); banki DX7
// przechowywane w przeglądarce
let node = null, access = null, input = null;
const bar = document.querySelector(".bar");
const label = document.createElement("label");
label.innerHTML = 'MIDI IN <select id="ogs-midi"><option value="">(brak)</option></select> <span id="ogs-midi-last"></span>';
bar.appendChild(label);
const sel = label.querySelector("#ogs-midi"), last = label.querySelector("#ogs-midi-last");
const hex = (b) => b.map((x) => x.toString(16).toUpperCase().padStart(2, "0")).join(" ");

const syxLabel = document.createElement("label");
syxLabel.innerHTML = '<button type="button" id="ogs-syx-btn">Wczytaj .syx (DX7 / CZ)</button>'
  + '<input type="file" id="ogs-syx" accept=".syx" hidden> <a href="manager.html" target="_blank">OG SYNTH MANAGER (FM1)</a>';
bar.appendChild(syxLabel);
const syxFile = syxLabel.querySelector("#ogs-syx");
syxLabel.querySelector("#ogs-syx-btn").addEventListener("click", () => syxFile.click());

// tablica WAVE z .wav (Serum, Vital…): do miejsca USER 1-3, tymi samymi ramkami co OG SYNTH MANAGER do FM1
const wavLabel = document.createElement("label");
wavLabel.innerHTML = '<button type="button" id="ogs-wav-btn">Wczytaj .wav (tablica WAVE)</button><input type="file" id="ogs-wav" accept=".wav" hidden>';
bar.appendChild(wavLabel);
const wavFile = wavLabel.querySelector("#ogs-wav");
wavLabel.querySelector("#ogs-wav-btn").addEventListener("click", () => wavFile.click());
wavFile.addEventListener("change", async () => {
  const f = wavFile.files[0];
  wavFile.value = "";
  if (!f || !node) return;
  if (!globalThis.ogsWave) await import("./wavetable.js");
  const slot = parseInt(prompt("Miejsce tablicy: 1, 2 albo 3 (USER 1-3 w TABLE silnika WAVE):", "1"), 10);
  if (!(slot >= 1 && slot <= 3)) return;
  try {
    const fr = ogsWave.frames(ogsWave.parse(await f.arrayBuffer()));
    if (fr.warn && !confirm(`${fr.warn}

Wczytać mimo to?`)) return;
    node.port.postMessage({ type: "app", sysex: ogsWave.sysex(fr.frames, slot - 1, f.name.replace(/\.wav$/i, "")) });
  } catch (e) { alert(`${f.name}: ${e.message}`); }
});

const SYX_KEY = "ogs-syx-area";
function b64(u8) { let s = ""; for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode(...u8.subarray(i, i + 0x8000)); return btoa(s); }
function unb64(s) { const b = atob(s), u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i); return u; }

/* plik .syx -> ramki dla FM1 (bez F0 / F7): bank DX7 w kawałkach po 512 B + koniec z nazwą, reszta bez zmian;
   brzmienia Casio CZ (do PHASE): jedno — nr pick (1..), albo pick 0 = cały bank (plik z bankiem CZ to do 16 osobnych
   brzmień): ramki F0 7D 4F 47 53 03 nr <brzmienie> F7, na końcu 04 nazwa -> okno CZ BANK na FM1 */
export const isCZ = (f) => f[0] === 0x44 && (f.length === 262 || f.length === 261);
export function czCount(bytes) {
  let n = 0;
  for (let i = 0; i < bytes.length; i++) {
    if (bytes[i] !== 0xF0) continue;
    let j = i + 1;
    while (j < bytes.length && bytes[j] !== 0xF7) j++;
    if (isCZ(bytes.subarray(i + 1, j))) n++;
    i = j;
  }
  return n;
}
export function syxFrames(bytes, name, pick) {
  const frames = [], cz = [];
  for (let i = 0; i < bytes.length; i++) {
    if (bytes[i] !== 0xF0) continue;
    let j = i + 1;
    while (j < bytes.length && bytes[j] !== 0xF7) j++;
    const f = bytes.subarray(i + 1, j);
    if (f.length === 4102 && f[0] === 0x43 && f[2] === 0x09) {
      for (let k = 0; k * 512 < f.length; k++) frames.push(Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x01, k, ...f.subarray(k * 512, k * 512 + 512)]));
      const nm = (name || "").toUpperCase().replace(/[^ A-Z0-9-]/g, " ").slice(0, 12).trimEnd();
      frames.push(Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x02, ...[...nm].map((c) => c.charCodeAt(0))]));
    } else if (isCZ(f)) {
      cz.push(f);
    } else {
      frames.push(Uint8Array.from(f));
    }
    i = j;
  }
  if (cz.length && pick === 0) {
    cz.slice(0, 16).forEach((f, k) => frames.push(Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x03, k, ...f])));
    const nm = (name || "").toUpperCase().replace(/[^ A-Z0-9-]/g, " ").slice(0, 12).trimEnd();
    frames.push(Uint8Array.from([0x7D, 0x4F, 0x47, 0x53, 0x04, ...[...nm].map((c) => c.charCodeAt(0))]));
  } else if (cz.length) {
    frames.push(Uint8Array.from(cz[Math.min(Math.max(pick || 1, 1), cz.length) - 1]));
  }
  return frames;
}

syxFile.addEventListener("change", async () => {
  const f = syxFile.files[0];
  if (!f || !node) return;
  const bytes = new Uint8Array(await f.arrayBuffer()), ncz = czCount(bytes);
  let pick = 1;
  if (ncz > 1) {                                             // bank Casio CZ: cały (0) albo jedno brzmienie do PHASE
    pick = parseInt(prompt(`Plik ma ${ncz} brzmień Casio CZ. 0 = cały bank do pamięci (okno CZ BANK), 1-${ncz} = jedno brzmienie do PHASE:`, "0"), 10);
    if (!(pick >= 0 && pick <= ncz)) { syxFile.value = ""; return; }
  }
  const frames = syxFrames(bytes, f.name.replace(/\.syx$/i, ""), pick);
  node.port.postMessage({ type: "app", sysex: frames });
  syxFile.value = "";
});

function onMsg(e) {
  const b = [...e.data];
  if (b[0] === 0xF8 || b[0] === 0xFE) return;                // zegar, active sensing: pomijamy
  if (b[0] === 0xF0) {                                       // SysEx (np. brzmienie DX7 z edytora): do FM1 jak z USB
    if (node) node.port.postMessage({ type: "app", sysex: syxFrames(new Uint8Array(b), "") });
    return;
  }
  last.textContent = hex(b);
  if (node) node.port.postMessage({ type: "app", midi: b });
}
function use(id) {
  if (input) input.onmidimessage = null;
  input = id ? access.inputs.get(id) : null;
  if (input) input.onmidimessage = onMsg;
  try { localStorage.setItem("ogs-midi-in", input ? input.name : ""); } catch {}
}
function fill() {
  let want = "";
  try { want = localStorage.getItem("ogs-midi-in") ?? ""; } catch {}
  const cur = input && input.id;
  sel.length = 1;
  for (const i of access.inputs.values()) {
    sel.add(new Option(i.name, i.id));
    if (!cur && (want ? i.name === want : /FM-?1/i.test(i.name))) { sel.value = i.id; use(i.id); }
  }
  if (cur) sel.value = cur;
}
sel.addEventListener("change", () => use(sel.value));

export function message(m) {                                 // z workletu: obszar banków DX7 i CZ po zapisie
  if (m.mjbSyx) try { localStorage.setItem(SYX_KEY, b64(m.mjbSyx)); } catch { /* prywatne okno: banki do restartu */ }
}

export async function attach(ctx) {
  node = ctx.node;
  const post = node.port.postMessage.bind(node.port);      // start symulatora: banki DX7 z przeglądarki do flasha
  node.port.postMessage = (m, t) => {
    if (m && m.type === "load") try { const s = localStorage.getItem(SYX_KEY); if (s) m.mjbSyx = unb64(s); } catch {}
    return post(m, t);
  };
  if (!navigator.requestMIDIAccess) { ctx.status("Ta przeglądarka nie ma Web MIDI (użyj Edge lub Chrome)."); return; }
  try {
    access = await navigator.requestMIDIAccess({ sysex: true });
    access.onstatechange = fill;
    fill();
  } catch {
    try {
      access = await navigator.requestMIDIAccess();
      access.onstatechange = fill;
      fill();
    } catch { ctx.status("Brak zgody na MIDI: kliknij ikonę kłódki przy adresie i zezwól na MIDI."); }
  }
}
