/* Read-only diagnosis of the application's native frame; no input automation. */
#include "desktop.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static BOOL CALLBACK inspect(HWND window,LPARAM process)
{
    DWORD id;
    DesktopInfo d;
    GetWindowThreadProcessId(window,&id);
    if (id!=(DWORD)process || !IsWindowVisible(window) || !desktop_window_info(window,&d)) return TRUE;
    printf("work=%d,%d %dx%d outer=%d,%d %dx%d client=%dx%d dpi=%d caption=%d minimize=%d close=%d minimized=%d\n",
        d.work_x,d.work_y,d.work_width,d.work_height,d.x,d.y,d.width,d.height,
        d.client_width,d.client_height,d.dpi,d.decorated,d.minimize_box,d.close_enabled,d.minimized);
    return TRUE;
}
int main(int argc,char **argv)
{
    if (argc!=2) return 2;
    SetProcessDPIAware(); /* Only this diagnostic process; never changes display settings. */
    EnumWindows(inspect,(LPARAM)strtoul(argv[1],NULL,10));
    return 0;
}
#else
int main(void) { return 2; }
#endif
