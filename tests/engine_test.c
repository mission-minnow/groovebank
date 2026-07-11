/*
 * engine_test.c — off-hardware sanity check for the Groove Bank engine.
 *
 * Drives move_midi_fx_init's plugin with fake MIDI and asserts the core
 * contract: swallow-and-retrigger, downbeat fire, retrigger cadence on the
 * clock, live chord changes, and stop-flush. Build/run:
 *
 *   gcc -std=c99 -Wall -Wextra -Isrc/dsp -Isrc/host \
 *       tests/engine_test.c src/dsp/grooves.c src/host/groovebank_plugin.c \
 *       -o build/native/engine_test && build/native/engine_test
 */

#include "midi_fx_api_v1.h"
#include "plugin_api_v1.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host);

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ok   %s\n", msg); } \
    else      { printf("  FAIL %s\n", msg); g_fail = 1; } } while (0)

static int g_clock = MOVE_CLOCK_STATUS_RUNNING;
static int host_clock(void) { return g_clock; }

/* count note-ons / note-offs in a process_midi output batch */
static void tally(uint8_t out[][3], int lens[], int n, int *ons, int *offs)
{
    *ons = *offs = 0;
    for (int i = 0; i < n; i++) {
        uint8_t st = out[i][0] & 0xF0;
        if (lens[i] < 3) continue;
        if (st == 0x90 && out[i][2] > 0) (*ons)++;
        else if (st == 0x80 || (st == 0x90 && out[i][2] == 0)) (*offs)++;
    }
}

static int send(midi_fx_api_v1_t *api, void *inst,
                uint8_t a, uint8_t b, uint8_t c, int len,
                int *ons, int *offs)
{
    uint8_t msg[3] = { a, b, c };
    uint8_t out[16][3];
    int lens[16];
    int n = api->process_midi(inst, msg, len, out, lens, 16);
    tally(out, lens, n, ons, offs);
    return n;
}

/* send one message, return the velocity of its first emitted note-on (0 if none) */
static uint8_t send_first_vel(midi_fx_api_v1_t *api, void *inst,
                              uint8_t a, uint8_t b, uint8_t c, int len)
{
    uint8_t msg[3] = { a, b, c };
    uint8_t out[16][3];
    int lens[16];
    int n = api->process_midi(inst, msg, len, out, lens, 16);
    for (int i = 0; i < n; i++)
        if (lens[i] >= 3 && (out[i][0] & 0xF0) == 0x90 && out[i][2] > 0) return out[i][2];
    return 0;
}

/* call tick(), tally note-ons/offs */
static void tk(midi_fx_api_v1_t *api, void *inst, int frames, int sr, int *ons, int *offs)
{
    uint8_t out[16][3];
    int lens[16];
    int n = api->tick(inst, frames, sr, out, lens, 16);
    tally(out, lens, n, ons, offs);
}

/* advance `clocks` MIDI ticks, summing note-ons/offs produced */
static void run_clocks(midi_fx_api_v1_t *api, void *inst, int clocks, int *ons, int *offs)
{
    *ons = *offs = 0;
    for (int i = 0; i < clocks; i++) {
        int o, f;
        send(api, inst, 0xF8, 0, 0, 1, &o, &f);
        *ons += o; *offs += f;
    }
}

int main(void)
{
    host_api_v1_t host;
    memset(&host, 0, sizeof(host));
    host.get_clock_status = host_clock;

    midi_fx_api_v1_t *api = move_midi_fx_init(&host);
    CHECK(api && api->api_version == MIDI_FX_API_VERSION, "plugin init + api version");

    void *inst = api->create_instance("src", NULL);
    CHECK(inst != NULL, "create_instance");

    char buf[128];
    api->get_param(inst, "pattern_count", buf, sizeof buf);
    int count = atoi(buf);
    CHECK(count > 0, "bank loaded patterns");

    /* find "Four Floor" (row x...x...x...x...) for a deterministic cadence */
    int four = -1;
    for (int i = 0; i < count; i++) {
        char key[24]; snprintf(key, sizeof key, "name@%d", i);
        api->get_param(inst, key, buf, sizeof buf);
        if (strcmp(buf, "Four Floor") == 0) { four = i; break; }
    }
    CHECK(four >= 0, "found 'Four Floor' groove");
    char pv[8]; snprintf(pv, sizeof pv, "%d", four);
    api->set_param(inst, "pattern", pv);
    api->get_param(inst, "row0", buf, sizeof buf);
    CHECK(strncmp(buf, "x...x...x...x...", 16) == 0, "row0 = x...x...x...x...");
    api->set_param(inst, "gate", "50");   /* 3 clocks, < one step, clean gates */

    int ons, offs, n;

    /* 1. Hold a triad — each note-on must be swallowed (0 output) */
    n = send(api, inst, 0x90, 60, 100, 3, &ons, &offs);
    CHECK(n == 0, "note-on 60 swallowed (0 out)");
    send(api, inst, 0x90, 64, 100, 3, &ons, &offs);
    n = send(api, inst, 0x90, 67, 100, 3, &ons, &offs);
    CHECK(n == 0, "note-on 67 swallowed");
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 3, "held_count == 3");

    /* 2. Start: downbeat (step 0 is a hit) fires the whole held triad */
    n = send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);
    CHECK(ons == 3 && offs == 0, "0xFA fires 3 note-ons (downbeat chord)");

    /* 3. Retrigger cadence: over the next 16 steps (96 clocks) Four Floor hits
     *    steps 4,8,12, then 0 of the next bar = 4 hits x 3 notes = 12 ons. */
    run_clocks(api, inst, 96, &ons, &offs);
    CHECK(ons == 12, "12 note-ons across one bar (4 hits x triad)");
    CHECK(offs >= 12, "note-offs scheduled (gate releases)");

    /* 4. Live chord change: drop the 3rd (64). Next hits fire only 60 & 67. */
    send(api, inst, 0x80, 64, 0, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 2, "release note 64 -> held_count == 2");
    run_clocks(api, inst, 96, &ons, &offs);
    CHECK(ons == 8, "after release: 8 note-ons (4 hits x 2 notes)");

    /* 5. Non-note channel message passes through (mod wheel) */
    n = send(api, inst, 0xB0, 1, 64, 3, &ons, &offs);
    CHECK(n == 1, "CC passes through (1 out)");

    /* 6. Stop flushes any sounding voices */
    send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);   /* restart, fire chord */
    n = send(api, inst, 0xFC, 0, 0, 1, &ons, &offs);
    CHECK(offs >= 1, "0xFC flushes sounding voices (note-offs)");

    /* 7. Empty register: hits are silent (no crash, no output) */
    send(api, inst, 0x80, 60, 0, 3, &ons, &offs);
    send(api, inst, 0x80, 67, 0, 3, &ons, &offs);
    send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);
    run_clocks(api, inst, 96, &ons, &offs);
    CHECK(ons == 0, "no held notes -> silent (0 note-ons)");

    /* 8. Accent depth (velocity) — find any groove whose base row starts with 'A' */
    int acc_pat = -1;
    for (int i = 0; i < count; i++) {
        char p[8]; snprintf(p, sizeof p, "%d", i);
        api->set_param(inst, "pattern", p);
        api->get_param(inst, "row0", buf, sizeof buf);
        if (buf[0] == 'A') { acc_pat = i; break; }
    }
    CHECK(acc_pat >= 0, "found a groove accented on step 0");
    send(api, inst, 0x90, 60, 100, 3, &ons, &offs);        /* hold one note */
    api->set_param(inst, "accent", "0");
    CHECK(send_first_vel(api, inst, 0xFA, 0, 0, 1) == 100, "accent 0 -> flat velocity 100");
    send(api, inst, 0xFC, 0, 0, 1, &ons, &offs);
    api->set_param(inst, "accent", "100");
    CHECK(send_first_vel(api, inst, 0xFA, 0, 0, 1) == 127, "accent 100 -> accented velocity 127");
    send(api, inst, 0xFC, 0, 0, 1, &ons, &offs);

    /* 9. Strum — first onset fires now, the rest emerge from tick() */
    snprintf(pv, sizeof pv, "%d", four);   /* Four Floor: step 0 is a hit */
    api->set_param(inst, "pattern", pv);
    api->set_param(inst, "accent", "50");
    send(api, inst, 0x90, 64, 100, 3, &ons, &offs);        /* held is now 60,64,67 */
    send(api, inst, 0x90, 67, 100, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 3, "strum setup: triad held");
    api->set_param(inst, "strum", "60");                   /* up-strum */
    send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);
    CHECK(ons == 1, "strum: 0xFA fires only the first onset immediately");
    {
        int total = 0, o, f;
        for (int k = 0; k < 40; k++) { tk(api, inst, 1024, 44100, &o, &f); total += o; }
        CHECK(total == 2, "strum: remaining 2 onsets fire from tick()");
    }
    /* tight strum (0) fires the whole chord at once again */
    send(api, inst, 0xFC, 0, 0, 1, &ons, &offs);
    api->set_param(inst, "strum", "0");
    send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);
    CHECK(ons == 3, "strum 0 (tight): whole triad fires immediately");

    /* 10. Authored variants — 'Offbeat Stab' = base + 2 alts */
    int oi = -1;
    for (int i = 0; i < count; i++) {
        char key[24]; snprintf(key, sizeof key, "name@%d", i);
        api->get_param(inst, key, buf, sizeof buf);
        if (strcmp(buf, "Offbeat Stab") == 0) { oi = i; break; }
    }
    CHECK(oi >= 0, "found 'Offbeat Stab' groove");
    snprintf(pv, sizeof pv, "%d", oi);
    api->set_param(inst, "pattern", pv);
    api->get_param(inst, "variant_count", buf, sizeof buf);
    int ovc = atoi(buf);
    CHECK(ovc >= 3, "Offbeat Stab has multiple variants");
    api->set_param(inst, "variant", "0");
    api->get_param(inst, "row0", buf, sizeof buf);
    CHECK(strncmp(buf, "..x...x...x...x.", 16) == 0, "variant 0 = base row");
    api->set_param(inst, "variant", "1");
    api->get_param(inst, "row0", buf, sizeof buf);
    CHECK(strncmp(buf, "..x.x.x...x.x.x.", 16) == 0, "variant 1 = busy alt");
    api->set_param(inst, "variant", "99");   /* out of range -> clamps to last */
    api->get_param(inst, "variant", buf, sizeof buf);
    CHECK(atoi(buf) == ovc - 1, "variant clamps to last");
    snprintf(pv, sizeof pv, "%d", four);
    api->set_param(inst, "pattern", pv);
    int fvc, fvv;
    api->get_param(inst, "variant_count", buf, sizeof buf); fvc = atoi(buf);
    api->get_param(inst, "variant", buf, sizeof buf); fvv = atoi(buf);
    CHECK(fvc >= 1, "variant_count >= 1 after switch");
    CHECK(fvv >= 0 && fvv < fvc, "variant clamped into range on pattern switch");

    /* 11. Latch — held chord sticks after release */
    api->set_param(inst, "latch", "0");
    for (int rn = 0; rn < 128; rn++) send(api, inst, 0x80, (uint8_t)rn, 0, 3, &ons, &offs); /* clear */
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 0, "latch off: register empty after releasing all");

    /* latch OFF: press then release -> register empties */
    send(api, inst, 0x90, 60, 100, 3, &ons, &offs);
    send(api, inst, 0x90, 64, 100, 3, &ons, &offs);
    send(api, inst, 0x80, 60, 0, 3, &ons, &offs);
    send(api, inst, 0x80, 64, 0, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 0, "latch off: release clears the register");

    /* latch ON: press then release -> chord STICKS */
    api->set_param(inst, "latch", "1");
    send(api, inst, 0x90, 60, 100, 3, &ons, &offs);
    send(api, inst, 0x90, 64, 100, 3, &ons, &offs);
    send(api, inst, 0x90, 67, 100, 3, &ons, &offs);
    send(api, inst, 0x80, 60, 0, 3, &ons, &offs);
    send(api, inst, 0x80, 64, 0, 3, &ons, &offs);
    send(api, inst, 0x80, 67, 0, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 3, "latch on: chord sticks after release");

    /* the latched chord still fires on the downbeat */
    send(api, inst, 0xFA, 0, 0, 1, &ons, &offs);
    CHECK(ons >= 1, "latch on: latched chord fires on 0xFA");
    send(api, inst, 0xFC, 0, 0, 1, &ons, &offs);

    /* a fresh press (nothing down) replaces the latched chord */
    send(api, inst, 0x90, 62, 100, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 1, "latch on: fresh press replaces the latched chord");

    /* turning latch OFF syncs the register to what's physically held (62 still down) */
    api->set_param(inst, "latch", "0");
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 1, "latch off: register syncs to physically-held notes");
    send(api, inst, 0x80, 62, 0, 3, &ons, &offs);
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 0, "after latch off, release clears the register");

    /* 12. Local-off — emit a note-off on input (kills the played-track drone) */
    api->set_param(inst, "latch", "0");
    for (int rn = 0; rn < 128; rn++) send(api, inst, 0x80, (uint8_t)rn, 0, 3, &ons, &offs); /* clear */
    api->set_param(inst, "local_off", "0");
    n = send(api, inst, 0x90, 60, 100, 3, &ons, &offs);
    CHECK(n == 0, "local_off off: note-on is swallowed (0 out)");
    api->set_param(inst, "local_off", "1");
    n = send(api, inst, 0x90, 62, 100, 3, &ons, &offs);
    CHECK(n == 1 && offs == 1 && ons == 0, "local_off on: note-on emits a note-off (kills drone)");
    api->get_param(inst, "held_count", buf, sizeof buf);
    CHECK(atoi(buf) == 2, "local_off on: note still captured to the register");

    api->destroy_instance(inst);

    printf(g_fail ? "\nSOME TESTS FAILED\n" : "\nALL TESTS PASSED\n");
    return g_fail;
}
