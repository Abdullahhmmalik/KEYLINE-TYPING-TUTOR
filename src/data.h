#ifndef DATA_H
#define DATA_H
#include "typing.h"

typedef struct { int id; char name[USERNAME_MAX + 1]; unsigned int achievements; } Profile;
typedef struct {
    int id, profile_id;
    long long timestamp;
    int duration;
    double elapsed, wpm, accuracy;
    int mistakes, completed, matched, typed, length, correct_keys, total_keys;
} Record;
typedef struct {
    char directory[MAX_PATH_LEN];
    Profile profiles[MAX_PROFILES];
    Record records[MAX_RECORDS];
    int profile_count, record_count, next_profile_id, next_record_id;
    Settings settings;
    int ready, skipped_lines;
    char message[256];
} Database;
typedef struct {
    int count, latest, previous, best;
    double average_wpm, average_accuracy, best_accuracy;
} UserSummary;
typedef struct { int profile_index, record_index; } LeaderEntry;

int data_open(Database *db, const char *directory);
int data_valid_username(const char *name);
int data_find_profile(const Database *db, const char *name);
int data_profile_index(const Database *db, int id);
int data_create_profile(Database *db, const char *name);
int data_append_record(Database *db, Record *record);
int data_save_settings(Database *db, Settings settings);
int data_save_achievements(Database *db);
int data_record_valid(const Record *r);
Record data_record_from_test(const TypingTest *test, int profile_id, long long timestamp);
UserSummary data_summary(const Database *db, int profile_id);
int data_record_better(const Record *a, const Record *b);
/* Feature edit points: change the board policy without changing personal bests.
 * data_record_better remains the shared, backwards-compatible default order. */
int data_personal_best_better(const Record *a, const Record *b);
int data_leaderboard_better(const Record *a, const Record *b);
int data_record_eligible(const Record *record, int duration, double min_accuracy);
int data_leaderboard(const Database *db, int duration, LeaderEntry *entries, int capacity);
void data_format_date(long long timestamp, char *out, int capacity);
#endif
