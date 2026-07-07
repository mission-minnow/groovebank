# 🎸 Groove Bank

**A library of standard rhythm templates for [Schwung](https://github.com/charlesvestal/schwung) on the Ableton Move — played by the chord you hold.**

![grooves](https://img.shields.io/badge/grooves-235-6C5CE7)
![genres](https://img.shields.io/badge/genres-32-00B894)
![platform](https://img.shields.io/badge/platform-Ableton%20Move-2D3436)
![module](https://img.shields.io/badge/Schwung-MIDI%20FX-0984E3)

*You own the pitches, Groove Bank owns the timing.* Hold a chord (or a single
note) and the selected **rhythm template retriggers your held notes on the beat**
— clock-locked, with a real re-attack on every hit. Change your chord mid-pattern
and the next hit follows your hands. It's a chainable `midi_fx` module: the module
makes the *rhythm*, your fingers make the *harmony*, and the chain plays the synth.

> The sibling to [Beat Bank](https://github.com/mission-minnow/beatbank): Beat Bank
> is drum patterns that generate their own notes; Groove Bank is pure rhythm
> applied to *your* notes. Layer them — a beat and a groove playing themselves —
> and just noodle a melody on top.

---

## 🚀 Quick start

Groove Bank plays the **Sound Generator loaded in its slot** — a Schwung synth —
the same way Super Arp and the other chain MIDI-FX tools do. (It does *not* play
Move's native track instruments; see [Feeding it a chord](#-feeding-it-a-chord).)

1. In a Schwung chain **slot**, set the **Sound Generator** to any synth (a pad,
   bass, keys, an SF2 — anything melodic).
2. Set the slot's **MIDI FX** to **Groove Bank**.
3. Feed it a chord (see below), **press Play**, and hold. You'll hear the chord
   chopped to the current groove through your chain synth. 🎉
4. **Turn the jog** to audition grooves in the current genre; **Knob 8** hops
   genres; **Knob 7** adds swing; **Knob 1** sets the gate (staccato ↔ legato).
5. Move your hand to a new chord — the groove keeps running and strikes the new
   notes on the next hit.

Groove Bank follows Move's transport (it's silent when stopped), so it locks
tight to a beat playing on the same clock.

---

## 🎹 Feeding it a chord

Groove Bank needs MIDI notes arriving *at its slot*. Two ways:

**A. External MIDI keyboard (cleanest).** Plug a USB keyboard into the Move, set
the slot's **Receive** channel to match (or **All**), and hold a chord. Nothing
of Move's is in the way — you hear only the patterned chain synth.

**B. Move's own pads (as a silent controller).** Move's pads normally play the
selected *track's* instrument, which would sustain over the top. To use the pads
purely as a chord input for Groove Bank:

1. Set the slot's **Receive** channel to **All**.
2. On the Move track you'll play, turn **MIDI Out ON** (this is what echoes your
   pad notes to the chain).
3. **Mute that track** (or pull its volume to −inf). This silences Move's native
   instrument — but the notes *still* reach Groove Bank — so you hear only the
   patterned chain synth.

Either way, the sound you hear is the **chain synth in the slot**, not Move's
track instrument. Pick the chain synth to get the sound you want (an SF2 kit, a
pad, a bass…).

---

## 🎛 The Pattern view

A fullscreen grid. Genre-first browsing on the jog, one rhythm lane (the pitches
are your hands).

```
┌──────────────────────────────────────┐
│ Offbeat Stab                    1/7   │
│                                       │
│   ▐   ▐   ▐   ▐   ▐   ▐   ▐   ▐        │
│   ·           ·           ·           │
│ hold 3                                │
│ sw:0 gt:80  K8: HOUSE                 │
└──────────────────────────────────────┘
```

Row marks: `x` hit · `A` accent · `g` ghost (soft) · `-` tie/sustain · `.` rest.
`hold N` shows how many notes you're holding.

---

## 🎚 Controls

| Control            | Action                                                   |
| ------------------ | -------------------------------------------------------- |
| **Jog** turn       | Browse grooves *within* the current genre                |
| **Knob 1**         | Gate — staccato ↔ legato (note length)                   |
| **Knob 2**         | Strum — tight chord (center) ↔ up-strum (right) / down-strum (left) |
| **Knob 3**         | Accent — velocity depth (flat ↔ punchy)                  |
| **Knob 4**         | Variant — morph the groove's flavors (regular / busy / sparse …) |
| **Knob 7**         | Swing (0–100)                                            |
| **Knob 8**         | Switch genre (geared: ~8 genres per rotation)            |
| **Jog-click / Back** | Exit the Pattern view                                  |

These follow the **Bank-family convention** shared with Beat Bank: jog browses,
K7 = swing, K8 = genre.

---

## 🎵 Genres

**235 grooves across 32 genres**, each with a base rhythm + authored flavors:

> **Latin/World** — Latin (clave, tresillo, montuno, cascara…) · Brazil (bossa, samba, baião…) · Afro · Mideast (maqsum, baladi, saidi…) · India (teental, keherwa…) · World
> **Electronic** — house · techno · electro · garage · dnb · jungle · dubstep · breaks
> **Caribbean** — reggae · ska · dancehall · reggaeton · soca · calypso
> **Groove** — funk · soul · gospel · rnb · hip-hop · trap · lofi · trip-hop
> **Band** — jazz · rock · pop · strum (folk / flamenco / bluegrass…)

Browse genre-first: the jog cycles grooves inside a genre; Knob 8 jumps between
genres. Odd-meter traditions use their natural step counts (12 for 6/8 & 3/4,
14 for 7/8, etc.), so the world/jazz-waltz grooves feel right.

---

## ✍️ Add your own grooves

Grooves load at startup from plain `.groove` text files, merged from two folders:

1. **Shipped defaults** — `…/modules/midi_fx/groovebank/patterns/*.groove`
2. **Your grooves** — `/data/UserData/schwung/groovebank/patterns/*.groove`
   (persist across upgrades; a `_HOWTO.txt` is seeded here on first run)

A `.groove` file is a list of blocks separated by blank lines:

```
name: Son Clave 3-2
genre: LATIN
steps: 16
hit:  x..x..x...x.x...
```

- `steps` — `16` (one bar) or `32` (two bars)
- Row chars — `.` rest · `x` hit · `A` accent · `g` ghost · `-` tie/sustain
- A single `hit:` row (the pitch is whatever you hold), exactly `steps` long
- `A---` is one accented note held for four steps

**Variants (a "font family" for the groove).** Add `alt:` rows after `hit:` for
extra flavors of the same groove; **Knob 4** morphs between them live:

```
name: Offbeat Stab
genre: HOUSE
steps: 16
hit:  ..x...x...x...x.      # regular
alt:  ..x.x.x...x.x.x.      # busy
alt:  ..x.......x.....      # sparse
```

Up to 6 variants (base + 5 `alt:`), all the same length as `steps`.

---

## 🔧 Build & install

```bash
./scripts/build.sh        # cross-compile dsp.so (Docker or native)
./scripts/install.sh      # deploy to move.local (incl. the patterns/ folder)
./scripts/package.sh      # or build a release tarball
make test                 # run the engine unit checks (no hardware)
```

---

## 🙏 Acknowledgements

Built on the [Schwung](https://github.com/charlesvestal/schwung) MIDI FX plugin
API. Sibling to [Beat Bank](https://github.com/mission-minnow/beatbank). Same
driver philosophy as [Super Arp](https://github.com/handcraftedcc/schwung-superarp)
— the module makes the timing, the chain plays the synth — but chord-first and
queue-free: your pads change the held chord, they never enqueue a phrase.
