#ifndef RENDERER_H
#define RENDERER_H

#include <cairo.h>
#include "dsl.h"
#include "animation.h"

typedef struct {
    double camX, camY;
    double zoom;
    int width, height;
} Camera;

void renderer_render(Camera* cam, AnimationData* data, Vec2* positions, int np,
                     double time, double* outputValues, int numOutputs,
                     Vec2 trailPoints[][MAX_TRAIL], int* trailLen,
                     cairo_t *cr);

#endif
