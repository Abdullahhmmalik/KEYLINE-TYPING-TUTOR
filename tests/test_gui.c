/* Runs the real screen code in a hidden raylib/OpenGL window. Mouse and key
 * events use raylib 5.5 automation event IDs (rcore.c); printable text uses the
 * separately tested typing engine because raylib does not replay text events. */
#include "app.h"
#include "ui.h"
#include "desktop.h"
#include "native_test_input.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

enum { EVENT_KEY_UP=1, EVENT_KEY_DOWN=2, EVENT_MOUSE_UP=5, EVENT_MOUSE_DOWN=6, EVENT_MOUSE_POSITION=7 };
static App app;
static Database reloaded;
static int checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; fprintf(stderr,"GUI FAIL line %d: %s\n",__LINE__,#e); } } while (0)
static void event(unsigned int type,int a,int b)
{ AutomationEvent e={0}; e.type=type; e.params[0]=a; e.params[1]=b; PlayAutomationEvent(e); }
static void click(float x,float y)
{
    float scale=fminf(GetScreenWidth()/(float)WINDOW_WIDTH,GetScreenHeight()/(float)WINDOW_HEIGHT);
    float ox=(GetScreenWidth()-WINDOW_WIDTH*scale)/2, oy=(GetScreenHeight()-WINDOW_HEIGHT*scale)/2;
    event(EVENT_MOUSE_POSITION,(int)(x*scale+ox),(int)(y*scale+oy));
    event(EVENT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,0); app_frame(&app);
    event(EVENT_MOUSE_UP,MOUSE_BUTTON_LEFT,0); app_frame(&app);
}
static void key(int keycode)
{
    event(EVENT_KEY_DOWN,keycode,0); app_frame(&app);
    event(EVENT_KEY_UP,keycode,0); app_frame(&app);
}
static void text_input(const char *text)
{
    /* Small chunks stay below raylib's finite per-frame character queue. The
     * messages go through Windows -> GLFW CharCallback -> GetCharPressed. */
    while (*text) {
        char chunk[7];
        int n=0;
        while (*text && n<6) chunk[n++]=*text++;
        chunk[n]=0;
        CHECK(native_test_text(GetWindowHandle(),chunk));
        app_frame(&app); app_frame(&app);
    }
}
static void replace_input(const char *current,const char *text)
{
    while (*current) key(KEY_BACKSPACE);
    text_input(text);
}
static int accent_pixel(Image image,int x,int y)
{
    Color c=GetImageColor(image,x,y);
    return abs(c.r-THEME.accent.r)<3 && abs(c.g-THEME.accent.g)<3 && abs(c.b-THEME.accent.b)<3;
}
/* Independent oracle: locate the actual green Create button in the final
 * window framebuffer, rather than repeat production's layout formula. */
static Vector2 rendered_create_button(int edge)
{
    Image image;
    int x,y,min_x=100000,min_y=100000,max_x=-1,max_y=-1;
    Vector2 point={-100,-100};
    event(EVENT_MOUSE_POSITION,0,0); app_frame(&app); app_frame(&app);
    image=LoadImageFromScreen();
    CHECK(image.data!=NULL);
    if (!image.data) return point;
    for (y=image.height/2;y<image.height-6;y+=2) for (x=image.width/2;x<image.width-20;x+=2) {
        /* Require a wide solid patch: thin focused-field borders/carets also
         * use mint, but they are not the rendered Create button. */
        if (accent_pixel(image,x,y) && accent_pixel(image,x+20,y) &&
            accent_pixel(image,x,y+6) && accent_pixel(image,x+20,y+6)) {
            if (x<min_x) min_x=x;
            if (x>max_x) max_x=x;
            if (y<min_y) min_y=y;
            if (y>max_y) max_y=y;
        }
    }
    CHECK(max_x>min_x && max_y>min_y);
    if (max_x>min_x) {
        float px=edge<0?min_x-4:edge>0?min_x+4:(min_x+max_x)/2.0f;
        point=(Vector2){px*GetScreenWidth()/image.width,((min_y+max_y)/2.0f)*GetScreenHeight()/image.height};
    }
    UnloadImage(image);
    return point;
}
static void physical_click(Vector2 point)
{
    event(EVENT_MOUSE_POSITION,(int)point.x,(int)point.y);
    event(EVENT_MOUSE_DOWN,MOUSE_BUTTON_LEFT,0); app_frame(&app);
    event(EVENT_MOUSE_UP,MOUSE_BUTTON_LEFT,0); app_frame(&app);
}
static void viewport_checks(void)
{
    /* Hand-calculated expected positions, independent from the implementation. */
    UiViewport v=ui_viewport_for_size(1000,700);
    Vector2 p=ui_to_logical(v,(Vector2){203.125f,152.34375f});
    CHECK(fabsf(v.destination.y-37.5f)<0.01f);
    CHECK(fabsf(p.x-260)<0.01f && fabsf(p.y-147)<0.01f);
    v=ui_viewport_for_size(1280,720); p=ui_to_logical(v,(Vector2){640,360});
    CHECK(fabsf(v.destination.x-64)<0.01f);
    CHECK(fabsf(p.x-640)<0.01f && fabsf(p.y-400)<0.01f);
    v=ui_viewport_for_size(1920,1080); p=ui_to_logical(v,(Vector2){960,540});
    CHECK(fabsf(v.destination.x-96)<0.01f);
    CHECK(fabsf(p.x-640)<0.01f && fabsf(p.y-400)<0.01f);
    v=ui_viewport_for_size(1366,768);
    CHECK(fabsf(v.destination.x-68.6f)<0.01f && fabsf(v.scale-0.96f)<0.001f);
    CHECK(!ui_viewport_for_size(0,0).valid);
    p=ui_to_logical(ui_viewport_for_size(0,0),(Vector2){1,1}); CHECK(p.x==-1 && p.y==-1);
}
static void picture(const char *folder,const char *name)
{
    char filename[1024];
    app.toast_until=0; app_frame(&app);
    snprintf(filename,sizeof filename,"%s/%s.png",folder,name);
    CHECK(ui_save_screenshot(filename));
}
int main(int argc,char **argv)
{
    int first,i,second;
    if (argc!=3) { fprintf(stderr,"Usage: keyline_gui_tests DATA_DIR IMAGE_DIR\n"); return 2; }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(MIN_WINDOW_WIDTH,MIN_WINDOW_HEIGHT,"Keyline GUI verification");
    if (!IsWindowReady()) return 2;
    desktop_fit_window(GetWindowHandle(),WINDOW_WIDTH,WINDOW_HEIGHT);
    SetExitKey(KEY_NULL); SetTargetFPS(120); ui_init(); app_init(&app,argv[1]);
    viewport_checks();
    CHECK(app.screen==SCREEN_PROFILES && app.db.profile_count==0);
    picture(argv[2],"01-welcome");
    click(800,615); text_input("QaStudent"); click(1110,615);
    CHECK(app.screen==SCREEN_DASHBOARD); CHECK(app.db.profile_count==1);
    picture(argv[2],"02-overview-empty");
    click(100,486); CHECK(app.screen==SCREEN_SETTINGS);
    click(620,330); replace_input(app.duration_text,"0"); click(350,650); CHECK(app.db.settings.duration==DEFAULT_DURATION);
    click(620,330); replace_input(app.duration_text,"5"); click(350,650); CHECK(app.db.settings.duration==5);
    click(100,205); CHECK(app.screen==SCREEN_DASHBOARD);
    click(1100,390); CHECK(app.screen==SCREEN_TEST); first=app.exercise;
    CHECK(!app.test.started && app.test.duration==5);
    picture(argv[2],"03-typing-ready");
    typing_character(&app.test,app.test.passage[0],GetTime());
    typing_character(&app.test,'~',GetTime());
    app_frame(&app); CHECK(app.test.position==2); CHECK(typing_metrics(&app.test).mistakes==1);
    picture(argv[2],"04a-typing-error");
    key(KEY_BACKSPACE); CHECK(app.test.position==1); CHECK(app.test.total_keys==2);
    typing_character(&app.test,app.test.passage[1],GetTime());
    picture(argv[2],"04-typing-corrected");
    click(340,705); CHECK(app.modal); click(850,470);
    CHECK(app.screen==SCREEN_TEST && !app.test.started && app.test.position==0); CHECK(app.exercise==first);
    click(520,705); CHECK(app.screen==SCREEN_TEST && app.exercise!=first);
    /* Force the authoritative clock to its real expiration boundary without waiting. */
    for (i=0;i<48;++i) typing_character(&app.test,app.test.passage[app.test.position],GetTime());
    app.db.ready=0; /* Controlled storage-unavailable fixture; no OS permissions changed. */
    app.test.start_time=GetTime()-app.test.duration; app_frame(&app);
    CHECK(app.screen==SCREEN_RESULTS && !app.result_saved && app.db.record_count==0);
    picture(argv[2],"05a-unsaved-results");
    click(740,710); CHECK(app.modal && app.screen==SCREEN_RESULTS);
    click(500,470); CHECK(!app.modal && app.screen==SCREEN_RESULTS);
    app.db.ready=1;
    click(1120,708);
    CHECK(app.screen==SCREEN_RESULTS && app.result_saved); CHECK(app.db.record_count==1);
    CHECK(app.result.duration==5 && app.result.elapsed==5 && !app.result.completed);
    picture(argv[2],"05-results");
    click(740,710); CHECK(app.screen==SCREEN_HISTORY);
    picture(argv[2],"06-history");
    click(100,373); CHECK(app.screen==SCREEN_LEADERBOARD);
    click(750,171); CHECK(app.leader_duration==5);
    picture(argv[2],"07-leaderboard");
    click(100,430); CHECK(app.screen==SCREEN_ACHIEVEMENTS);
    CHECK(app.db.profiles[0].achievements!=0);
    picture(argv[2],"08-achievements");
    click(100,485); CHECK(app.screen==SCREEN_SETTINGS);
    picture(argv[2],"09-settings");
    click(100,758); CHECK(app.screen==SCREEN_PROFILES);
    click(800,615); text_input("SecondStudent"); click(1110,615);
    CHECK(app.screen==SCREEN_DASHBOARD && app.selected_profile==1);
    CHECK(data_summary(&app.db,app.db.profiles[1].id).count==0);
    second=app.selected_profile;
    click(100,758); click(900,227); CHECK(app.selected_profile==0 && app.selected_profile!=second);
    CHECK(app.screen==SCREEN_DASHBOARD);
    click(1100,390); CHECK(app.screen==SCREEN_TEST);
    /* Early passage completion must save once, rather than wait for the timer. */
    for (i=0;i<app.test.length;++i) typing_character(&app.test,app.test.passage[i],GetTime());
    app_frame(&app); CHECK(app.screen==SCREEN_RESULTS && app.result.completed && app.result_saved);
    CHECK(app.db.record_count==2); app_frame(&app); CHECK(app.db.record_count==2);
    CHECK(data_open(&reloaded,argv[1])); CHECK(reloaded.profile_count==2 && reloaded.record_count==2);
    click(100,205); CHECK(app.screen==SCREEN_DASHBOARD);
    picture(argv[2],"10-overview-progress");
    SetWindowSize(MIN_WINDOW_WIDTH,MIN_WINDOW_HEIGHT); app_frame(&app); app_frame(&app);
    click(100,485); CHECK(app.screen==SCREEN_SETTINGS);
    click(100,205); CHECK(app.screen==SCREEN_DASHBOARD);
    /* An unsaved result must remain reviewable until the user explicitly leaves. */
    app.screen=SCREEN_RESULTS; app.result_saved=0; app_frame(&app);
    click(100,205); CHECK(app.modal && app.screen==SCREEN_RESULTS);
    click(500,470); CHECK(!app.modal && app.screen==SCREEN_RESULTS);
    click(1120,708); CHECK(app.result_saved); CHECK(app.db.record_count==2);
    app.screen=SCREEN_DASHBOARD; app_frame(&app);
    click(1100,390); CHECK(app.screen==SCREEN_TEST);
    {
        double started=GetTime();
        typing_character(&app.test,app.test.passage[0],started);
        while (app.screen==SCREEN_TEST && GetTime()-started<8) app_frame(&app);
        CHECK(app.screen==SCREEN_RESULTS && app.result_saved);
        CHECK(app.result.elapsed==5 && !app.result.completed);
        CHECK(GetTime()-started>=5 && GetTime()-started<8);
    }
    CHECK(app.db.record_count==3);
    /* Profile paging, history paging and leaderboard paging use bounded arrays. */
    app.screen=SCREEN_HISTORY; app.history_page=999; app_frame(&app);
    CHECK(app.history_page==0);
    app.screen=SCREEN_LEADERBOARD; app.leader_page=999; app_frame(&app);
    CHECK(app.leader_page==0);
    {
        DesktopInfo d;
        Vector2 button;
        int before;
        CHECK(desktop_window_info(GetWindowHandle(),&d));
        CHECK(d.decorated && d.minimize_box && d.close_enabled);
        CHECK(d.x>=d.work_x && d.y>=d.work_y && d.x+d.width<=d.work_x+d.work_width && d.y+d.height<=d.work_y+d.work_height);
        printf("Native baseline: work=%dx%d client=%dx%d raylib=%dx%d dpi=%d frame=%d,%d %dx%d\n",
            d.work_width,d.work_height,d.client_width,d.client_height,GetScreenWidth(),GetScreenHeight(),d.dpi,d.x,d.y,d.width,d.height);
        CHECK(d.client_width==GetScreenWidth() && d.client_height==GetScreenHeight());
        app.screen=SCREEN_PROFILES; app_frame(&app);
        click(800,615); CHECK(app.username_focus);
        text_input("Blur");
        button=rendered_create_button(0);
        CHECK(native_test_text(GetWindowHandle(),"X")); app_frame(&app);
        before=app.db.profile_count;
        physical_click(button); /* Outside click + last queued character in ONE frame. */
        CHECK(app.screen==SCREEN_DASHBOARD && app.db.profile_count==before+1);
        CHECK(data_find_profile(&app.db,"BlurX")>=0 && data_find_profile(&app.db,"Blur")==-1);
        for (i=0;i<3;++i) {
            char name[32];
            click(100,758); CHECK(app.screen==SCREEN_PROFILES);
            click(800,615); CHECK(app.username_focus);
            snprintf(name,sizeof name,"Focus%dz",i); text_input(name);
            key(KEY_BACKSPACE); snprintf(name,sizeof name,"Focus%d",i);
            CHECK(strcmp(app.username,name)==0);
            click(750,550); CHECK(!app.username_focus);
            text_input("NO"); CHECK(strcmp(app.username,name)==0);
            click(800,615); CHECK(app.username_focus);
            key(KEY_ENTER); CHECK(app.screen==SCREEN_DASHBOARD && data_find_profile(&app.db,name)>=0);
        }
        click(100,758); click(800,615);
        text_input("bad name"); before=app.db.profile_count;
        click(1110,615); CHECK(app.screen==SCREEN_PROFILES && app.db.profile_count==before);
        CHECK(strstr(app.db.message,"letters")!=NULL);
        click(800,615); replace_input(app.username,"qastudent"); key(KEY_ENTER);
        CHECK(app.screen==SCREEN_PROFILES && app.db.profile_count==before);
        CHECK(strstr(app.db.message,"already")!=NULL);
        click(800,615); replace_input(app.username,"");
        click(890,515); CHECK(app.profile_page==1);
        click(680,515); CHECK(app.profile_page==0);
        /* Raster-derived button coordinates remain independent of production's
         * inverse-mapping formula. These are CLIENT sizes, not DPI changes. */
        {
            const int sizes[][2]={{800,500},{1280,720},{1366,768},{1920,1080},{1280,900}};
            for (i=0;i<5;++i) {
                char name[32];
                SetWindowSize(sizes[i][0],sizes[i][1]); app_frame(&app); app_frame(&app);
                desktop_window_info(GetWindowHandle(),&d);
                printf("Client resize: requested=%dx%d actual=%dx%d raylib=%dx%d render=%dx%d\n",
                    sizes[i][0],sizes[i][1],d.client_width,d.client_height,GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight());
                CHECK(d.client_width==GetScreenWidth() && d.client_height==GetScreenHeight());
                key(KEY_TAB); CHECK(app.username_focus);
                snprintf(name,sizeof name,"Pixel%d",i); text_input(name);
                before=app.db.profile_count;
                physical_click(rendered_create_button(-1)); CHECK(app.db.profile_count==before);
                physical_click(rendered_create_button(1));
                CHECK(app.screen==SCREEN_DASHBOARD && data_find_profile(&app.db,name)>=0);
                click(100,758); CHECK(app.screen==SCREEN_PROFILES);
            }
        }
        desktop_fit_window(GetWindowHandle(),WINDOW_WIDTH,WINDOW_HEIGHT); app_frame(&app);
        MinimizeWindow(); app_frame(&app);
        CHECK(IsWindowMinimized());
        RestoreWindow(); app_frame(&app); app_frame(&app);
        CHECK(!IsWindowMinimized());
        click(900,227); CHECK(app.screen==SCREEN_DASHBOARD);
        click(1100,390); typing_character(&app.test,app.test.passage[0],GetTime());
        CHECK(native_test_close(GetWindowHandle())); app_frame(&app);
        CHECK(WindowShouldClose()); app_request_exit(&app);
        CHECK(app.modal && app.exit_pending && !app.exit_requested);
        key(KEY_ESCAPE); CHECK(!app.modal && !app.exit_pending && !app.exit_requested);
        app_request_exit(&app); key(KEY_ENTER); CHECK(app.exit_requested);
        app.exit_requested=0; app.exit_pending=0; app.modal=0;
        app.screen=SCREEN_RESULTS; app.result_saved=0;
        app_request_exit(&app); CHECK(app.modal && !app.exit_requested);
        key(KEY_ESCAPE); app.result_saved=1;
        click(1190,24); CHECK(app.exit_requested); /* In-app secondary exit. */
        CHECK(data_open(&reloaded,argv[1])); CHECK(reloaded.record_count==3);
    }
    ui_shutdown(); CloseWindow();
    printf("GUI checks: %d passed, %d failed (%d total).\n",checks-failures,failures,checks);
    return failures?1:0;
}
