#ifndef CONFIG_H
#define CONFIG_H

#define APP_NAME "Keyline"
#define APP_VERSION "1.0.2"
#ifndef APP_BUILD_TAG
#define APP_BUILD_TAG "local"
#endif
#define MAX_PROFILES 64
#define MAX_RECORDS 10000
#define USERNAME_MIN 3
#define USERNAME_MAX 20
#define MAX_PASSAGE 2048
#define MAX_PATH_LEN 512
#define MIN_DURATION 5
#define MAX_DURATION 600
#define DEFAULT_DURATION 60
#define DEFAULT_MIN_ACCURACY 80.0
#define RANK_MIN_SECONDS 10.0
#define ACHIEVEMENT_COUNT 6
#define ACH_SPEED_ONE 30.0
#define ACH_SPEED_TWO 50.0
#define ACH_ACCURACY 95.0
#define ACH_TEST_COUNT 5
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define MIN_WINDOW_WIDTH 800
#define MIN_WINDOW_HEIGHT 500

typedef struct { int duration; double min_accuracy; } Settings;
typedef struct { const char *title; const char *text; } Exercise;
extern const int DURATION_PRESETS[];
extern const int DURATION_PRESET_COUNT;
extern const Exercise EXERCISES[];
extern const int EXERCISE_COUNT;
int config_valid_duration(int seconds);
int config_parse_duration(const char *text, int *seconds);

#endif
