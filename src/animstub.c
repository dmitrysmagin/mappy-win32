/* Stub replacement for animprev.cpp for compilers that don't
 * support DirectX
 */

#define STRICT
#include <windows.h>

void APDisplayMode(char * c) { (void)c; }
void APSetup(HWND h, int i) { (void)h; (void)i; }
void APCleanup(void) { return; }
int APStart(void) { return 0; }
HWND CreateAnimPreviewWindow(HWND h) { (void)h; return NULL; }
