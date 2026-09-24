#ifndef TSPGL_GLES1_H
#define TSPGL_GLES1_H

#include <stdint.h>

void tspgl_es1_set_pointer(unsigned index, int size, uint32_t type, int normalized,
                           int stride, const void *ptr);
void tspgl_es1_enable_array(unsigned index, int on);
void tspgl_es1_set_active_tex(uint32_t texture);
void tspgl_es1_bind_tex(uint32_t target, uint32_t id);
int tspgl_es1_cap(uint32_t cap);
int tspgl_es1_set_enable(uint32_t cap, int on);
int tspgl_es1_is_enabled(uint32_t cap);
int tspgl_es1_get_integerv(uint32_t pname, int32_t *params);
int tspgl_es1_get_floatv(uint32_t pname, float *params);
int tspgl_es1_get_booleanv(uint32_t pname, uint8_t *params);
int tspgl_es1_get_fixedv(uint32_t pname, int32_t *params);
void *tspgl_es1_get_proc(const char *name);

#endif
