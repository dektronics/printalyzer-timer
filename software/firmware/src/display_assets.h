#ifndef DISPLAY_ASSETS_H
#define DISPLAY_ASSETS_H

#include <stdint.h>

typedef struct {
    uint8_t *bits;
    uint16_t width;
    uint16_t height;
} asset_info_t;

typedef enum {
    ASSET_PRINTALYZER,
    ASSET_TIMER_ICON_48,
    ASSET_TIMER_ICON_24,
    ASSET_EXPOSURE_ADJ_ICON_48,
    ASSET_TIMER_ADJ_ICON_48
} asset_name_t;

uint8_t display_asset_get(asset_info_t *asset_info, asset_name_t asset_name);

#endif /* DISPLAY_ASSETS_H */
