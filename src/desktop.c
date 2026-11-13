#include "desktop.h"
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int desktop_window_info(void *handle, DesktopInfo *info)
{
    HWND window=(HWND)handle;
    MONITORINFO monitor;
    RECT outer,client;
    LONG_PTR style;
    typedef UINT (WINAPI *DpiFunction)(HWND);
    DpiFunction get_dpi;
    FARPROC address;
    if (!window || !info) return 0;
    memset(info,0,sizeof *info);
    memset(&monitor,0,sizeof monitor); monitor.cbSize=sizeof monitor;
    if (!GetMonitorInfoA(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor) ||
        !GetWindowRect(window,&outer) || !GetClientRect(window,&client)) return 0;
    info->work_x=monitor.rcWork.left; info->work_y=monitor.rcWork.top;
    info->work_width=monitor.rcWork.right-monitor.rcWork.left;
    info->work_height=monitor.rcWork.bottom-monitor.rcWork.top;
    info->x=outer.left; info->y=outer.top;
    info->width=outer.right-outer.left; info->height=outer.bottom-outer.top;
    info->client_width=client.right; info->client_height=client.bottom;
    style=GetWindowLongPtrA(window,GWL_STYLE);
    info->decorated=(style & WS_CAPTION)==WS_CAPTION;
    info->minimize_box=(style & WS_MINIMIZEBOX)!=0;
    info->close_enabled=(style & WS_SYSMENU)!=0 &&
        (GetMenuState(GetSystemMenu(window,FALSE),SC_CLOSE,MF_BYCOMMAND)&(MF_DISABLED|MF_GRAYED))==0;
    info->minimized=IsIconic(window)!=0;
    info->dpi=96;
    address=GetProcAddress(GetModuleHandleA("user32.dll"),"GetDpiForWindow");
    /* memcpy avoids an ISO-C object/function-pointer cast warning on MinGW. */
    get_dpi=NULL;
    if (address && sizeof get_dpi==sizeof address) memcpy(&get_dpi,&address,sizeof get_dpi);
    if (get_dpi) info->dpi=(int)get_dpi(window);
    return 1;
}
int desktop_fit_window(void *handle,int preferred_width,int preferred_height)
{
    DesktopInfo d;
    int frame_w,frame_h,available_w,available_h,w,h,margin;
    double scale=1;
    if (!desktop_window_info(handle,&d)) return 0;
    frame_w=d.width-d.client_width; frame_h=d.height-d.client_height;
    margin=12*d.dpi/96;
    available_w=d.work_width-frame_w-2*margin;
    available_h=d.work_height-frame_h-2*margin;
    if (available_w<1 || available_h<1 || preferred_width<1 || preferred_height<1) return 0;
    if (preferred_width>available_w) scale=(double)available_w/preferred_width;
    if (preferred_height*scale>available_h) scale=(double)available_h/preferred_height;
    w=(int)(preferred_width*scale); h=(int)(preferred_height*scale);
    if (w<1) w=1;
    if (h<1) h=1;
    return SetWindowPos((HWND)handle,NULL,d.work_x+(d.work_width-w-frame_w)/2,
        d.work_y+(d.work_height-h-frame_h)/2,w+frame_w,h+frame_h,
        SWP_NOZORDER|SWP_NOACTIVATE)!=0;
}
#else
int desktop_window_info(void *handle,DesktopInfo *info) { (void)handle; (void)info; return 0; }
int desktop_fit_window(void *handle,int width,int height) { (void)handle; (void)width; (void)height; return 0; }
#endif
