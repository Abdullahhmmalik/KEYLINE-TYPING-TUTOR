#include "typing.h"
#include <string.h>
#include <math.h>
#include <limits.h>

int typing_init(TypingTest *t, const char *passage, int duration)
{
    size_t n;
    if (!t) return 0;
    memset(t, 0, sizeof *t);
    if (!passage || !config_valid_duration(duration)) return 0;
    /* Scan only the supported capacity; never copy an unchecked passage. */
    for (n = 0; n <= MAX_PASSAGE && passage[n]; ++n)
        if ((unsigned char)passage[n] < 32 || (unsigned char)passage[n] > 126) return 0;
    if (n == 0 || n > MAX_PASSAGE) return 0;
    memcpy(t->passage, passage, n + 1);
    t->length = (int)n;
    t->duration = duration;
    return 1;
}
void typing_update(TypingTest *t, double now)
{
    if (!t->started || t->finished || !isfinite(now)) return;
    t->elapsed = now > t->start_time ? now - t->start_time : 0;
    if (t->elapsed >= t->duration) {
        t->elapsed = t->duration;
        t->finished = 1;
    }
}
int typing_character(TypingTest *t, int c, double now)
{
    typing_update(t, now); /* Expiration wins over a key arriving at the deadline. */
    if (!isfinite(now) || t->finished || t->length == 0 || c < 32 || c > 126 ||
        t->position >= t->length || t->total_keys == INT_MAX) return 0;
    if (!t->started) { t->started = 1; t->start_time = now; }
    ++t->total_keys;
    if (c == (unsigned char)t->passage[t->position]) ++t->correct_keys;
    t->typed[t->position++] = (char)c;
    t->typed[t->position] = '\0';
    if (t->position == t->length) { t->finished = 1; t->completed = 1; }
    return 1;
}
void typing_backspace(TypingTest *t, double now)
{
    typing_update(t, now);
    if (!t->finished && t->position > 0) t->typed[--t->position] = '\0';
}
Metrics typing_metrics(const TypingTest *t)
{
    int i, matched = 0;
    for (i = 0; i < t->position; ++i) if (t->typed[i] == t->passage[i]) ++matched;
    return stats_calculate(matched, t->position, t->length, t->correct_keys,
                           t->total_keys, t->elapsed, t->duration);
}
