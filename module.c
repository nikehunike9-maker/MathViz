#include "module.h"
#include "math_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#ifdef __unix__
#include <dlfcn.h>
#endif

#define MAX_LOADED 8

typedef struct {
    char name[64];
    void* handle;
} LoadedModule;

static LoadedModule s_loaded[MAX_LOADED];
static int s_numLoaded = 0;

static double sigmoid_func(double x) { return 1.0 / (1.0 + exp(-x)); }
static double relu_func(double x) { return x > 0 ? x : 0; }
static double deg2rad_func(double x) { return x * 3.14159265358979323846 / 180.0; }
static double rad2deg_func(double x) { return x * 180.0 / 3.14159265358979323846; }
static double gauss_func(double x) { return exp(-x * x / 2.0) / sqrt(2.0 * 3.14159265358979323846); }

void module_init(void) {
    set_module_var("fib_10", 55.0);
    set_module_var("fib_20", 6765.0);
    set_module_var("inv_sqrt2pi", 1.0 / sqrt(2.0 * 3.14159265358979323846));
    set_module_var("sqrt2", sqrt(2.0));
    set_module_func("sigmoid", sigmoid_func);
    set_module_func("relu", relu_func);
    set_module_func("deg2rad", deg2rad_func);
    set_module_func("rad2deg", rad2deg_func);
    set_module_func("gauss", gauss_func);
    // These are already in math_engine builtins, but also register them for consistency:
    // Binary funcs (atan2, pow, min, max) are handled as ops in math_engine, not as func calls
}

int module_load(const char* name) {
    if (s_numLoaded >= MAX_LOADED) return -1;

    if (strcmp(name, "math_extra") == 0 ||
        strcmp(name, "builtin") == 0 ||
        strcmp(name, "geometry_extra") == 0) {
        return 0;
    }

#ifdef __unix__
    char libname[256];
    snprintf(libname, sizeof(libname), "extensions/lib%s.so", name);
    void* handle = dlopen(libname, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Module load failed for '%s': %s\n", name, dlerror());
        return -1;
    }
    LoadedModule* mod = &s_loaded[s_numLoaded++];
    strncpy(mod->name, name, 63);
    mod->handle = handle;
#endif
    return 0;
}

void module_apply(AnimationData* data) {
    if (!data) return;

    // Apply module transformations for each import
    for (int i = 0; i < data->numImports; i++) {
        const char* name = data->imports[i].name;

        if (strcmp(name, "math_extra") == 0 || strcmp(name, "builtin") == 0) {
            // Already handled in module_init
            continue;
        }

        // Add imported name as a module variable prefix (for expressions like modulename.func)
        if (data->imports[i].alias[0]) {
            set_module_var(data->imports[i].alias, 1.0);
        }
    }
}
