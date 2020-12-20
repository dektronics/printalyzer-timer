#include "tcs3472.h"

#include "stm32f4xx_hal.h"

#include "esp_log.h"
#include "i2c_util.h"

static const char *TAG = "tcs3472";

/* I2C device address */
static const uint8_t TCS3472_ADDRESS = 0x29 << 1; // Use 8-bit address

/* Registers */
#define TCS3472_ENABLE  0x00 // Enables states and interrupts
#define TCS3472_ATIME   0x01 // RGBC time
#define TCS3472_WTIME   0x03 // Wait time
#define TCS3472_AILTL   0x04 // Clear interrupt low threshold low byte
#define TCS3472_AILTH   0x05 // Clear interrupt low threshold high byte
#define TCS3472_AIHTL   0x06 // Clear interrupt high threshold low byte
#define TCS3472_AIHTH   0x07 // Clear interrupt high threshold high byte
#define TCS3472_PERS    0x0C // Interrupt persistence filter
#define TCS3472_CONFIG  0x0D // Configuration
#define TCS3472_CONTROL 0x0F // Control
#define TCS3472_ID      0x12 // Device ID
#define TCS3472_STATUS  0x13 // Device status
#define TCS3472_CDATAL  0x14 // Clear data low byte
#define TCS3472_CDATAH  0x15 // Clear data high byte
#define TCS3472_RDATAL  0x16 // Red data low byte
#define TCS3472_RDATAH  0x17 // Red data high byte
#define TCS3472_GDATAL  0x18 // Green data low byte
#define TCS3472_GDATAH  0x19 // Green data high byte
#define TCS3472_BDATAL  0x1A // Blue data low byte
#define TCS3472_BDATAH  0x1B // Blue data high byte

/* Command Register Values */
#define TCS3472_CMD                0x80 // Select command register
#define TCS3472_CMD_TYPE_REPEATED  0x00 // Repeated byte protocol transaction
#define TCS3472_CMD_TYPE_AUTO_INC  0x20 // Auto-increment protocol transaction
#define TCS3472_CMD_TYPE_INT_CLEAR 0x66 // Special function - clear channel interrupt clear

/* Enable Register Values */
#define TCS3472_ENABLE_AIEN        0x10 // RGBC interrupt enable
#define TCS3472_ENABLE_WEN         0x08 // Wait enable
#define TCS3472_ENABLE_AEN         0x02 // RGBC enable
#define TCS3472_ENABLE_PON         0x01 // Power ON

/* Coefficients for Lux and CT equations */
#define TCS3472_GA        (1.0F)
#define TCS3472_DF        (310)
#define TCS3472_R_COEF    (0.136F)
#define TCS3472_G_COEF    (1.000F)
#define TCS3472_B_COEF    (-0.444F)
#define TCS3472_CT_COEF   (3810)
#define TCS3472_CT_OFFSET (1391)

static tcs3472_again_t _gain = TCS3472_AGAIN_1X;
static tcs3472_atime_t _integration = TCS3472_ATIME_2_4MS;

HAL_StatusTypeDef tcs3472_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ESP_LOGI(TAG, "Initializing TCS3472");

    ret = i2c_read_register(hi2c, TCS3472_ADDRESS, TCS3472_CMD | TCS3472_ID, &data);
    if (ret != HAL_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Device ID: %02X", data);

    if (data != 0x44 && data != 0x4D) {
        ESP_LOGE(TAG, "Invalid Device ID");
        return HAL_ERROR;
    }

    ret = i2c_read_register(hi2c, TCS3472_ADDRESS, TCS3472_CMD | TCS3472_STATUS, &data);
    if (ret != HAL_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Status: %02X", data);

    // Power on the sensor
    ret = tcs3472_enable(hi2c);
    if (ret != HAL_OK) {
        return ret;
    }

    // Set default integration time and gain
    _gain = TCS3472_AGAIN_1X;
    ret = tcs3472_set_gain(hi2c, _gain);
    if (ret != HAL_OK) {
        return ret;
    }
    _integration = TCS3472_ATIME_2_4MS;
    ret = tcs3472_set_time(hi2c, _integration);
    if (ret != HAL_OK) {
        return ret;
    }


    // Power off the sensor
    ret = tcs3472_disable(hi2c);
    if (ret != HAL_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "TCS3472 Initialized");

    return HAL_OK;
}

HAL_StatusTypeDef tcs3472_enable(I2C_HandleTypeDef *hi2c)
{
    return i2c_write_register(hi2c, TCS3472_ADDRESS,
        TCS3472_CMD | TCS3472_ENABLE,
        TCS3472_ENABLE_PON | TCS3472_ENABLE_AEN);
}

HAL_StatusTypeDef tcs3472_disable(I2C_HandleTypeDef *hi2c)
{
    return i2c_write_register(hi2c, TCS3472_ADDRESS,
        TCS3472_CMD | TCS3472_ENABLE, 0x00);
}

HAL_StatusTypeDef tcs3472_set_time(I2C_HandleTypeDef *hi2c, tcs3472_atime_t atime)
{
    HAL_StatusTypeDef ret = i2c_write_register(hi2c, TCS3472_ADDRESS,
        TCS3472_CMD | TCS3472_ATIME, (uint8_t)atime);
    if (ret == HAL_OK) {
        _integration = atime;
    }
    return ret;
}

HAL_StatusTypeDef tcs3472_get_time(I2C_HandleTypeDef *hi2c, tcs3472_atime_t *atime)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data;

    ret = i2c_read_register(hi2c, TCS3472_ADDRESS, TCS3472_CMD | TCS3472_ATIME, &data);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "i2c_read_register error: %d", ret);
        return ret;
    }

    if (atime) {
        *atime = data;
    }

    return ret;
}

HAL_StatusTypeDef tcs3472_set_gain(I2C_HandleTypeDef *hi2c, tcs3472_again_t gain)
{
    if (gain < TCS3472_AGAIN_1X || gain > TCS3472_AGAIN_60X) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef ret = i2c_write_register(hi2c, TCS3472_ADDRESS,
        TCS3472_CMD | TCS3472_CONTROL, ((uint8_t)gain) & 0x03);
    if (ret == HAL_OK) {
        _gain = gain;
    }
    return ret;
}

HAL_StatusTypeDef tcs3472_get_gain(I2C_HandleTypeDef *hi2c, tcs3472_again_t *gain)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data;

    ret = i2c_read_register(hi2c, TCS3472_ADDRESS, TCS3472_CMD | TCS3472_CONTROL, &data);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "i2c_read_register error: %d", ret);
        return ret;
    }

    if (gain) {
        *gain = data & 0x03;
    }

    return ret;
}

HAL_StatusTypeDef tcs3472_get_status_valid(I2C_HandleTypeDef *hi2c, bool *valid)
{
    if (!valid) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data;

    ret = i2c_read_register(hi2c, TCS3472_ADDRESS, TCS3472_CMD | TCS3472_STATUS, &data);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "i2c_read_register error: %d", ret);
        return ret;
    }

    *valid = (data & 0x01) ? true : false;

    return ret;
}

HAL_StatusTypeDef tcs3472_get_full_channel_data(I2C_HandleTypeDef *hi2c, tcs3472_channel_data_t *ch_data)
{
    HAL_StatusTypeDef ret;
    uint8_t data[8];

    if (!ch_data) {
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCS3472_ADDRESS,
        TCS3472_CMD | TCS3472_CDATAL, I2C_MEMADD_SIZE_8BIT,
        data, sizeof(data), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ch_data->clear = data[0] | data[1] << 8;
    ch_data->red = data[2] | data[3] << 8;
    ch_data->green = data[4] | data[5] << 8;
    ch_data->blue = data[6] | data[7] << 8;
    ch_data->integration = _integration;
    ch_data->gain = _gain;

    return HAL_OK;
}

const char* tcs3472_atime_str(tcs3472_atime_t atime)
{
    switch (atime) {
    case TCS3472_ATIME_2_4MS:
        return "2.4ms";
    case TCS3472_ATIME_4_8MS:
        return "4.8ms";
    case TCS3472_ATIME_7_2MS:
        return "7.2ms";
    case TCS3472_ATIME_9_6MS:
        return "9.6ms";
    case TCS3472_ATIME_24MS:
        return "24ms";
    case TCS3472_ATIME_50MS:
        return "50ms";
    case TCS3472_ATIME_101MS:
        return "101ms";
    case TCS3472_ATIME_154MS:
        return "154ms";
    case TCS3472_ATIME_614MS:
        return "614ms";
    default:
        return "?";
    }
}

uint16_t tcs3472_atime_max_count(tcs3472_atime_t atime)
{
    return (256 - (uint16_t)atime) * 1024;
}

const char* tcs3472_gain_str(tcs3472_again_t gain)
{
    switch (gain) {
    case TCS3472_AGAIN_1X:
        return "1x";
    case TCS3472_AGAIN_4X:
        return "4x";
    case TCS3472_AGAIN_16X:
        return "16x";
    case TCS3472_AGAIN_60X:
        return "60x";
    default:
        return "?";
    }
}

uint16_t tcs3472_calculate_color_temp(const tcs3472_channel_data_t *ch_data)
{
    // Calculate color temperature using the algorithm from DN40
    // Based on Adafruit sample code:
    // https://github.com/adafruit/Adafruit_TCS34725

    uint16_t r2, b2; /* RGB values minus IR component */
    uint16_t sat;    /* Digital saturation level */
    uint16_t ir;     /* Inferred IR content */

    if (!ch_data) {
        return HAL_ERROR;
    }

    if (ch_data->clear == 0) {
        return 0;
    }

    /* Analog/Digital saturation:
     *
     * (a) As light becomes brighter, the clear channel will tend to
     *     saturate first since R+G+B is approximately equal to C.
     * (b) The TCS34725 accumulates 1024 counts per 2.4ms of integration
     *     time, up to a maximum values of 65535. This means analog
     *     saturation can occur up to an integration time of 153.6ms
     *     (64*2.4ms=153.6ms).
     * (c) If the integration time is > 153.6ms, digital saturation will
     *     occur before analog saturation. Digital saturation occurs when
     *     the count reaches 65535.
     */
    if ((256 - ch_data->integration) > 63) {
        /* Track digital saturation */
        sat = 65535;
    } else {
        /* Track analog saturation */
        sat = 1024 * (256 - ch_data->integration);
    }

    /* Ripple rejection:
     *
     * (a) An integration time of 50ms or multiples of 50ms are required to
     *     reject both 50Hz and 60Hz ripple.
     * (b) If an integration time faster than 50ms is required, you may need
     *     to average a number of samples over a 50ms period to reject ripple
     *     from fluorescent and incandescent light sources.
     *
     * Ripple saturation notes:
     *
     * (a) If there is ripple in the received signal, the value read from C
     *     will be less than the max, but still have some effects of being
     *     saturated. This means that you can be below the 'sat' value, but
     *     still be saturating. At integration times >150ms this can be
     *     ignored, but <= 150ms you should calculate the 75% saturation
     *     level to avoid this problem.
     */
    if ((256 - ch_data->integration) <= 63) {
        /* Adjust sat to 75% to avoid analog saturation if atime < 153.6ms */
        sat -= sat / 4;
    }

    /* Check for saturation and mark the sample as invalid if true */
    if (ch_data->clear >= sat) {
        return 0;
    }

    /* AMS RGB sensors have no IR channel, so the IR content must be */
    /* calculated indirectly. */
    ir = (ch_data->red + ch_data->green + ch_data->blue > ch_data->clear) ? (ch_data->red + ch_data->green + ch_data->blue - ch_data->clear) / 2 : 0;

    /* Remove the IR component from the raw RGB values */
    r2 = ch_data->red - ir;
    b2 = ch_data->blue - ir;

    if (r2 == 0) {
        return 0;
    }

    /* A simple method of measuring color temp is to use the ratio of blue */
    /* to red light, taking IR cancellation into account. */
    uint16_t cct = (TCS3472_CT_COEF * (uint32_t)b2) / /** Color temp coefficient. */
        (uint32_t)r2 +
        TCS3472_CT_OFFSET; /** Color temp offset. */

    return cct;
}

float tcs3472_calculate_lux(const tcs3472_channel_data_t *ch_data)
{
    uint16_t r2, g2, b2; /* RGB values minus IR component */
    uint16_t sat;        /* Digital saturation level */
    uint16_t ir;         /* Inferred IR content */
    uint16_t again_x;
    float atime_ms;
    float cpl;
    float lux;

    if (!ch_data) {
        return HAL_ERROR;
    }

    // Saturation handling copied from above function
    if ((256 - ch_data->integration) > 63) {
        /* Track digital saturation */
        sat = 65535;
    } else {
        /* Track analog saturation */
        sat = 1024 * (256 - ch_data->integration);
    }

    if ((256 - ch_data->integration) <= 63) {
        /* Adjust sat to 75% to avoid analog saturation if atime < 153.6ms */
        sat -= sat / 4;
    }

    /* Check for saturation and mark the sample as invalid if true */
    if (ch_data->clear >= sat) {
        return 0;
    }

    /* AMS RGB sensors have no IR channel, so the IR content must be */
    /* calculated indirectly. */
    // IR = (R + G + B – C)/2
    ir = (ch_data->red + ch_data->green + ch_data->blue > ch_data->clear) ? (ch_data->red + ch_data->green + ch_data->blue - ch_data->clear) / 2 : 0;

    /* Remove the IR component from the raw RGB values */
    // R’ = R – IR, G’ = G – IR, B’ = B – IR
    r2 = ch_data->red - ir;
    g2 = ch_data->green - ir;
    b2 = ch_data->blue - ir;

    /* Find the numeric gain value */
    switch (ch_data->gain) {
    case TCS3472_AGAIN_1X:
        again_x = 1;
        break;
    case TCS3472_AGAIN_4X:
        again_x = 4;
        break;
    case TCS3472_AGAIN_16X:
        again_x = 16;
        break;
    case TCS3472_AGAIN_60X:
        again_x = 60;
        break;
    default:
        return 0;
    }

    /* Find the numeric integration time value */
    atime_ms = 2.4 * (256 - ch_data->integration);

    // CPL = (AGAINx * ATIME_ms) / (GA * DF)
    cpl = (again_x * atime_ms) / (TCS3472_GA * TCS3472_DF);

    // G” = R_Coef * R’ + G_Coef * G’ + B_Coef * B’
    // Lux = G” / CPL
    lux = ((TCS3472_R_COEF * r2) + (TCS3472_G_COEF * g2) + (TCS3472_B_COEF * b2)) / cpl;

    return lux;
}
