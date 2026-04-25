#ifndef EFFECTS_H
#define EFFECTS_H

#include <math.h>
#include <stdlib.h>

static inline float soft_clip(float x) {
    if (x > 1.0) return 0.6667f;
    if (x < -1.0f) return -0.6667f;

    // f(x) = x - (x^3 / 3)
    return x - (x * x * x) * 0.3333333f;
}

static inline float hard_clip(float x, float threshold) {
    if (x > threshold) return threshold;
    if (x < -threshold) return -threshold;
    return x;
}

#endif //EFFECTS_H