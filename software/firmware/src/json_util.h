#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <ff.h>

int json_count_elements(const char *buf, size_t len);
int json_parse_int(const char *buf, size_t len, int def_value);
float json_parse_float(const char *buf, size_t len, float def_value);

void json_write_string(FIL *fp, int indent, const char *key, const char *val, bool has_more);
void json_write_int(FIL *fp, int indent, const char *key, int val, bool has_more);
void json_write_float02(FIL *fp, int indent, const char *key, float val, bool has_more);
void json_write_float06(FIL *fp, int indent, const char *key, float val, bool has_more);

#endif /* JSON_UTIL_H */
