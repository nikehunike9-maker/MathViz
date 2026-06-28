#include "renderer.h"
#include "math_engine.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define PI 3.14159265358979323846

static void world_to_screen(Camera* cam, double wx, double wy, double* sx, double* sy) {
    *sx = (wx - cam->camX) * cam->zoom + cam->width / 2.0;
    *sy = -(wy - cam->camY) * cam->zoom + cam->height / 2.0;
}

static double screen_to_world_x(Camera* cam, double sx) {
    return (sx - cam->width / 2.0) / cam->zoom + cam->camX;
}
static double screen_to_world_y(Camera* cam, double sy) {
    return -(sy - cam->height / 2.0) / cam->zoom + cam->camY;
}

static void set_color(cairo_t *cr, double c[4]) {
    cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3]);
}

static void text(cairo_t *cr, double x, double y, const char *s, double size,
                 int align, int baseline, double r, double g, double b, double a) {
    cairo_set_source_rgba(cr, r, g, b, a);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t te;
    cairo_text_extents(cr, s, &te);
    double dx = 0, dy = 0;
    if (align == 0) dx = -te.width / 2;
    else if (align > 0) dx = -te.width;
    if (baseline == 0) dy = te.height / 2;
    else if (baseline > 0) dy = 0;
    else dy = te.height;
    cairo_move_to(cr, x + dx, y + dy);
    cairo_show_text(cr, s);
}

static void text_color(cairo_t *cr, double x, double y, const char *s, double size,
                       int align, int baseln, double c[4]) {
    text(cr, x, y, s, size, align, baseln, c[0], c[1], c[2], c[3]);
}

static void format_num(double n, char *buf, int sz) {
    if (isnan(n)) snprintf(buf, sz, "?");
    else if (n == (int)n) snprintf(buf, sz, "%d", (int)n);
    else snprintf(buf, sz, "%.2f", n);
}

// ── Grid step calculation (matches HTML) ──
static double calc_grid_step(double visRange) {
    double target = visRange / 8.0;
    double mag = pow(10, floor(log10(target)));
    double res = target / mag;
    if (res < 1.5) return mag;
    if (res < 3.5) return 2 * mag;
    if (res < 7.5) return 5 * mag;
    return 10 * mag;
}

// ── Grid ──
static void draw_grid(Camera* cam, double xMin, double xMax, double yMin, double yMax, cairo_t *cr) {
    double sx0 = 0, sy0 = 0;
    world_to_screen(cam, 0, 0, &sx0, &sy0);

    double visW = (cam->width / cam->zoom);
    double visH = (cam->height / cam->zoom);
    double step = calc_grid_step(visW < visH ? visW : visH);

    // Grid lines (#e8e8e8, 1px)
    cairo_set_source_rgb(cr, 0xE8/255.0, 0xE8/255.0, 0xE8/255.0);
    cairo_set_line_width(cr, 1);
    cairo_new_path(cr);
    double wL = screen_to_world_x(cam, 0);
    double wR = screen_to_world_x(cam, cam->width);
    double wB = screen_to_world_y(cam, cam->height);
    double wT = screen_to_world_y(cam, 0);
    for (double x = ceil(wL / step) * step; x <= wR; x += step) {
        if (fabs(x) < step * 0.01) continue;
        double sx, sy1, sy2;
        world_to_screen(cam, x, wT, &sx, &sy1);
        world_to_screen(cam, x, wB, &sx, &sy2);
        cairo_move_to(cr, sx, sy1); cairo_line_to(cr, sx, sy2);
    }
    for (double y = ceil(wB / step) * step; y <= wT; y += step) {
        if (fabs(y) < step * 0.01) continue;
        double sx1, sy, sx2;
        world_to_screen(cam, wL, y, &sx1, &sy);
        world_to_screen(cam, wR, y, &sx2, &sy);
        cairo_move_to(cr, sx1, sy); cairo_line_to(cr, sx2, sy);
    }
    cairo_stroke(cr);

    // Axis lines (#555, 2px) — full visible area
    cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
    cairo_set_line_width(cr, 2);
    cairo_new_path(cr);
    if (0 >= wL && 0 <= wR) {
        double sx, sy1, sy2;
        world_to_screen(cam, 0, wT, &sx, &sy1);
        world_to_screen(cam, 0, wB, &sx, &sy2);
        cairo_move_to(cr, sx, sy1); cairo_line_to(cr, sx, sy2);
    }
    if (0 >= wB && 0 <= wT) {
        double sx1, sy, sx2;
        world_to_screen(cam, wL, 0, &sx1, &sy);
        world_to_screen(cam, wR, 0, &sx2, &sy);
        cairo_move_to(cr, sx1, sy); cairo_line_to(cr, sx2, sy);
    }
    cairo_stroke(cr);

    // Tick labels (#888, 11px sans-serif)
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 0x88/255.0, 0x88/255.0, 0x88/255.0);
    for (double x = ceil(wL / step) * step; x <= wR; x += step) {
        if (fabs(x) < step * 0.01) continue;
        double sx, sy;
        world_to_screen(cam, x, 0, &sx, &sy);
        char buf[32]; format_num(x, buf, sizeof(buf));
        cairo_text_extents_t te;
        cairo_text_extents(cr, buf, &te);
        cairo_move_to(cr, sx - te.width / 2, sy + 14 + te.height);
        cairo_show_text(cr, buf);
    }
    for (double y = ceil(wB / step) * step; y <= wT; y += step) {
        if (fabs(y) < step * 0.01) continue;
        double sx, sy;
        world_to_screen(cam, 0, y, &sx, &sy);
        char buf[32]; format_num(y, buf, sizeof(buf));
        cairo_text_extents_t te;
        cairo_text_extents(cr, buf, &te);
        cairo_move_to(cr, sx - te.width - 6, sy + te.height / 3);
        cairo_show_text(cr, buf);
    }

    // O label (center, top relative to origin)
    if (0 >= wL && 0 <= wR && 0 >= wB && 0 <= wT) {
        cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
        cairo_set_font_size(cr, 12);
        cairo_text_extents_t te;
        cairo_text_extents(cr, "O", &te);
        cairo_move_to(cr, sx0 - te.width / 2 - 8, sy0 + te.height + 6);
        cairo_show_text(cr, "O");
    }

    // x label (bottom-right of x-axis end)
    {
        double sx, sy;
        world_to_screen(cam, wR, 0, &sx, &sy);
        cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
        cairo_set_font_size(cr, 13);
        cairo_text_extents_t te;
        cairo_text_extents(cr, "x", &te);
        cairo_move_to(cr, fmin(sx, cam->width - 4) - te.width, sy + 4 + te.height);
        cairo_show_text(cr, "x");
    }

    // y label (top-right of y-axis top)
    {
        double sx, sy;
        world_to_screen(cam, 0, wT, &sx, &sy);
        cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
        cairo_set_font_size(cr, 13);
        cairo_text_extents_t te;
        cairo_text_extents(cr, "y", &te);
        cairo_move_to(cr, sx + 4, fmin(sy, 4));
        cairo_show_text(cr, "y");
    }
}

void renderer_render(Camera* cam, AnimationData* data, Vec2* positions, int np,
                     double time, double* outputValues, int numOutputs,
                     Vec2 trailPoints[][MAX_TRAIL], int* trailLen,
                     cairo_t *cr) {
    int W = cam->width, H = cam->height;

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    // Background #f8f9fa
    cairo_set_source_rgb(cr, 0xf8/255.0, 0xf9/255.0, 0xfa/255.0);
    cairo_paint(cr);

    // Grid
    draw_grid(cam, data->xMin, data->xMax, data->yMin, data->yMax, cr);

    // Areas (fill + stroke + label) — use current positions from vertexLabels
    cairo_set_line_width(cr, 1.5);
    for (int i = 0; i < data->numAreas; i++) {
        Area* a = &data->areas[i];
        Vec2 vpts[8]; int nvpts = 0;
        for (int j = 0; j < a->numPts && j < 8; j++) {
            if (a->vertexLabels[j][0]) {
                int found = 0;
                for (int k = 0; k < np; k++) {
                    if (strcmp(data->points[k].label, a->vertexLabels[j]) == 0) {
                        vpts[nvpts++] = positions[k]; found = 1; break;
                    }
                }
                if (!found) vpts[nvpts++] = a->pts[j];
            } else {
                vpts[nvpts++] = a->pts[j];
            }
        }
        if (nvpts < 3) continue;
        double areaVal = compute_polygon_area(vpts, nvpts);
        set_color(cr, a->color);
        cairo_new_path(cr);
        for (int j = 0; j < nvpts; j++) {
            double sx, sy; world_to_screen(cam, vpts[j].x, vpts[j].y, &sx, &sy);
            if (j == 0) cairo_move_to(cr, sx, sy);
            else cairo_line_to(cr, sx, sy);
        }
        cairo_close_path(cr);
        cairo_fill_preserve(cr);
        double sc[4] = { a->color[0], a->color[1], a->color[2], a->color[3] < 0.4 ? 0.6 : a->color[3] };
        set_color(cr, sc);
        cairo_stroke(cr);

        // Centroid label
        double cx = 0, cy = 0;
        for (int j = 0; j < nvpts; j++) { cx += vpts[j].x; cy += vpts[j].y; }
        cx /= nvpts; cy /= nvpts;
        double scx, scy; world_to_screen(cam, cx, cy, &scx, &scy);
        char buf[64];
        format_num(areaVal, buf, sizeof(buf));
        char label[128];
        if (a->label[0]) snprintf(label, sizeof(label), "%s = %s", a->label, buf);
        else snprintf(label, sizeof(label), "%s", buf);
        cairo_set_font_size(cr, 13);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        text(cr, scx, scy, label, 13, 0, 0, 0x33/255.0, 0x33/255.0, 0x33/255.0, 1);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    }

    // Congruent/Similar
    for (int i = 0; i < data->numCongruents; i++) {
        Congruent* cg = &data->congruents[i];
        int isSimilar = strcmp(cg->type, "similar") == 0;
        const char *sym = isSimilar ? "∼" : "≅";
        char label[128];
        snprintf(label, sizeof(label), "△%s%s%s %s △%s%s%s",
                 cg->tri1[0], cg->tri1[1], cg->tri1[2],
                 sym, cg->tri2[0], cg->tri2[1], cg->tri2[2]);
        text(cr, 12, H - 50, label, 14, -1, -1, 0x55/255.0, 0x55/255.0, 0x55/255.0, 1);

        for (int t = 0; t < 2; t++) {
            char(*tri)[MAX_LABEL] = t == 0 ? cg->tri1 : cg->tri2;
            Vec2 pts[3]; int found = 1;
            for (int j = 0; j < 3; j++) {
                int ok = 0;
                for (int k = 0; k < np; k++) {
                    if (strcmp(data->points[k].label, tri[j]) == 0) {
                        pts[j] = positions[k]; ok = 1; break;
                    }
                }
                if (!ok) { found = 0; break; }
            }
            if (!found) continue;
            cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
            cairo_set_line_width(cr, 1.5);
            for (int j = 0; j < 3; j++) {
                int k = (j + 1) % 3;
                double sx1, sy1, sx2, sy2;
                world_to_screen(cam, pts[j].x, pts[j].y, &sx1, &sy1);
                world_to_screen(cam, pts[k].x, pts[k].y, &sx2, &sy2);
                double mx = (sx1 + sx2) / 2, my = (sy1 + sy2) / 2;
                double dx = sx2 - sx1, dy = sy2 - sy1;
                double len = hypot(dx, dy);
                if (len < 1) continue;
                double nx = -dy / len, ny = dx / len;
                double hl = 5; int count = j + 1;
                double start = -(count - 1) * 4 / 2.0;
                for (int h = 0; h < count; h++) {
                    double off = start + h * 4;
                    cairo_move_to(cr, mx + nx * off - ny * hl, my + ny * off + nx * hl);
                    cairo_line_to(cr, mx + nx * off + ny * hl, my + ny * off - nx * hl);
                }
            }
            cairo_stroke(cr);
        }
    }

    // Lines
    cairo_set_line_width(cr, 2);
    for (int i = 0; i < data->numLines; i++) {
        Line* l = &data->lines[i];
        set_color(cr, l->color);
        double sx1, sy1, sx2, sy2;
        world_to_screen(cam, l->x1, l->y1, &sx1, &sy1);
        world_to_screen(cam, l->x2, l->y2, &sx2, &sy2);
        cairo_move_to(cr, sx1, sy1); cairo_line_to(cr, sx2, sy2);
        cairo_stroke(cr);
        if (l->label[0]) {
            double mx = (sx1 + sx2) / 2, my = (sy1 + sy2) / 2;
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            text_color(cr, mx, my - 4, l->label, 13, 0, 1, l->color);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        }
    }

    // Functions
    cairo_set_line_width(cr, 2.5);
    for (int i = 0; i < data->numFunctions; i++) {
        Function* f = &data->functions[i];
        if (f->isSolve) continue;
        set_color(cr, f->color);
        cairo_new_path(cr);
        int steps = 200;
        int first = 1;
        for (int j = 0; j <= steps; j++) {
            double v = data->xMin + (data->xMax - data->xMin) * j / steps;
            double yv = eval_math_expr(f->expr, v, f->params[0]);
            if (isnan(yv) || !isfinite(yv)) continue;
            double sx, sy; world_to_screen(cam, v, yv, &sx, &sy);
            if (sx < -100 || sx > W + 100 || sy < -100 || sy > H + 100) continue;
            if (first) { cairo_move_to(cr, sx, sy); first = 0; }
            else cairo_line_to(cr, sx, sy);
        }
        cairo_stroke(cr);
        if (!first) {
            double labelX = data->xMin + (data->xMax - data->xMin) * 0.85;
            double lY = eval_math_expr(f->expr, labelX, f->params[0]);
            if (!isnan(lY) && isfinite(lY)) {
                double slx, sly; world_to_screen(cam, labelX, lY, &slx, &sly);
                char buf[64];
                snprintf(buf, sizeof(buf), "%s(%s)", f->fname, f->params[0]);
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                text_color(cr, slx, sly - 6, buf, 14, 0, 1, f->color);
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            }
        }
    }

    // Vectors (with filled arrowhead matching HTML)
    for (int i = 0; i < data->numVectors; i++) {
        Vector* v = &data->vectors[i];
        double x1 = v->x1, y1 = v->y1, x2 = v->x2, y2 = v->y2;
        double sx1, sy1, sx2, sy2;
        world_to_screen(cam, x1, y1, &sx1, &sy1);
        world_to_screen(cam, x2, y2, &sx2, &sy2);
        double dx = sx2 - sx1, dy = sy2 - sy1, len = hypot(dx, dy);
        if (len < 3) continue;
        set_color(cr, v->color);
        cairo_set_line_width(cr, 2.5);
        cairo_move_to(cr, sx1, sy1); cairo_line_to(cr, sx2, sy2);
        cairo_stroke(cr);
        double angle = atan2(dy, dx), alen = fmin(12, len * 0.3), aa = 0.45;
        cairo_new_path(cr);
        cairo_move_to(cr, sx2, sy2);
        cairo_line_to(cr, sx2 - alen * cos(angle - aa), sy2 - alen * sin(angle - aa));
        cairo_close_path(cr);
        cairo_line_to(cr, sx2 - alen * cos(angle + aa), sy2 - alen * sin(angle + aa));
        cairo_close_path(cr);
        cairo_fill(cr);
        if (v->label[0]) {
            double mx = (sx1 + sx2) / 2, my = (sy1 + sy2) / 2;
            // Perpendicular offset
            double nx = -dy / len, ny = dx / len;
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            text_color(cr, mx + nx * 12, my + ny * 12, v->label, 13, 0, 1, v->color);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        }
    }

    // Matrix display (bottom-right, before marks per HTML order)
    if (data->numMatrices > 0) {
        double my = H - 30;
        cairo_set_font_size(cr, 12);
        for (int i = 0; i < data->numMatrices; i++) {
            Matrix* m = &data->matrices[i];
            double rows = m->rows, cols = m->cols;
            if (rows == 0 || cols == 0) continue;
            double cellW = 50, cellH = 22;
            double totalW = cols * cellW, totalH = rows * cellH;
            double leftX = W - 12 - totalW - 20;
            double topY = my - totalH;
            if (topY < 60) { topY = 60; my = topY + totalH; }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_source_rgb(cr, 0x66/255.0, 0x7e/255.0, 0xea/255.0);
            cairo_move_to(cr, leftX - 4, topY + cellH / 2 + 4);
            cairo_show_text(cr, m->label);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            double bx = leftX, by = topY;
            cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
            cairo_set_line_width(cr, 2);
            cairo_move_to(cr, bx, by); cairo_line_to(cr, bx - 8, by);
            cairo_line_to(cr, bx - 8, by + totalH); cairo_line_to(cr, bx, by + totalH);
            cairo_stroke(cr);
            bx = leftX + totalW;
            cairo_move_to(cr, bx, by); cairo_line_to(cr, bx + 8, by);
            cairo_line_to(cr, bx + 8, by + totalH); cairo_line_to(cr, bx, by + totalH);
            cairo_stroke(cr);
            cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 12);
            cairo_set_source_rgb(cr, 0x33/255.0, 0x33/255.0, 0x33/255.0);
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) {
                    char buf[32]; format_num(m->data[r][c], buf, sizeof(buf));
                    cairo_text_extents_t te;
                    cairo_text_extents(cr, buf, &te);
                    cairo_move_to(cr, leftX + c * cellW + (cellW - te.width) / 2,
                                  topY + r * cellH + (cellH + te.height) / 2);
                    cairo_show_text(cr, buf);
                }
            }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            my -= (totalH + 20);
        }
    }

    // Table display (bottom-right, left of matrices)
    if (data->numTables > 0) {
        double my = H - 30;
        cairo_set_font_size(cr, 11);
        for (int i = 0; i < data->numTables; i++) {
            Table* t = &data->tables[i];
            double rows = t->rows, cols = t->cols;
            if (rows == 0 || cols == 0) continue;
            double cellW = 50, cellH = 22;
            double totalW = cols * cellW, totalH = (rows + 1) * cellH;
            double leftX = W - 12 - totalW - 20 - 200;
            double topY = my - totalH;
            if (topY < 60) { topY = 60; my = topY + totalH; }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_source_rgb(cr, 0x66/255.0, 0x7e/255.0, 0xea/255.0);
            cairo_move_to(cr, leftX, topY - 4);
            cairo_show_text(cr, t->label);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_rectangle(cr, leftX, topY, totalW, cellH);
            cairo_set_source_rgb(cr, 0xf0/255.0, 0xf2/255.0, 0xff/255.0);
            cairo_fill(cr);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 11);
            cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
            for (int c = 0; c < cols; c++) {
                char hdr[16]; snprintf(hdr, sizeof(hdr), "x%d", c + 1);
                cairo_text_extents_t te;
                cairo_text_extents(cr, hdr, &te);
                cairo_move_to(cr, leftX + c * cellW + (cellW - te.width) / 2,
                              topY + (cellH + te.height) / 2);
                cairo_show_text(cr, hdr);
            }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 11);
            for (int r = 0; r < rows; r++) {
                double ry = topY + (r + 1) * cellH;
                cairo_set_source_rgb(cr, 0x33/255.0, 0x33/255.0, 0x33/255.0);
                for (int c = 0; c < cols; c++) {
                    char buf[32]; format_num(t->data[r][c], buf, sizeof(buf));
                    cairo_text_extents_t te;
                    cairo_text_extents(cr, buf, &te);
                    cairo_move_to(cr, leftX + c * cellW + (cellW - te.width) / 2,
                                  ry + (cellH + te.height) / 2);
                    cairo_show_text(cr, buf);
                }
                cairo_set_source_rgb(cr, 0xee/255.0, 0xee/255.0, 0xee/255.0);
                cairo_set_line_width(cr, 0.5);
                cairo_move_to(cr, leftX, ry); cairo_line_to(cr, leftX + totalW, ry);
                cairo_stroke(cr);
            }
            cairo_set_source_rgb(cr, 0xcc/255.0, 0xcc/255.0, 0xcc/255.0);
            cairo_set_line_width(cr, 1);
            cairo_rectangle(cr, leftX, topY, totalW, (rows + 1) * cellH);
            cairo_stroke(cr);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            my -= (totalH + 20);
        }
    }

    // Marks (angle arcs, right angle, hash marks, parallel)
    for (int i = 0; i < data->numMarks; i++) {
        Mark* m = &data->marks[i];
        Vec2 pts[4]; int found = 1;
        for (int j = 0; j < 4; j++) {
            if (!m->labels[j][0]) { found = 0; break; }
            int ok = 0;
            for (int k = 0; k < np; k++) {
                if (strcmp(data->points[k].label, m->labels[j]) == 0) {
                    pts[j] = positions[k]; ok = 1; break;
                }
            }
            if (!ok) { found = 0; break; }
        }
        if (!found) continue;
        set_color(cr, m->color);
        cairo_set_line_width(cr, 1.5);

        if (strcmp(m->type, "angle") == 0) {
            double sx1, sy1, sx2, sy2, sx3, sy3;
            world_to_screen(cam, pts[0].x, pts[0].y, &sx1, &sy1);
            world_to_screen(cam, pts[1].x, pts[1].y, &sx2, &sy2);
            world_to_screen(cam, pts[2].x, pts[2].y, &sx3, &sy3);
            double a1 = atan2(sy1 - sy2, sx1 - sx2), a2 = atan2(sy3 - sy2, sx3 - sx2);
            double start = a1, end = a2;
            double diff = end - start;
            if (diff < 0) diff += 2 * PI;
            if (diff > PI) { double t = start; start = end; end = t; }
            for (int r = 0; r < fmax(1, m->number); r++) {
                cairo_new_path(cr);
                cairo_arc(cr, sx2, sy2, 16 + r * 5, start, end);
                cairo_stroke(cr);
            }
            if (m->number > 0) {
                double mid = (start + end) / 2;
                char nb[16]; snprintf(nb, sizeof(nb), "%d", m->number);
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                text(cr, sx2 + cos(mid) * (24 + (fmax(1,m->number)-1)*5),
                     sy2 + sin(mid) * (24 + (fmax(1,m->number)-1)*5),
                     nb, 11, 0, 0, m->color[0], m->color[1], m->color[2], 1);
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            }
        } else if (strcmp(m->type, "right-angle") == 0) {
            double sx1, sy1, sx2, sy2, sx3, sy3;
            world_to_screen(cam, pts[0].x, pts[0].y, &sx1, &sy1);
            world_to_screen(cam, pts[1].x, pts[1].y, &sx2, &sy2);
            world_to_screen(cam, pts[2].x, pts[2].y, &sx3, &sy3);
            double d1x = sx1 - sx2, d1y = sy1 - sy2, d2x = sx3 - sx2, d2y = sy3 - sy2;
            double l1 = hypot(d1x, d1y), l2 = hypot(d2x, d2y);
            if (l1 < 0.1 || l2 < 0.1) continue;
            double ux = d1x / l1, uy = d1y / l1, vx = d2x / l2, vy = d2y / l2;
            double sz = 10;
            double p1x = sx2 + ux * sz, p1y = sy2 + uy * sz;
            double p2x = p1x + vx * sz, p2y = p1y + vy * sz;
            double p3x = sx2 + vx * sz, p3y = sy2 + vy * sz;
            cairo_move_to(cr, p1x, p1y); cairo_line_to(cr, p2x, p2y); cairo_line_to(cr, p3x, p3y);
            cairo_stroke(cr);
        } else if (strcmp(m->type, "segment") == 0 && m->labels[1][0]) {
            // Segment hash marks
            double sx1, sy1, sx2, sy2;
            world_to_screen(cam, pts[0].x, pts[0].y, &sx1, &sy1);
            world_to_screen(cam, pts[1].x, pts[1].y, &sx2, &sy2);
            double mx = (sx1 + sx2) / 2, my = (sy1 + sy2) / 2;
            double dx = sx2 - sx1, dy = sy2 - sy1, len = hypot(dx, dy);
            if (len < 1) continue;
            double nx = -dy / len, ny = dx / len;
            int count = m->number > 0 ? m->number : 1;
            double hl = 3;
            double start = -(count - 1) * 5 / 2.0;
            for (int h = 0; h < count; h++) {
                double off = start + h * 5;
                cairo_move_to(cr, mx + nx * off - ny * hl, my + ny * off + nx * hl);
                cairo_line_to(cr, mx + nx * off + ny * hl, my + ny * off - nx * hl);
            }
            cairo_stroke(cr);
        } else if (strcmp(m->type, "parallel") == 0) {
            // Parallel marks (arrowhead-like symbols)
            double sx1, sy1, sx2, sy2;
            world_to_screen(cam, pts[0].x, pts[0].y, &sx1, &sy1);
            world_to_screen(cam, pts[1].x, pts[1].y, &sx2, &sy2);
            double mx = (sx1 + sx2) / 2, my = (sy1 + sy2) / 2;
            double dx = sx2 - sx1, dy = sy2 - sy1, len = hypot(dx, dy);
            if (len < 1) continue;
            double nx = -dy / len, ny = dx / len;
            double hl = 3;
            for (int side = -1; side <= 1; side += 2) {
                double cx = mx + nx * side * 3, cy = my + ny * side * 3;
                cairo_move_to(cr, cx - ny * hl, cy + nx * hl);
                cairo_line_to(cr, cx + ny * hl, cy - nx * hl);
            }
            cairo_stroke(cr);
        }
    }

    // Trajectory (dashed trail before points)
    if (trailPoints && trailLen) {
        cairo_set_line_width(cr, 2);
        for (int i = 0; i < np; i++) {
            if (trailLen[i] < 2) continue;
            Point* pt = &data->points[i];
            cairo_set_source_rgba(cr, pt->color[0], pt->color[1], pt->color[2], 0.35);
            const double dashes[] = {5, 4};
            cairo_set_dash(cr, dashes, 2, 0);
            cairo_new_path(cr);
            for (int j = 0; j < trailLen[i]; j++) {
                double sx, sy;
                world_to_screen(cam, trailPoints[i][j].x, trailPoints[i][j].y, &sx, &sy);
                if (j == 0) cairo_move_to(cr, sx, sy);
                else cairo_line_to(cr, sx, sy);
            }
            cairo_stroke(cr);
            cairo_set_dash(cr, NULL, 0, 0);
        }
    }
    // Points
    for (int i = 0; i < np; i++) {
        Vec2 pos = positions[i];
        double sx, sy;
        world_to_screen(cam, pos.x, pos.y, &sx, &sy);
        Point* pt = &data->points[i];

        // Radial gradient glow (matches HTML createRadialGradient)
        cairo_pattern_t *grad = cairo_pattern_create_radial(sx, sy, 0, sx, sy, 16);
        cairo_pattern_add_color_stop_rgba(grad, 0, pt->color[0], pt->color[1], pt->color[2], pt->color[3] * 0.3);
        cairo_pattern_add_color_stop_rgba(grad, 1, pt->color[0], pt->color[1], pt->color[2], 0);
        cairo_set_source(cr, grad);
        cairo_arc(cr, sx, sy, 16, 0, 2 * PI);
        cairo_fill(cr);
        cairo_pattern_destroy(grad);

        // Point fill (radius 7)
        set_color(cr, pt->color);
        cairo_arc(cr, sx, sy, 7, 0, 2 * PI);
        cairo_fill(cr);

        // White border (2.5px)
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_line_width(cr, 2.5);
        cairo_arc(cr, sx, sy, 7, 0, 2 * PI);
        cairo_stroke(cr);

        // Label (bold 15px #222)
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        text(cr, sx + 11, sy - 5, pt->label, 15, -1, -1, 0x22/255.0, 0x22/255.0, 0x22/255.0, 1);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

        // Coordinates (11px monospace #666)
        cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        char full[64] = "(";
        char coord[32];
        format_num(pos.x, coord, sizeof(coord));
        strncat(full, coord, 63 - strlen(full));
        strncat(full, ", ", 63 - strlen(full));
        format_num(pos.y, coord, sizeof(coord));
        strncat(full, coord, 63 - strlen(full));
        strncat(full, ")", 63 - strlen(full));
        text(cr, sx + 11, sy + 5, full, 11, -1, 0, 0x66/255.0, 0x66/255.0, 0x66/255.0, 1);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    }

    // Output values on canvas (showOnCanvas) - 12px monospace
    double mY = 12;
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    for (int i = 0; i < numOutputs; i++) {
        if (!data->outputs[i].showOnCanvas) continue;
        double val = outputValues[i];
        char buf[64];
        format_num(val, buf, sizeof(buf));
        char line[128];
        snprintf(line, sizeof(line), "%s = %s", data->outputs[i].label, buf);
        text_color(cr, 12, mY, line, 12, -1, -1, data->outputs[i].color);
        mY += 18;
    }
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // Title
    if (data->title[0]) {
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        text(cr, 12, fmax(mY, 8), data->title, 14, -1, -1, 0x33/255.0, 0x33/255.0, 0x33/255.0, 1);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    }

    // Text statements (with {expr} interpolation)
    if (data->numTexts > 0) {
        double textY = fmax(mY, 8) + 30;
        for (int i = 0; i < data->numTexts; i++) {
            Text *t = &data->texts[i];
            cairo_set_source_rgba(cr, t->color[0], t->color[1], t->color[2], t->color[3]);
            cairo_set_font_size(cr, 15);
            char tmp[256]; strncpy(tmp, t->text, 255); tmp[255] = 0;
            char *line = tmp;
            double ty = textY;
            while (line) {
                char *nl = strchr(line, '\n');
                if (nl) *nl = 0;
                char rendered[512]; int ri = 0;
                const char *lp = line;
                while (*lp && ri < 500) {
                    if (*lp == '{') {
                        lp++;
                        char expr[128]; int ei = 0;
                        while (*lp && *lp != '}' && ei < 127)
                            expr[ei++] = *lp++;
                        expr[ei] = 0;
                        if (*lp == '}') lp++;
                        double ev = eval_math_expr(expr, time, "t");
                        char val[32];
                        if (isnan(ev)) snprintf(val, sizeof(val), "?");
                        else if (ev == (int)ev) snprintf(val, sizeof(val), "%d", (int)ev);
                        else snprintf(val, sizeof(val), "%.2f", ev);
                        int vl = strlen(val);
                        if (ri + vl < 500) { memcpy(rendered + ri, val, vl); ri += vl; }
                    } else {
                        rendered[ri++] = *lp++;
                    }
                }
                rendered[ri] = 0;
                cairo_text_extents_t te;
                cairo_text_extents(cr, rendered, &te);
                cairo_move_to(cr, W / 2 - te.width / 2, ty);
                cairo_show_text(cr, rendered);
                if (nl) { *nl = '\n'; line = nl + 1; } else break;
                ty += 24;
            }
            textY = ty + 10;
        }
    }

    // Check event history (matching HTML: white semi-transparent box)
    {
        int triggeredCount = 0;
        for (int i = 0; i < data->numChecks; i++)
            if (data->checks[i].triggered) triggeredCount++;
        if (triggeredCount > 0) {
            char lines[16][128]; int nLines = 0;
            snprintf(lines[nLines++], 128, "检查结果");
            for (int i = 0; i < data->numChecks; i++) {
                if (!data->checks[i].triggered) continue;
                char tbuf[32];
                snprintf(tbuf, sizeof(tbuf), "#%d  t=%.1fs", nLines, data->checks[i].triggerTime);
                char header[128]; snprintf(header, sizeof(header), "%s", tbuf);
                strncpy(lines[nLines], header, 127); nLines++;
                const char* msg = data->checks[i].message;
                if (msg[0]) {
                    char detail[128];
                    snprintf(detail, sizeof(detail), "  %s", msg);
                    strncpy(lines[nLines], detail, 127); nLines++;
                }
            }
            double lineH = 20, pad = 8, titleH = 16;
            double maxW = 0;
            for (int i = 0; i < nLines; i++) {
                cairo_text_extents_t te;
                int isTitle = (i == 0);
                int isDetail = (i > 0 && lines[i][0] == ' ');
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                    isTitle ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
                cairo_set_font_size(cr, isTitle ? 12 : (isDetail ? 12 : 13));
                cairo_text_extents(cr, lines[i], &te);
                if (te.width > maxW) maxW = te.width;
            }
            double boxX = 12, boxY = mY + 6;
            double boxW = maxW + pad * 2;
            double boxH = nLines * lineH + pad * 2 + titleH;
            cairo_set_source_rgba(cr, 1, 1, 1, 0.9);
            cairo_rectangle(cr, boxX, boxY, boxW, boxH);
            cairo_fill(cr);
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 12);
            cairo_set_source_rgb(cr, 0x2c/255.0, 0x3e/255.0, 0x50/255.0);
            cairo_move_to(cr, boxX + pad, boxY + pad);
            cairo_show_text(cr, lines[0]);
            for (int i = 1; i < nLines; i++) {
                int isDetail = (lines[i][0] == ' ');
                cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                    isDetail ? CAIRO_FONT_WEIGHT_NORMAL : CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, isDetail ? 12 : 13);
                cairo_set_source_rgb(cr, 0x2c/255.0, 0x3e/255.0, 0x50/255.0);
                cairo_move_to(cr, boxX + pad, boxY + pad + titleH + i * lineH);
                cairo_show_text(cr, lines[i]);
            }
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            mY = boxY + boxH + 6;
        }
    }

    // Time display (bold 16px monospace #667eea, top-right)
    char buf[64];
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    snprintf(buf, sizeof(buf), "t = %.1fs", time);
    text(cr, W - 12, 8, buf, 16, -1, -1, 0x66/255.0, 0x7e/255.0, 0xea/255.0, 1);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    // Legend (bottom-left)
    {
        double ly = H - 26;
        cairo_set_font_size(cr, 12);
        cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
        for (int i = 0; i < np; i++) {
            Point* p = &data->points[i];
            double px = 18, py = ly;
            set_color(cr, p->color);
            cairo_arc(cr, px, py, 5, 0, 2 * PI);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 0x55/255.0, 0x55/255.0, 0x55/255.0);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s (%s)", p->label,
                     p->numSegments > 0 ? "动点" : "定点");
            cairo_move_to(cr, 28, py + 3);
            cairo_show_text(cr, buf);
            ly += 20;
        }
    }
}
