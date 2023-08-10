#ifndef SETTINGS_UTIL_H
#define SETTINGS_UTIL_H

#include <stdint.h>
#include <stdbool.h>

void copy_from_u32(uint8_t *buf, uint32_t val);
uint32_t copy_to_u32(const uint8_t *buf);
void copy_from_f32(uint8_t *buf, float val);
float copy_to_f32(const uint8_t *buf);
void copy_from_u16(uint8_t *buf, uint16_t val);
uint16_t copy_to_u16(const uint8_t *buf);

#endif /* SETTINGS_UTIL_H */
