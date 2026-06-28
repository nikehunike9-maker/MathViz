#ifndef MODULE_H
#define MODULE_H

#include "dsl.h"

void module_init(void);
int module_load(const char* name);
void module_apply(AnimationData* data);

#endif
