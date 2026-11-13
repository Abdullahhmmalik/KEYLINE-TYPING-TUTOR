#include "typing.h"
#include "data.h"
#include "achievements.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#include <process.h>
#define process_id _getpid
#else
#include <unistd.h>
#define process_id getpid
#endif

static int checks, failures;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    ++failures; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expression); } } while (0)
#define NEAR(a,b) CHECK(fabs((a)-(b)) < 0.00001)
static Database db, loaded;

static Record make_record(int profile, int duration, double elapsed, int matched, int correct, int total)
{
    Record r={0};
    Metrics m=stats_calculate(matched,matched,matched,correct,total,elapsed,duration);
    r.profile_id=profile; r.timestamp=1793880000LL; r.duration=duration;
    r.elapsed=elapsed; r.wpm=m.wpm; r.accuracy=m.accuracy; r.mistakes=m.mistakes;
    r.completed=1; r.matched=r.typed=r.length=matched;
    r.correct_keys=correct; r.total_keys=total;
    return r;
}
static void test_stats(void)
{
    Metrics m=stats_calculate(250,260,500,270,300,60,120);
    NEAR(m.wpm,50); NEAR(m.accuracy,90); CHECK(m.mistakes==30);
    NEAR(m.progress,52); NEAR(m.elapsed,60); NEAR(m.remaining,60);
    m=stats_calculate(0,0,0,0,0,0,60);
    NEAR(m.wpm,0); NEAR(m.accuracy,0); NEAR(m.progress,0); CHECK(isfinite(m.wpm));
    m=stats_calculate(1,1,1,1,1,0.001,15); CHECK(isfinite(m.wpm)); NEAR(m.wpm,12000);
    m=stats_calculate(-10,-10,-10,-10,-10,NAN,-1);
    NEAR(m.wpm,0); NEAR(m.accuracy,0); NEAR(m.remaining,0);
    m=stats_calculate(300,400,500,300,400,200,60); NEAR(m.elapsed,60); NEAR(m.wpm,60);
}
static void test_typing(void)
{
    TypingTest t;
    Metrics m;
    char long_text[MAX_PASSAGE+2];
    int i, d;
    CHECK(typing_init(&t,"abc",15));
    CHECK(!typing_character(&t,10,0)); CHECK(!t.started);
    CHECK(!typing_character(&t,233,0)); CHECK(!t.started);
    CHECK(typing_character(&t,'a',10)); CHECK(t.started); NEAR(t.elapsed,0);
    CHECK(typing_character(&t,'x',11));
    m=typing_metrics(&t); NEAR(m.accuracy,50); CHECK(m.mistakes==1); NEAR(m.wpm,12);
    typing_backspace(&t,12); CHECK(t.position==1); CHECK(t.total_keys==2);
    CHECK(typing_character(&t,'b',13)); CHECK(typing_character(&t,'c',14));
    CHECK(t.finished && t.completed); CHECK(t.position==3);
    m=typing_metrics(&t); NEAR(m.accuracy,75); NEAR(m.wpm,9); CHECK(m.mistakes==1);
    CHECK(!typing_character(&t,'d',15)); typing_backspace(&t,16); CHECK(t.position==3);
    CHECK(typing_init(&t,"abc",5)); CHECK(!t.started && !t.finished && t.position==0);
    typing_update(&t,1000); CHECK(!t.finished); NEAR(t.elapsed,0);
    typing_character(&t,'x',1000); typing_update(&t,1004.999); CHECK(!t.finished);
    CHECK(!typing_character(&t,'b',1005)); CHECK(t.finished && !t.completed); NEAR(t.elapsed,5);
    CHECK(typing_init(&t,"abc",5)); typing_character(&t,'a',20); typing_backspace(&t,21);
    CHECK(t.started && t.position==0); typing_update(&t,26); CHECK(t.finished); NEAR(t.elapsed,5);
    CHECK(typing_init(&t,"A b!",30));
    for (i=0;i<4;++i) typing_character(&t,"A b!"[i],i+1);
    CHECK(t.completed); NEAR(typing_metrics(&t).accuracy,100);
    CHECK(typing_init(&t,"abc",60)); typing_character(&t,'x',1); typing_character(&t,'y',2); typing_character(&t,'z',3);
    CHECK(t.completed); NEAR(typing_metrics(&t).wpm,0); NEAR(typing_metrics(&t).accuracy,0);
    memset(long_text,'a',sizeof long_text); long_text[sizeof long_text-1]=0;
    CHECK(!typing_init(&t,long_text,60)); long_text[MAX_PASSAGE]=0;
    CHECK(typing_init(&t,long_text,60));
    for (i=0;i<MAX_PASSAGE+100;++i) typing_character(&t,'a',i*0.01);
    CHECK(t.position==MAX_PASSAGE && t.typed[MAX_PASSAGE]==0);
    CHECK(!typing_init(&t,"",60)); CHECK(!typing_init(&t,"line\nbreak",60));
    CHECK(!typing_init(&t,"abc",0)); CHECK(!typing_init(&t,"abc",MAX_DURATION+1));
    CHECK(config_parse_duration("15",&d) && d==15); CHECK(config_parse_duration("600",&d));
    CHECK(!config_parse_duration("",&d)); CHECK(!config_parse_duration("-1",&d));
    CHECK(!config_parse_duration("20x",&d)); CHECK(!config_parse_duration("1.5",&d));
    CHECK(!config_parse_duration("99999999999999999999999",&d)); CHECK(!config_parse_duration("4",&d));
    for (i=0;i<EXERCISE_COUNT;++i) CHECK(typing_init(&t,EXERCISES[i].text,DEFAULT_DURATION));
}
static void test_storage(const char *directory)
{
    int alice,bob,carol,i,n;
    char path[MAX_PATH_LEN+32];
    FILE *f;
    Record r, invalid;
    UserSummary s;
    LeaderEntry board[MAX_PROFILES];
    CHECK(data_open(&db,directory)); CHECK(db.profile_count==0 && db.record_count==0);
    CHECK(db.settings.duration==DEFAULT_DURATION);
    CHECK(!data_valid_username("ab")); CHECK(!data_valid_username("has space"));
    CHECK(!data_valid_username("bad,name")); CHECK(!data_valid_username("averyveryverylongusername"));
    CHECK(!data_valid_username("\xc3\xa9" "clair")); CHECK(data_valid_username("User-123_ok"));
    alice=data_create_profile(&db,"Alice"); CHECK(alice==0);
    CHECK(data_create_profile(&db,"aLiCe")==-1); CHECK(data_create_profile(&db,"bad name")==-1);
    bob=data_create_profile(&db,"Bob"); carol=data_create_profile(&db,"Carol");
    CHECK(bob==1 && carol==2); CHECK(data_find_profile(&db,"ALICE")==alice);
    CHECK(data_open(&loaded,directory)); CHECK(loaded.profile_count==3);
    CHECK(strcmp(loaded.profiles[0].name,"Alice")==0);
    r=make_record(db.profiles[alice].id,60,60,150,150,150);
    CHECK(data_record_valid(&r)); CHECK(data_append_record(&db,&r));
    CHECK(data_append_record(&db,&r)); CHECK(db.record_count==1);
    s=data_summary(&db,db.profiles[alice].id); CHECK(s.count==1); NEAR(s.average_wpm,30);
    CHECK(db.profiles[alice].achievements & 1u); CHECK(db.profiles[alice].achievements & (1u<<1));
    CHECK(db.profiles[alice].achievements & (1u<<3));
    r=make_record(db.profiles[bob].id,60,60,250,250,250); CHECK(data_append_record(&db,&r));
    r=make_record(db.profiles[alice].id,60,60,250,250,300); CHECK(data_append_record(&db,&r));
    r=make_record(db.profiles[carol].id,60,60,250,250,250); CHECK(data_append_record(&db,&r));
    n=data_leaderboard(&db,60,board,MAX_PROFILES); CHECK(n==3);
    CHECK(board[0].profile_index==bob); CHECK(board[1].profile_index==carol); CHECK(board[2].profile_index==alice);
    CHECK(data_leaderboard(&db,15,board,MAX_PROFILES)==0);
    r=make_record(db.profiles[alice].id,15,15,150,150,150); CHECK(data_append_record(&db,&r));
    n=data_leaderboard(&db,15,board,MAX_PROFILES); CHECK(n==1 && board[0].profile_index==alice);
    r=make_record(db.profiles[bob].id,15,1,150,150,150); CHECK(data_append_record(&db,&r));
    n=data_leaderboard(&db,15,board,MAX_PROFILES); CHECK(n==1); /* Too brief to rank. */
    r=make_record(db.profiles[alice].id,60,60,300,300,300); CHECK(data_append_record(&db,&r));
    r=make_record(db.profiles[alice].id,60,60,310,310,310); CHECK(data_append_record(&db,&r));
    CHECK(achievements_count(db.profiles[alice].achievements)==ACHIEVEMENT_COUNT);
    CHECK(achievements_count(db.profiles[carol].achievements)<ACHIEVEMENT_COUNT);
    s=data_summary(&db,db.profiles[alice].id); CHECK(s.count==5); CHECK(s.previous>=0);
    NEAR(s.best_accuracy,100); NEAR(db.records[s.best].wpm,120);
    invalid=r; invalid.id=0; invalid.accuracy=NAN; CHECK(!data_append_record(&db,&invalid));
    invalid=r; invalid.id=0; invalid.elapsed=0; CHECK(!data_append_record(&db,&invalid));
    invalid=r; invalid.id=0; invalid.total_keys=-1; CHECK(!data_record_valid(&invalid));
    invalid=r; invalid.id=0; invalid.wpm=999; CHECK(!data_record_valid(&invalid));
    invalid=r; invalid.id=0; invalid.profile_id=999; CHECK(!data_append_record(&db,&invalid));
    CHECK(data_save_settings(&db,(Settings){37,90})); CHECK(db.settings.duration==37);
    CHECK(!data_save_settings(&db,(Settings){0,90})); CHECK(!data_save_settings(&db,(Settings){60,101}));
    CHECK(data_open(&loaded,directory)); CHECK(loaded.record_count==db.record_count);
    CHECK(loaded.settings.duration==37); NEAR(loaded.settings.min_accuracy,90);
    CHECK(loaded.profiles[alice].achievements==db.profiles[alice].achievements);
    NEAR(loaded.records[0].wpm,30); CHECK(loaded.records[0].duration==60);
    CHECK(loaded.records[0].timestamp==1793880000LL);
    CHECK(data_leaderboard(&loaded,60,board,MAX_PROFILES)==3);
    CHECK(data_save_settings(&loaded,(Settings){60,100}));
    CHECK(data_leaderboard(&loaded,60,board,MAX_PROFILES)==3);
    r=make_record(db.profiles[bob].id,60,0.000123456789,150,150,150);
    CHECK(data_append_record(&db,&r)); CHECK(data_open(&loaded,directory));
    NEAR(loaded.records[loaded.record_count-1].elapsed,r.elapsed);
    NEAR(loaded.records[loaded.record_count-1].wpm,r.wpm);
    /* A damaged/truncated entry must not prevent loading or appending valid records. */
    snprintf(path,sizeof path,"%s/results.csv",directory); f=fopen(path,"a"); CHECK(f!=NULL);
    if (f) {
        fputs("\ninvalid\n1,broken\n99999999999999999999999999999,1,1,1,1,1,1,1,1,1,1,1,1,1\n",f);
        for (i=0;i<3000;++i) fputc('x',f);
        fputs("\n999,1,1793880000,60,nan,0,0,0,0,0,0,2,0,1\ntruncated",f); fclose(f);
    }
    CHECK(data_open(&loaded,directory)); CHECK(loaded.record_count==db.record_count); CHECK(loaded.skipped_lines>=5);
    r=make_record(loaded.profiles[bob].id,5,5,50,50,50); CHECK(data_append_record(&loaded,&r));
    CHECK(data_open(&db,directory)); CHECK(db.record_count==loaded.record_count);
    CHECK(data_leaderboard(&db,5,board,MAX_PROFILES)==1);
    /* Removing derived cache can be recovered from the authoritative history. */
    snprintf(path,sizeof path,"%s/achievements.csv",directory); CHECK(remove(path)==0);
    CHECK(data_open(&loaded,directory)); CHECK(achievements_count(loaded.profiles[alice].achievements)==ACHIEVEMENT_COUNT);
    i=loaded.profile_count; loaded.profile_count=MAX_PROFILES;
    CHECK(data_create_profile(&loaded,"CapacityCheck")==-1); loaded.profile_count=i;
    i=loaded.record_count; loaded.record_count=MAX_RECORDS;
    r=make_record(loaded.profiles[0].id,60,60,100,100,100);
    CHECK(!data_append_record(&loaded,&r)); loaded.record_count=i;
    snprintf(path,sizeof path,"%s/profiles.csv",directory); f=fopen(path,"a"); CHECK(f!=NULL);
    if (f) { fputs("\n999999999999999999999999999999,Overflow\n88,Alice\n89,bad name\n",f); fclose(f); }
    CHECK(data_open(&loaded,directory)); CHECK(loaded.profile_count==3);
    /* A file path cannot serve as a data directory; this must fail safely. */
    snprintf(path,sizeof path,"%s/not-a-directory",directory); f=fopen(path,"w");
    CHECK(f!=NULL); if (f) { fputs("fixture",f); fclose(f); }
    CHECK(!data_open(&loaded,path)); CHECK(data_create_profile(&loaded,"NewUser")==-1);
}
static void test_comparator(void)
{
    Record a=make_record(1,60,60,100,100,100), b=a;
    a.id=1; b.id=2;
    CHECK(data_record_better(&a,&b)); CHECK(!data_record_better(&b,&a));
    b.wpm=21; CHECK(data_record_better(&b,&a)); b.wpm=a.wpm;
    b.accuracy=99; CHECK(data_record_better(&a,&b)); b.accuracy=a.accuracy;
    b.mistakes=1; CHECK(data_record_better(&a,&b));
    CHECK(!data_record_better(&a,&a));
}
static void test_missing_profiles(const char *base)
{
    char directory[MAX_PATH_LEN+16],path[MAX_PATH_LEN+32];
    Record r;
    int old_id;
    snprintf(directory,sizeof directory,"%s-orphans",base);
    CHECK(data_open(&db,directory)); CHECK(data_create_profile(&db,"Original")==0);
    old_id=db.profiles[0].id;
    r=make_record(old_id,60,60,100,100,100); CHECK(data_append_record(&db,&r));
    snprintf(path,sizeof path,"%s/profiles.csv",directory); CHECK(remove(path)==0);
    CHECK(data_open(&loaded,directory)); CHECK(loaded.profile_count==0 && loaded.record_count==0);
    CHECK(data_create_profile(&loaded,"Replacement")==0);
    CHECK(loaded.profiles[0].id!=old_id);
    CHECK(data_open(&db,directory)); CHECK(db.record_count==0); CHECK(db.skipped_lines>0);
}
static void test_legacy_format(const char *base)
{
    const char *legacy="7,1,1793880000,60,60.000000000,50.000000000,90.000000000,30,1,250,250,250,270,300\n";
    char directory[MAX_PATH_LEN+16],path[MAX_PATH_LEN+32],line[256];
    Record r;
    FILE *f;
    snprintf(directory,sizeof directory,"%s-legacy",base);
    CHECK(data_open(&db,directory)); CHECK(data_create_profile(&db,"LegacyUser")==0);
    snprintf(path,sizeof path,"%s/results.csv",directory);
    f=fopen(path,"w"); CHECK(f!=NULL); if (f) { fputs(legacy,f); fclose(f); }
    CHECK(data_open(&loaded,directory)); CHECK(loaded.record_count==1 && loaded.next_record_id==8);
    NEAR(loaded.records[0].wpm,50); NEAR(loaded.records[0].accuracy,90);
    r=make_record(1,30,30,150,150,150); CHECK(data_append_record(&loaded,&r)); CHECK(r.id==8);
    f=fopen(path,"r"); CHECK(f!=NULL);
    if (f) { CHECK(fgets(line,sizeof line,f)!=NULL); CHECK(strcmp(line,legacy)==0); fclose(f); }
    CHECK(data_open(&db,directory)); CHECK(db.record_count==2);
    snprintf(path,sizeof path,"%s/settings.csv",directory);
    f=fopen(path,"w"); CHECK(f!=NULL); if (f) { fputs("15,101\n37,85\n",f); fclose(f); }
    CHECK(data_open(&db,directory)); CHECK(db.settings.duration==37); NEAR(db.settings.min_accuracy,85);
    CHECK(db.skipped_lines==1);
}
static void test_ranking_policy(void)
{
    LeaderEntry entries[MAX_PROFILES];
    int i,n;
    memset(&db,0,sizeof db);
    db.profile_count=3; db.settings.min_accuracy=80;
    for (i=0;i<3;++i) db.profiles[i].id=i+1;
    /* All three records are 50 WPM. B has fewer errors than A at the same
     * accuracy; C wins on accuracy. Extra correct keys represent corrections. */
    db.records[0]=make_record(1,60,60,250,270,300);
    db.records[1]=make_record(2,60,12,50,90,100);
    db.records[2]=make_record(3,60,60,250,250,250);
    db.records[3]=make_record(1,60,9,150,150,150); /* Too brief to rank. */
    db.records[4]=make_record(2,30,30,300,300,300); /* A different duration. */
    db.records[5]=make_record(1,60,60,500,500,1000); /* Too inaccurate. */
    db.record_count=6;
    for (i=0;i<db.record_count;++i) { db.records[i].id=i+1; CHECK(data_record_valid(&db.records[i])); }
    n=data_leaderboard(&db,60,entries,MAX_PROFILES); CHECK(n==3);
    CHECK(entries[0].profile_index==2); CHECK(entries[1].profile_index==1); CHECK(entries[2].profile_index==0);
    CHECK(entries[2].record_index==0);
    db.settings.min_accuracy=95;
    n=data_leaderboard(&db,60,entries,MAX_PROFILES); CHECK(n==1 && entries[0].profile_index==2);
    CHECK(data_leaderboard(&db,30,entries,MAX_PROFILES)==1);
    CHECK(data_leaderboard(&db,15,entries,MAX_PROFILES)==0);
    CHECK(data_leaderboard(&db,60,entries,0)==0);
}
static void test_feature_policies(void)
{
    Record a=make_record(1,60,60,250,250,250),b=a;
    LeaderEntry entries[MAX_PROFILES];
    UserSummary personal;
    a.id=1; b.id=2;
    /* Both feature entry points retain every original default tie rule. */
    CHECK(data_personal_best_better(&a,&b)); CHECK(data_leaderboard_better(&a,&b));
    b.wpm=51;
    CHECK(data_personal_best_better(&b,&a)); CHECK(data_leaderboard_better(&b,&a));
    b.wpm=a.wpm; b.accuracy=99;
    CHECK(data_personal_best_better(&a,&b)); CHECK(data_leaderboard_better(&a,&b));
    b.accuracy=a.accuracy; b.mistakes=1;
    CHECK(data_personal_best_better(&a,&b)); CHECK(data_leaderboard_better(&a,&b));
    CHECK(!data_personal_best_better(&a,&a)); CHECK(!data_leaderboard_better(&a,&a));
    /* Eligibility is separate from raw ranking/personal-best calculations. */
    CHECK(data_record_eligible(&a,60,100));
    a.elapsed=9.999; CHECK(!data_record_eligible(&a,60,80));
    a.elapsed=10; CHECK(data_record_eligible(&a,60,80));
    a.accuracy=79.999; CHECK(!data_record_eligible(&a,60,80));
    a.accuracy=80; CHECK(data_record_eligible(&a,60,80));
    CHECK(!data_record_eligible(&a,30,80));
    a.duration=5; a.elapsed=4.999; CHECK(!data_record_eligible(&a,5,80));
    a.elapsed=5; CHECK(data_record_eligible(&a,5,80));
    memset(&db,0,sizeof db);
    db.profile_count=1; db.profiles[0].id=1; db.settings.min_accuracy=80;
    db.records[0]=make_record(1,60,60,250,250,300); db.records[0].id=1;
    db.records[1]=make_record(1,60,60,200,200,200); db.records[1].id=2;
    db.record_count=2;
    personal=data_summary(&db,1);
    CHECK(personal.best==0);
    CHECK(data_leaderboard(&db,60,entries,MAX_PROFILES)==1);
    CHECK(entries[0].record_index==0);
    /* A board eligibility edit must not filter personal best/history. */
    db.settings.min_accuracy=100;
    CHECK(data_leaderboard(&db,60,entries,MAX_PROFILES)==1);
    CHECK(entries[0].record_index==1);
    personal=data_summary(&db,1);
    CHECK(personal.best==0 && personal.count==2);
}
static void test_achievement_descriptions(void)
{
    char text[128],expected[128],small[5];
    achievements_description(1,text,sizeof text);
    snprintf(expected,sizeof expected,"Reach %.15g WPM in a finished test.",(double)ACH_SPEED_ONE);
    CHECK(strcmp(text,expected)==0);
    achievements_description(2,text,sizeof text);
    snprintf(expected,sizeof expected,"Reach %.15g WPM in a finished test.",(double)ACH_SPEED_TWO);
    CHECK(strcmp(text,expected)==0);
    achievements_description(3,text,sizeof text);
    snprintf(expected,sizeof expected,"Finish a test with %.15g%% accuracy.",(double)ACH_ACCURACY);
    CHECK(strcmp(text,expected)==0);
    achievements_description(4,text,sizeof text);
    snprintf(expected,sizeof expected,"Finish %d typing tests.",ACH_TEST_COUNT);
    CHECK(strcmp(text,expected)==0);
    achievements_description(0,text,sizeof text); CHECK(strcmp(text,ACHIEVEMENTS[0].description)==0);
    achievements_description(-1,text,sizeof text); CHECK(text[0]==0);
    achievements_description(ACHIEVEMENT_COUNT,text,sizeof text); CHECK(text[0]==0);
    achievements_description(1,small,sizeof small); CHECK(strcmp(small,"Reac")==0);
    small[0]='X'; achievements_description(1,small,0); CHECK(small[0]=='X');
    achievements_description(1,NULL,0);
}
int main(int argc, char **argv)
{
    char directory[MAX_PATH_LEN];
    if (argc>1 && strlen(argv[1])>MAX_PATH_LEN-90) return EXIT_FAILURE;
    snprintf(directory,sizeof directory,"%s-%lld-%d",argc>1?argv[1]:"test-data",(long long)time(NULL),(int)process_id());
    test_stats(); test_typing(); test_comparator(); test_storage(directory);
    test_missing_profiles(directory); test_legacy_format(directory); test_ranking_policy(); test_feature_policies();
    test_achievement_descriptions();
    printf("Core checks: %d passed, %d failed (%d total).\n",checks-failures,failures,checks);
    printf("Test fixtures: %s (retained for inspection).\n",directory);
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
