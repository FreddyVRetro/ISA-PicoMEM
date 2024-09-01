#pragma once

#if PM_PRINTF
#define pmprintf printf
#else 
static void fakeprintf(const char *format, ...) {}  // Static means not public
#define pmprintf fakeprintf
#endif

//#if PM_PRINTF
#define PM_ERROR   pmprintf //"PM ERROR"
#define PM_WARNING pmprintf //"PM WARNING"
#define PM_INFO    pmprintf //"PM INFO"
#define PM_INFO2   pmprintf //"PM INFO2"
//#endif