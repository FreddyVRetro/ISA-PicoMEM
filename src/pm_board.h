// PicoMEM and Pico Boards specific definitions

// Picomem 1.0, 1.1, 1.11, 1.12, 1.14
#ifdef BOARD_PM
#include "pm_boards\pm.h"
#endif

// Picomem Low Profile (No difference for the moment)
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