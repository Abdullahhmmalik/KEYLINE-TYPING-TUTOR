#include "stats.h"
#include <math.h>

Metrics stats_calculate(int matched, int typed, int length, int correct_keys,
                        int total_keys, double elapsed, int duration)
{
    Metrics m = {0};
    if (!isfinite(elapsed) || elapsed < 0) elapsed = 0;
    if (duration < 0) duration = 0;
    if (elapsed > duration) elapsed = duration;
    if (typed < 0) typed = 0;
    if (length < 0) length = 0;
    if (typed > length) typed = length;
    if (matched < 0) matched = 0;
    if (matched > typed) matched = typed;
    if (total_keys < 0) total_keys = 0;
    if (correct_keys < 0) correct_keys = 0;
    if (correct_keys > total_keys) correct_keys = total_keys;
    m.elapsed = elapsed;
    m.remaining = duration - elapsed;
    m.matched = matched;
    m.mistakes = total_keys - correct_keys;
    m.wpm = elapsed > 0 ? (matched / 5.0) * 60.0 / elapsed : 0;
    m.accuracy = total_keys > 0 ? 100.0 * correct_keys / total_keys : 0;
    m.progress = length > 0 ? 100.0 * typed / length : 0;
    return m;
}
