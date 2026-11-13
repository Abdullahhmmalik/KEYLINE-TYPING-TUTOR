#include "native_test_input.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static int owned(HWND window)
{
    DWORD id;
    GetWindowThreadProcessId(window,&id);
    return id==GetCurrentProcessId();
}
int native_test_text(void *handle,const char *text)
{
    HWND window=(HWND)handle;
    if (!owned(window)) return 0;
    while (*text) if (!PostMessageA(window,WM_CHAR,(unsigned char)*text++,1)) return 0;
    return 1;
}
int native_test_close(void *handle)
{
    HWND window=(HWND)handle;
    return owned(window) && PostMessageA(window,WM_CLOSE,0,0)!=0;
}
#else
int native_test_text(void *window,const char *text) { (void)window; (void)text; return 0; }
int native_test_close(void *window) { (void)window; return 0; }
#endif
