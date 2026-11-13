#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H
#include "data.h"
#include <stddef.h>
typedef struct { const char *title; const char *description; } AchievementInfo;
extern const AchievementInfo ACHIEVEMENTS[ACHIEVEMENT_COUNT];
void achievements_description(int index, char *text, size_t size);
unsigned int achievements_evaluate(const Database *db, int profile_id);
void achievements_refresh(Database *db);
int achievements_count(unsigned int mask);
#endif
