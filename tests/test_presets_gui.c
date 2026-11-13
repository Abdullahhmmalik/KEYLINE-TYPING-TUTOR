/* Focused configuration-variant checks. Coordinates are explicit expected
 * screen positions, independent of the production timer_presets helper. */
#include "app.h"
#include "ui.h"
#include "desktop.h"
#include <math.h>
#include <stdio.h>

static App app;
static int checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; fprintf(stderr,"PRESET FAIL %d: %s\n",__LINE__,#e); } } while (0)

static void event(unsigned int type, int a, int b)
{
    AutomationEvent e={0};
    e.type=type; e.params[0]=a; e.params[1]=b;
    PlayAutomationEvent(e);
}
static void click(float x, float y)
{
    float scale=fminf(GetScreenWidth()/1280.0f,GetScreenHeight()/800.0f);
    float ox=(GetScreenWidth()-1280*scale)/2, oy=(GetScreenHeight()-800*scale)/2;
    event(7,(int)(x*scale+ox),(int)(y*scale+oy));
    event(6,MOUSE_BUTTON_LEFT,0); app_frame(&app);
    event(5,MOUSE_BUTTON_LEFT,0); app_frame(&app);
}
static void picture(const char *folder, const char *name)
{
    char path[1024];
    snprintf(path,sizeof path,"%s/%s.png",folder,name);
    app.toast_until=0; app_frame(&app);
    CHECK(ui_save_screenshot(path));
}
static void check_picker(Screen screen)
{
    int i, saved_duration=app.db.settings.duration;
    app.screen=screen; app.timer_preset_page=0; app_frame(&app);
    for (i=0;i<DURATION_PRESET_COUNT;++i) {
        int slot=i%8, before_count=app.db.record_count;
        float x,y;
        if (i>0 && slot==0) {
            float pager_y=screen==SCREEN_DASHBOARD?448:screen==SCREEN_SETTINGS?270:166;
            click(998,pager_y);
            CHECK(app.timer_preset_page==i/8);
        }
        if (screen==SCREEN_DASHBOARD) { x=323+(slot%4)*89; y=430+(slot/4)*40; }
        else if (screen==SCREEN_SETTINGS) { x=331+(slot%4)*108; y=261+(slot/4)*36; }
        else { x=302+(slot%4)*96; y=165+(slot/4)*36; }
        click(x,y);
        CHECK(app.screen==screen && !app.modal && !app.exit_requested);
        CHECK(app.db.record_count==before_count);
        if (screen==SCREEN_DASHBOARD) CHECK(app.db.settings.duration==DURATION_PRESETS[i]);
        else if (screen==SCREEN_SETTINGS) {
            CHECK(app.settings_duration==DURATION_PRESETS[i]);
            CHECK(app.db.settings.duration==saved_duration); /* Draft until Save. */
        } else {
            CHECK(app.leader_duration==DURATION_PRESETS[i]);
            CHECK(app.db.settings.duration==saved_duration);
        }
    }
    if (DURATION_PRESET_COUNT>8) {
        float y=screen==SCREEN_DASHBOARD?448:screen==SCREEN_SETTINGS?270:166;
        click(908,y); CHECK(app.timer_preset_page==(DURATION_PRESET_COUNT-1)/8-1);
    }
}
int main(int argc, char **argv)
{
    int size;
    if (argc!=3 || DURATION_PRESET_COUNT<5) return 2;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
    InitWindow(800,500,"Keyline expanded preset verification");
    if (!IsWindowReady()) return 2;
    desktop_fit_window(GetWindowHandle(),1280,800);
    SetExitKey(KEY_NULL); SetTargetFPS(120); ui_init(); app_init(&app,argv[1]);
    CHECK(app.db.ready);
    app.selected_profile=data_create_profile(&app.db,"PresetStudent"); CHECK(app.selected_profile>=0);
    for (size=0;size<2;++size) {
        SetWindowSize(size?1280:800,size?720:500); app_frame(&app); app_frame(&app);
        check_picker(SCREEN_DASHBOARD);
        if (!size) picture(argv[2],"expanded-overview");
        click(700,452); CHECK(app.screen==SCREEN_SETTINGS); /* Reserved Custom. */
        check_picker(SCREEN_SETTINGS);
        click(620,358); CHECK(app.duration_focus); /* Expanded custom field. */
        click(350,650); CHECK(app.db.settings.duration==app.settings_duration);
        if (!size) picture(argv[2],"expanded-settings");
        check_picker(SCREEN_LEADERBOARD);
        click(750,171); CHECK(app.leader_duration==app.db.settings.duration);
        if (!size) picture(argv[2],"expanded-leaderboard");
        app.screen=SCREEN_DASHBOARD; app_frame(&app);
        click(1100,390); CHECK(app.screen==SCREEN_TEST && !app.test.started);
        CHECK(app.test.duration==app.db.settings.duration);
    }
    ui_shutdown(); CloseWindow();
    printf("Expanded preset GUI: %d passed, %d failed (%d presets).\n",checks-failures,failures,DURATION_PRESET_COUNT);
    return failures?1:0;
}
