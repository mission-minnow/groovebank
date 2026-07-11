# Proposal: per-track "MIDI Local-Off" for Pre-mode chain MIDI-FX

**For:** Schwung (`charlesvestal/schwung`) — shim
**From:** Mission Minnow (Groove Bank / Beat Bank)
**Status:** draft / RFC

## Summary

Let a **Pre-mode** chain MIDI-FX play its processed output **on the same track
you're playing**, instead of the raw pad sustaining over the top. Do it by giving
that track a **MIDI local-off**: route its pad notes *directly into the chain*
(so the FX still receives them) while **suppressing the raw pad from Move's own
track instrument**. The FX's Pre-mode inject then becomes the only thing that
sounds the track — so you hear the groove/arp/generated pattern *in place*.

This is a small, self-contained shim addition, structurally identical to the
existing cable-2 `ext_midi_remap` (in-place cable MIDI rewrite per SPI frame).

## The problem

In Schw+Move (Pre) mode a chain MIDI-FX (Beat Bank, Groove Bank) injects its
processed MIDI into Move tracks. But when you press a pad, Move plays the
**selected track's instrument raw, in firmware** — before, and independent of,
the chain. So on the *played* track you hear **both** the raw pad **and** the
injected FX output, and the sustained raw chord masks the pattern.

The only workaround today is to **mute the played track** and route the FX output
to a **different** track (MIDI In on) to hear the processed result cleanly. That
works (nice for one-to-many arrangements — a drone on the played track + grooves
on listeners), but it means a Pre-mode FX can never play **in place** on the track
you're holding. A chain MIDI-FX is downstream of the pad→instrument path, so it
cannot fix this itself; only the shim sits upstream of it.

## Proposed behavior

For a slot with a Pre-mode MIDI-FX that opts in (see gating), enable **local-off**
for that track:

1. **Route the track's cable-0 pad note-on/off directly to the chain slot** (the
   same dispatch external cable-2 controllers use), so the FX receives the chord.
2. **Suppress those note events from Move's own track instrument** — zero/skip
   them in the hw mailbox before Move's firmware reads them (exactly the in-place
   cable rewrite `ext_midi_remap` already does, just *drop* instead of *remap*).
3. The FX's Pre-mode inject (on the slot's receive channel) then plays the track.

Net: **play pads on the track, hear the FX's pattern on that track's own
instrument.** Other tracks (MIDI In on the channel) still receive the inject as
today, so the existing multi-track workflow is unchanged.

## Why the shim (and why it mirrors an existing feature)

- Only the shim is upstream of Move's local pad→instrument play; the module can't
  reach it.
- The shim already rewrites **cable-0** MIDI in place every SPI frame for the
  `ext_midi_remap` table (rewrite channel byte in hw mailbox + shadow buffer,
  skip system messages, bypass when a slot is forward=THRU, force-reset on exit,
  gated in `features.json`). **This is the same operation** — drop the note for
  the local-off track instead of remapping its channel.

## Mechanism sketch

- Per-slot flag `pre_local_off` (set when an opted-in Pre-mode FX slot is active).
- Each SPI frame, for cable-0 MIDI_IN **note-on/off** whose channel maps to a
  `pre_local_off` slot's receive channel:
  - dispatch a copy to that chain slot (reuse `shadow_chain_dispatch_*` / the
    direct-external path) so the FX gets the chord;
  - drop the event from Move's hw mailbox so Move neither sounds nor records it
    locally.
- Non-note / system / realtime messages pass through untouched.
- Bypass for forward=THRU (MPE) slots, like `ext_midi_remap`.
- Force-reset to passthrough on slot unload / Pre toggle off.
- Gate with `pre_local_off_enabled` in `features.json` (default off), mirroring
  `ext_midi_remap_enabled`.

## Recording note

With local-off, the recorded clip captures the **FX output** (the groove), not
the raw chord — i.e. *record-what-you-hear*, which is normally what's wanted.
Anyone who wants the raw chords recorded simply leaves local-off off (current
behavior) or uses the separate-track workflow.

## Opt-in / scope

- Surfaced as a per-slot **"Local Off"** toggle, shown only when the loaded MIDI-FX
  is `pre_capable` and the slot is in Pre mode.
- Optionally a new capability, e.g. `"pre_replaces_input": true`, so only
  **swallow-and-retrigger** FX (which consume their input and re-emit) offer it —
  Groove Bank / Beat Bank yes; a passthrough/CC FX no.
- No module-side API change is strictly required; local-off is a slot/shim
  behavior driven by the existing `pre_capable` + the new toggle.

## Update — findings from a shim MIDI-flow audit (2026-07-11)

Two things surfaced while mapping the shim that refine this proposal:

1. **A raw suppress-and-redispatch is not enough — pitch mapping.** Cable-0 pad
   events carry the **physical pad index (68–99)**; the chain currently receives
   the **scale/pad-mapped musical pitch** only via the cable-2 echo (Move's
   firmware does the mapping). So if the shim drops the pad in MIDI_IN and
   re-dispatches the raw event to the chain, the FX grooves the *wrong notes*. A
   proper shim local-off must let Move **process the note for pitch + echo** but
   **not sound it** — i.e. real per-voice local-off inside Move, not just a
   MIDI_IN drop. That's the crux ask.

2. **A module-side "soft" local-off already works (shipping in Groove Bank
   v0.1.12) and may be enough** for most cases: when enabled, the FX emits a
   **note-off for each played pitch**, which (in Pre mode) is injected to Move and
   **kills the raw pad's sustain** — using the already-correct mapped pitch, no
   MIDI_IN editing, no wrong-note risk. The only cost is a brief attack transient
   (Move sounds the pad for ~1 frame before the injected note-off lands, because
   the FX is downstream of the pad). A shim/firmware local-off would remove even
   that transient. So the shim feature is the "hard" version; the module gives a
   good "soft" version today. Worth deciding whether the transient is worth a
   firmware change.

## Open questions for you

- Best surface for the toggle (slot MIDI-FX settings vs a global default)?
- Should local-off suppress **only** the notes the FX actually consumed, or all
  notes on the channel while active? (The former is safer if a slot's FX passes
  some notes through; the latter is simpler.)
- Any interaction with the root-match echo-skip in the Pre-mode inject path we
  should be aware of?
