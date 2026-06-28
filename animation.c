#include "animation.h"
#include "math_engine.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void compute_positions(AnimationData* data, double t, Vec2* out, int* n) {
    *n = data->numPoints;
    for (int i = 0; i < data->numPoints; i++) {
        Point* pt = &data->points[i];
        if (pt->numSegments == 0 && pt->numLoopBody == 0) {
            out[i] = (Vec2){pt->startX, pt->startY};
            continue;
        }
        double remaining = t, cx = pt->startX, cy = pt->startY;
        double fx = pt->startX, fy = pt->startY;
        int within = 0;

        for (int j = 0; j < pt->numSegments; j++) {
            Segment* seg = &pt->segments[j];
            if (seg->isVelocity) {
                fx = cx + seg->vx * remaining;
                fy = cy + seg->vy * remaining;
                within = 1; remaining = 0;
                break;
            }
            double dx = seg->endX - cx, dy = seg->endY - cy;
            double d = hypot(dx, dy);
            double segTime = seg->speed > 0 ? d / seg->speed : 0;
            if (remaining <= segTime || (segTime == 0 && j == pt->numSegments - 1)) {
                double progress = segTime > 0 ? fmin(remaining / segTime, 1) : 1;
                fx = cx + dx * progress;
                fy = cy + dy * progress;
                within = 1; remaining = 0;
                break;
            }
            remaining -= segTime;
            cx = seg->endX; cy = seg->endY;
            if (j == pt->numSegments - 1) { fx = seg->endX; fy = seg->endY; }
        }

        if (!within && pt->numLoopBody > 0 && pt->loopDuration > 0) {
            double lt = fmod(t, pt->loopDuration);
            cx = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endX : pt->startX;
            cy = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endY : pt->startY;
            for (int j = 0; j < pt->numLoopBody; j++) {
                Segment* seg = &pt->loopBody[j];
                double dx = seg->endX - cx, dy = seg->endY - cy;
                double d = hypot(dx, dy);
                double segTime = seg->speed > 0 ? d / seg->speed : 0;
                if (lt <= segTime || (segTime == 0 && j == pt->numLoopBody - 1)) {
                    double progress = segTime > 0 ? fmin(lt / segTime, 1) : 1;
                    fx = cx + dx * progress;
                    fy = cy + dy * progress;
                    break;
                }
                lt -= segTime;
                cx = seg->endX; cy = seg->endY;
                if (j == pt->numLoopBody - 1) { fx = seg->endX; fy = seg->endY; }
            }
        }
        out[i] = (Vec2){fx, fy};
    }
}

double compute_output_value(Output* out, Vec2* positions, int np, AnimationData* data) {
    if (strcmp(out->type, "judge") == 0) {
        if (out->numLabels < 6) return 0;
        Vec2 pts[6]; int found = 1;
        for (int i = 0; i < 6; i++) {
            int ok = 0;
            for (int j = 0; j < np; j++) {
                if (strcmp(data->points[j].label, out->labels[i]) == 0) {
                    pts[i] = positions[j]; ok = 1; break;
                }
            }
            if (!ok) { found = 0; break; }
        }
        if (!found) return 0;
        if (strcmp(out->judgeType, "congruent") == 0) return 1; // simplified
        return 0;
    }

    Vec2 pts[8]; int np2 = 0;
    for (int i = 0; i < out->numLabels && np2 < 8; i++) {
        double v = atof(out->labels[i]);
        if (v != 0 || out->labels[i][0] == '0' || out->labels[i][0] == '.') {
            pts[np2++] = (Vec2){v, 0};
        } else {
            int ok = 0;
            for (int j = 0; j < np; j++) {
                if (strcmp(data->points[j].label, out->labels[i]) == 0) {
                    pts[np2++] = positions[j]; ok = 1; break;
                }
            }
            if (!ok) return NAN;
        }
    }

    if (strcmp(out->type, "length") == 0 && np2 >= 2)
        return math_dist(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
    if (strcmp(out->type, "angle") == 0 && np2 >= 3)
        return math_ang_deg(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
    if (strcmp(out->type, "area") == 0 && np2 >= 3)
        return compute_polygon_area(pts, np2);
    if (strcmp(out->type, "perimeter") == 0 && np2 >= 2)
        return compute_perimeter(pts, np2);
    return NAN;
}
