/* Stub replacement for animprev.cpp for compilers that don't
 * support DirectX
 */

#define STRICT
#include <windows.h>

void APDisplayMode (char * c) { return; }
void APSetup (HWND h, int i) { return; }
void APCleanup (void) { return; }
int APStart (void) { return 0; }
HWND CreateAnimPreviewWindow (HWND h) { return NULL; }
