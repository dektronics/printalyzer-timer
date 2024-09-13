#ifndef DISPLAY_ASSETS_H
#define DISPLAY_ASSETS_H

#include <stdint.h>

typedef struct {
    const uint8_t *bits;
    uint16_t width;
    uint16_t height;
} asset_info_t;

typedef enum {
    ASSET_THUMBDRIVE = 0,
    ASSET_USB_PORT,
    ASSET_MEMORY,
    ASSET_ARROW,
    ASSET_HOURGLASS,
    ASSET_QUESTION,
    ASSET_RESTART,
    ASSET_PADLOCK,
    ASSET_MAX
} asset_name_t;

uint8_t display_asset_get(asset_info_t *asset_info, asset_name_t asset_name);

#endif /* DISPLAY_ASSETS_H */
