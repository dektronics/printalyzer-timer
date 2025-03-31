#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <ff.h>

int json_count_elements(const char *buf, size_t len);
int json_parse_int(const char *buf, size_t len, int def_value);
float json_parse_float(const char *buf, size_t len, float def_value);

#define json_write(fp, indent, key, val, has_more) \
    _Generic((val),                      \
        const char *: json_write_string, \
        char *: json_write_string,       \
        int: json_write_int,             \
        int32_t: json_write_int,         \
        int16_t: json_write_int,         \
        int8_t: json_write_int,          \
        unsigned int: json_write_uint,   \
        uint32_t: json_write_uint,       \
        uint16_t: json_write_uint,       \
        uint8_t: json_write_uint,        \
        bool: json_write_bool)           \
        (fp, indent, key, val, has_more)

void json_write_string(FIL *fp, int indent, const char *key, const char *val, bool has_more);
void json_write_int(FIL *fp, int indent, const char *key, int val, bool has_more);
void json_write_uint(FIL *fp, int indent, const char *key, unsigned int val, bool has_more);
void json_write_float02(FIL *fp, int indent, const char *key, float val, bool has_more);
void json_write_float06(FIL *fp, int indent, const char *key, float val, bool has_more);
void json_write_bool(FIL *fp, int indent, const char *key, bool val, bool has_more);

#endif /* JSON_UTIL_H */
