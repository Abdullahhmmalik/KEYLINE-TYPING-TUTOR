/* Run against copied, edited configuration: no production defaults assumed. */
#include "achievements.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Database db, loaded;
static int checks, failures;
#define CHECK(e) do { ++checks; if (!(e)) { ++failures; \
    fprintf(stderr,"CONFIG FAIL %d: %s\n",__LINE__,#e); } } while (0)
#define NEAR(a,b) CHECK(fabs((double)(a)-(double)(b))<0.000001)

static void test_durations(int expected_presets)
{
    char text[40];
    int i, duration=-1;
    const int boundaries[]={MIN_DURATION,DEFAULT_DURATION,MAX_DURATION};
    CHECK(MIN_DURATION>0 && MAX_DURATION>=MIN_DURATION);
    CHECK(config_valid_duration(DEFAULT_DURATION));
    for (i=0;i<3;++i) {
        CHECK(config_valid_duration(boundaries[i]));
        snprintf(text,sizeof text,"%d",boundaries[i]);
        CHECK(config_parse_duration(text,&duration));
        CHECK(duration==boundaries[i]);
    }
    CHECK(!config_valid_duration(MIN_DURATION-1));
    CHECK(!config_valid_duration(MAX_DURATION+1));
    snprintf(text,sizeof text,"%d",MIN_DURATION-1);
    duration=-1; CHECK(!config_parse_duration(text,&duration)); CHECK(duration==-1);
    snprintf(text,sizeof text,"%d",MAX_DURATION+1);
    CHECK(!config_parse_duration(text,&duration)); CHECK(duration==-1);
    snprintf(text,sizeof text,"%ds",DEFAULT_DURATION);
    CHECK(!config_parse_duration(text,&duration));
    CHECK(DURATION_PRESET_COUNT==expected_presets);
    for (i=0;i<DURATION_PRESET_COUNT;++i) CHECK(config_valid_duration(DURATION_PRESETS[i]));
}

static void test_settings(const char *directory)
{
    Settings settings;
    CHECK(data_open(&db,directory));
    CHECK(db.ready && db.profile_count==0 && db.record_count==0);
    CHECK(db.settings.duration==DEFAULT_DURATION);
    NEAR(db.settings.min_accuracy,DEFAULT_MIN_ACCURACY);
    settings=db.settings; settings.duration=MAX_DURATION;
    CHECK(data_save_settings(&db,settings));
    CHECK(data_open(&loaded,directory));
    CHECK(loaded.settings.duration==MAX_DURATION);
    NEAR(loaded.settings.min_accuracy,DEFAULT_MIN_ACCURACY);
}

static void test_eligibility(void)
{
    Record record={0};
    double elapsed=DEFAULT_DURATION<RANK_MIN_SECONDS?DEFAULT_DURATION:RANK_MIN_SECONDS;
    record.duration=DEFAULT_DURATION; record.accuracy=DEFAULT_MIN_ACCURACY;
    record.elapsed=elapsed;
    CHECK(data_record_eligible(&record,DEFAULT_DURATION,DEFAULT_MIN_ACCURACY));
    record.accuracy=DEFAULT_MIN_ACCURACY-0.01;
    CHECK(!data_record_eligible(&record,DEFAULT_DURATION,DEFAULT_MIN_ACCURACY));
    record.accuracy=DEFAULT_MIN_ACCURACY; record.elapsed=elapsed-0.001;
    CHECK(!data_record_eligible(&record,DEFAULT_DURATION,DEFAULT_MIN_ACCURACY));
    record.elapsed=elapsed;
    CHECK(!data_record_eligible(&record,DEFAULT_DURATION+1,DEFAULT_MIN_ACCURACY));
    record.duration=MIN_DURATION;
    record.elapsed=MIN_DURATION<RANK_MIN_SECONDS?MIN_DURATION:RANK_MIN_SECONDS;
    CHECK(data_record_eligible(&record,MIN_DURATION,DEFAULT_MIN_ACCURACY));
    record.elapsed-=0.001;
    CHECK(!data_record_eligible(&record,MIN_DURATION,DEFAULT_MIN_ACCURACY));
}

static void test_achievements(void)
{
    char description[160], expected[80], short_text[5];
    unsigned int mask;
    int i;
    memset(&db,0,sizeof db);
    db.record_count=1; db.records[0].profile_id=7;
    db.records[0].wpm=(double)ACH_SPEED_ONE-0.01;
    CHECK(!(achievements_evaluate(&db,7)&(1u<<1)));
    db.records[0].wpm=ACH_SPEED_ONE;
    CHECK(achievements_evaluate(&db,7)&(1u<<1));
    db.records[0].wpm=(double)ACH_SPEED_TWO-0.01;
    CHECK(!(achievements_evaluate(&db,7)&(1u<<2)));
    db.records[0].wpm=ACH_SPEED_TWO;
    CHECK(achievements_evaluate(&db,7)&(1u<<2));
    db.records[0].accuracy=(double)ACH_ACCURACY-0.01;
    CHECK(!(achievements_evaluate(&db,7)&(1u<<3)));
    db.records[0].accuracy=ACH_ACCURACY;
    CHECK(achievements_evaluate(&db,7)&(1u<<3));
    CHECK(ACH_TEST_COUNT>0 && ACH_TEST_COUNT<=MAX_RECORDS);
    if (ACH_TEST_COUNT>0 && ACH_TEST_COUNT<=MAX_RECORDS) {
        memset(&db,0,sizeof db);
        for (i=0;i<ACH_TEST_COUNT;++i) db.records[i].profile_id=7;
        db.record_count=ACH_TEST_COUNT-1; mask=achievements_evaluate(&db,7);
        CHECK(!(mask&(1u<<4)));
        db.record_count=ACH_TEST_COUNT; mask=achievements_evaluate(&db,7);
        CHECK(mask&(1u<<4));
    }
    achievements_description(1,description,sizeof description);
    snprintf(expected,sizeof expected,"%g WPM",(double)ACH_SPEED_ONE);
    CHECK(strstr(description,expected)!=NULL);
    achievements_description(2,description,sizeof description);
    snprintf(expected,sizeof expected,"%g WPM",(double)ACH_SPEED_TWO);
    CHECK(strstr(description,expected)!=NULL);
    achievements_description(3,description,sizeof description);
    snprintf(expected,sizeof expected,"%g%% accuracy",(double)ACH_ACCURACY);
    CHECK(strstr(description,expected)!=NULL);
    achievements_description(4,description,sizeof description);
    snprintf(expected,sizeof expected,"%d typing tests",ACH_TEST_COUNT);
    CHECK(strstr(description,expected)!=NULL);
    memset(short_text,'x',sizeof short_text);
    achievements_description(1,short_text,sizeof short_text);
    CHECK(short_text[sizeof short_text-1]==0);
    achievements_description(-1,description,sizeof description); CHECK(description[0]==0);
}

static void test_exercises(void)
{
    TypingTest test;
    int i;
    CHECK(EXERCISE_COUNT>0);
    for (i=0;i<EXERCISE_COUNT;++i) {
        const unsigned char *p=(const unsigned char *)EXERCISES[i].text;
        int ascii=1;
        CHECK(EXERCISES[i].title && EXERCISES[i].title[0]);
        CHECK(p && *p && strlen((const char *)p)<=MAX_PASSAGE);
        if (!p) continue;
        for (;*p;++p) if (*p<32 || *p>126) ascii=0;
        CHECK(ascii);
        CHECK(typing_init(&test,EXERCISES[i].text,DEFAULT_DURATION));
    }
}

int main(int argc, char **argv)
{
    if (argc!=3) { fprintf(stderr,"Usage: config_tests DATA_DIR EXPECTED_PRESET_COUNT\n"); return 2; }
    test_durations(atoi(argv[2])); test_settings(argv[1]); test_eligibility();
    test_achievements(); test_exercises();
    printf("Configuration variant: %d passed, %d failed (%d presets). Fixture: %s\n",
        checks-failures,failures,DURATION_PRESET_COUNT,argv[1]);
    return failures?1:0;
}
