#include "settings_util.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void copy_from_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

uint32_t copy_to_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] << 24
        | (uint32_t)buf[1] << 16
        | (uint32_t)buf[2] << 8
        | (uint32_t)buf[3];
}

void copy_from_f32(uint8_t *buf, float val)
{
    uint32_t int_val;
    memcpy(&int_val, &val, sizeof(float));
    copy_from_u32(buf, int_val);
}

float copy_to_f32(const uint8_t *buf)
{
    float val;
    uint32_t int_val = copy_to_u32(buf);
    memcpy(&val, &int_val, sizeof(float));
    return val;
}

void copy_from_u16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

uint16_t copy_to_u16(const uint8_t *buf)
{
    return (uint32_t)buf[0] << 8
        | (uint32_t)buf[1];
}
