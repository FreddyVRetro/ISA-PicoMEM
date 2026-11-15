#ifndef PM_SCREENPAD_HOST_H
#define PM_SCREENPAD_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

// Register the WonderMCA screenpad menu's host callbacks. Call once
// after pm_disk / PM_Config / FatFs are up. No-op-safe to call before
// screenpad_init() -- the menu just uses the vtable on first event.
void pm_screenpad_host_init(void);

#ifdef __cplusplus
}
#endif

#endif // PM_SCREENPAD_HOST_H
