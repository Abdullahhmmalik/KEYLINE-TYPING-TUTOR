#ifndef STATS_H
#define STATS_H
typedef struct {
    double wpm, accuracy, elapsed, remaining, progress;
    int matched, mistakes;
} Metrics;
Metrics stats_calculate(int matched, int typed, int length, int correct_keys,
                        int total_keys, double elapsed, int duration);
#endif
