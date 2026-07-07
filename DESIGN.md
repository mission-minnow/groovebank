# Groove Bank — Design Document

> **Working name: Groove Bank** (provisional — see Open Decision OD1). A Schwung
> chain **MIDI-FX**, sibling to Beat Bank. This document is written to survive a
> context wipe: a fresh session should be able to build the module from here
> without re-deriving the design.

---

## 0. Status & how to resume

- **Provenance:** Designed 2026-07-07 with Joe (git identity: `mission-minnow`).
  Grew out of Beat Bank rhythm-timing work and a survey of the four existing
  Schwung arp/chord modules.
- **Current stage:** v0.1.2, hardware-proven core + strum/accent + authored
  variants (2026-07-07). Module lives in this repo (`/Users/joe/Projects/groovebank`):
  engine, `.groove` parser, canvas, starter library, build infra. Compiles clean
  (native `-Wall -Wextra`), **all `make test` checks pass** (incl. strum, accent,
  variants), and **`./scripts/build.sh docker` cross-compiles a valid ARM aarch64
  `dsp.so`**. Built: K1 Gate, **K2 Strum**, **K3 Accent**, **K4 Variant (authored
  `alt:` flavors)**, K7 Swing, K8 Genre, ties. Hardware-confirmed as a chain-synth
  tool (§5.1). Not committed to git. **NOT yet built:** full 200–300 library with
  variants (Stage 6), CI release (Stage 7). OD2 stopped-start deferred.
- **Related memories** (in `~/.claude/.../memory/`):
  - `schwung-arp-chord-ecosystem-survey` — who already solves arp/chord/rhythm
    (Super Arp ≈ 90% of the engine; what to lift, what to skip).
  - `schwung-continuous-processing-vs-clock-tick` — **DECIDED: stay clock-driven
    via `process_midi`; do not switch to `tick()`.** The only thing that would
    pull in `tick()` is the optional stopped-transport feature (OD2).
  - `beatbank-prerelease-sync-plan` — release/versioning convention to mirror.
- **Stage 0 RESOLVED on hardware (2026-07-07) — see §5.1.** GB drives the slot's
  **chain synth**, like Super Arp; it does NOT and cannot pattern Move's *native*
  track instruments (pressing a pad sustains Move's own instrument, which the
  chain can't intercept). Decision: **keep GB a chain-synth tool** (Move-native
  patterning would need Pre-mode inject + non-pad chord source — rejected for v1).
- **Decisions RESOLVED 2026-07-07:** OD1 → **Groove Bank** (`id: groovebank`,
  abbrev `GRUV`); OD2 → **Phase 1 first** (transport must run; stopped-start
  deferred). OD3 (first-hit timing) still open, decide after playing Phase 1. §11.

---

## 1. Concept

**One line:** *You own the pitches, Groove Bank owns the timing.* Hold a chord or
note; a browsable, genre-organized **rhythm template** retriggers your held notes
on the beat — clock-locked, with a **real re-attack each hit**. Change your chord
mid-pattern and the next hit strikes whatever you're now holding.

**The mental model (settled with Joe):** it is *not* "pick a rhythm and have
fun." It is "**come with the notes you want to play, and we'll trigger them to
the rhythm, even if you change notes mid-pattern.**" The pattern is a pure,
pitch-blind rhythm (`|..xxx..x..x..xx..|`); the chords are your hands, live; what
you *hear* (`|..ccc..a..f..fc..|`) emerges from laying one over the other.

**Relationship to Beat Bank:**
| | Beat Bank | Groove Bank |
|---|---|---|
| Owns | multi-lane **drum** patterns (generates notes) | single-lane **rhythm** applied to *your* notes |
| Pitch source | fixed drum voices | your held chord/note, live |
| Format | `.beat` (kick/snare/hat rows) | `.groove` (one `hit:` row) |
| UX | genre-first fullscreen canvas | **same canvas, forked** |

They **layer**: Beat Bank on a drum slot + Groove Bank on a chord/bass slot, one
transport → an instant groove bed you solo a melody over. Same "the module makes
the timing, the chain plays the synth" driver philosophy.

**Why not just use Super Arp** (which does ~90% of the engine): Joe tested it.
Three reasons it isn't his tool: (a) it **queues a phrase copy per pad-hit** →
hammering pads stacks copies and locks up; (b) it's **arp-first** (pitch cycling)
with rhythm bolted on; (c) **params-only UI**, no fullscreen browse. Groove Bank
is **one transport-anchored free-running loop** (pads only mutate a held
register, never enqueue), **chord-first** (fire the whole held set per hit), with
Beat Bank's genre-first canvas. That queue-free design is the core differentiator
that justifies building rather than reusing.

---

## 2. Core design decisions (SETTLED — do not relitigate)

- **D1 — Clock-driven via `process_midi`.** All timing comes from Move's MIDI
  clock (`0xFA` start, `0xF8` tick @ 24 PPQN, `0xFB` continue, `0xFC` stop)
  handled in `process_midi` — the **event path, never render-gated**. This dodges
  the render-idle downbeat problem that bit Beat Bank, and needs no `tick()` and
  no `requires_continuous_processing`.
- **D2 — Swallow-and-retrigger.** Incoming note-on/off are **captured into a held
  register and NOT forwarded**. Output is *only* the on-grid retriggers.
  (Forwarding would double: sustained original + gated copies.)
- **D3 — Transport-anchored, free-running.** `cur_step` is derived from the
  running clock count — a pure function of transport position, **not** of when you
  press. Holding new notes just changes which pitches the next active step fires;
  **nothing restarts or enqueues.** This is the explicit anti-Super-Arp-bug design.
- **D4 — Real re-attack.** Each active step emits a fresh note-on for every held
  pitch and schedules note-offs at the gate length; a pitch already sounding is
  note-off'd first. Envelope/LFO restart on every hit (the thing an audio gate
  couldn't do).
- **D5 — Chord mode (v1).** An active step fires **all** currently-held notes.
  (Arp/cycle modes = future, out of v1 scope.)
- **D6 — Reuse Beat Bank UX & format.** `.groove` mirrors `.beat`; genre-first jog
  browse; the **geared K8 genre selector** (the detent-accumulator just added to
  Beat Bank's `canvas.js`) is lifted directly.

---

## 3. UX / Controls (canvas overlay — mirrors Beat Bank)

**Design rule: mirror Beat Bank as far as makes sense.** All "Bank"-family
modules share one look/feel/usage. The pieces below marked **[FAMILY]** are fixed
conventions every Bank module MUST match; **[MODULE]** items are this module's own
per-pattern edits, which live in the K1–K6 band. See the family-convention memory
`bank-family-ux-convention`.

**Family anchors — identical to Beat Bank [FAMILY]:**
- **Fullscreen canvas** (`canvas.js`), forked from Beat Bank's Pattern view.
- **Jog turn** — browse patterns within the current genre.
- **Jog click / Back** — exit.
- **K7 — swing** (0–100, step 5). *Never reassign K7.*
- **K8 — genre**, **geared** (detent-accumulator, ~8 genres per rotation — lift
  `GENRE_DETENTS_PER_STEP` from Beat Bank's `canvas.js`). *Never reassign K8.*
- **Display:** pattern name (top-left, full width), genre + position (top-right),
  the pattern grid, footer `sw:N   K8: <genre>` — Beat Bank's exact layout.

**Rationale for the anchor split (from Beat Bank):** K7/K8 are the "feel + browse"
pair, rarely touched once dialed; K1–K6 carry the frequent per-pattern edits. Beat
Bank puts *per-drum-voice pad selectors* on K1–K6. Groove Bank has **no drum
voices** (pitch is your live hands), so nothing pad-like to edit there — that band
is repurposed for articulation, the one justified divergence:

**This module's K1–K6 edits [MODULE]:**
- **K1 — Gate** length (staccato ↔ legato, incl. >100% overlap). Uniform
  articulation character; per-step ties (`-`) handle rhythmic sustains (§4).
- **K2 — Strum** (bipolar, center-detent): center = tight block chord; clockwise =
  up-strum (low→high) widening; counter-clockwise = down-strum (high→low)
  widening. One knob = direction + amount ("reggae bubble / guitar strum / stab").
- **K3 — Accent** depth (velocity spread between `A` / `x` / `g`).
- **K4 — Variant** (§4): morph the groove's authored `alt:` flavors
  (regular/busy/sparse/…), one flavor per detent. Built (v0.1.2).
- **K5–K6 — reserved.** Candidates: octave shift, latch on/off, humanize,
  held-note limit.

---

## 4. `.groove` file format (mirrors `.beat`)

```
name: Son Clave 2-3
genre: LATIN
steps: 16
hit:  ..x.x...x..x..x.
```

- One block per pattern, blank-line separated (same parser shape as `.beat`).
- `steps:` — `16` (one bar of 16ths) or `32` (two bars).
- **Single `hit:` row.** Chars:
  - **Dynamics (velocity):** `.` rest · `x` normal hit · `A` accent (louder) ·
    `g` ghost (softer). Same vocabulary as `.beat`.
  - **Articulation (length):** `-` **tie** — extend the *previous* hit through this
    step (no new attack; push its note-off later). This is the one articulation
    that belongs *in* the pattern because it's a rhythmic property. Example:
    `A---x.g.x---.gx.` = accented note held 4 steps, short `x`, ghost, short `x`,
    another held note… Combined with a low Gate knob, un-tied hits stay crisply
    staccato while tied hits sustain → "short-short-**long**" phrasing with no
    extra syntax.
- Row must be exactly `steps` long.
- **Staccato/legato *character* is NOT per-step** — it's the global **Gate** knob
  (K1, §3): short gate = staccato stabs, ~1 step = tenuto, >1 step = legato/overlap.
  Kept off the pattern so grooves stay readable and the feel is a live dial.
- *Deferred to v2:* rolls / flams / ratchets (subdivide a step into a fast burst,
  e.g. a per-step ratchet-count digit) — possible, but complicates parser + eye.

**Authored variants (DECIDED 2026-07-07 — "font family" model, NOT yet built).**
A groove is a *family*: a base rhythm plus a few hand-authored flavors (the
"regular / italic / bold" of a beat — sparse / busy / syncopated / rolled). The
`hit:` row is variant 0; each subsequent **`alt:`** row is variant 1, 2, …:
```
name: Offbeat
genre: HOUSE
steps: 16
hit:  ..x...x...x...x.      # 0  regular
alt:  ..x.x.x...x.x.x.      # 1  busy
alt:  ..x.......x.....      # 2  sparse
```
- All rows share `steps`; `alt:` uses the same `. x A g -` vocabulary.
- **K4 = variant selector** (0..variant_count−1), live — morph flavor without
  changing groove. Authored (not algorithmic) so each flavor is musical.
- Engine changes needed (next build): `GroovePattern` holds `row[GB_MAX_VARIANTS]`
  + `variant_count`; a `variant` param (K4); `fire_step` reads the selected
  variant's row; canvas shows `v{n}/{count}` + maps K4. Parser: `alt:` appends a
  row. This is the "variants on a knob" Joe asked for — like font styles.
- Loaded from **shipped defaults** (`…/modules/midi_fx/groovebank/patterns/*.groove`)
  merged with **user folder** (`/data/UserData/schwung/groovebank/patterns/*.groove`,
  persists across upgrades; seed a `_HOWTO.txt` on first run) — exactly like Beat Bank.
- **Rationale for single lane:** pitch comes from your hands, so a pattern is pure
  rhythm = one row.
- *Deferred:* optional per-pattern `feel:` swing hint. The global swing knob is
  enough for v1.

---

## 5. Chain integration & `module.json`

- `component_type: "midi_fx"`, `chainable: true`, `api_version: 2` in
  `module.json`; C code targets `midi_fx_api_v1` (`move_midi_fx_init`, the
  MIDI-emitting `.process_midi`/`.tick` vtable — same API family as Beat Bank and
  all four surveyed modules).
- **Chain position:** `[your pads / MIDI source] → [Groove Bank] → [synth] → [audio FX]`.
- **`chain_params`** (for Shadow UI step sizes/ranges): `pattern` (int),
  `swing` (int 0–100 %), `gate` (int %, e.g. 5–200), `accent` (int),
  `note_mode` (enum: `chord` [`arp` future]).
- **`ui_hierarchy`:** `root` → a `canvas` "Groove" view (`canvas_script: canvas.js`,
  `show_value:false`, `show_footer:false`) + a `Globals` level (swing, gate,
  accent, note_map-equivalents) with `knobs` mapping.
- **Canvas param contract** (via `get_param`, mirror Beat Bank's exactly so
  `canvas.js` scaffolding ports over): `pattern_count`, `pattern`, `steps`,
  `pattern_name`, `pattern_genre`, `row0` (the single `hit` row string),
  `genre_list` (`name:count|name:count|…`), `preview_rev`, `swing`.

---

### 5.1 Input routing reality (HARDWARE-CONFIRMED 2026-07-07)

The Stage-0 question ("do Move's pads feed the slot?") was answered by deploying a
counter-instrumented build and reading it on the Move. Findings:

- **The clock reaches the slot; notes are channel-filtered.** 1-byte realtime
  messages (`0xF8/0xFA/…`) are broadcast to all slots, but note messages go
  through `shadow_chain_dispatch_midi_to_slots` (`schwung src/host/shadow_midi.c:332`)
  which **drops any note whose channel ≠ the slot's Receive channel**. So the slot
  must be **Receive = All** (or match) to see played notes.
- **Move's pads only reach the slot if the track's MIDI Out is ON.** Pad notes are
  echoed to the chain via the MIDI_OUT path; with the track's MIDI Out off, GB
  gets zero notes. With it on + Receive All → GB receives the chord (verified:
  note-on counter climbed, held-count tracked, steps fired).
- **A pad ALSO plays Move's native track instrument directly (the sustain), and
  the chain cannot intercept or re-time that.** Muting the track / volume −inf
  silences the native instrument but the notes still reach GB — so you hear only
  the patterned **chain synth**. There is no way to pattern Move's *own* instrument
  in place (the sustain and the voice you'd pattern are the same firmware voice).

**Consequence (decided):** GB is a **chain-synth** rhythm tool — exactly the model
Super Arp / Impressive Chords use. It plays the Sound Generator loaded in its slot,
never Move's native track instruments. Chord input = an external MIDI keyboard
(cleanest) OR Move's pads used as a silent controller (Receive All + track MIDI
Out on + mute the track). Documented in README "Feeding it a chord".

**Not pursued (v1):** patterning Move's native instruments via Pre-mode inject
(Beat Bank's Schw+Move). It's technically reachable (add `pre_capable`, reuse the
Beat Bank inject path), but the held-chord input can't come from a pad on the
target instrument (it would sustain), so it needs an external keyboard or a
separate muted input track. Deferred; the clean fit for Move-native patterning is
a *generator* (specify chords), not a live-held retrigger.

## 6. Timing engine (DSP, C) — precise behavior

**State:** held register (pitches, sorted + as-played, `MAX_HELD` ~16); active
voices (pitch + `gate_clocks_remaining`); `cur_step`; `clock_running`;
`clocks_until_step`; current pattern (row + length).

**Constant:** `CLOCKS_PER_STEP = 6` (24 PPQN ÷ 4 = 16th-note steps), same as Beat
Bank.

**`process_midi(msg)`:**
- `0xFA` **start** → `cur_step = 0`; `clock_running = 1`; flush all voices
  (note-off); if step 0 is a hit, `fire_step()`; set `clocks_until_step`.
- `0xFB` **continue** → `clock_running = 1`.
- `0xF8` **tick** → if `!clock_running` skip. Decrement every voice's
  `gate_clocks_remaining`; emit note-off for any that hit 0. `clocks_until_step--`;
  when it reaches 0 → advance `cur_step` (wrap on pattern length); if that step is
  a hit → `fire_step()`; reset `clocks_until_step = CLOCKS_PER_STEP`
  (swing-adjusted on odd steps).
- `0xFC` **stop** → flush all voices (note-off); `clock_running = 0`.
- **note-on** (`0x9n`, vel>0) → add pitch to held register. **Do NOT forward.**
  **Do NOT fire immediately** (default strict on-grid; see OD3).
- **note-off** (`0x8n`, or `0x9n` vel 0) → remove pitch from held register.
  **Do NOT forward.** (Voices already sounding keep their scheduled note-offs —
  releasing your pad stops *future* hits, not the current ring, like Beat Bank.)
- **other channel messages** (CC, pitchbend, aftertouch) → pass through
  (needed for expressive/MPE synths downstream).

**`fire_step()`:**
- Velocity from the step char: `A` → accent vel, `x` → normal vel, `g` → ghost vel
  (spread governed by the `accent` param).
- For each pitch `P` in the held register: if `P` is currently sounding, emit
  note-off `P` first (re-attack); emit note-on `P` at vel; register a voice with
  `gate_clocks_remaining = gate_length_in_clocks`.
- If the register is empty → nothing fires (silent step).

**Tie step (`-`):** does **not** fire a new attack. Instead it **extends** the
currently-sounding voices by adding `CLOCKS_PER_STEP` to their
`gate_clocks_remaining` (so a note followed by `---` sustains across those steps
regardless of the base Gate length). A `-` with nothing sounding is a no-op (acts
as a rest). `.` (rest) never *cuts* a sounding voice — it just adds no attack;
the voice releases when its own gate expires.

**Strum (K2) — sub-step onset spreading.** When the `Strum` param ≠ center,
`fire_step()` does not emit the chord's note-ons simultaneously. It emits the
**first** note immediately (in `process_midi`, dead-tight), then **queues** the
remaining held notes with per-note **sample** countdowns — spread by
`strum_ms × sample_rate/1000` each, ordered low→high (up-strum) or high→low
(down-strum) per the knob's sign. That queue is drained in **`tick()`**
(per-128-frame block, ~2.9 ms resolution — fine enough for a smooth strum).
- **Clamp / fit:** total spread is capped to a fraction of the current step
  interval so a wide strum at fast tempo never bleeds into the next hit
  (Impressive Chords' `fit`). Compute the cap from `clocks_until_step`.
- Each strummed note is still a real note-on + scheduled note-off (gate applies
  per note), so re-attack and voice-stealing are unchanged — only the *onset
  time* is staggered.
- **Why this is a SAFE use of `tick()`** (does not reopen the render-idle risk
  we avoided with D1): step/downbeat timing stays fully clock-driven; the strum
  queue only ever holds entries in the tens of ms **after** a hit, when the synth
  is already sounding → render is awake → `tick()` is reliable. The idle problem
  only bites in silence, where there is no strum in flight. This is a *bounded*
  `tick()` dependency, distinct from OD2's stopped-transport timebase.

**Gate length:** `gate` param as a % of the step interval (e.g. 50% =
`CLOCKS_PER_STEP × 0.5`; >100% overlaps into later steps for legato). Counted in
clocks, decremented per `0xF8`.

**Swing:** delay odd steps by `swing%` of the step interval (adjust
`clocks_until_step` on odd steps) — same math as Beat Bank.

**No queue anywhere.** `cur_step` is a pure function of clock count; fast pad input
only edits the register set. This is what structurally prevents Super Arp's
lockup.

---

## 7. Pattern library — scope & sourcing

- **Target:** ~**200–300 curated, canonical, single-lane rhythm cells** across
  ~18–24 genres (same hand-authored, recognizable-grooves philosophy as Beat
  Bank). Up to ~600 with 1-bar/2-bar + rotation/density variants. **Euclidean
  generation is out of scope** (eucalypso/euclidrum own that lane).
- **Genre taxonomy (draft):** Afro-Cuban/Latin · Brazilian · Caribbean
  (reggae/ska/dancehall/soca/reggaeton) · African · Middle-Eastern *iqa'at* ·
  Funk/Soul/R&B · Jazz · Electronic (house/techno/garage/DnB/breaks) ·
  Rock/Pop/Folk strums · Indian/Asian.
- **Sourcing methodology (mirror Beat Bank):** each cell = `name` + `genre` +
  `hit:` row (quantized to 16/32 grid) + cross-check against **2+ references**;
  document per-genre references in research notes.
- **Authored variants per groove (DECIDED):** most grooves ship a base `hit:` +
  a few `alt:` flavors (busy / sparse / syncopated / rolled) — the "font family"
  model, morphed live on K4 (§4). Authoring these is part of the library pass.
- **Upside vs Beat Bank:** each groove is reusable across chords/bass/arps (it's
  just timing), so ~250 rhythm patterns go *further* than Beat Bank's 457 drum
  patterns — one "reggae skank" works under any chord you hold.
- This is a **separate research pass** (its own workflow, like Beat Bank's) that
  can run in parallel with the code.

---

## 8. Implementation stages (build order)

- **Stage 0 — GATE (BLOCKING, still Joe's to run):** verify Move's own pads feed
  a chain slot as live MIDI. Test: load the built-in `arp` + a synth into a slot,
  hold pads → confirm it arpeggiates *from Move's pads*. If it does, the input
  model is real. **If not**, input = external-controller-only → re-scope.
- **Stage 1 — Scaffold. ✅ DONE.** Repo mirrors `beatbank` (build.sh, Dockerfile,
  Makefile, `.dockerignore` + `rm -f build/aarch64/dsp.so`, module.json,
  release.json, install.sh, package.sh).
- **Stage 2 — DSP core. ✅ DONE.** `src/host/groovebank_plugin.c` — held register,
  swallow-and-retrigger, clock stepper, `fire_step`, voice/gate re-attack, ties,
  swing. Chord mode. Verified by `tests/engine_test.c` (`make test`).
- **Stage 3 — Canvas. ✅ DONE (partial).** `src/canvas.js` — single-row grid,
  geared K8, K7 swing, K1 gate. (K2 strum / K3 accent knobs deferred.)
- **Stage 4 — Starter set. ✅ DONE.** ~32 `.groove` patterns across 5 genres
  (latin/house/reggae/funk/strum) in `src/patterns/`.
- **Strum + Accent pass. ✅ DONE (v0.1.1).** K2 bipolar strum (sub-step onset
  spread via the bounded `tick()` scheduler) + K3 accent depth. Verified by
  `make test`.
- **Stage 5 — Hardware test (Joe):** `./scripts/build.sh && ./scripts/install.sh`;
  verify tightness, no lockup, chord changes, and the new strum/accent/gate feel.
- **Authored variants (K4). ✅ DONE (v0.1.2).** Parser `alt:` rows →
  `GroovePattern.row[GB_MAX_VARIANTS]` + `variant_count`; `variant` param on K4;
  `fire_step` reads the selected variant; canvas shows `v{n}/{count}`. Verified by
  `make test` (one starter groove, "Offbeat Stab", ships with 2 alts).
- **Stage 6 — Full library. ✅ DONE (v0.1.3), then CURATED DOWN (v0.1.4).**
  First pass: 235 grooves / 32 genres via 6 parallel research agents. Joe found
  it too much choice + thin (within-genre exact dups, 40% cross-genre repeats,
  too many tiny genres). Re-curated to **97 distinct roots / 198 variations /
  14 genres** (`src/patterns/{10-house..46-world}.groove`), one file per genre.
  Rules: no within-genre exact-dup base rows (was 16 → 0); duplicate feels folded
  into K4 variants of one root (e.g. REGGAE's 5 identical skanks → one Skank root);
  32→14 genres (folded electro/jungle/lofi/triphop/gospel/brazil/ska/dancehall/
  reggaeton/soca/calypso/mideast/india into parents; kept AFRO + one WORLD).
  Cross-genre sharing 40%→24% (remainder intrinsic: offbeat = skank = house stab).
  Also **widened accent contrast** (`step_velocity`: A/x/g now 127/90/38 at
  default 50, so accents pop above normal, not just above ghosts). `make test`
  green (variant assertions made count-robust).
- **Stage 7 — Ship:** `.github/workflows/release.yml`, docs, catalog entry,
  release synced with Charles's next Schwung release.

---

## 9. Build / deploy / release (mirror Beat Bank)

- Cross-compile `dsp.so` for ARM64 via Docker. **Keep** `.dockerignore`
  (exclude `.git`/`build`/`dist`) and `rm -f build/aarch64/dsp.so` before `make`
  (prevents a stale host `dsp.so` leaking into the Docker context — the real cause
  of Beat Bank's "cached build" confusion).
- Catalog entry: `id: groovebank` (pending OD1), `component_type: midi_fx`,
  `github_repo: mission-minnow/groovebank`, `asset_name: groovebank-module.tar.gz`.
- **Version convention (from Beat Bank):** `module.json` `version` = the
  dev/verification marker (bump patch per build so it's identifiable in Schwung
  Manager); `release.json` stays at the last *actually-released* version until a
  real tag, so the catalog never offers a missing tarball. Sync releases with
  Charles.

---

## 10. Prior art to lift (from the survey memory)

- **Held-note register + hold/latch:** Eucalypso `note_on`/`note_off`/
  `sync_active_to_physical`; Super Arp `physical_notes[]` + `physical_as_played[]`.
- **Clock stepper** (`clocks_per_step`, step-boundary advance): all four — but
  Beat Bank's own `bb_process_midi` is the closest match and **we already own it**;
  start there.
- **Voice/gate re-attack + voice-stealing:** Eucalypso `schedule_note` /
  `advance_voice_timers_*`; Super Arp `schedule_notes` / `kill_voice_notes`.
- **Strum / sub-step onset spread (K2):** Impressive Chords' `strum` (0–100 ms
  inter-note delay), strum *order* (articulate), and `fit` clamp + its `.tick()`
  sample-countdown note queue — lift this directly for the strum scheduler (§6).
- **Fire-first-step-synchronously-in-`process_midi`** (OD2 Phase 2, and also the
  strum's first note): Super Arp's grace-window trick — the clean way to nail the
  first onset from the MIDI callback.
- **Do NOT copy:** Genera's generative degree model; Eucalypso's 4-lane Euclidean
  register selector; **Super Arp's pending-queue** (the lockup source); Super
  Arp's progression/modifier engines.

---

## 11. Open decisions (RESOLVE before / early in build)

- **OD1 — Name. ✅ RESOLVED → Groove Bank.** `id: groovebank`, chain abbrev
  `GRUV`, menu name "Groove Bank". Part of the deliberate "Bank" family (Beat
  Bank's abbrev was changed `BANK`→`BEAT` to make room).
- **OD2 — Stopped-transport scope. ✅ RESOLVED → Phase 1 first.**
  - **v1 = Phase 1:** clock-driven only — **transport must be running.** Covers
    the primary workflow (groove bed under Beat Bank / over any running
    transport). Zero `tick()` dependency for step timing, maximally robust. *(The
    K2 strum still uses a bounded, safe `tick()` sub-step scheduler per §6 — that
    is separate from and unaffected by this decision.)*
  - **Phase 2 = DEFERRED** (revisit after feel-testing Phase 1 on hardware):
    pad-start-**when-stopped** via an internal frame-count clock in `tick()`, first
    hit fired synchronously in `process_midi`, gated behind a `sync` mode param.
    This is the one feature that would pull step timing into `tick()` — keep it
    isolated; do NOT build it in v1.
- **OD3 — First-hit timing. STILL OPEN.** Default: strict on-grid (tightest).
  Option to sound the very first press instantly for immediate feedback — decide
  after playing Phase 1.
