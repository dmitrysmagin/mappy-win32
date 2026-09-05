/* compat.h - Win64 / modern MinGW compatibility shims for Mappy 1.3.11 sources.
 *
 * - MakeProcInstance/FreeProcInstance were no-ops since Win32 and are removed
 *   in Win64 headers. Map them to identity.
 * - Provides uintptr_t-safe alignment helpers.
 */
#ifndef MAPPY_COMPAT_H
#define MAPPY_COMPAT_H

#include <stdint.h>

#ifndef MakeProcInstance
#define MakeProcInstance(p, i) (p)
#endif
#ifndef FreeProcInstance
#define FreeProcInstance(p) ((void)0)
#endif

/* MinGW typedefs DLGPROC as FARPROC (INT_PTR (*)(void)). Cast through void *
 * so a real INT_PTR CALLBACK proc can be passed to DialogBox without
 * -Wincompatible-pointer-types / -Wcast-function-type. */
#define AsDlgProc(fn) ((DLGPROC)(void *)(fn))

#define UNUSED(x) ((void)(x))

/* Pointer alignment check without truncating to 32-bit int on Win64 */
static inline int mappy_ptr_is_odd(const void *p) {
    return (((uintptr_t)p) & 1) ? 1 : 0;
}

#endif /* MAPPY_COMPAT_H */
