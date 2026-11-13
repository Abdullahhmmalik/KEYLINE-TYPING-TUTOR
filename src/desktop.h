#ifndef DESKTOP_H
#define DESKTOP_H
/* Native window coordinates only; raylib types stay out of this Win32 boundary. */
typedef struct {
    int work_x, work_y, work_width, work_height;
    int x, y, width, height, client_width, client_height;
    int dpi, decorated, minimize_box, close_enabled, minimized;
} DesktopInfo;
int desktop_window_info(void *handle, DesktopInfo *info);
int desktop_fit_window(void *handle, int preferred_width, int preferred_height);
#endif
