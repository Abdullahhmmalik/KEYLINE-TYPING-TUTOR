#include "app.h"
#include "ui.h"
#include "desktop.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Large bounded history lives in static storage, not the small Windows stack. */
static App app;
static void fit_desktop(int first,DesktopInfo *previous)
{
    DesktopInfo d;
    if (!desktop_window_info(GetWindowHandle(),&d)) return;
    if (first || d.work_x!=previous->work_x || d.work_y!=previous->work_y ||
        d.work_width!=previous->work_width || d.work_height!=previous->work_height || d.dpi!=previous->dpi) {
        int margin=12*d.dpi/96;
        int max_w=d.work_width-(d.width-d.client_width)-2*margin;
        int max_h=d.work_height-(d.height-d.client_height)-2*margin;
        if (max_w<1) max_w=1;
        if (max_h<1) max_h=1;
        SetWindowMinSize(max_w<MIN_WINDOW_WIDTH?max_w:MIN_WINDOW_WIDTH,
            max_h<MIN_WINDOW_HEIGHT?max_h:MIN_WINDOW_HEIGHT);
        if (!IsWindowMaximized() && !d.minimized)
            desktop_fit_window(GetWindowHandle(),first?WINDOW_WIDTH:d.client_width,first?WINDOW_HEIGHT:d.client_height);
        desktop_window_info(GetWindowHandle(),&d);
    }
    *previous=d;
}
int main(int argc, char **argv)
{
    DesktopInfo desktop={0};
    char directory[MAX_PATH_LEN];
    const char *base=getenv("LOCALAPPDATA");
    int i;
    if (base && strlen(base)<MAX_PATH_LEN-40) snprintf(directory,sizeof directory,"%s/KeylineTypingTutor",base);
    else snprintf(directory,sizeof directory,"./keyline-data");
    for (i=1;i<argc;++i) {
        if (strcmp(argv[i],"--data-dir")==0 && i+1<argc) {
            if (strlen(argv[++i])>MAX_PATH_LEN-40) { fprintf(stderr,"Data path is too long.\n"); return 1; }
            snprintf(directory,sizeof directory,"%s",argv[i]);
        } else { fprintf(stderr,"Usage: keyline [--data-dir PATH]\n"); return 1; }
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    /* Start small so Windows cannot clamp the native client before GLFW's
     * resize callbacks are installed, leaving raylib with stale dimensions. */
    InitWindow(MIN_WINDOW_WIDTH,MIN_WINDOW_HEIGHT,TextFormat("Keyline %s [%s] - Profiles",APP_VERSION,APP_BUILD_TAG));
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(MIN_WINDOW_WIDTH,MIN_WINDOW_HEIGHT);
    fit_desktop(1,&desktop);
    /* raylib 5.5 otherwise waits inside its minimized event loop, preventing a
     * close request from reaching our confirmation until the window restores. */
    SetWindowState(FLAG_WINDOW_ALWAYS_RUN);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    ui_init();
    app_init(&app,directory);
    while (!app.exit_requested) {
        if (WindowShouldClose()) {
            if (IsWindowMinimized()) RestoreWindow();
            app_request_exit(&app);
        }
        fit_desktop(0,&desktop);
        if (!app.exit_requested) app_frame(&app);
    }
    ui_shutdown();
    CloseWindow();
    return 0;
}
