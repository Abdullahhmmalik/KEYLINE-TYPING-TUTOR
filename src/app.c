#include "app.h"
#include "ui.h"
#include "achievements.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define CONTENT_X 260
#define CONTENT_W 980
#define PAGE_ROWS 7
#define TIMER_COLUMNS 4
#define TIMER_PAGE_SIZE 8

/* A navigation target is a Screen value, or one of these test actions. */
enum { NAV_RESTART_TEST = -1, NAV_NEW_TEST = -2 };

static const char *SCREEN_NAMES[] = {"Profiles", "Overview", "Typing studio", "Your results", "Performance history", "Leaderboard", "Achievements", "Settings"};
static void notify(App *a, const char *text)
{ snprintf(a->toast, sizeof a->toast, "%s", text); a->toast_until = GetTime()+7; }
static Profile *current_profile(App *a) { return &a->db.profiles[a->selected_profile]; }
static UserSummary summary(App *a) { return data_summary(&a->db, current_profile(a)->id); }
/* Navigation and every exit route must protect the same unsaved work. */
static int has_unsaved_attempt(const App *a)
{
    return (a->screen==SCREEN_TEST && a->test.started && !a->test.finished) ||
           (a->screen==SCREEN_RESULTS && !a->result_saved);
}
void app_request_exit(App *a)
{
    if (has_unsaved_attempt(a)) {
        a->exit_pending=1; a->modal=1;
    } else a->exit_requested=1;
}
static void start_test(App *a, int new_passage)
{
    if (new_passage || a->exercise < 0) {
        int next = GetRandomValue(0, EXERCISE_COUNT-1);
        if (EXERCISE_COUNT > 1 && next == a->exercise) next = (next+1)%EXERCISE_COUNT;
        a->exercise = next;
    }
    if (!typing_init(&a->test, EXERCISES[a->exercise].text, a->db.settings.duration)) {
        notify(a, "This exercise is invalid. Use 1-2048 printable ASCII characters."); return;
    }
    a->screen = SCREEN_TEST;
    a->modal = 0;
    a->new_achievements = 0;
}
static void navigate(App *a, int target)
{
    if (has_unsaved_attempt(a)) {
        a->modal = 1; a->abandon_target = target; return;
    }
    if (target < 0) start_test(a, target == NAV_NEW_TEST);
    else a->screen = (Screen)target;
}
static void select_duration(App *a, int seconds)
{
    Settings s = a->db.settings;
    s.duration = seconds;
    if (!data_save_settings(&a->db, s)) notify(a, a->db.message);
}
/* Four columns, at most two rows. Extra pages keep any number of presets
 * clear of Custom/Start. first and step use logical canvas coordinates. */
static int timer_presets(App *a, int selected, Rectangle first, Vector2 step, float pager_y)
{
    int i, chosen=0;
    int pages=(DURATION_PRESET_COUNT+TIMER_PAGE_SIZE-1)/TIMER_PAGE_SIZE;
    if (pages<1) pages=1;
    if (a->timer_preset_page<0) a->timer_preset_page=0;
    if (a->timer_preset_page>=pages) a->timer_preset_page=pages-1;
    for (i=0;i<TIMER_PAGE_SIZE;++i) {
        int index=a->timer_preset_page*TIMER_PAGE_SIZE+i;
        Rectangle button=first;
        int duration;
        if (index>=DURATION_PRESET_COUNT) break;
        duration=DURATION_PRESETS[index];
        button.x+=(i%TIMER_COLUMNS)*step.x;
        button.y+=(i/TIMER_COLUMNS)*step.y;
        if (ui_button(button,TextFormat("%d s",duration),selected==duration)) chosen=duration;
    }
    if (pages>1) {
        if (ui_button((Rectangle){892,pager_y,32,32},"<",0) && a->timer_preset_page>0) --a->timer_preset_page;
        ui_text(TextFormat("%d/%d",a->timer_preset_page+1,pages),934,pager_y+8,14,THEME.muted);
        if (ui_button((Rectangle){982,pager_y,32,32},">",0) && a->timer_preset_page+1<pages) ++a->timer_preset_page;
    }
    return chosen; /* 0 means no selection; valid durations start at MIN_DURATION. */
}
static void page_controls(int *page, int total, float x, float y)
{
    char label[64];
    int pages = (total+PAGE_ROWS-1)/PAGE_ROWS;
    if (pages < 1) pages = 1;
    if (*page >= pages) *page = pages-1;
    if (*page < 0) *page = 0;
    if (ui_button((Rectangle){x,y,90,34}, "Previous",0) && *page > 0) --*page;
    snprintf(label,sizeof label,"%d / %d",*page+1,pages);
    ui_text(label,x+113,y+8,16,THEME.muted);
    if (ui_button((Rectangle){x+180,y,90,34},"Next",0) && *page+1 < pages) ++*page;
}
static void heading(const char *title, const char *subtitle)
{
    ui_text(title,CONTENT_X,50,34,THEME.text);
    ui_text(subtitle,CONTENT_X,98,18,THEME.muted);
}
static void sidebar(App *a)
{
    const Screen screens[] = {SCREEN_DASHBOARD,SCREEN_TEST,SCREEN_HISTORY,SCREEN_LEADERBOARD,SCREEN_ACHIEVEMENTS,SCREEN_SETTINGS};
    const char *labels[] = {"Overview","Typing studio","History","Leaderboard","Achievements","Settings"};
    int i;
    char short_name[USERNAME_MAX+4];
    DrawRectangle(0,0,220,WINDOW_HEIGHT,THEME.sidebar);
    DrawLine(219,0,219,WINDOW_HEIGHT,THEME.border);
    ui_panel((Rectangle){24,32,38,38},THEME.accent);
    ui_text("K",33,35,28,THEME.background);
    ui_text("keyline",74,34,28,THEME.text);
    ui_text("MAKE EVERY KEY COUNT",25,91,11,THEME.muted);
    ui_text("YOUR WORKSPACE",25,152,12,THEME.muted);
    for (i=0;i<6;++i) {
        Rectangle r = {16,184+i*56.0f,188,46};
        int active = a->screen == screens[i] || (a->screen == SCREEN_RESULTS && i==1);
        Color ink = active ? THEME.accent : THEME.muted;
        if (active || ui_hover(r)) ui_panel(r,THEME.panel_alt);
        if (active) DrawRectangle(16,(int)r.y+12,3,22,THEME.accent);
        DrawCircleLines(40,(int)r.y+23,7,ink);
        if (active) DrawCircle(40,(int)r.y+23,3,ink);
        ui_text(labels[i],59,r.y+13,18,ink);
        if (ui_hover(r)) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        if (ui_clicked(r)) {
            if (screens[i] == SCREEN_TEST) { if (a->screen != SCREEN_TEST) navigate(a,NAV_NEW_TEST); }
            else navigate(a,screens[i]);
            return;
        }
    }
    ui_panel((Rectangle){16,654,188,74},THEME.panel);
    DrawCircle(43,681,13,THEME.accent);
    ui_text(TextFormat("%.1s",current_profile(a)->name),37,670,19,THEME.background);
    snprintf(short_name,sizeof short_name,"%s",current_profile(a)->name);
    if (ui_text_width(short_name,16)>128) {
        size_t n=strlen(short_name);
        while (n>0) {
            short_name[--n]=0;
            if (ui_text_width(short_name,16)+ui_text_width("...",16)<=128) break;
        }
        memcpy(short_name+n,"...",4);
    }
    ui_text(short_name,65,666,16,THEME.text);
    ui_text("Local profile",65,692,12,THEME.muted);
    if (ui_button((Rectangle){16,740,188,36},"Switch profile",0)) navigate(a,SCREEN_PROFILES);
}
static void accuracy_chart(App *a, Rectangle r, int maximum)
{
    int indices[32], count=0, i;
    UserSummary s = summary(a);
    float left=r.x+42, top=r.y+45, width=r.width-66, height=r.height-82;
    ui_panel(r,THEME.panel);
    ui_text("Accuracy over time",r.x+20,r.y+16,18,THEME.text);
    for (i=a->db.record_count-1;i>=0 && count<maximum && count<32;--i)
        if (a->db.records[i].profile_id == current_profile(a)->id) indices[count++]=i;
    if (!count) {
        ui_text("Your progress starts with your first test.",r.x+25,r.y+r.height/2,16,THEME.muted); return;
    }
    for (i=0;i<=2;++i) {
        float y=top+height*i/2;
        DrawLine((int)left,(int)y,(int)(left+width),(int)y,THEME.border);
        ui_text(TextFormat("%d",100-i*50),r.x+9,y-6,11,THEME.muted);
    }
    for (i=0;i<count;++i) {
        const Record *record = &a->db.records[indices[count-1-i]];
        float x=left+(count>1 ? width*i/(count-1) : width/2);
        float y=top+height*(float)(1-record->accuracy/100);
        if (i>0) {
            const Record *prev = &a->db.records[indices[count-i]];
            float px=left+width*(i-1)/(count-1);
            float py=top+height*(float)(1-prev->accuracy/100);
            DrawLineEx((Vector2){px,py},(Vector2){x,y},2,THEME.accent);
        }
        DrawCircleV((Vector2){x,y},4,THEME.accent);
    }
    ui_text(TextFormat("Last %d attempts  /  %.1f%% overall average",count,s.average_accuracy),r.x+20,r.y+r.height-25,12,THEME.muted);
}
static void profiles_screen(App *a)
{
    int i;
    DrawRectangle(0,0,590,WINDOW_HEIGHT,THEME.sidebar);
    ui_panel((Rectangle){60,64,46,46},THEME.accent);
    ui_text("K",72,68,32,THEME.background);
    ui_text("keyline",120,69,32,THEME.text);
    ui_text("A LITTLE PRACTICE. REAL PROGRESS.",60,191,14,THEME.accent);
    ui_text("Find your\ntyping rhythm.",56,237,56,THEME.text);
    ui_text("A quiet place to build speed, sharpen accuracy,\nand see how far you have come.",60,402,21,THEME.muted);
    ui_panel((Rectangle){60,530,465,134},THEME.panel);
    ui_text("YOUR PROGRESS, ON YOUR COMPUTER",82,551,13,THEME.accent);
    ui_text("Personal history. Friendly competition.\nSmall milestones worth coming back for.",82,585,19,THEME.text);
    ui_text("C99 + raylib  /  Programming Fundamentals",60,745,13,THEME.muted);
    ui_text("Welcome to your workspace",640,68,30,THEME.text);
    ui_text("Choose a profile or make a fresh start.",640,112,18,THEME.muted);
    ui_text("SELECT A PROFILE",640,169,13,THEME.muted);
    if (!a->db.profile_count) ui_text("No profiles yet. Create yours below.",640,224,19,THEME.muted);
    for (i=0;i<5;++i) {
        int index=a->profile_page*5+i;
        Rectangle row={640,205+i*57.0f,570,48};
        if (index >= a->db.profile_count) break;
        ui_panel(row,ui_hover(row)?THEME.panel_alt:THEME.panel);
        DrawCircle(663,(int)row.y+24,12,THEME.accent);
        ui_text(TextFormat("%.1s",a->db.profiles[index].name),658,row.y+13,17,THEME.background);
        ui_text(a->db.profiles[index].name,687,row.y+13,18,THEME.text);
        ui_text("Open  >",1120,row.y+14,16,THEME.accent);
        if (ui_clicked(row)) { a->selected_profile=index; a->screen=SCREEN_DASHBOARD; a->username_focus=0; return; }
    }
    if (a->db.profile_count>5) {
        if (ui_button((Rectangle){640,500,88,30},"Previous",0) && a->profile_page>0) --a->profile_page;
        ui_text(TextFormat("Page %d",a->profile_page+1),750,507,14,THEME.muted);
        if (ui_button((Rectangle){850,500,88,30},"Next",0) && (a->profile_page+1)*5<a->db.profile_count) ++a->profile_page;
    }
    ui_text("CREATE A NEW PROFILE",640,557,13,THEME.muted);
    ui_input((Rectangle){640,590,360,52},a->username,sizeof a->username,&a->username_focus,0);
    if (ui_button((Rectangle){1016,590,194,52},"Create profile",1) || (a->username_focus && IsKeyPressed(KEY_ENTER))) {
        int index=data_create_profile(&a->db,a->username);
        if (index>=0) { a->selected_profile=index; a->screen=SCREEN_DASHBOARD; a->username[0]=0; a->username_focus=0; }
        notify(a,a->db.message);
    }
    ui_text("3-20 characters: letters, numbers, _ and -",640,656,15,THEME.muted);
    ui_text("No passwords. No account. Just you and the keyboard.",640,711,15,THEME.muted);
}
static void dashboard_screen(App *a)
{
    UserSummary s=summary(a);
    char title[80];
    int duration, expanded=DURATION_PRESET_COUNT>TIMER_COLUMNS;
    snprintf(title,sizeof title,"Make a little progress, %s.",current_profile(a)->name);
    heading(title,"Build a steady rhythm. Your next personal best starts here.");
    ui_metric(260,147,312,"PERSONAL BEST",s.best>=0?TextFormat("%.1f",a->db.records[s.best].wpm):"--","WPM / all durations",THEME.accent);
    ui_metric(594,147,312,"AVERAGE ACCURACY",s.count?TextFormat("%.1f%%",s.average_accuracy):"--","Across your saved attempts",THEME.text);
    ui_metric(928,147,312,"TESTS COMPLETED",TextFormat("%d",s.count),"One session at a time",THEME.text);
    ui_panel((Rectangle){260,290,980,208},THEME.panel);
    ui_text("READY WHEN YOU ARE",284,312,13,THEME.accent);
    ui_text("Set a timer. Find your flow.",284,341,30,THEME.text);
    ui_text("The clock starts with your first character.",284,386,18,THEME.muted);
    duration=timer_presets(a,a->db.settings.duration,(Rectangle){284,expanded?414:432,78,expanded?32:40},(Vector2){89,40},432);
    if (duration) select_duration(a,duration);
    if (ui_button((Rectangle){640,432,135,40},"Custom timer",0)) a->screen=SCREEN_SETTINGS;
    if (ui_button((Rectangle){999,365,214,56},"Start typing  >",1)) start_test(a,1);
    ui_text(TextFormat("Selected: %d seconds",a->db.settings.duration),1025,437,15,THEME.muted);
    accuracy_chart(a,(Rectangle){260,522,600,216},10);
    ui_panel((Rectangle){882,522,358,216},THEME.panel);
    ui_text("Little wins add up",904,545,23,THEME.text);
    ui_text(TextFormat("%d of %d milestones unlocked",achievements_count(current_profile(a)->achievements),ACHIEVEMENT_COUNT),904,584,17,THEME.muted);
    ui_progress((Rectangle){904,620,312,7},100.0f*achievements_count(current_profile(a)->achievements)/ACHIEVEMENT_COUNT,THEME.accent);
    if (ui_button((Rectangle){904,657,312,48},"Explore achievements",0)) a->screen=SCREEN_ACHIEVEMENTS;
}
/* Word wrapping uses the original character indices, including spaces at line ends. */
static void draw_passage(App *a)
{
    TypingTest *t=&a->test;
    Vector2 positions[MAX_PASSAGE+1];
    const float size=25, line_height=43;
    float advance=ui_character_width(size), x=0, y=0, width=906;
    float scroll;
    int i;
    Rectangle clip={284,288,932,258};
    for (i=0;i<t->length;++i) {
        if (i==0 || t->passage[i-1]==' ') {
            int end=i;
            while (end<t->length && t->passage[end]!=' ') ++end;
            if (x>0 && x+(end-i)*advance>width) { x=0; y+=line_height; }
        }
        if (x+advance>width) { x=0; y+=line_height; }
        positions[i]=(Vector2){x,y};
        x+=advance;
    }
    positions[t->length]=(Vector2){x,y};
    scroll=fmaxf(0,positions[t->position].y-line_height*3);
    BeginScissorMode((int)clip.x,(int)clip.y,(int)clip.width,(int)clip.height);
    for (i=0;i<t->length;++i) {
        float px=clip.x+positions[i].x, py=clip.y+positions[i].y-scroll;
        char c[2]={t->passage[i],0};
        Color color=THEME.muted;
        if (py<clip.y-line_height || py>clip.y+clip.height) continue;
        if (i<t->position) {
            if (t->typed[i]==t->passage[i]) color=THEME.green;
            else {
                color=THEME.red;
                ui_panel((Rectangle){px-1,py-2,advance+2,33},(Color){78,32,47,255});
                DrawLine((int)px,(int)py+31,(int)(px+advance-1),(int)py+31,THEME.red);
            }
        }
        ui_mono(c,px,py,size,color);
    }
    if (!t->finished && ((int)(GetTime()*2.5)%2==0)) {
        Vector2 cursor=positions[t->position];
        DrawRectangle((int)(clip.x+cursor.x)-2,(int)(clip.y+cursor.y-scroll)-2,2,32,THEME.accent);
    }
    EndScissorMode();
}
/* First save and retries must update the result and unlock feedback identically. */
static void save_result(App *a)
{
    unsigned int old=current_profile(a)->achievements;
    a->result_saved=data_append_record(&a->db,&a->result);
    a->new_achievements|=current_profile(a)->achievements & ~old;
    notify(a,a->db.message);
}
static void finish_test(App *a)
{
    a->result=data_record_from_test(&a->test,current_profile(a)->id,(long long)time(NULL));
    save_result(a);
    a->screen=SCREEN_RESULTS;
    a->modal=a->exit_pending && !a->result_saved;
    if (a->exit_pending && a->result_saved) a->exit_requested=1;
}
static void test_input(App *a)
{
    int c, control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    if (IsKeyPressed(KEY_ESCAPE)) { navigate(a,SCREEN_DASHBOARD); return; }
    if (control && IsKeyPressed(KEY_R)) { navigate(a,NAV_RESTART_TEST); return; }
    if (control && IsKeyPressed(KEY_N)) { navigate(a,NAV_NEW_TEST); return; }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) typing_backspace(&a->test,GetTime());
    while ((c=GetCharPressed()) != 0) if (!control) typing_character(&a->test,c,GetTime());
}
static void test_screen(App *a)
{
    Metrics m=typing_metrics(&a->test);
    char subtitle[128];
    snprintf(subtitle,sizeof subtitle,"%s  /  %d-second session",EXERCISES[a->exercise].title,a->test.duration);
    heading("Find your focus.",subtitle);
    ui_metric(260,147,180,"SPEED",TextFormat("%.1f",m.wpm),"words / minute",THEME.accent);
    ui_metric(460,147,180,"ACCURACY",a->test.total_keys?TextFormat("%.1f%%",m.accuracy):"--","printable keystrokes",THEME.text);
    ui_metric(660,147,180,"MISTAKES",TextFormat("%d",m.mistakes),"including corrections",THEME.red);
    ui_metric(860,147,180,"REMAINING",TextFormat("%d s",(int)ceil(m.remaining)),"time left",THEME.text);
    ui_metric(1060,147,180,"ELAPSED",TextFormat("%.1f s",m.elapsed),"since first character",THEME.text);
    ui_panel((Rectangle){260,278,980,303},THEME.panel);
    draw_passage(a);
    ui_text(a->test.started?"Keep going. Accuracy first, speed follows.":"Start typing anywhere. The timer is waiting for you.",284,549,15,THEME.muted);
    ui_progress((Rectangle){260,600,980,6},(float)m.progress,THEME.accent);
    ui_text(TextFormat("%d / %d characters",a->test.position,a->test.length),260,622,16,THEME.muted);
    ui_text(TextFormat("%.0f%% through the passage",m.progress),1030,622,16,THEME.muted);
    if (ui_button((Rectangle){260,680,170,48},"Restart",0)) navigate(a,NAV_RESTART_TEST);
    if (ui_button((Rectangle){446,680,170,48},"New passage",0)) navigate(a,NAV_NEW_TEST);
    if (ui_button((Rectangle){1060,680,180,48},"Back to overview",0)) navigate(a,SCREEN_DASHBOARD);
    ui_text("BACKSPACE  correct   /   CTRL + R  restart   /   CTRL + N  new passage   /   ESC  back",260,745,12,THEME.muted);
}
static void results_screen(App *a)
{
    Record *r=&a->result;
    UserSummary s=summary(a);
    int previous=a->result_saved?s.previous:s.latest;
    heading(r->completed?"Passage complete. Nicely done.":"Time is up. Every session counts.",a->result_saved?"Your result is saved. Take a breath, then try again.":"This result has not been saved. Use Retry save below.");
    ui_metric(260,147,312,"TYPING SPEED",TextFormat("%.1f",r->wpm),"WPM / correct current characters",THEME.accent);
    ui_metric(594,147,312,"ACCURACY",TextFormat("%.1f%%",r->accuracy),"Correct / total printable keystrokes",THEME.text);
    ui_metric(928,147,312,"MISTAKES",TextFormat("%d",r->mistakes),"Corrections do not erase mistakes",THEME.red);
    ui_panel((Rectangle){260,291,980,181},THEME.panel);
    ui_text("SESSION BREAKDOWN",284,314,13,THEME.muted);
    ui_text(TextFormat("%.2f / %d s",r->elapsed,r->duration),284,351,27,THEME.text);
    ui_text("Elapsed / selected duration",284,400,15,THEME.muted);
    ui_text(TextFormat("%d / %d",r->typed,r->length),626,351,27,THEME.text);
    ui_text("Passage characters entered",626,400,15,THEME.muted);
    ui_text(r->completed?"Completed":"Timer expired",951,351,25,THEME.blue);
    ui_text(TextFormat("%d matched characters",r->matched),951,400,15,THEME.muted);
    ui_panel((Rectangle){260,496,980,123},THEME.panel);
    ui_text("YOUR ACCURACY RECORD",284,518,13,THEME.muted);
    ui_text(previous>=0?TextFormat("%+.1f pts",r->accuracy-a->db.records[previous].accuracy):"First attempt",284,549,27,THEME.accent);
    ui_text("vs. previous test",284,587,13,THEME.muted);
    ui_text(previous>=0?TextFormat("%.1f%% previous",a->db.records[previous].accuracy):"No previous result",584,551,21,THEME.text);
    ui_text(s.count?TextFormat("%.1f%% average",s.average_accuracy):"No saved average",929,551,21,THEME.text);
    if (a->new_achievements) ui_text(TextFormat("%d new milestone(s) unlocked. View your achievements!",achievements_count(a->new_achievements)),284,638,18,THEME.accent);
    if (ui_button((Rectangle){260,684,208,50},"New test  >",1)) navigate(a,NAV_NEW_TEST);
    if (ui_button((Rectangle){484,684,174,50},"Retry passage",0)) navigate(a,NAV_RESTART_TEST);
    if (ui_button((Rectangle){674,684,174,50},"View history",0)) navigate(a,SCREEN_HISTORY);
    if (!a->result_saved && ui_button((Rectangle){1010,684,230,50},"Retry save",1)) save_result(a);
}
static void history_screen(App *a)
{
    UserSummary s=summary(a);
    int i, indices[PAGE_ROWS], count=0, skipped=0;
    heading("Your practice, in perspective.","Every point comes from your saved tests. Keep showing up.");
    ui_metric(260,146,228,"LATEST ACCURACY",s.latest>=0?TextFormat("%.1f%%",a->db.records[s.latest].accuracy):"--","Most recent attempt",THEME.accent);
    ui_metric(510,146,228,"PREVIOUS",s.previous>=0?TextFormat("%.1f%%",a->db.records[s.previous].accuracy):"--",s.previous>=0?TextFormat("%+.1f points since then",a->db.records[s.latest].accuracy-a->db.records[s.previous].accuracy):"Complete two tests to compare",THEME.text);
    ui_metric(760,146,228,"AVERAGE",s.count?TextFormat("%.1f%%",s.average_accuracy):"--",s.count?TextFormat("%.1f WPM average",s.average_wpm):"All saved attempts",THEME.text);
    ui_metric(1010,146,230,"BEST ACCURACY",s.count?TextFormat("%.1f%%",s.best_accuracy):"--",s.best>=0?TextFormat("Speed best: %.1f WPM",a->db.records[s.best].wpm):"Your best is still ahead",THEME.text);
    accuracy_chart(a,(Rectangle){260,283,980,150},16);
    ui_text(TextFormat("SAVED ATTEMPTS (%d)",s.count),260,452,13,THEME.muted);
    ui_text("DATE / LOCAL TIME",280,483,12,THEME.muted);
    ui_text("WPM",584,483,12,THEME.muted);
    ui_text("ACCURACY",697,483,12,THEME.muted);
    ui_text("ERRORS",843,483,12,THEME.muted);
    ui_text("TIME / LIMIT",946,483,12,THEME.muted);
    ui_text("STATUS",1116,483,12,THEME.muted);
    for (i=a->db.record_count-1;i>=0 && count<PAGE_ROWS;--i) {
        if (a->db.records[i].profile_id != current_profile(a)->id) continue;
        if (skipped++ < a->history_page*PAGE_ROWS) continue;
        indices[count++]=i;
    }
    if (!count) ui_text("No saved tests yet. Visit the typing studio to begin.",280,553,20,THEME.muted);
    for (i=0;i<count;++i) {
        Record *r=&a->db.records[indices[i]];
        char date[64];
        float y=507+i*28.0f;
        data_format_date(r->timestamp,date,sizeof date);
        if (i%2==0) ui_panel((Rectangle){260,y-3,980,28},THEME.panel);
        ui_text(date,280,y+2,14,THEME.text);
        ui_text(TextFormat("%.1f",r->wpm),584,y+2,14,THEME.accent);
        ui_text(TextFormat("%.1f%%",r->accuracy),697,y+2,14,THEME.text);
        ui_text(TextFormat("%d",r->mistakes),843,y+2,14,THEME.text);
        ui_text(TextFormat("%.1f / %d s",r->elapsed,r->duration),946,y+2,14,THEME.text);
        ui_text(r->completed?"Complete":"Time up",1116,y+2,14,r->completed?THEME.green:THEME.muted);
    }
    page_controls(&a->history_page,s.count,970,720);
    ui_text("Personal best and averages include all selected durations.",260,729,13,THEME.muted);
}
static void leaderboard_screen(App *a)
{
    LeaderEntry entries[MAX_PROFILES];
    int i, count, duration, expanded=DURATION_PRESET_COUNT>TIMER_COLUMNS;
    heading("A little friendly competition.","One personal best per profile. Compare the same selected duration.");
    duration=timer_presets(a,a->leader_duration,(Rectangle){260,150,84,expanded?30:42},(Vector2){96,36},150);
    if (duration) {
        a->leader_duration=duration; a->leader_page=0;
    }
    if (ui_button((Rectangle){644,150,220,42},TextFormat("Selected timer: %d s",a->db.settings.duration),a->leader_duration==a->db.settings.duration?1:0)) {
        a->leader_duration=a->db.settings.duration; a->leader_page=0;
    }
    ui_text(TextFormat("Eligibility: at least %.15g%% accuracy and %.15g s elapsed. Adjust accuracy in Settings.",a->db.settings.min_accuracy,fmin(a->leader_duration,RANK_MIN_SECONDS)),260,expanded?230:216,16,THEME.muted);
    count=data_leaderboard(&a->db,a->leader_duration,entries,MAX_PROFILES);
    ui_panel((Rectangle){260,260,980,445},THEME.panel);
    ui_text("RANK",284,281,13,THEME.muted);
    ui_text("PROFILE",379,281,13,THEME.muted);
    ui_text("WPM",718,281,13,THEME.muted);
    ui_text("ACCURACY",871,281,13,THEME.muted);
    ui_text("MISTAKES",1071,281,13,THEME.muted);
    if (!count) {
        ui_text("The board is waiting for its first result.",330,398,28,THEME.text);
        ui_text("Finish an eligible test for this timer to appear here.",330,448,19,THEME.muted);
    }
    for (i=0;i<PAGE_ROWS;++i) {
        int index=a->leader_page*PAGE_ROWS+i;
        LeaderEntry e;
        Record *r;
        float y=324+i*51.0f;
        if (index>=count) break;
        e=entries[index]; r=&a->db.records[e.record_index];
        if (e.profile_index==a->selected_profile) ui_panel((Rectangle){275,y-6,950,46},THEME.panel_alt);
        ui_text(TextFormat("%02d",index+1),290,y+4,23,index<3?THEME.accent:THEME.muted);
        ui_text(a->db.profiles[e.profile_index].name,379,y+6,20,THEME.text);
        ui_text(TextFormat("%.2f",r->wpm),718,y+6,20,THEME.accent);
        ui_text(TextFormat("%.2f%%",r->accuracy),871,y+6,20,THEME.text);
        ui_text(TextFormat("%d",r->mistakes),1071,y+6,20,THEME.muted);
    }
    page_controls(&a->leader_page,count,970,720);
    ui_text("Ranked by WPM, then accuracy, then fewer mistakes.",260,729,14,THEME.muted);
}
static void achievements_screen(App *a)
{
    int i;
    unsigned int mask=current_profile(a)->achievements;
    heading("Small milestones. Lasting progress.",TextFormat("%d of %d unlocked. Every achievement belongs to this profile.",achievements_count(mask),ACHIEVEMENT_COUNT));
    for (i=0;i<ACHIEVEMENT_COUNT;++i) {
        int unlocked=(mask & (1u<<i)) != 0;
        float x=260+(i%2)*502.0f, y=151+(i/2)*190.0f;
        char description[128];
        achievements_description(i,description,sizeof description);
        ui_panel((Rectangle){x,y,478,168},THEME.panel);
        DrawCircle((int)x+37,(int)y+38,18,unlocked?THEME.accent:THEME.border);
        if (unlocked) {
            DrawLineEx((Vector2){x+29,y+38},(Vector2){x+35,y+44},3,THEME.background);
            DrawLineEx((Vector2){x+35,y+44},(Vector2){x+46,y+32},3,THEME.background);
        } else { DrawCircleLines((int)x+37,(int)y+35,5,THEME.muted); DrawRectangle((int)x+31,(int)y+36,12,10,THEME.muted); }
        ui_text(unlocked?"UNLOCKED":"KEEP PRACTICING",x+68,y+31,12,unlocked?THEME.accent:THEME.muted);
        ui_text(ACHIEVEMENTS[i].title,x+22,y+73,24,THEME.text);
        ui_text(description,x+22,y+116,16,THEME.muted);
    }
    ui_text("Milestones stay unlocked. Your saved history can restore them if the cache is missing.",260,748,14,THEME.muted);
}
static void settings_screen(App *a)
{
    int preset_duration, expanded=DURATION_PRESET_COUNT>TIMER_COLUMNS;
    float custom_y=expanded?334:309;
    heading("Make practice your own.","Choose your pace and the rules for your local leaderboard.");
    ui_panel((Rectangle){260,147,980,264},THEME.panel);
    ui_text("Practice timer",284,171,25,THEME.text);
    ui_text("The timer starts on your first printable character, and keeps running if you switch windows.",284,211,17,THEME.muted);
    preset_duration=timer_presets(a,a->settings_duration,(Rectangle){284,expanded?246:254,94,expanded?30:42},(Vector2){108,36},254);
    if (preset_duration) {
        a->settings_duration=preset_duration; snprintf(a->duration_text,sizeof a->duration_text,"%d",preset_duration);
    }
    ui_text("Custom duration (seconds)",284,custom_y+13,16,THEME.muted);
    if (ui_input((Rectangle){560,custom_y,145,49},a->duration_text,sizeof a->duration_text,&a->duration_focus,1)) {
        int duration;
        if (config_parse_duration(a->duration_text,&duration)) a->settings_duration=duration;
    }
    ui_text(TextFormat("%d-%d seconds",MIN_DURATION,MAX_DURATION),731,custom_y+16,16,THEME.muted);
    ui_text(TextFormat("Saved timer: %d seconds",a->db.settings.duration),284,expanded?387:375,15,THEME.accent);
    ui_panel((Rectangle){260,431,980,170},THEME.panel);
    ui_text("Leaderboard accuracy",284,453,25,THEME.text);
    ui_text("Minimum accuracy for a ranked result. Applies to every duration view.",284,493,17,THEME.muted);
    if (ui_button((Rectangle){284,537,46,40},"-",0)) a->settings_accuracy=fmax(0,a->settings_accuracy-5);
    ui_text(TextFormat("%.15g%%",a->settings_accuracy),356,540,28,THEME.accent);
    if (ui_button((Rectangle){450,537,46,40},"+",0)) a->settings_accuracy=fmin(100,a->settings_accuracy+5);
    ui_text(TextFormat("At least %.15g s of typing (or the full timer if shorter).",(double)RANK_MIN_SECONDS),550,548,16,THEME.muted);
    if (ui_button((Rectangle){260,628,210,50},"Save settings",1)) {
        int duration;
        if (!config_parse_duration(a->duration_text,&duration)) notify(a,TextFormat("Enter a whole number between %d and %d seconds.",MIN_DURATION,MAX_DURATION));
        else {
            Settings settings={duration,a->settings_accuracy};
            data_save_settings(&a->db,settings); notify(a,a->db.message);
        }
    }
    if (ui_button((Rectangle){488,628,180,50},"Reset to defaults",0)) {
        a->settings_duration=DEFAULT_DURATION; a->settings_accuracy=DEFAULT_MIN_ACCURACY;
        snprintf(a->duration_text,sizeof a->duration_text,"%d",DEFAULT_DURATION);
    }
    ui_text("LOCAL STORAGE",260,709,12,THEME.muted);
    ui_text(a->db.directory,260,732,13,THEME.muted);
}
static void modal_screen(App *a)
{
    int live=a->screen==SCREEN_TEST;
    ui_panel((Rectangle){362,241,690,296},THEME.panel);
    ui_text(a->exit_pending?"Exit Keyline?":live?"Leave this practice session?":"Leave without saving?",393,271,30,THEME.text);
    ui_text(live?"This attempt will be discarded. Your timer is still running.":"Your result is still unsaved. Stay here to retry saving it.",393,330,19,THEME.muted);
    ui_text("Finished tests already in your history are safe.",393,370,17,THEME.muted);
    if (ui_button((Rectangle){393,444,292,52},live?"Keep typing":"Stay on results",1) || IsKeyPressed(KEY_ESCAPE)) {
        a->modal=0; a->exit_pending=0;
    }
    if (ui_button((Rectangle){705,444,313,52},a->exit_pending?"Exit without saving":"Leave session",0) ||
        (a->exit_pending && IsKeyPressed(KEY_ENTER))) {
        int target=a->abandon_target;
        a->modal=0;
        if (a->exit_pending) { a->exit_requested=1; a->exit_pending=0; }
        else if (target<0) start_test(a,target==NAV_NEW_TEST);
        else a->screen=(Screen)target;
    }
    if (a->exit_pending) ui_text("ENTER exits without saving this attempt. ESC keeps the window open.",393,408,16,THEME.muted);
}
void app_init(App *a, const char *directory)
{
    memset(a,0,sizeof *a);
    a->selected_profile=-1; a->exercise=-1; a->screen=SCREEN_PROFILES;
    data_open(&a->db,directory);
    a->leader_duration=a->db.settings.duration;
    a->settings_duration=a->db.settings.duration;
    a->settings_accuracy=a->db.settings.min_accuracy;
    snprintf(a->duration_text,sizeof a->duration_text,"%d",a->settings_duration);
    if (a->db.message[0]) notify(a,a->db.message);
    a->username_focus=a->db.profile_count==0;
}
/* Screen drawing stays separate from the frame's input/timer/transition order.
 * Sidebar clicks may open a modal: check again before drawing the page. */
static void draw_current_screen(App *a)
{
    if (a->modal) { modal_screen(a); return; }
    if (a->screen==SCREEN_PROFILES) { profiles_screen(a); return; }
    sidebar(a);
    if (a->modal) return;
    switch (a->screen) {
        case SCREEN_DASHBOARD: dashboard_screen(a); break;
        case SCREEN_TEST: test_screen(a); break;
        case SCREEN_RESULTS: results_screen(a); break;
        case SCREEN_HISTORY: history_screen(a); break;
        case SCREEN_LEADERBOARD: leaderboard_screen(a); break;
        case SCREEN_ACHIEVEMENTS: achievements_screen(a); break;
        case SCREEN_SETTINGS: settings_screen(a); break;
        default: break;
    }
}
void app_frame(App *a)
{
    Screen before=a->screen;
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if ((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Q)) app_request_exit(a);
    if (a->screen==SCREEN_TEST) {
        typing_update(&a->test,GetTime());
        if (!a->modal && !a->test.finished) test_input(a);
        if (a->test.finished) finish_test(a);
    } else if (!a->modal && a->screen!=SCREEN_PROFILES && IsKeyPressed(KEY_ESCAPE)) navigate(a,SCREEN_DASHBOARD);
    else if (!a->modal && a->screen==SCREEN_PROFILES && IsKeyPressed(KEY_ESCAPE)) a->username_focus=0;
    ui_begin();
    if (ui_button((Rectangle){1138,10,102,30},"Exit",0)) app_request_exit(a);
    draw_current_screen(a);
    if (a->screen!=before) {
        SetWindowTitle(TextFormat("Keyline %s [%s] - %s",APP_VERSION,APP_BUILD_TAG,SCREEN_NAMES[a->screen]));
        if (a->screen==SCREEN_SETTINGS) {
            a->settings_duration=a->db.settings.duration; a->settings_accuracy=a->db.settings.min_accuracy;
            snprintf(a->duration_text,sizeof a->duration_text,"%d",a->settings_duration);
            a->duration_focus=0;
        }
        while (GetCharPressed()) { } /* A navigation key must not leak into the next screen. */
    }
    if (GetTime()<a->toast_until) {
        float width=fminf(1216,ui_text_width(a->toast,15)+40);
        ui_panel((Rectangle){(1280-width)/2,760,width,32},THEME.panel_alt);
        ui_text(a->toast,(1280-width)/2+20,767,15,THEME.accent);
    }
    if (!IsWindowFocused() && a->screen==SCREEN_TEST && a->test.started)
        ui_text("Window inactive - timer continues",860,108,14,THEME.red);
    ui_end();
}
