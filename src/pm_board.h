// PicoMEM and Pico Boards specific definitions
#pragma once

// Picomem 1.5
#ifdef BOARD_PM15
#include "pm_boards\pm15.h"
#endif

// Picomem 2.0
#ifdef BOARD_PM20   
#include "pm_boards\pm20.h"
#endif

// Picomem 1.0, 1.1, 1.11, 1.12, 1.14
#ifdef BOARD_PM
#include "pm_boards\pm.h"
#endif

// Picomem 1.3 (New multiplexing)
#ifdef BOARD_PM13
#include "pm_boards\pm13.h"
#endif

// Picomem Low Profile and 1.2A
#ifdef BOARD_PM_LP
#include "pm_boards\pm_lp.h"
#endif

#ifdef BOARD_PM_NG
#include "pm_boards\pm_ng.h"
#endif

// Picomem Prototype
#ifdef BOARD_PMPROTO
#include "pm_boards\pm_proto.h"
#endif

// WonderMCA 1.0
#ifdef BOARD_WM10
#include "wm_board.h"
#endif

// PIN_PM_IRQ — legacy multiplexed PicoMEM-IRQ pin used by isa_irq.h's
// inline pm_irq_up/down/down_nolock/isa_pm_irq_ack helpers. PicoMEM 1.5/2.0
// have a real GPIO for this; WonderMCA WM10 instead routes IRQs through
// dedicated MCA-bus pins (PIN_IRQ3/5/6/7 via mca_irq_raise/lower).
// On WM10 the legacy helpers aren't called for normal IRQ delivery, but
// the inline functions still get parsed wherever isa_irq.h is included,
// so the symbol must be defined or compilation fails. Stub to GPIO 0
// (unused on WM10 — UART TX is on a different pin).
#if BOARD_WM10
#define PIN_PM_IRQ 41
#elif USE_HWIRQ
//#define PIN_PM_IRQ 41
#else
#define PIN_PM_IRQ 0
#endif
