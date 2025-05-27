/*
 * Part of ethersrv-linux
 */

/* set to 1 to enable debug */
#define DEBUG 0

/* set to 1 for frame loss simulation (for tests only!) */
#define SIMLOSS 0

/* do not modify the magic below */

#if DEBUG > 0

#define DBG printf

#else

#define DBG(...)

#endif
