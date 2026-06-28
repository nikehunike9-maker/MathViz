#include "interpreter.h"
#include "ast.h"
#include "animation.h"
#include "math_engine.h"
#include "module.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

void interp_init(InterpState* s, const char* dslText) {
    memset(s, 0, sizeof(*s));
    s->speed = 1.0;

    module_init();

    AstNode *ast = ast_parse(dslText);
    if (!ast) {
        snprintf(s->statusMsg, sizeof(s->statusMsg), "DSL parse failed");
        return;
    }

    if (!ast_codegen(ast, &s->data)) {
        snprintf(s->statusMsg, sizeof(s->statusMsg), "DSL codegen failed");
        ast_free(ast);
        return;
    }
    ast_free(ast);

    // Load modules
    for (int i = 0; i < s->data.numImports; i++)
        module_load(s->data.imports[i].name);
    module_apply(&s->data);

    interp_recompute(s);
    snprintf(s->statusMsg, sizeof(s->statusMsg),
             "%d points, %d outputs | H for help",
             s->data.numPoints, s->data.numOutputs);
}

void interp_recompute(InterpState* s) {
    compute_positions(&s->data, s->time, s->positions, &s->numPositions);
    s->numOutputs = 0;
    for (int i = 0; i < s->data.numOutputs && i < MAX_OUTPUTS; i++) {
        s->outputValues[i] = compute_output_value(
            &s->data.outputs[i], s->positions, s->numPositions, &s->data);
        set_module_var(s->data.outputs[i].label, s->outputValues[i]);
        s->numOutputs++;
    }
    // Record trail positions (skip if change < 0.01)
    for (int i = 0; i < s->numPositions; i++) {
        if (s->trailLen[i] == 0) {
            s->trailPoints[i][0] = s->positions[i];
            s->trailLen[i] = 1;
        } else {
            Vec2 last = s->trailPoints[i][s->trailLen[i] - 1];
            double dx = s->positions[i].x - last.x;
            double dy = s->positions[i].y - last.y;
            if (hypot(dx, dy) > 0.01 && s->trailLen[i] < MAX_TRAIL) {
                s->trailPoints[i][s->trailLen[i]++] = s->positions[i];
            }
        }
    }
}

void interp_seek(InterpState* s, double t) {
    s->time = fmax(0, fmin(t, s->data.totalTime));
    memset(s->trailLen, 0, sizeof(s->trailLen));
    interp_recompute(s);
}

bool interp_update(InterpState* s, double dt) {
    if (!s->playing) return false;

    s->time += dt * s->speed;
    if (!s->data.hasInfiniteLoop && s->time > s->data.totalTime) {
        s->time = s->data.totalTime;
        s->playing = false;
    }
    interp_recompute(s);

    // Clear previous check info
    s->checkMessage[0] = 0;
    s->numRecorded = 0;

    // Check triggers
    bool triggered = false;
    for (int i = 0; i < s->data.numChecks; i++) {
        if (s->data.checks[i].triggered) continue;

        // Resolve point labels in condition
        char resolved[1024] = ""; int ri = 0;
        const char* cp = s->data.checks[i].condition;
        while (*cp && ri < 1023) {
            if (isalpha((unsigned char)*cp) || *cp == '_') {
                char label[64]; int li = 0;
                while ((isalnum((unsigned char)*cp) || *cp == '_') && li < 63)
                    label[li++] = *cp++;
                label[li] = 0;
                if (*cp == '.') {
                    cp++;
                    char coord = *cp; if (coord == 'x' || coord == 'y') cp++;
                    int found = 0;
                    for (int j = 0; j < s->numPositions; j++) {
                        if (strcmp(s->data.points[j].label, label) == 0) {
                            ri += snprintf(resolved + ri, 1024 - ri, "%f",
                                          coord == 'x' ? s->positions[j].x
                                                       : s->positions[j].y);
                            found = 1; break;
                        }
                    }
                    if (!found) ri += snprintf(resolved + ri, 1024 - ri, "0");
                } else {
                    ri += snprintf(resolved + ri, 1024 - ri, "%s", label);
                }
            } else {
                resolved[ri++] = *cp++;
            }
        }
        resolved[ri] = 0;

        double result = eval_expr(resolved);
        if (result != 0) {
            s->data.checks[i].triggered = 1;
            s->data.checks[i].triggerTime = s->time;
            snprintf(s->statusMsg, sizeof(s->statusMsg),
                     "[OK] t=%.1fs %s", s->time, s->data.checks[i].message);
            // Store check event info for GUI dialog
            snprintf(s->checkMessage, sizeof(s->checkMessage), "%s",
                     s->data.checks[i].message);
            // Evaluate RECORD expressions (outputs are already module vars from interp_recompute)
            for (int r = 0; r < s->data.checks[i].numRecordExprs && r < 8; r++) {
                const char* expr = s->data.checks[i].recordExprs[r];
                double val = eval_expr(expr);
                s->recordedValues[s->numRecorded] = val;
                strncpy(s->recordedLabels[s->numRecorded], expr, 63);
                s->numRecorded++;
            }
            triggered = true;
            if (!s->data.hasInfiniteLoop) s->playing = false;
            break;
        }
    }
    return triggered;
}

Vec2 interp_get_pos(InterpState* s, int idx) {
    if (idx >= 0 && idx < s->numPositions) return s->positions[idx];
    return (Vec2){0, 0};
}

double interp_get_output(InterpState* s, int idx) {
    if (idx >= 0 && idx < s->numOutputs) return s->outputValues[idx];
    return NAN;
}
