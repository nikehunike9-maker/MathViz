#include "math_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_VARS 256
#define MAX_STACK 64

typedef struct {
    char name[64];
    enum { VAL_NUM, VAL_FUNC1 } type;
    double numVal;
    double (*func1)(double);
} MathVar;

static MathVar s_vars[MAX_VARS];
static int s_numVars = 0;

static void ensure_builtins(void) {
    static int inited = 0;
    if (inited) return;
    inited = 1;
    set_module_var("pi", 3.14159265358979323846);
    set_module_var("e", 2.718281828459045);
    set_module_func("sin", sin);
    set_module_func("cos", cos);
    set_module_func("tan", tan);
    set_module_func("asin", asin);
    set_module_func("acos", acos);
    set_module_func("atan", atan);
    set_module_func("sqrt", sqrt);
    set_module_func("fabs", fabs);
    set_module_func("abs", fabs);
    set_module_func("cbrt", cbrt);
    set_module_func("log10", log10);
    set_module_func("log", log);
    set_module_func("log2", log2);
    set_module_func("floor", floor);
    set_module_func("ceil", ceil);
    set_module_func("round", round);
    set_module_func("exp", exp);
    set_module_func("sinh", sinh);
    set_module_func("cosh", cosh);
    set_module_func("tanh", tanh);
}

void set_module_var(const char* name, double val) {
    // Update existing entry if found
    for (int i = 0; i < s_numVars; i++) {
        if (strcmp(s_vars[i].name, name) == 0) {
            s_vars[i].numVal = val;
            return;
        }
    }
    if (s_numVars >= MAX_VARS) return;
    strncpy(s_vars[s_numVars].name, name, 63);
    s_vars[s_numVars].type = VAL_NUM;
    s_vars[s_numVars].numVal = val;
    s_numVars++;
}

void set_module_func(const char* name, double (*func)(double)) {
    if (s_numVars >= MAX_VARS) return;
    strncpy(s_vars[s_numVars].name, name, 63);
    s_vars[s_numVars].type = VAL_FUNC1;
    s_vars[s_numVars].func1 = func;
    s_numVars++;
}

static double find_var(const char* name) {
    for (int i = 0; i < s_numVars; i++) {
        if (strcmp(s_vars[i].name, name) == 0) {
            if (s_vars[i].type == VAL_NUM) return s_vars[i].numVal;
            break;
        }
    }
    return NAN;
}

static MathVar* find_any(const char* name) {
    for (int i = 0; i < s_numVars; i++)
        if (strcmp(s_vars[i].name, name) == 0) return &s_vars[i];
    return NULL;
}

double math_dist(double x1, double y1, double x2, double y2) {
    return hypot(x1 - x2, y1 - y2);
}

double math_ang_deg(double x1, double y1, double x2, double y2, double x3, double y3) {
    double dx1 = x1 - x2, dy1 = y1 - y2;
    double dx2 = x3 - x2, dy2 = y3 - y2;
    return fabs(atan2(dx1 * dy2 - dy1 * dx2, dx1 * dx2 + dy1 * dy2) * 180.0 / 3.14159265358979323846);
}

double math_tri_area(double x1, double y1, double x2, double y2, double x3, double y3) {
    return fabs((x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2)) / 2.0);
}

double math_parallel(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) {
    return fabs((x2 - x1) * (y4 - y3) - (y2 - y1) * (x4 - x3)) < 0.001;
}

double math_perp(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) {
    return fabs((x2 - x1) * (x4 - x3) + (y2 - y1) * (y4 - y3)) < 0.001;
}

double compute_polygon_area(Vec2* pts, int n) {
    if (n < 3) return 0;
    double s = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        s += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return fabs(s) / 2.0;
}

double compute_perimeter(Vec2* pts, int n) {
    double sum = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        sum += math_dist(pts[i].x, pts[i].y, pts[j].x, pts[j].y);
    }
    return sum;
}

// --- Expression evaluator using shunting-yard ---
typedef struct {
    enum { TOK_NUM, TOK_OP, TOK_VAR, TOK_LPAREN, TOK_RPAREN, TOK_FUNC, TOK_COMMA } type;
    double numVal;
    char str[64];
} Token;

static int get_token(const char** s, Token* t) {
    while (**s && isspace((unsigned char)**s)) (*s)++;
    if (!**s) return 0;
    char c = **s;

    if (c == '+' || c == '-') {
        // Check if unary: previous token was operator, lparen, or beginning
        t->type = TOK_OP; t->str[0] = c; t->str[1] = 0; (*s)++; return 1;
    }
    if (c == '*' || c == '/' || c == '^') {
        t->type = TOK_OP; t->str[0] = c; t->str[1] = 0; (*s)++; return 1;
    }
    if (c == '(') { t->type = TOK_LPAREN; t->str[0] = c; t->str[1] = 0; (*s)++; return 1; }
    if (c == ')') { t->type = TOK_RPAREN; t->str[0] = c; t->str[1] = 0; (*s)++; return 1; }
    if (c == ',') { t->type = TOK_COMMA; t->str[0] = c; t->str[1] = 0; (*s)++; return 1; }

    if (isdigit((unsigned char)c) || c == '.') {
        int i = 0;
        while (**s && (isdigit((unsigned char)**s) || **s == '.' || **s == 'e' || **s == 'E')) {
            t->str[i++] = **s; (*s)++;
            if (**s == '-' || **s == '+') {
                if (i > 0 && (t->str[i-1] == 'e' || t->str[i-1] == 'E')) {
                    t->str[i++] = **s; (*s)++;
                } else break;
            }
        }
        t->str[i] = 0;
        t->numVal = atof(t->str);
        t->type = TOK_NUM;
        return 1;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        int i = 0;
        while (**s && (isalnum((unsigned char)**s) || **s == '_')) { t->str[i++] = **s; (*s)++; }
        t->str[i] = 0;

        // Check if it's a function (followed by open paren)
        // We'll handle this during parsing
        t->type = TOK_VAR;
        return 1;
    }

    t->type = TOK_OP; t->str[0] = c; t->str[1] = 0; (*s)++;
    return 1;
}

static int op_prec(const char* op) {
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 1;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return 2;
    if (strcmp(op, "^") == 0) return 3;
    return 0;
}

static double apply_op(double a, double b, const char* op) {
    if (strcmp(op, "+") == 0) return a + b;
    if (strcmp(op, "-") == 0) return a - b;
    if (strcmp(op, "*") == 0) return a * b;
    if (strcmp(op, "/") == 0) return b != 0 ? a / b : NAN;
    if (strcmp(op, "^") == 0) return pow(a, b);
    return NAN;
}

double eval_math_expr(const char* expr, double varVal, const char* varName) {
    ensure_builtins();
    if (!expr || !*expr) return NAN;

    // Check if expr is comparison and return 0/1
    const char* cmp = strstr(expr, "==");
    if (!cmp) cmp = strstr(expr, "!=");
    if (!cmp) cmp = strstr(expr, ">=");
    if (!cmp) cmp = strstr(expr, "<=");
    if (!cmp) cmp = strstr(expr, ">");
    if (!cmp) cmp = strstr(expr, "<");
    if (cmp) {
        // Simple comparison handling
        char left[512], right[512], comp[4];
        int ci = (int)(cmp - expr);
        strncpy(left, expr, ci); left[ci] = 0;
        // Trim left
        char* e = left + strlen(left) - 1;
        while (e >= left && isspace((unsigned char)*e)) *e-- = 0;
        
        const char* rstart = cmp;
        if (rstart[0] == '>' || rstart[0] == '<' || rstart[0] == '!') {
            comp[0] = rstart[0];
            if (rstart[1] == '=') { comp[1] = '='; comp[2] = 0; rstart += 2; }
            else { comp[1] = 0; rstart += 1; }
        } else if (rstart[0] == '=' && rstart[1] == '=') {
            comp[0] = '='; comp[1] = '='; comp[2] = 0; rstart += 2;
        }
        while (*rstart && isspace((unsigned char)*rstart)) rstart++;
        strncpy(right, rstart, 511); right[511] = 0;

        double lv = eval_math_expr(left, varVal, varName);
        double rv = eval_math_expr(right, varVal, varName);
        if (strcmp(comp, "==") == 0) return fabs(lv - rv) < 1e-10;
        if (strcmp(comp, "!=") == 0) return fabs(lv - rv) > 1e-10;
        if (strcmp(comp, ">") == 0) return lv > rv;
        if (strcmp(comp, "<") == 0) return lv < rv;
        if (strcmp(comp, ">=") == 0) return lv >= rv;
        if (strcmp(comp, "<=") == 0) return lv <= rv;
        return NAN;
    }

    // Check for && and ||
    const char* land = strstr(expr, "&&");
    const char* lor = strstr(expr, "||");
    if (land) {
        char left[512], right[512];
        int ci = (int)(land - expr);
        strncpy(left, expr, ci); left[ci] = 0;
        strncpy(right, land + 2, 511); right[511] = 0;
        double lv = eval_math_expr(left, varVal, varName);
        if (lv == 0) return 0;
        return eval_math_expr(right, varVal, varName);
    }
    if (lor) {
        char left[512], right[512];
        int ci = (int)(lor - expr);
        strncpy(left, expr, ci); left[ci] = 0;
        strncpy(right, lor + 2, 511); right[511] = 0;
        double lv = eval_math_expr(left, varVal, varName);
        if (lv != 0) return 1;
        return eval_math_expr(right, varVal, varName);
    }

    // Shunting-yard
    double valStack[MAX_STACK];
    char opStack[MAX_STACK][64];
    int vsp = 0, osp = 0;

    const char* p = expr;
    Token tok;
    int expectUnary = 1;

    while (get_token(&p, &tok)) {
        if (tok.type == TOK_NUM) {
            valStack[vsp++] = tok.numVal;
            expectUnary = 0;
        } else if (tok.type == TOK_VAR) {
            // Check if it's a function call (next token is '(')
            const char* peek = p;
            while (*peek && isspace((unsigned char)*peek)) peek++;
            if (*peek == '(') {
                // Function call - push function name and handle later
                strncpy(opStack[osp++], tok.str, 63);
                // Expect '(' to follow
            } else {
                // Variable
                if (strcmp(tok.str, varName) == 0) {
                    valStack[vsp++] = varVal;
                } else {
                    double v = find_var(tok.str);
                    valStack[vsp++] = isnan(v) ? 0 : v;
                }
                expectUnary = 0;
            }
        } else if (tok.type == TOK_LPAREN) {
            strncpy(opStack[osp++], "(", 63);
            expectUnary = 1;
        } else if (tok.type == TOK_RPAREN) {
            while (osp > 0 && strcmp(opStack[osp-1], "(") != 0) {
                osp--;
                if (vsp >= 2) {
                    double b = valStack[--vsp];
                    double a = valStack[--vsp];
                    valStack[vsp++] = apply_op(a, b, opStack[osp]);
                }
            }
            if (osp > 0 && strcmp(opStack[osp-1], "(") == 0) osp--; // pop '('
            // Check if previous was a function
            if (osp > 0) {
                MathVar* f = find_any(opStack[osp-1]);
                if (f && f->type == VAL_FUNC1) {
                    osp--;
                    if (vsp >= 1 && f->func1) {
                        double arg = valStack[--vsp];
                        valStack[vsp++] = f->func1(arg);
                    }
                }
            }
            expectUnary = 0;
        } else if (tok.type == TOK_OP) {
            // Handle unary minus
            if (expectUnary && strcmp(tok.str, "-") == 0) {
                valStack[vsp++] = 0;
                strncpy(opStack[osp++], "-", 63);
                expectUnary = 0;
                continue;
            }
            if (expectUnary && strcmp(tok.str, "+") == 0) {
                expectUnary = 0;
                continue;
            }
            while (osp > 0 && strcmp(opStack[osp-1], "(") != 0 &&
                   op_prec(opStack[osp-1]) >= op_prec(tok.str)) {
                osp--;
                if (vsp >= 2) {
                    double b = valStack[--vsp];
                    double a = valStack[--vsp];
                    valStack[vsp++] = apply_op(a, b, opStack[osp]);
                }
            }
            strncpy(opStack[osp++], tok.str, 63);
            expectUnary = 1;
        } else if (tok.type == TOK_COMMA) {
            while (osp > 0 && strcmp(opStack[osp-1], "(") != 0) {
                osp--;
                if (vsp >= 2) {
                    double b = valStack[--vsp];
                    double a = valStack[--vsp];
                    valStack[vsp++] = apply_op(a, b, opStack[osp]);
                }
            }
        }
    }

    while (osp > 0) {
        osp--;
        if (vsp >= 2) {
            double b = valStack[--vsp];
            double a = valStack[--vsp];
            valStack[vsp++] = apply_op(a, b, opStack[osp]);
        }
    }

    return vsp > 0 ? valStack[0] : NAN;
}

double eval_expr(const char* expr) {
    return eval_math_expr(expr, 0, "t");
}
