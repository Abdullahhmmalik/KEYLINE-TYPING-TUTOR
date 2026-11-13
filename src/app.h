#ifndef APP_H
#define APP_H
#include "data.h"
typedef enum { SCREEN_PROFILES, SCREEN_DASHBOARD, SCREEN_TEST, SCREEN_RESULTS,
               SCREEN_HISTORY, SCREEN_LEADERBOARD, SCREEN_ACHIEVEMENTS, SCREEN_SETTINGS } Screen;
typedef struct {
    Database db;
    Screen screen;
    int selected_profile, exercise, profile_page, history_page, leader_page, leader_duration;
    int timer_preset_page; /* Shared, transient page for the three timer pickers. */
    TypingTest test;
    Record result;
    int result_saved, username_focus, duration_focus, settings_duration;
    double settings_accuracy;
    char username[USERNAME_MAX + 1], duration_text[8];
    char toast[256];
    double toast_until;
    unsigned int new_achievements;
    int abandon_target, modal;
    int exit_pending, exit_requested;
} App;
void app_init(App *app, const char *data_directory);
void app_frame(App *app);
void app_request_exit(App *app);
#endif
