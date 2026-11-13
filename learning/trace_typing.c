/* Educational driver only. The production application remains a raylib GUI.
 * Predict each state first; then run this to inspect the real typing engine. */
#include "typing.h"
#include <stdio.h>

static void show_state(const TypingTest *test, const char *action)
{
    Metrics m=typing_metrics(test);
    printf("%-16s text=\"%s\" pos=%d keys=%d correct=%d matched=%d "
           "elapsed=%.1f WPM=%.1f accuracy=%.1f errors=%d done=%d\n",
        action,test->typed,test->position,test->total_keys,test->correct_keys,
        m.matched,m.elapsed,m.wpm,m.accuracy,m.mistakes,test->finished);
}
int main(void)
{
    TypingTest test;
    if (!typing_init(&test,"abc",15)) return 1;
    show_state(&test,"initialize");
    typing_character(&test,'a',10); show_state(&test,"a at 10");
    typing_character(&test,'x',11); show_state(&test,"x at 11");
    typing_backspace(&test,12);    show_state(&test,"backspace at 12");
    typing_character(&test,'b',13); show_state(&test,"b at 13");
    typing_character(&test,'c',14); show_state(&test,"c at 14");
    return 0;
}
