#ifndef MATH_ENGINE_H
#define MATH_ENGINE_H

#include "dsl.h"
#include <math.h>

double eval_math_expr(const char* expr, double varVal, const char* varName);
double eval_expr(const char* expr);

double math_dist(double x1, double y1, double x2, double y2);
double math_ang_deg(double x1, double y1, double x2, double y2, double x3, double y3);
double math_tri_area(double x1, double y1, double x2, double y2, double x3, double y3);
double math_parallel(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
double math_perp(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);

void set_module_var(const char* name, double val);
void set_module_func(const char* name, double (*func)(double));
double compute_polygon_area(Vec2* pts, int n);
double compute_perimeter(Vec2* pts, int n);

#endif
