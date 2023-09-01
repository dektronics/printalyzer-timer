#include "json_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <ff.h>

#include "util.h"
#include "core_json.h"

int json_count_elements(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int count = 0;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        count++;
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return count;
}

int json_parse_int(const char *buf, size_t len, int def_value)
{
    char num_buf[12];
    char *endptr;
    long result;

    if (!buf || len == 0 || len > sizeof(num_buf) - 1) {
        return def_value;
    }

    memset(num_buf, 0, sizeof(num_buf));
    memcpy(num_buf, buf, len);

    errno = 0;
    result = strtol(num_buf, &endptr, 10);
    if (endptr == num_buf || *endptr != '\0' ||
        ((result == LONG_MIN || result == LONG_MAX) && errno == ERANGE)) {
        result = def_value;
    }
    return result;
}

float json_parse_float(const char *buf, size_t len, float def_value)
{
    char num_buf[32];
    char *endptr;
    float result;

    if (!buf || len == 0 || len > sizeof(num_buf) - 1) {
        return def_value;
    }

    memset(num_buf, 0, sizeof(num_buf));
    memcpy(num_buf, buf, len);

    errno = 0;
    result = strtof(num_buf, &endptr);
    if (endptr == num_buf || *endptr != '\0' ||
        ((result == -HUGE_VALF || result == HUGE_VALF) && errno == ERANGE)) {
        result = def_value;
    }
    return result;
}

void json_write_string(FIL *fp, int indent, const char *key, const char *val, bool has_more)
{
    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": \"%s\"", key, val);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

void json_write_int(FIL *fp, int indent, const char *key, int val, bool has_more)
{
    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": %d", key, val);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

void json_write_float02(FIL *fp, int indent, const char *key, float val, bool has_more)
{
    char buf[32];
    if (!is_valid_number(val)) {
        sprintf(buf, "null");
    } else {
        sprintf(buf, "%0.2f", val);
    }

    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": %s", key, buf);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

