#include "config.h"
#include <stdlib.h>
#include <errno.h>

/* Add whole seconds within MIN_DURATION..MAX_DURATION. All three pickers
 * wrap after four buttons and page after eight; no layout edits are needed. */
const int DURATION_PRESETS[] = {15, 30, 60, 120};
const int DURATION_PRESET_COUNT = (int)(sizeof DURATION_PRESETS / sizeof DURATION_PRESETS[0]);
const Exercise EXERCISES[] = {
    {"Small steps", "Every skill begins with a small step. Keep your shoulders relaxed and your eyes on the words ahead. Let accuracy guide your hands; speed will follow with practice. A steady rhythm is more useful than a hurried start. Take a breath, find your focus, and enjoy the simple progress you make today."},
    {"Morning light", "The city wakes slowly under a pale blue sky. A little shop opens its doors, and the smell of fresh bread drifts across the street. Someone waves from a bicycle. In a quiet room above the square, a student opens a notebook and writes down one clear goal for the day. There is time to begin again."},
    {"Build something", "A useful program starts with a clear question. Break the problem into smaller parts, choose helpful names, and test one idea at a time. When an error appears, read the message carefully. Good code is not written in a single attempt; it grows through patient changes, thoughtful tests, and a willingness to learn."},
    {"The long way home", "We took the long way home, past the river and the old wooden bridge. The water carried tiny leaves toward the sea. \"Do you hear that?\" asked Sam. A bird was singing somewhere in the trees. We stopped for a moment, then followed the path as the last warm light settled over the hills."},
    {"Curiosity", "Ask a question. Make a guess. Try a simple experiment and look closely at what happens. Curiosity turns ordinary moments into discoveries. You do not need to know every answer before you begin. Sometimes the most important thing you can say is, \"I wonder why.\" There is always another detail waiting to be noticed."},
    {"A better rhythm", "Place your fingers gently on the home row. Type each word with care, including spaces, commas, and capital letters. If you make a mistake, use Backspace to correct it and keep moving. Practice for 15, 30, or 60 seconds, then pause to reflect. Consistent effort builds confidence, one well-placed keystroke at a time."}
};
const int EXERCISE_COUNT = (int)(sizeof EXERCISES / sizeof EXERCISES[0]);

int config_valid_duration(int seconds) { return seconds >= MIN_DURATION && seconds <= MAX_DURATION; }
int config_parse_duration(const char *text, int *seconds)
{
    char *end;
    long n;
    const char *p;
    if (!text || !*text || !seconds) return 0;
    for (p = text; *p; ++p) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    n = strtol(text, &end, 10);
    if (errno || *end || n < MIN_DURATION || n > MAX_DURATION) return 0;
    *seconds = (int)n;
    return 1;
}
