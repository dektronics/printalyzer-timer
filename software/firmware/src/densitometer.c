#include "densitometer.h"

#include <stdlib.h>
#include <math.h>
#include <esp_log.h>

#include "usb_host.h"

static const char *TAG = "densitometer";

static void densitometer_clear_reading(densitometer_reading_t *reading);
static densitometer_result_t densitometer_parse_reading(densitometer_reading_t *reading, const char *line);
static densitometer_result_t densitometer_parse_reading_heiland(densitometer_reading_t *reading, const char *line, size_t len);
static densitometer_result_t densitometer_parse_reading_xrite(densitometer_reading_t *reading, const char *line, size_t len);

void densitometer_clear_reading(densitometer_reading_t *reading)
{
    if (reading) {
        reading->mode = DENSITOMETER_MODE_UNKNOWN;
        reading->visual = NAN;
        reading->red = NAN;
        reading->green = NAN;
        reading->blue = NAN;
    }
}

densitometer_result_t densitometer_parse_reading(densitometer_reading_t *reading, const char *line)
{
    // First pass at parsing to log messages, planning to
    // simply handle all possible formats to avoid needing device
    // selection/detection except for advanced control features

    size_t len = strlen(line);

    /*
     * Decide which format we are parsing based on a few easy-to-check
     * characters in their respective strings. Since the Heiland format
     * is far more rigid, and has a similar prefix as some X-Rite readings,
     * it is easier to check for it first to avoid misidentification.
     */
    if (len == 7
        && (line[0] == 'T' || line[0] == 'R' || line[0] == '0')
        && (line[1] == '+' || line[1] == '-')
        && (line[6] == 'D' || line[6] == 'Z' || line[6] == '%')) {
        return densitometer_parse_reading_heiland(reading, line, len);
    }
    else if (len >= 4 &&
        (line[0] == 'V' || line[0] == 'R' || line[0] == 'G' || line[0] == 'B' ||
        line[0] == 'v' || line[0] == 'r' || line[0] == 'g' || line[0] == 'b' ||
        line[0] == 'p' || line[0] == 'c' || line[0] == 'm' || line[0] == 'y' ||
        line[0] == 'P' || line[0] == 'C' || line[0] == 'M' || line[0] == 'Y')
        && ((line[1] == '-' && line[2] >= '0' && line[2] <= '9')
            || (line[1] >= '0' && line[1] <= '9'))) {
        return densitometer_parse_reading_xrite(reading, line, len);
    }
    else {
        return DENSITOMETER_RESULT_UNKNOWN;
    }
}

densitometer_result_t densitometer_parse_reading_heiland(densitometer_reading_t *reading, const char *line, size_t len)
{
    /*
     * Heiland TRD-2 Data Format
     *
     *  Index | Values  | Description
     * -------+---------+---------------------------------------
     *      0 | T/R/0   | Transmission/Reflection/StandBy
     *      1 | +/-     | Sign
     *      2 | 0...9/. | Value MSB
     *      3 | 0...9/. | Value
     *      4 | 0...9/. | Value
     *      5 | 0...9/. | Value LSB
     *      6 | D/Z/%   | Density/Zone/Percentage of dot area
     *      7 | <CR>    | End of string
     *
     * Examples:
     * "T+1.53D<cr>", "R-0.05D<cr>", "T+.278D<cr>", "0+....D<cr>"
     */

    char num_buf[6];

    /* Parse the reading mode */
    densitometer_mode_t mode = DENSITOMETER_MODE_UNKNOWN;
    if (line[0] == 'T') {
        mode = DENSITOMETER_MODE_TRANSMISSION;
    } else if (line[0] == 'R') {
        mode = DENSITOMETER_MODE_REFLECTION;
    } else {
        return DENSITOMETER_RESULT_INVALID;
    }

    /* Make sure we are only reading density */
    if (line[6] != 'D') {
        return DENSITOMETER_RESULT_INVALID;
    }

    /* Copy the numeric portion to a separate buffer for parsing */
    strncpy(num_buf, line + 1, 5);
    num_buf[5] = '\0';

    /* Parse and validate the number */
    float val = strtof(num_buf, NULL);
    if (!isnormal(val) && fpclassify(val) != FP_ZERO) {
        return DENSITOMETER_RESULT_INVALID;
    }

    /* Clean up zero values */
    if (fabs(val) < 0.001) {
        val = 0.0F;
    }

    densitometer_clear_reading(reading);
    reading->mode = mode;
    reading->visual = val;
    return DENSITOMETER_RESULT_OK;
}

densitometer_result_t densitometer_parse_reading_xrite(densitometer_reading_t *reading, const char *line, size_t len)
{
    /*
     * X-Rite 810 Data Format
     *
     * All on one line:
     * VX.XX<sp>RX.XX<sp>GX.XX<sp>BX.XX<sp>
     *
     * Separate lines:
     * VX.XX<cr>
     * RX.XX<cr>
     * GX.XX<cr>
     * BX.XX<cr>
     *
     * Two decimal-point formats:
     * VX.XX or VXXX
     *
     * AIDm:
     * T: p000, R: P000
     * T: c004 m008 y000, R: C033 M030 Y033
     *
     * AIDa:
     * T: v000, R: P030
     * T: r003 g007 b000, R: C033 M030 Y033
     *
     */

    densitometer_reading_t working_reading;
    densitometer_clear_reading(&working_reading);

    const char *p = line;
    char *q = NULL;
    char *r = NULL;
    do {
        char prefix = *p;
        p++;
        float val = strtof(p, &q);

        /* Check if a valid number was parsed */
        if (!isnormal(val) && fpclassify(val) != FP_ZERO) {
            break;
        }

        /* Check if an implied decimal point needs to be added */
        r = strchr(p, '.');
        if (!r || r >= q) {
            val /= 100.0f;
        }

        /* Clean up zero values */
        if (fabs(val) < 0.001) {
            val = 0.0F;
        }

        /* Figure out the reading mode, if indicated by the prefix */
        if (working_reading.mode == DENSITOMETER_MODE_UNKNOWN) {
            switch (prefix) {
            case 'p':
            case 'c':
            case 'm':
            case 'y':
            case 'v':
            case 'r':
            case 'g':
            case 'b':
                working_reading.mode = DENSITOMETER_MODE_TRANSMISSION;
                break;
            case 'P':
            case 'C':
            case 'M':
            case 'Y':
                working_reading.mode = DENSITOMETER_MODE_REFLECTION;
                break;
            default:
                break;
            }
        }

        /* Assign the reading value based on the prefix */
        switch (prefix) {
        case 'p':
        case 'P':
        case 'v':
        case 'V':
            working_reading.visual = val;
            break;
        case 'c':
        case 'C':
        case 'r':
        case 'R':
            working_reading.red = val;
            break;
        case 'm':
        case 'M':
        case 'g':
        case 'G':
            working_reading.green = val;
            break;
        case 'y':
        case 'Y':
        case 'b':
        case 'B':
            working_reading.blue = val;
            break;
        default:
            break;
        }

        if (q) {
            p = q;
            if (*p == ' ') {
                p++;
            }
        } else {
            break;
        }
    } while (p && *p != '\0');

    if (!isnanf(working_reading.visual)
        || !isnanf(working_reading.red)
        || !isnanf(working_reading.green)
        || !isnanf(working_reading.blue)) {
        memcpy(reading, &working_reading, sizeof(densitometer_reading_t));
        return DENSITOMETER_RESULT_OK;
    } else {
        return DENSITOMETER_RESULT_INVALID;
    }
}

densitometer_result_t densitometer_reading_poll(densitometer_reading_t *reading, uint32_t ms_to_wait)
{
    char line_buf[64];

    if (!reading) {
        return DENSITOMETER_RESULT_INVALID;
    }

    if (!usb_serial_is_attached()) {
        return DENSITOMETER_RESULT_NOT_CONNECTED;
    }

    if (usb_serial_receive_line((uint8_t *)line_buf, sizeof(line_buf), ms_to_wait) == USBH_OK) {
        return densitometer_parse_reading(reading, line_buf);
    } else {
        return DENSITOMETER_RESULT_TIMEOUT;
    }
}

void densitometer_log_reading(const densitometer_reading_t *reading)
{
    if (reading) {
        char mode_ch;
        switch (reading->mode) {
        case DENSITOMETER_MODE_TRANSMISSION:
            mode_ch = 'T';
            break;
        case DENSITOMETER_MODE_REFLECTION:
            mode_ch = 'R';
            break;
        case DENSITOMETER_MODE_UNKNOWN:
        default:
            mode_ch = 'U';
            break;
        }
        ESP_LOGI(TAG, "mode=%c, vis=%0.02f, red=%0.02f, green=%0.02f, blue=%0.02f",
            mode_ch, reading->visual, reading->red, reading->green, reading->blue);
    } else {
        ESP_LOGI(TAG, "Reading invalid");
    }
}
