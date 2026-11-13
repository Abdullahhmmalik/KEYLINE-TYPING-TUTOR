#include "achievements.h"
#include <stdio.h>

/* Descriptions are formatted from the same thresholds used for evaluation. */
const AchievementInfo ACHIEVEMENTS[ACHIEVEMENT_COUNT] = {
    {"First steps", "Finish your first test."},
    {"Finding your rhythm", "Reach the first speed milestone."},
    {"Picking up speed", "Reach the second speed milestone."},
    {"Precision matters", "Reach the accuracy milestone."},
    {"Showing up", "Build a consistent practice habit."},
    {"A little better", "Improve on your previous test's WPM."}
};
/* Keep displayed conditions next to their evaluation, including fractional
 * thresholds. Indices are persistent achievement bits: never reorder them. */
void achievements_description(int index, char *text, size_t size)
{
    if (!text || !size) return;
    if (index<0 || index>=ACHIEVEMENT_COUNT) { text[0]=0; return; }
    switch (index) {
        case 1: snprintf(text,size,"Reach %.15g WPM in a finished test.",(double)ACH_SPEED_ONE); break;
        case 2: snprintf(text,size,"Reach %.15g WPM in a finished test.",(double)ACH_SPEED_TWO); break;
        case 3: snprintf(text,size,"Finish a test with %.15g%% accuracy.",(double)ACH_ACCURACY); break;
        case 4: snprintf(text,size,"Finish %d typing tests.",ACH_TEST_COUNT); break;
        default: snprintf(text,size,"%s",ACHIEVEMENTS[index].description); break;
    }
}
unsigned int achievements_evaluate(const Database *db, int profile_id)
{
    unsigned int mask = 0;
    int i, count = 0, previous = -1;
    for (i = 0; i < db->record_count; ++i) {
        const Record *r = &db->records[i];
        if (r->profile_id != profile_id) continue;
        ++count;
        mask |= 1u;
        if (r->wpm >= ACH_SPEED_ONE) mask |= 1u << 1;
        if (r->wpm >= ACH_SPEED_TWO) mask |= 1u << 2;
        if (r->accuracy >= ACH_ACCURACY) mask |= 1u << 3;
        if (count >= ACH_TEST_COUNT) mask |= 1u << 4;
        if (previous >= 0 && r->wpm > db->records[previous].wpm) mask |= 1u << 5;
        previous = i;
    }
    return mask;
}
void achievements_refresh(Database *db)
{
    int i;
    for (i = 0; i < db->profile_count; ++i)
        db->profiles[i].achievements |= achievements_evaluate(db, db->profiles[i].id);
}
int achievements_count(unsigned int mask)
{
    int i, n = 0;
    for (i = 0; i < ACHIEVEMENT_COUNT; ++i) if (mask & (1u << i)) ++n;
    return n;
}
