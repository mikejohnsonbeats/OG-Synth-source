# OG Synth

Własny firmware do M-VAVE FM-1 do sound designu: 16 silników (analogowe MINI, SVF, VCO, DCO, ALPHA, LPG; cyfrowe FM6,
PHASE, WAVE, 8BIT, PHYS, FORMANT; perkusja MORPH, 88, 99, DUAL), efekty, sekwencer i arpeggiator, tryb MULTI (4 ścieżki naraz).
Import brzmień DX7 i Casio CZ. [Instrukcja](manual/).

Wersja {{VERSION}}. Instalujesz na własną odpowiedzialność; instalator potrafi też przywrócić firmware M-VAVE.

- [Wypróbuj w przeglądarce](emu/): ten sam kod co na FM-1, z dźwiękiem. FM-1 niepotrzebne.
- [Zainstaluj](install/): w Chrome albo Edge, FM-1 podłączone przez USB.
- [Kopia zapasowa](backup/): presety i ustawienia FM-1 na komputer i z powrotem — zrób ją przed instalacją.
- [OG SYNTH MANAGER](emu/manager.html): brzmienia i banki DX7 oraz Casio CZ (.syx) do FM-1.
- [Pobierz {{PKG}}](firmware/{{PKG}}): do instalatora z wiersza poleceń, `python3 tools/fm1_install.py {{PKG}}`.

Kod: MPL-2.0, na bazie fm1-base i felucca-bsp (Felucca: Leo Kuroshita, Hügelton Instruments).
M-VAVE i FM-1 to znaki towarowe swoich właścicieli; OG Synth nie jest powiązany z M-VAVE. Casio i CZ to znaki
towarowe Casio; import brzmień CZ służy zgodności z ich plikami.
