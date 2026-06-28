#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "dsl.h"
#include <stdbool.h>

// ── Interpreter: parses DSL → computes animation state ──

typedef struct {
    AnimationData data;
    Vec2 positions[MAX_POINTS];
    int numPositions;
    double outputValues[MAX_OUTPUTS];
    int numOutputs;
    double time;
    double speed;
    bool playing;
    char statusMsg[256];
    // Check event info (for GUI dialog)
    char checkMessage[512];
    double recordedValues[8];
    char recordedLabels[8][64];
    int numRecorded;
    // Trajectory (trail) data
    Vec2 trailPoints[MAX_POINTS][MAX_TRAIL];
    int trailLen[MAX_POINTS];
} InterpState;

// Initialize interpreter and parse DSL text
void interp_init(InterpState* s, const char* dslText);

// Advance animation by dt seconds, returns true if a check triggered
bool interp_update(InterpState* s, double dt);

// Seek to specific time
void interp_seek(InterpState* s, double t);

// Recompute current frame
void interp_recompute(InterpState* s);

// Get point position at index
Vec2 interp_get_pos(InterpState* s, int idx);

// Get output value
double interp_get_output(InterpState* s, int idx);

#endif
