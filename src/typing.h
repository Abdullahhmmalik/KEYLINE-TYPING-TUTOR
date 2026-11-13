#ifndef TYPING_H
#define TYPING_H
#include "config.h"
#include "stats.h"
typedef struct {
    char passage[MAX_PASSAGE + 1], typed[MAX_PASSAGE + 1];
    int length, position, correct_keys, total_keys, duration;
    int started, finished, completed;
    double start_time, elapsed;
} TypingTest;
int typing_init(TypingTest *test, const char *passage, int duration);
void typing_update(TypingTest *test, double now);
int typing_character(TypingTest *test, int character, double now);
void typing_backspace(TypingTest *test, double now);
Metrics typing_metrics(const TypingTest *test);
#endif
