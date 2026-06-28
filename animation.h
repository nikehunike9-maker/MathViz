#ifndef ANIMATION_H
#define ANIMATION_H

#include "dsl.h"

typedef struct {
    Vec2 positions[MAX_POINTS];
    int count;
} FrameData;

void compute_positions(AnimationData* data, double t, Vec2* out, int* n);
double compute_output_value(Output* out, Vec2* positions, int np, AnimationData* data);

#endif
