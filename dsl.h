#ifndef DSL_H
#define DSL_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_LABEL 64
#define MAX_EXPR 256
#define MAX_POINTS 64
#define MAX_SEGMENTS 256
#define MAX_LINES 64
#define MAX_AREAS 32
#define MAX_MARKS 32
#define MAX_CHECKS 32
#define MAX_OUTPUTS 32
#define MATCHES 32
#define MAX_KEYPOINTS 16
#define MAX_FUNCTIONS 16
#define MAX_VECTORS 32
#define MAX_MATRICES 8
#define MAX_TABLES 8
#define MAX_TEXTS 16
#define MAX_MODULES 8
#define MAX_TOKEN_LEN 64
#define MAX_TOKENS 128
#define MAX_IMPORTS 8
#define MAX_TRAIL 800

typedef struct { double x, y; } Vec2;

typedef struct {
    double endX, endY, speed;
    double vx, vy;
    int isVelocity;
    int relative;
} Segment;

typedef struct {
    char label[MAX_LABEL];
    double startX, startY;
    Segment segments[MAX_SEGMENTS];
    int numSegments;
    Segment loopBody[MAX_SEGMENTS];
    int numLoopBody;
    double loopDuration;
    double color[4];
} Point;

typedef struct {
    double x1, y1, x2, y2;
    char label[MAX_LABEL];
    double color[4];
} Line;

typedef struct {
    char type[MAX_LABEL];
    Vec2 pts[8];
    int numPts;
    char label[MAX_LABEL];
    double color[4];
    double radius;
    double value;
    char vertexLabels[8][MAX_LABEL];
} Area;

typedef struct {
    char type[MAX_LABEL];
    char labels[4][MAX_LABEL];
    double color[4];
    int number;
} Mark;

typedef struct {
    char condition[MAX_EXPR];
    char message[MAX_EXPR];
    char recordExprs[8][MAX_EXPR];
    int numRecordExprs;
    double triggerTime;
    int triggered;
} Check;

typedef struct {
    int isNumeric;
    char type[MAX_LABEL];
    char judgeType[MAX_LABEL];
    char labels[8][MAX_LABEL];
    int numLabels;
    double numeric[8];
    int numNumeric;
    char label[MAX_LABEL];
    double color[4];
    int showOnCanvas;
} Output;

typedef struct {
    char tri1[3][MAX_LABEL];
    char tri2[3][MAX_LABEL];
    char type[MAX_LABEL];
} Congruent;

typedef struct {
    double time;
    char label[MAX_LABEL];
} Keypoint;

typedef struct {
    char fname[MAX_LABEL];
    char params[4][MAX_LABEL];
    int numParams;
    char expr[MAX_EXPR];
    double color[4];
    int isSolve;
} Function;

typedef struct {
    char type[MAX_LABEL];
    char labels[3][MAX_LABEL];
    int numLabels;
    char label[MAX_LABEL];
    double color[4];
    double dx, dy;
    double x1, y1, x2, y2;
} Vector;

typedef struct {
    double data[8][8];
    int rows, cols;
    char label[MAX_LABEL];
} Matrix;

typedef struct {
    double data[8][8];
    int rows, cols;
    char label[MAX_LABEL];
} Table;

typedef struct {
    char text[MAX_EXPR];
    double color[4];
} Text;

typedef struct {
    char name[MAX_LABEL];
    char alias[MAX_LABEL];
} Import;

typedef struct {
    char title[MAX_EXPR];
    double xMin, xMax, yMin, yMax;
    Point points[MAX_POINTS];
    int numPoints;
    Line lines[MAX_LINES];
    int numLines;
    Area areas[MAX_AREAS];
    int numAreas;
    Mark marks[MAX_MARKS];
    int numMarks;
    Check checks[MAX_CHECKS];
    int numChecks;
    Output outputs[MAX_OUTPUTS];
    int numOutputs;
    Congruent congruents[8];
    int numCongruents;
    Keypoint keypoints[MAX_KEYPOINTS];
    int numKeypoints;
    Function functions[MAX_FUNCTIONS];
    int numFunctions;
    Vector vectors[MAX_VECTORS];
    int numVectors;
    Matrix matrices[MAX_MATRICES];
    int numMatrices;
    Table tables[MAX_TABLES];
    int numTables;
    Text texts[MAX_TEXTS];
    int numTexts;
    Import imports[MAX_IMPORTS];
    int numImports;
    double totalTime;
    int hasInfiniteLoop;
} AnimationData;

#endif
