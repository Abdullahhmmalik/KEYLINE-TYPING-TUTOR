#include "data.h"
#include "achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#endif

static void message(Database *db, const char *text)
{ snprintf(db->message, sizeof db->message, "%s", text); }
static int path_for(const Database *db, const char *file, char *out)
{
    size_t a=strlen(db->directory), b=strlen(file);
    if (a+b+2>MAX_PATH_LEN) return 0;
    memcpy(out,db->directory,a); out[a]='/'; memcpy(out+a+1,file,b+1);
    return 1;
}
static int make_directory(const char *path)
{
#ifdef _WIN32
    struct _stat info;
    if (_mkdir(path)==0) return 1;
    return errno==EEXIST && _stat(path,&info)==0 && (info.st_mode & _S_IFDIR)!=0;
#else
    struct stat info;
    if (mkdir(path,0700)==0) return 1;
    return errno==EEXIST && stat(path,&info)==0 && S_ISDIR(info.st_mode);
#endif
}
static int make_directories(const char *path)
{
    char copy[MAX_PATH_LEN];
    size_t i, n = strlen(path);
    if (!n || n >= sizeof copy) return 0;
    memcpy(copy, path, n + 1);
    for (i = 1; i < n; ++i) if ((copy[i] == '/' || copy[i] == '\\') && copy[i-1] != ':') {
        char c = copy[i];
        copy[i] = 0;
        if (!make_directory(copy)) return 0;
        copy[i] = c;
    }
    return make_directory(copy);
}
static int same_name(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        ++a; ++b;
    }
    return *a == *b;
}
int data_valid_username(const char *name)
{
    size_t i, n;
    if (!name) return 0;
    n = strlen(name);
    if (n < USERNAME_MIN || n > USERNAME_MAX) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}
int data_find_profile(const Database *db, const char *name)
{
    int i;
    for (i = 0; i < db->profile_count; ++i) if (same_name(db->profiles[i].name, name)) return i;
    return -1;
}
int data_profile_index(const Database *db, int id)
{
    int i;
    for (i = 0; i < db->profile_count; ++i) if (db->profiles[i].id == id) return i;
    return -1;
}
/* Skip an overlong physical line as one damaged entry, including its remainder. */
static int read_line(FILE *file, char *line, int capacity)
{
    int c;
    if (!fgets(line, capacity, file)) return 0;
    if (!strchr(line, '\n') && !feof(file)) {
        while ((c = fgetc(file)) != '\n' && c != EOF) { }
        line[0] = '!'; line[1] = 0;
    }
    return 1;
}
static int split_fields(char *line, char **fields, int count)
{
    int i;
    char *p=line;
    line[strcspn(line,"\r\n")]=0;
    for (i=0;i<count;++i) {
        char *comma;
        fields[i]=p;
        comma=strchr(p,',');
        if (!*p || (i==count-1 && comma) || (i<count-1 && !comma)) return 0;
        if (comma) { *comma=0; p=comma+1; }
    }
    return 1;
}
static int parse_integer(const char *text, long long minimum, long long maximum, long long *out)
{
    char *end;
    const char *p=text;
    long long n;
    if (*p=='-') ++p;
    if (!*p) return 0;
    while (*p) { if (*p<'0' || *p>'9') return 0; ++p; }
    errno=0; n=strtoll(text,&end,10);
    if (errno || *end || n<minimum || n>maximum) return 0;
    *out=n; return 1;
}
static int parse_int(const char *text, int *out)
{
    long long n;
    if (!parse_integer(text,INT_MIN,INT_MAX,&n)) return 0;
    *out=(int)n; return 1;
}
static int parse_double(const char *text, double *out)
{
    char *end;
    double n;
    if (!*text || isspace((unsigned char)*text)) return 0;
    errno=0; n=strtod(text,&end);
    if (errno || *end || !isfinite(n)) return 0;
    *out=n; return 1;
}
static int parse_record(char *line, Record *r)
{
    char *v[14];
    return split_fields(line,v,14) && parse_int(v[0],&r->id) && parse_int(v[1],&r->profile_id) &&
        parse_integer(v[2],1,4102444800LL,&r->timestamp) && parse_int(v[3],&r->duration) &&
        parse_double(v[4],&r->elapsed) && parse_double(v[5],&r->wpm) && parse_double(v[6],&r->accuracy) &&
        parse_int(v[7],&r->mistakes) && parse_int(v[8],&r->completed) && parse_int(v[9],&r->matched) &&
        parse_int(v[10],&r->typed) && parse_int(v[11],&r->length) &&
        parse_int(v[12],&r->correct_keys) && parse_int(v[13],&r->total_keys);
}
static FILE *read_file(Database *db, const char *name)
{
    char path[MAX_PATH_LEN];
    FILE *f;
    if (!path_for(db, name, path)) { db->ready = 0; return NULL; }
    errno = 0;
    f = fopen(path, "r");
    if (!f && errno != ENOENT) {
        db->ready = 0;
        message(db, "A data file cannot be read. Check folder permissions; saving is disabled.");
    }
    return f;
}
static void close_read(Database *db, FILE *f)
{
    if (ferror(f)) { db->ready = 0; message(db, "Reading data failed. Saving is disabled to protect your records."); }
    fclose(f);
}
int data_record_valid(const Record *r)
{
    Metrics m;
    if (!r || r->id < 0 || r->id >= INT_MAX || r->profile_id < 1 || r->timestamp <= 0 ||
        r->timestamp > 4102444800LL || !config_valid_duration(r->duration) ||
        !isfinite(r->elapsed) || r->elapsed <= 0 || r->elapsed > r->duration ||
        !isfinite(r->wpm) || !isfinite(r->accuracy) || r->length < 1 || r->length > MAX_PASSAGE ||
        r->typed < 0 || r->typed > r->length || r->matched < 0 || r->matched > r->typed ||
        r->correct_keys < r->matched || r->total_keys < r->typed ||
        r->total_keys < 1 || r->total_keys > 100000000 || r->correct_keys > r->total_keys ||
        r->correct_keys < 0 || r->mistakes != r->total_keys - r->correct_keys ||
        (r->completed != 0 && r->completed != 1) ||
        (r->completed && r->typed != r->length) || (!r->completed && r->typed == r->length) ||
        (!r->completed && fabs(r->elapsed - r->duration) > 0.00001)) return 0;
    m = stats_calculate(r->matched, r->typed, r->length, r->correct_keys,
                        r->total_keys, r->elapsed, r->duration);
    return fabs(m.wpm - r->wpm) <= 0.002 && fabs(m.accuracy - r->accuracy) <= 0.002;
}
/* Each loader owns one format. read_file/close_read keep the same error policy. */
static void load_profiles(Database *db)
{
    FILE *f = read_file(db, "profiles.csv");
    char line[1024];
    if (f) {
        while (read_line(f, line, sizeof line)) {
            Profile p = {0};
            char *v[2];
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            if (split_fields(line,v,2) && parse_int(v[0],&p.id) &&
                p.id > 0 && p.id < INT_MAX && data_valid_username(v[1]) &&
                data_find_profile(db, v[1]) < 0 && data_profile_index(db, p.id) < 0 &&
                db->profile_count < MAX_PROFILES) {
                memcpy(p.name,v[1],strlen(v[1])+1);
                db->profiles[db->profile_count++] = p;
                if (p.id >= db->next_profile_id) db->next_profile_id = p.id + 1;
            } else ++db->skipped_lines;
        }
        close_read(db, f);
    }
}
static void load_records(Database *db)
{
    FILE *f = read_file(db, "results.csv");
    char line[1024];
    int i;
    if (f) {
        while (read_line(f, line, sizeof line)) {
            Record r = {0};
            int duplicate = 0;
            int valid;
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            valid=parse_record(line,&r) && r.id>0 && data_record_valid(&r);
            if (valid) {
                /* Reserve IDs even for orphaned records: a new profile must never
                 * inherit someone else's history after profiles.csv is lost. */
                if (r.id>=db->next_record_id) db->next_record_id=r.id+1;
                if (r.profile_id>=db->next_profile_id)
                    db->next_profile_id=r.profile_id<INT_MAX?r.profile_id+1:INT_MAX;
            }
            if (valid &&
                data_profile_index(db, r.profile_id) >= 0 && db->record_count < MAX_RECORDS) {
                for (i = 0; i < db->record_count; ++i) if (db->records[i].id == r.id) { duplicate = 1; break; }
                if (!duplicate) {
                    db->records[db->record_count++] = r;
                } else ++db->skipped_lines;
            } else ++db->skipped_lines;
        }
        close_read(db, f);
    }
}
static void load_settings(Database *db)
{
    FILE *f = read_file(db, "settings.csv");
    char line[1024];
    if (f) {
        int found = 0;
        while (read_line(f, line, sizeof line)) {
            Settings s = {0};
            char *v[2];
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            if (!found && split_fields(line,v,2) && parse_int(v[0],&s.duration) && parse_double(v[1],&s.min_accuracy) &&
                config_valid_duration(s.duration) &&
                s.min_accuracy >= 0 && s.min_accuracy <= 100) { db->settings = s; found = 1; }
            else ++db->skipped_lines;
        }
        close_read(db, f);
    }
}
static void load_achievements(Database *db)
{
    FILE *f = read_file(db, "achievements.csv");
    char line[1024];
    int i;
    if (f) {
        while (read_line(f, line, sizeof line)) {
            int id;
            long long mask;
            char *v[2];
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            if (split_fields(line,v,2) && parse_int(v[0],&id) && parse_integer(v[1],0,(1u<<ACHIEVEMENT_COUNT)-1,&mask) &&
                (i = data_profile_index(db, id)) >= 0)
                db->profiles[i].achievements |= (unsigned int)mask;
            else ++db->skipped_lines;
        }
        close_read(db, f);
    }
}
int data_open(Database *db, const char *directory)
{
    memset(db, 0, sizeof *db);
    db->settings = (Settings){DEFAULT_DURATION, DEFAULT_MIN_ACCURACY};
    db->next_profile_id = db->next_record_id = 1;
    if (!directory || strlen(directory) > MAX_PATH_LEN - 40) {
        message(db, "The data folder path is too long."); return 0;
    }
    snprintf(db->directory, sizeof db->directory, "%s", directory);
    if (!make_directories(directory)) { message(db, "Cannot create the data folder. Choose a writable location."); return 0; }
    db->ready = 1;
    load_profiles(db); /* Records refer to these stable profile IDs. */
    load_records(db);
    load_settings(db);
    load_achievements(db);
    achievements_refresh(db); /* Recover an absent/stale achievement cache from the source records. */
    if (db->ready && db->skipped_lines)
        snprintf(db->message, sizeof db->message, "Skipped %d damaged or unsupported data entries. Original files were preserved.", db->skipped_lines);
    return db->ready;
}
static FILE *append_file(Database *db, const char *name)
{
    char path[MAX_PATH_LEN];
    FILE *f;
    if (!db->ready) { message(db, "Storage is unavailable. Check the data folder and restart."); return NULL; }
    if (!path_for(db, name, path)) return NULL;
    f = fopen(path, "a");
    if (!f) message(db, "Could not save. Check free space and data folder permissions, then retry.");
    return f;
}
static int close_write(Database *db, FILE *f, int written)
{
    int ok = written >= 0;
    if (fflush(f) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok) message(db, "Writing failed. Your result is still on screen; check storage and retry.");
    return ok;
}
int data_create_profile(Database *db, const char *name)
{
    Profile p = {0};
    FILE *f;
    int written;
    if (!data_valid_username(name)) { message(db, "Use 3-20 letters, numbers, underscores or hyphens."); return -1; }
    if (data_find_profile(db, name) >= 0) { message(db, "That username already exists (names ignore letter case)."); return -1; }
    if (db->profile_count >= MAX_PROFILES || db->next_profile_id >= INT_MAX) {
        message(db, "The profile capacity has been reached."); return -1;
    }
    p.id = db->next_profile_id;
    snprintf(p.name, sizeof p.name, "%s", name);
    f = append_file(db, "profiles.csv");
    if (!f) return -1;
    /* Leading newline isolates a previous truncated line after an interrupted write. */
    written = fprintf(f, "\n%d,%s\n", p.id, p.name);
    if (!close_write(db, f, written)) return -1;
    db->profiles[db->profile_count] = p;
    ++db->next_profile_id;
    message(db, "Profile created.");
    return db->profile_count++;
}
Record data_record_from_test(const TypingTest *t, int profile_id, long long timestamp)
{
    Metrics m = typing_metrics(t);
    Record r = {0};
    r.profile_id = profile_id; r.timestamp = timestamp; r.duration = t->duration;
    r.elapsed = m.elapsed; r.wpm = m.wpm; r.accuracy = m.accuracy; r.mistakes = m.mistakes;
    r.completed = t->completed; r.matched = m.matched; r.typed = t->position; r.length = t->length;
    r.correct_keys = t->correct_keys; r.total_keys = t->total_keys;
    return r;
}
int data_append_record(Database *db, Record *r)
{
    FILE *f;
    int i, written;
    if (!data_record_valid(r) || data_profile_index(db, r->profile_id) < 0) {
        message(db, "This result is invalid or has zero elapsed time and cannot be saved."); return 0;
    }
    if (r->id > 0) for (i = 0; i < db->record_count; ++i)
        if (db->records[i].id == r->id) return 1; /* Idempotent UI retries. */
    if (db->record_count >= MAX_RECORDS || db->next_record_id >= INT_MAX) {
        message(db, "History is full. Back up your data folder before starting a fresh history."); return 0;
    }
    if (!r->id) r->id = db->next_record_id;
    f = append_file(db, "results.csv");
    if (!f) return 0;
    written = fprintf(f, "\n%d,%d,%lld,%d,%.17g,%.17g,%.17g,%d,%d,%d,%d,%d,%d,%d\n",
        r->id, r->profile_id, r->timestamp, r->duration, r->elapsed, r->wpm, r->accuracy,
        r->mistakes, r->completed, r->matched, r->typed, r->length, r->correct_keys, r->total_keys);
    if (!close_write(db, f, written)) return 0;
    db->records[db->record_count++] = *r;
    db->next_record_id = r->id + 1;
    achievements_refresh(db);
    message(db, "Result saved to your local history.");
    if (!data_save_achievements(db))
        message(db, "Result saved. Achievement cache could not be saved; it will recover from history.");
    return 1;
}
static int replace_file(Database *db, const char *temporary, const char *destination)
{
#ifdef _WIN32
    int ok = MoveFileExA(temporary, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    int ok = rename(temporary, destination) == 0;
#endif
    if (!ok) message(db, "Could not replace the settings file. The previous file was preserved.");
    return ok;
}
int data_save_settings(Database *db, Settings settings)
{
    char path[MAX_PATH_LEN], temp[MAX_PATH_LEN];
    FILE *f;
    int written;
    if (!db->ready || !config_valid_duration(settings.duration) || !isfinite(settings.min_accuracy) ||
        settings.min_accuracy < 0 || settings.min_accuracy > 100) {
        message(db, "Settings are invalid or storage is unavailable."); return 0;
    }
    if (!path_for(db, "settings.csv", path) || !path_for(db, "settings.tmp", temp)) return 0;
    f = fopen(temp, "w");
    if (!f) { message(db, "Could not save settings. Check folder permissions."); return 0; }
    written = fprintf(f, "# Keyline settings v1: duration,minimum_accuracy\n%d,%.2f\n", settings.duration, settings.min_accuracy);
    if (!close_write(db, f, written) || !replace_file(db, temp, path)) return 0;
    db->settings = settings;
    message(db, "Settings saved.");
    return 1;
}
int data_save_achievements(Database *db)
{
    char path[MAX_PATH_LEN], temp[MAX_PATH_LEN];
    FILE *f;
    int i, written = 0;
    if (!db->ready) return 0;
    if (!path_for(db, "achievements.csv", path) || !path_for(db, "achievements.tmp", temp)) return 0;
    f = fopen(temp, "w");
    if (!f) return 0;
    for (i = 0; i < db->profile_count; ++i)
        if (fprintf(f, "%d,%u\n", db->profiles[i].id, db->profiles[i].achievements) < 0) written = -1;
    return close_write(db, f, written) && replace_file(db, temp, path);
}
int data_record_better(const Record *a, const Record *b)
{
    if (a->wpm != b->wpm) return a->wpm > b->wpm;
    if (a->accuracy != b->accuracy) return a->accuracy > b->accuracy;
    if (a->mistakes != b->mistakes) return a->mistakes < b->mistakes;
    return a->id < b->id;
}
int data_personal_best_better(const Record *a, const Record *b)
{
    return data_record_better(a,b);
}
int data_leaderboard_better(const Record *a, const Record *b)
{
    /* Edit board-only tie rules here; personal-best selection has its own entry. */
    return data_record_better(a,b);
}
int data_record_eligible(const Record *record, int duration, double min_accuracy)
{
    double minimum_elapsed=duration<RANK_MIN_SECONDS?duration:RANK_MIN_SECONDS;
    return record->duration==duration && record->accuracy>=min_accuracy &&
        record->elapsed>=minimum_elapsed;
}
UserSummary data_summary(const Database *db, int profile_id)
{
    UserSummary s = {0};
    int i;
    s.latest = s.previous = s.best = -1;
    for (i = 0; i < db->record_count; ++i) {
        const Record *r = &db->records[i];
        if (r->profile_id != profile_id) continue;
        s.previous = s.latest; s.latest = i; ++s.count;
        s.average_wpm += r->wpm; s.average_accuracy += r->accuracy;
        if (s.best < 0 || data_personal_best_better(r, &db->records[s.best])) s.best = i;
        if (r->accuracy > s.best_accuracy) s.best_accuracy = r->accuracy;
    }
    if (s.count) { s.average_wpm /= s.count; s.average_accuracy /= s.count; }
    return s;
}
int data_leaderboard(const Database *db, int duration, LeaderEntry *entries, int capacity)
{
    int p, i, j, count = 0;
    for (p = 0; p < db->profile_count && count < capacity; ++p) {
        int best = -1;
        for (i = 0; i < db->record_count; ++i) {
            const Record *r = &db->records[i];
            if (r->profile_id != db->profiles[p].id ||
                !data_record_eligible(r,duration,db->settings.min_accuracy)) continue;
            if (best < 0 || data_leaderboard_better(r, &db->records[best])) best = i;
        }
        if (best >= 0) entries[count++] = (LeaderEntry){p, best};
    }
    /* Insertion sort: at most MAX_PROFILES entries, easy to explain in class. */
    for (i = 1; i < count; ++i) {
        LeaderEntry key = entries[i];
        j = i - 1;
        while (j >= 0 && data_leaderboard_better(&db->records[key.record_index], &db->records[entries[j].record_index])) {
            entries[j + 1] = entries[j]; --j;
        }
        entries[j + 1] = key;
    }
    return count;
}
void data_format_date(long long timestamp, char *out, int capacity)
{
    time_t value = (time_t)timestamp;
    struct tm *local = localtime(&value);
    if (!local || !strftime(out, (size_t)capacity, "%d %b %Y  %H:%M", local))
        snprintf(out, (size_t)capacity, "Unknown date");
}
