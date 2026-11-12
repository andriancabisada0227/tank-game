# Tank Battle

A terminal tank game written in Turbo C style — `conio.h`, `gotoxy()`,
`textcolor()`, scan-code arrow keys — that compiles on modern Linux with
plain `gcc` **and** on real Turbo C 3.0, all from a single `tank.c`.

![text-mode](https://img.shields.io/badge/interface-text%20mode-brightgreen)
![language](https://img.shields.io/badge/language-C89-blue)

```
 TANK BATTLE   Lives:1  Score:20   Enemies:2  Turns:12

 +------------------------------------------------------------------------+
 |                              >>                                        |
 |          **                                                            |
 |          **                                                            |
 |                **                                                      |
 |                                              >>                        |
 |                                  **                                    |
 +------------------------------------------------------------------------+
 Arrows/WASD move   R restart   Q quit
```

## Gameplay

Turn-based survival on a 36×18 field. Every move you make, **each enemy
tank steps one cell toward you**. You have 1 life.

- You: green `^^` `vv` `<<` `>>` (facing) — Enemies: same glyphs in red
- `**` land mines destroy **any** tank that steps on them (+10 pts for
  enemy kills) — lure the chasers across the field as bait
- Tanks colliding — either direction — is GAME OVER
- Running into the border is GAME OVER
- Destroy every enemy to win: **FIELD CLEARED +100 pts**

## Controls

| Key | Action |
|-----|--------|
| Arrows / `WASD` | Move (one cell = one turn) |
| `R` | Restart |
| `Q` | Quit |

## Options menu (at start)

| Key | Option |
|-----|--------|
| `-` / `+` | Enemies (1–10) |
| `,` / `.` | Land mines (0–30) |
| `M` | Show/hide mines |

Placement is always random, with rules: mines drop first anywhere in the
field; tanks spawn only inside the border, never on mines or each other;
enemies stay ≥4 cells from the player.

## Build & run

Linux / macOS terminal:

```sh
gcc -O2 tank.c -o tank
./tank
```

or with the Makefile:

```sh
make run      # build + play
make          # build only
make clean
```

Real Turbo C 3.0 (DOS / DOSBox):

```
tcc tank.c
tank
```

The single `tank.c` switches between the two worlds itself:
`#ifdef __TURBOC__` pulls in the genuine `<conio.h>`/`<dos.h>`; otherwise
an inlined compatibility layer reimplements them with ANSI escapes and
termios (including Turbo C's `getch()`-returns-0-then-scan-code arrow-key
behaviour and DOS scan codes 72/80/75/77).

## Requirements

- Any ANSI-capable terminal ≥ 80×24
- C89 compiler (tested with gcc 13)
- No external dependencies — C standard library only

## Files

| File | Purpose |
|------|---------|
| `tank.c` | The entire game + Linux conio/dos shim in one file |
| `Makefile` | `make` / `make run` / `make clean` |
| `.vscode/tasks.json` | `Ctrl+Shift+B` → build & play in the IDE terminal |
