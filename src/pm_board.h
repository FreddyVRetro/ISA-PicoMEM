// PicoMEM and Pico Boards specific definitions
#pragma once

// Picomem 1.0, 1.1, 1.11, 1.12, 1.14
#ifdef BOARD_PM15
#include "pm_boards\pm15.h"
#endif

// Picomem 1.3 (New multiplexing)
#ifdef BOARD_PM
#include "pm_boards\pm.h"
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

