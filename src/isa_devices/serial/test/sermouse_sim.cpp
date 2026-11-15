// sermouse_sim.cpp -- host-side end-to-end simulation of the serial-mouse
// emulation chain, stages 4-7 (UART register model -> SPSC rings -> Core-0
// pacing/IRQ -> mouse layer), driving the REAL firmware sources unmodified:
//   8250uart.cpp + sermouse.cpp  (compiled for the host via test/stubs/)
//
// The test driver plays both sides:
//   "Core 1" = uartemu_c1_read/c1_write calls  (the guest's IN/OUT)
//   "Core 0" = uartemu_task + sermouse_task    (the firmware hot loop)
// Time is simulated (g_sim_now), so pacing, the 3 ms power rule and the
// detection window are tested deterministically, including Core-0 stalls.
//
// Scenarios mirror the real guests: SERTEST (IER=01 IRQ-driven), CTMOUSE
// (loopback probe + reset scan), Windows 3.11 (IER=0F, MCR churn), plus
// framing/pacing/stall invariants. Build & run: make -C .  (see makefile)
//
// Exit code = number of failed checks (0 = all green).

#include <stdio.h>
#include <string.h>
#include <vector>
#include "8250uart.h"
#include "sermouse.h"

uint64_t g_sim_now = 1000000;         // t0 = 1 s (nonzero, arbitrary)

// ---------------------------------------------------------------- checks
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

// ---------------------------------------------------------------- IRQ line model
// Mirrors dev_sermouse's hooks + the physical line (assert-capable only;
// the open-drain release is modeled as an immediate deassert here -- the
// bus-side pull-up question is Phase 0's bench test, not simulatable).
static bool     g_line = false;       // asserted?
static uint32_t g_raises = 0, g_lowers = 0;
static void hook_raise(void *)   { if (!g_line) g_raises++; g_line = true;  }
static void hook_lower(void *)   { if (g_line)  g_lowers++; g_line = false; }
static void hook_mcr(void *, uint8_t p, uint8_t n, uint32_t t) { sermouse_mcr_change(p, n, t); }

static uartemu_t U;

// ---------------------------------------------------------------- helpers
static void fw_pass(void) {           // one Core-0 hot-loop pass
    uartemu_task(&U);
    sermouse_task();
}
// advance simulated time, running the firmware every 20 us (hot loop cadence)
static void run_us(uint64_t us, uint64_t step = 20) {
    uint64_t end = g_sim_now + us;
    while (g_sim_now < end) { g_sim_now += step; fw_pass(); }
}
static uint8_t inb (uint8_t reg)              { return uartemu_c1_read(&U, reg); }
static void    outb(uint8_t reg, uint8_t v)   { uartemu_c1_write(&U, reg, v); }

static void feed(int8_t dx, int8_t dy, uint8_t buttons) {
    hid_mouse_report_t r = {}; r.x = dx; r.y = dy; r.buttons = buttons;
    if (!sermouse_in_detect_window()) sermouse_process_report(&r); // dev gate
}

// Level-triggered guest ISR (SERTEST/Windows shape): while the line is
// asserted, read IIR (servicing modem-status via MSR like a real driver),
// drain RBR while LSR.DR, EOI. Returns bytes collected.
// `budget` guards against a stuck-line storm (the sim equivalent of 30000x).
static std::vector<uint8_t> isr_drain(int budget = 50) {
    std::vector<uint8_t> got;
    while (g_line && budget-- > 0) {
        uint8_t iir = inb(2);
        if ((iir & 0x0F) == 0x00) (void)inb(6);    // modem status -> read MSR
        while (inb(5) & 0x01) got.push_back(inb(0));
        // EOI -- on a level 8259 the line re-fires only if still asserted
    }
    return got;
}

// Faithful steady-state model: interleave the Core-0 hot loop with the
// level-triggered ISR (a real 8259 re-fires after EOI while the line is
// asserted, and the firmware pushes the next paced byte between IRQs).
// Runs until the wire has been quiet for `quiet_us`.
static void pump(std::vector<uint8_t> &sink, uint64_t quiet_us = 40000,
                 uint64_t cap_us = 2000000) {
    uint64_t last_evt = g_sim_now, start = g_sim_now;
    while (g_sim_now - last_evt < quiet_us && g_sim_now - start < cap_us) {
        g_sim_now += 20; fw_pass();
        if (g_line) {
            auto got = isr_drain(4);
            sink.insert(sink.end(), got.begin(), got.end());
            if (!got.empty()) last_evt = g_sim_now;
        }
    }
}

// Decode a Microsoft 3-byte stream back into (dx, dy, buttons) packets.
struct Pkt { int dx, dy; bool lb, rb; };
static std::vector<Pkt> decode_ms(const std::vector<uint8_t> &bytes) {
    std::vector<Pkt> out;
    for (size_t i = 0; i + 2 < bytes.size(); ) {
        if (!(bytes[i] & 0x40)) { i++; continue; }         // hunt SYNC
        uint8_t b0 = bytes[i], b1 = bytes[i + 1], b2 = bytes[i + 2];
        if ((b1 & 0x40) || (b2 & 0x40)) { i++; continue; } // data bytes must have bit6=0
        Pkt p;
        p.dx = (int8_t)(uint8_t)(((b0 & 0x03) << 6) | b1);
        p.dy = (int8_t)(uint8_t)(((b0 & 0x0C) << 4) | b2);
        p.lb = b0 & 0x20; p.rb = b0 & 0x10;
        out.push_back(p);
        i += 3;
    }
    return out;
}

// ---------------------------------------------------------------- scenarios

// Fresh boot for each scenario.
static void boot(void) {
    uartemu_ops_t ops = {};
    ops.raise_irq = hook_raise; ops.lower_irq = hook_lower; ops.c1_lower_irq = hook_lower;
    ops.mcr_change = hook_mcr;
    uartemu_init(&U, 0x2F8, &ops);
    sermouse_init(&U, SERMOUSE_REPORTRATE_DEFAULT, 256);
    g_line = false; g_raises = g_lowers = 0;
    run_us(1000);
}

// --- 1. SERTEST: reset 'M' delivered by IRQ exactly once, line balanced ---
static void scn_sertest_reset(void) {
    printf("-- SERTEST: init -> 'M' by IRQ3, no refire\n");
    boot();
    outb(4, 0x00);                    // MCR=0 (power off)
    outb(1, 0x00);
    outb(3, 0x80); outb(0, 96); outb(1, 0);   // DLAB, 1200 baud
    outb(3, 0x02);                    // 7N1
    (void)inb(0);                     // flush
    outb(1, 0x01);                    // IER = RX only
    run_us(5000);                     // host pause (>=3ms off time)
    outb(4, 0x0B);                    // DTR+RTS+OUT2 -> power on
    run_us(200);                      // a couple of fw passes

    CHECK(g_line, "IRQ3 should be asserted for the 'M'");
    CHECK(inb(2) == 0x04, "IIR should read RX-data-available");
    auto got = isr_drain();
    CHECK(got.size() == 1 && got[0] == 'M', "expected exactly ['M'], got %zu bytes", got.size());
    CHECK(!g_line, "line must be LOW after the ISR drained");
    run_us(2000);
    CHECK(!g_line, "no refire with nothing pending (task passes)");
    CHECK(g_raises == g_lowers || g_raises == g_lowers + (g_line ? 1 : 0),
          "raise/lower balanced: R=%u L=%u", g_raises, g_lowers);
    // idle: no data, LSR shows THRE|TEMT only
    CHECK(inb(5) == 0x60, "idle LSR=0x60, got %02X", inb(5));
}

// --- 2. movement: framed packets, one IRQ per byte, motion reconstructs ---
static void scn_movement(void) {
    printf("-- movement: 3-byte framing, pacing, motion integrity\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B);
    run_us(200); isr_drain();                       // consume the 'M'
    run_us(320000);                                 // leave the detect window

    // feed a known motion pattern over 200 ms of USB reports
    int fed_x = 0, fed_y = 0;
    std::vector<uint8_t> wire;
    for (int i = 0; i < 50; i++) {
        feed(+3, -2, (i > 25) ? MOUSE_BUTTON_LEFT : 0);
        fed_x += 3; fed_y += -2;
        run_us(4000);                               // 250 Hz USB cadence
        auto got = isr_drain();
        wire.insert(wire.end(), got.begin(), got.end());
    }
    pump(wire, 60000);                              // drain the tail faithfully

    auto pkts = decode_ms(wire);
    CHECK(!pkts.empty(), "expected packets on the wire, got %zu bytes", wire.size());
    CHECK(wire.size() % 3 == 0, "wire bytes %zu should be whole 3-byte packets", wire.size());
    int sum_x = 0, sum_y = 0; bool saw_lb = false;
    for (auto &p : pkts) { sum_x += p.dx; sum_y += p.dy; saw_lb |= p.lb; }
    CHECK(sum_x == fed_x && sum_y == fed_y,
          "motion must reconstruct exactly: fed(%d,%d) wire(%d,%d)", fed_x, fed_y, sum_x, sum_y);
    CHECK(saw_lb, "left-button press must appear in packets");
    CHECK(g_raises >= wire.size(), "one raise per delivered byte (R=%u bytes=%zu)", g_raises, wire.size());
    CHECK(!g_line, "line idle-low at the end");
}

// --- 3. Core-0 stall: guest drains byte, line must drop without Core 0 ---
static void scn_core0_stall(void) {
    printf("-- Core-0 stall: c1 lower-on-empty prevents refire burst\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B);
    run_us(200);
    CHECK(g_line, "'M' pending, line up");
    // Core 0 stalls (SD/USB): NO fw_pass while the guest ISR runs
    (void)inb(2);
    uint8_t b = 0;
    if (inb(5) & 1) b = inb(0);
    CHECK(b == 'M', "guest got the 'M' during the stall");
    CHECK(!g_line, "line LOW immediately after drain -- WITHOUT any Core-0 pass");
    run_us(2000);
    CHECK(!g_line, "and stays low once Core 0 resumes");
}

// --- 4. CTMOUSE: loopback probe + MSR map + fast echo + reset scan -------
static void scn_ctmouse(void) {
    printf("-- CTMOUSE: loopback MSR map, echo latency, reset scan\n");
    boot();
    outb(4, 0x0B); run_us(400); isr_drain();        // initial power-on
    // loopback probe: MCR outputs must map to MSR inputs SAME-CYCLE
    outb(4, 0x13);                                  // loopback + DTR+RTS
    uint8_t msr = inb(6);
    CHECK((msr & 0xF0) == 0x30, "loopback MSR map RTS->CTS|DTR->DSR: %02X", msr);
    outb(4, 0x1F);                                  // + OUT1 + OUT2
    msr = inb(6);
    CHECK((msr & 0xF0) == 0xF0, "loopback MSR all-high: %02X", msr);
    // echo: byte written in loopback comes back promptly (not paced 5.5 ms)
    (void)inb(0);                                   // consuming flush first
    outb(0, 0xA5);
    run_us(200);                                    // a few fw passes only
    CHECK(inb(5) & 0x01, "loopback echo ready within ~200 us");
    CHECK(inb(0) == 0xA5, "echo byte intact");
    CHECK(!g_line, "loopback gates OUT2 -> no IRQ during the probe");
    // reset: RTS low >=3 ms then raise -> 'M' first in the scan window
    outb(4, 0x03);                                  // leave loopback, power on
    run_us(1000);
    outb(4, 0x01);                                  // RTS drop
    run_us(120000);                                 // CTMOUSE waits ~100 ms
    outb(4, 0x03);                                  // RTS raise (no OUT2 -- polled!)
    run_us(40000);                                  // detection scan window
    CHECK(inb(5) & 0x01, "'M' waiting for the polled scan");
    CHECK(inb(0) == 'M', "first scanned byte is 'M'");
    CHECK(!g_line, "OUT2 off -> no IRQ during polled detection");
}

// --- 5. Windows: IER=0F, MCR churn must not reset/leak 'M', deltas clear --
static void scn_windows(void) {
    printf("-- Windows 3.11: MCR churn, IER=0F, modem deltas\n");
    boot();
    outb(3, 0x02); outb(1, 0x0F);                   // 7N1, ALL ints enabled
    run_us(5000); outb(4, 0x0B);                    // power on -> reset
    run_us(200);
    // modem-status int from the reset's CTS/DSR/DCD rise + 'M' RX pending +
    // TX-empty from enabling IER bit1: the ISR/firmware interleave services
    // all three (level 8259 re-fires per remaining cause).
    std::vector<uint8_t> got;
    pump(got, 20000);
    bool saw_m = false; for (auto b : got) saw_m |= (b == 'M');
    CHECK(saw_m, "Windows sees the 'M' ID");
    CHECK(!g_line, "all interrupt causes cleared");
    // steady-state MCR churn (0x0B <-> 0x0F), never dropping RTS/DTR
    uint32_t resets_before; sermouse_debug_pkt(0, 0, &resets_before);
    for (int i = 0; i < 200; i++) { outb(4, (i & 1) ? 0x0F : 0x0B); run_us(500); }
    uint32_t resets_after; sermouse_debug_pkt(0, 0, &resets_after);
    CHECK(resets_after == resets_before, "MCR churn must not re-reset (%u -> %u)",
          resets_before, resets_after);
    run_us(320000);                                 // clear detect window
    // movement under churn still flows and frames
    std::vector<uint8_t> wire;
    for (int i = 0; i < 20; i++) {
        feed(-5, +7, MOUSE_BUTTON_RIGHT);
        outb(4, (i & 1) ? 0x0F : 0x0B);
        run_us(17000);
        auto g2 = isr_drain(); wire.insert(wire.end(), g2.begin(), g2.end());
    }
    pump(wire, 60000);                              // tail, faithfully
    auto pkts = decode_ms(wire);
    int sx = 0, sy = 0; for (auto &p : pkts) { sx += p.dx; sy += p.dy; }
    CHECK(sx == -100 && sy == +140, "churned motion reconstructs: (%d,%d)", sx, sy);
    CHECK(wire.size() % 3 == 0 && !pkts.empty(), "no framing slip under churn (%zu bytes)", wire.size());
}

// --- 6. quick power-dip must NOT emit 'M'; real off/on must ---------------
static void scn_power_rules(void) {
    printf("-- power rules: 3 ms dip filter\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B); run_us(200); isr_drain();
    uint32_t r0; sermouse_debug_pkt(0, 0, &r0);
    outb(4, 0x09);                                  // RTS dip...
    run_us(1000);                                   // ...1 ms only
    outb(4, 0x0B);
    run_us(1000);
    uint32_t r1; sermouse_debug_pkt(0, 0, &r1);
    CHECK(r1 == r0, "1 ms dip: no reset (%u -> %u)", r0, r1);
    CHECK(!(inb(5) & 1), "1 ms dip: no 'M' on the wire");
    outb(4, 0x09); run_us(10000); outb(4, 0x0B); run_us(200);
    uint32_t r2; sermouse_debug_pkt(0, 0, &r2);
    CHECK(r2 == r1 + 1, "10 ms off: reset fires (%u -> %u)", r1, r2);
    auto got = isr_drain();
    CHECK(got.size() == 1 && got[0] == 'M', "10 ms off: fresh 'M'");
}

// --- 7. register fidelity odds & ends -------------------------------------
static void scn_registers(void) {
    printf("-- register fidelity: DLAB, scratch, IIR idle, empty RBR\n");
    boot();
    outb(3, 0x80); outb(0, 96); outb(1, 0);
    CHECK(inb(0) == 96 && inb(1) == 0, "divisor readback via DLAB");
    outb(3, 0x02);
    CHECK(inb(3) == 0x02, "LCR readback");
    outb(7, 0x5A);
    CHECK(inb(7) == 0x5A, "scratchpad");
    CHECK(inb(2) == 0x01, "IIR idle = 0x01");
    uint8_t stale = inb(0); (void)stale;            // empty RBR: no crash, stale ok
    CHECK(!(inb(5) & 1), "no phantom DR");
    CHECK(uartemu_get_divisor(&U) == 96, "divisor accessor");
}

// ================= regression scenarios for the 2026-07-15 review =========

// --- R1. rxq lap: repeated unread resets must deliver exactly ONE 'M' -----
static void scn_rxq_lap(void) {
    printf("-- R1: 5 unread power-cycles -> exactly one 'M' (ring lap guard)\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B); run_us(200);   // reset #1, unread
    for (int i = 0; i < 5; i++) {                              // 5 more, all unread
        outb(4, 0x09); run_us(10000);                          // committed off
        outb(4, 0x0B); run_us(200);                            // fresh 'M' queued
    }
    std::vector<uint8_t> got;
    pump(got, 20000);
    int ms = 0; for (auto b : got) if (b == 'M') ms++;
    CHECK(ms == 1, "exactly one fresh 'M' (got %d of %zu bytes)", ms, got.size());
    CHECK(got.size() == 1, "no duplicate/stale bytes (got %zu)", got.size());
    CHECK(!(inb(5) & 1), "queue empty after");
    CHECK(!g_line, "line low after");
}

// --- R2. TX interrupt chain: IIR-read + THR-write in the SAME pass window --
static void scn_tx_chain(void) {
    printf("-- R2: interrupt-driven TX chain survives stall-window read+write\n");
    boot();
    run_us(5000); outb(4, 0x0B); run_us(400);
    while (inb(5) & 1) (void)inb(0);                 // flush the 'M' (polled)
    run_us(200);
    outb(1, 0x02);                                   // THRE int only (edge 0->1)
    run_us(200);
    CHECK(g_line, "THRE int raised on IER enable edge");
    int chained = 0;
    for (int i = 0; i < 10; i++) {
        if (!g_line) break;
        uint8_t iir = inb(2);                        // ISR: read IIR...
        CHECK((iir & 0x0F) == 0x02, "IIR presents THRE (%02X)", iir);
        CHECK(!g_line, "IIR read lowers the line (C1, no fw pass needed)");
        outb(0, (uint8_t)i);                         // ...write next byte --
                                                     // SAME stall window!
        run_us(200);                                 // Core 0 catches up
        if (g_line) chained++;                       // must re-raise for byte i
    }
    CHECK(chained == 10, "TX chain: %d/10 re-raises (ordering bug kills this)", chained);
    (void)inb(2); run_us(200);                       // final service
    CHECK(!g_line, "chain ends clean");
}

// --- R3. short dip full symmetry: lines/deltas/bytes all untouched ---------
static void scn_dip_symmetry(void) {
    printf("-- R3: <3ms dip leaves modem lines, deltas and RX bytes intact\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B); run_us(200);
    { auto got = isr_drain(); (void)got; }           // consume 'M'
    (void)inb(6);                                    // clear reset deltas
    run_us(320000);                                  // leave detect window
    feed(+9, 0, 0); run_us(17000);                   // a packet: SYNC in flight
    uint8_t before = (uint8_t)(inb(6) & 0xF0);
    CHECK(before == 0xB0, "powered lines CTS|DSR|DCD (%02X)", before);
    run_us(200);
    outb(4, 0x09);                                   // dip...
    run_us(1000);                                    // ...1 ms
    outb(4, 0x0B);
    run_us(1000);
    uint8_t msr = inb(6);
    CHECK((msr & 0xF0) == 0xB0, "lines STILL up after ridden-out dip (%02X)", msr);
    CHECK((msr & 0x0F) == 0x00, "no delta spray from the dip (%02X)", msr);
    std::vector<uint8_t> wire; pump(wire, 40000);
    auto pkts = decode_ms(wire);
    int sx = 0; for (auto &p : pkts) sx += p.dx;
    CHECK(sx == 9, "in-flight packet survives the dip intact (dx=%d)", sx);
}

// --- R4. dip timing measured at guest WRITE time, not Core-0 drain time ----
static void scn_dip_timing_stall(void) {
    printf("-- R4: power-rule timing immune to Core-0 stalls (write timestamps)\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B); run_us(200); isr_drain();
    uint32_t r0; sermouse_debug_pkt(0, 0, &r0);
    // (a) 10 ms dip, but BOTH edges drain in ONE pass (Core 0 stalled):
    outb(4, 0x09);
    g_sim_now += 10000;                              // guest time passes, NO fw
    outb(4, 0x0B);
    run_us(400);                                     // Core 0 wakes, drains both
    uint32_t r1; sermouse_debug_pkt(0, 0, &r1);
    CHECK(r1 == r0 + 1, "10ms dip inside a stall still resets (%u -> %u)", r0, r1);
    { auto got = isr_drain(); CHECK(got.size() == 1 && got[0] == 'M', "and delivers 'M'"); }
    // (b) 1 ms dip, drained only after a 50 ms stall: must NOT reset
    outb(4, 0x09);
    g_sim_now += 1000;
    outb(4, 0x0B);
    g_sim_now += 50000;                              // long stall before drain
    run_us(400);
    uint32_t r2; sermouse_debug_pkt(0, 0, &r2);
    CHECK(r2 == r1, "1ms dip drained late must NOT reset (%u -> %u)", r1, r2);
    CHECK(!(inb(5) & 1), "and no spurious 'M'");
}

// --- R5. IER rewrites are edge-triggered ------------------------------------
static void scn_ier_rewrite(void) {
    printf("-- R5: IER rewrite with bit1 already set does not re-fire THRE\n");
    boot();
    run_us(5000); outb(4, 0x0B); run_us(400);
    while (inb(5) & 1) (void)inb(0);
    run_us(200);
    outb(1, 0x02); run_us(200);
    CHECK(g_line, "first enable fires");
    (void)inb(2); run_us(200);
    CHECK(!g_line, "serviced");
    for (int i = 0; i < 5; i++) { outb(1, 0x02); run_us(200); }
    CHECK(!g_line, "rewrites with bit1 already set: no re-fire");
    outb(1, 0x00); run_us(100); outb(1, 0x02); run_us(200);
    CHECK(g_line, "a real 0->1 edge fires again");
    (void)inb(2); run_us(200);
}

// --- R6. click shorter than one report interval survives -------------------
static void scn_fast_click(void) {
    printf("-- R6: press+release inside one report interval = 2 packets\n");
    boot();
    outb(1, 0x01); run_us(5000); outb(4, 0x0B); run_us(200); isr_drain();
    run_us(320000);
    feed(0, 0, MOUSE_BUTTON_LEFT);                   // press...
    run_us(1000);
    feed(0, 0, 0);                                   // ...release, 1 ms later
    std::vector<uint8_t> wire; pump(wire, 60000);
    auto pkts = decode_ms(wire);
    bool saw_press = false, saw_release_after = false;
    for (auto &p : pkts) { if (p.lb) saw_press = true; else if (saw_press) saw_release_after = true; }
    CHECK(saw_press, "press latched into >=1 packet");
    CHECK(saw_release_after, "release follows in a later packet");
}

// --- R7. modem-status service during a Core-0 stall lowers the line --------
static void scn_msr_stall_lower(void) {
    printf("-- R7: MSR read lowers a presented modem int without Core 0\n");
    boot();
    outb(3, 0x02); outb(1, 0x08);                    // modem-status int ONLY
    run_us(5000); outb(4, 0x0B);                     // reset -> CTS/DSR/DCD deltas
    run_us(400);
    CHECK(g_line, "modem-status int raised");
    CHECK(inb(2) == 0x00, "IIR presents modem status");
    // Core 0 stalls; guest ISR reads MSR:
    uint8_t msr = inb(6);
    CHECK(msr & 0x0F, "deltas visible (%02X)", msr);
    CHECK(!g_line, "line dropped by the MSR read itself (C1 lower)");
    run_us(2000);
    CHECK(!g_line, "and stays low once Core 0 clears the deltas");
}

int main(void) {
    printf("=== sermouse host simulation (stages 4-7, real firmware sources) ===\n");
    scn_sertest_reset();
    scn_movement();
    scn_core0_stall();
    scn_ctmouse();
    scn_windows();
    scn_power_rules();
    scn_registers();
    printf("--- review-regression scenarios ---\n");
    scn_rxq_lap();
    scn_tx_chain();
    scn_dip_symmetry();
    scn_dip_timing_stall();
    scn_ier_rewrite();
    scn_fast_click();
    scn_msr_stall_lower();
    printf("=== %d passed, %d FAILED ===\n", g_pass, g_fail);
    return g_fail;
}
