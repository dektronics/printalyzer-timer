#include "usb_hid_keyboard.h"

#include <cmsis_os.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "keypad.h"

#define LOG_TAG "usb_kbd"
#include <elog.h>

typedef struct {
    uint8_t state;
    uint8_t keys[6];
} keyboard_info_t;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_buffer[128];

static uint8_t usb_hid_keyboard_report_state = 0x00;
static keyboard_info_t usb_hid_keyboard_last_event = {0};
static uint32_t usb_hid_keyboard_last_event_time = 0;

static void usbh_hid_keyboard_thread(void *argument);
static void usbh_hid_keyboard_callback(void *arg, int nbytes);
static uint8_t keyboard_ascii_code(const keyboard_info_t *info);
static void keyboard_process_event(struct usbh_hid *hid_class, const keyboard_info_t *info);

static const uint8_t  keyboard_keys[] = {
    '\0',  '`',  '1',  '2',  '3',  '4',  '5',  '6',
    '7',  '8',  '9',  '0',  '-',  '=',  '\0', '\r',
    '\t',  'q',  'w',  'e',  'r',  't',  'y',  'u',
    'i',  'o',  'p',  '[',  ']',  '\\',
    '\0',  'a',  's',  'd',  'f',  'g',  'h',  'j',
    'k',  'l',  ';',  '\'', '\0', '\n',
    '\0',  '\0', 'z',  'x',  'c',  'v',  'b',  'n',
    'm',  ',',  '.',  '/',  '\0', '\0',
    '\0',  '\0', '\0', ' ',  '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0',  '\0', '\0', '\0', '\0', '\r', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0',
    '\0',  '\0', '7',  '4',  '1',
    '\0',  '/',  '8',  '5',  '2',
    '0',   '*',  '9',  '6',  '3',
    '.',   '-',  '+',  '\0', '\n', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0',  '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0'
};

static  const  uint8_t  keyboard_shift_keys[] = {
    '\0', '~',  '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
    '_',  '+',  '\0', '\0', '\0', 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',
    'I',  'O',  'P',  '{',  '}',  '|',  '\0', 'A',  'S',  'D',  'F',  'G',
    'H',  'J',  'K',  'L',  ':',  '"',  '\0', '\n', '\0', '\0', 'Z',  'X',
    'C',  'V',  'B',  'N',  'M',  '<',  '>',  '?',  '\0', '\0',  '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0',    '\0', '\0', '\0', '\0', '\0',
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'
};

static const uint8_t keyboard_codes[] = {
    0,     0,    0,    0,   31,   50,   48,   33,
    19,   34,   35,   36,   24,   37,   38,   39,       /* 0x00 - 0x0F */
    52,   51,   25,   26,   17,   20,   32,   21,
    23,   49,   18,   47,   22,   46,    2,    3,       /* 0x10 - 0x1F */
    4,     5,    6,    7,    8,    9,   10,   11,
    43,  110,   15,   16,   61,   12,   13,   27,       /* 0x20 - 0x2F */
    28,   29,   42,   40,   41,    1,   53,   54,
    55,   30,  112,  113,  114,  115,  116,  117,       /* 0x30 - 0x3F */
    118, 119,  120,  121,  122,  123,  124,  125,
    126,  75,   80,   85,   76,   81,   86,   89,       /* 0x40 - 0x4F */
    79,   84,   83,   90,   95,  100,  105,  106,
    108,  93,   98,  103,   92,   97,  102,   91,       /* 0x50 - 0x5F */
    96,  101,   99,  104,   45,  129,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0x60 - 0x6F */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0x70 - 0x7F */
    0,     0,    0,    0,    0,  107,    0,   56,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0x80 - 0x8F */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0x90 - 0x9F */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0xA0 - 0xAF */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0xB0 - 0xBF */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0xC0 - 0xCF */
    0,     0,    0,    0,    0,    0,    0,    0,
    0,     0,    0,    0,    0,    0,    0,    0,       /* 0xD0 - 0xDF */
    58,   44,   60,  127,   64,   57,   62,  128        /* 0xE0 - 0xE7 */
};


void usbh_hid_keyboard_attached(struct usbh_hid *hid_class)
{
    //TODO create a context handle to keep track of which device was attached

    usb_osal_thread_create("usbh_hid_keyboard", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_hid_keyboard_thread,
        hid_class);
}

void usbh_hid_keyboard_detached(struct usbh_hid *hid_class)
{
    //TODO Tell the thread to shut down
}

void usbh_hid_keyboard_thread(void *argument)
{
    int ret;
    struct usbh_hid *hid_class = (struct usbh_hid *)argument;

    usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
        hid_buffer, hid_class->intin->wMaxPacketSize, 0,
        usbh_hid_keyboard_callback, hid_class);
    ret = usbh_submit_urb(&hid_class->intin_urb);
    if (ret < 0) {
        log_d("usbh_submit_urb error: %d", ret);
    }

    //TODO Start an event loop to receive data from the callback's IRQ context

    usb_osal_thread_delete(NULL);

}

void usbh_hid_keyboard_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        if (nbytes == 8) {
            keyboard_info_t info = {0};

            info.state = hid_buffer[0];
            for (uint8_t i = 0; i < 6; i++) {
                info.keys[i] = hid_buffer[i + 2];
            }

            //FIXME We're in an IRQ context when this is called. Queue the event and make sure to handle it elsewhere

#if 0
            uint8_t key = keyboard_ascii_code(&info);
            log_i("'%c' %02X %02X [%02X %02X %02X %02X %02X %02X]",
                key, hid_buffer[0], hid_buffer[1], hid_buffer[2], hid_buffer[3],
                hid_buffer[4], hid_buffer[5], hid_buffer[6], hid_buffer[7]);
#endif

            keyboard_process_event(hid_class, &info);

        } else {
            log_w("Unexpected byte count: %d", nbytes);
        }

        usbh_submit_urb(&hid_class->intin_urb);

    } else if (nbytes == -USB_ERR_NAK) { /* for dwc2 */
        usbh_submit_urb(&hid_class->intin_urb);
    }
}

uint8_t keyboard_ascii_code(const keyboard_info_t *info)
{
    uint8_t output;
    if ((info->state & (HID_MODIFER_LSHIFT | HID_MODIFER_RSHIFT)) != 0) {
        output = keyboard_shift_keys[keyboard_codes[info->keys[0]]];
    } else {
        output = keyboard_keys[keyboard_codes[info->keys[0]]];
    }
    return output;
}

void keyboard_process_event(struct usbh_hid *hid_class, const keyboard_info_t *info)
{
    uint8_t key = keyboard_ascii_code(info);
    uint8_t report_state = usb_hid_keyboard_report_state;
    uint32_t event_time = osKernelGetTickCount();

    /*
     * Sometimes we seem to receive the same event twice, in very quick
     * succession. This mostly happens with the CapsLock/NumLock keys
     * during the first press after startup, and it messes up our
     * handling of those keys. For now, the easy fix is to simply
     * detect and ignore such events.
     */
    if (memcmp(info, &usb_hid_keyboard_last_event, sizeof(keyboard_info_t)) == 0
        && (event_time - usb_hid_keyboard_last_event_time) < 20) {
        return;
    } else {
        memcpy(&usb_hid_keyboard_last_event, info, sizeof(keyboard_info_t));
        usb_hid_keyboard_last_event_time = event_time;
    }

#if 1
    if (key >= 32 && key <= 126) {
        log_d("Keyboard: key='%c', code={%02X,%02X,%02X,%02X,%02X,%02X}",
            key, info->keys[0], info->keys[1], info->keys[2],
            info->keys[3], info->keys[4], info->keys[5]);
    } else {
        log_d("Keyboard: key=0x%02X, code={%02X,%02X,%02X,%02X,%02X,%02X}",
            key, info->keys[0], info->keys[1], info->keys[2],
            info->keys[3], info->keys[4], info->keys[5]);
    }
#endif


    if (info->keys[0] == HID_KBD_USAGE_KPDNUMLOCK) {
        report_state ^= HID_KBD_OUTPUT_REPORT_NUMLOCK;
    } else if (info->keys[0] == HID_KBD_USAGE_CAPSLOCK) {
        report_state ^= HID_KBD_OUTPUT_REPORT_CAPSLOCK;
    } else if (info->keys[0] == HID_KBD_USAGE_SCROLLLOCK) {
        report_state ^= HID_KBD_OUTPUT_REPORT_SCROLLLOCK;
    }


    if (report_state != usb_hid_keyboard_report_state) {
//        int ret = usbh_hid_set_report(hid_class, HID_REPORT_OUTPUT, 0x00, &report_state, 1);
//        if (ret >= 0) {
//            usb_hid_keyboard_report_state = report_state;
//        }
    } else {
        /*
         * Toggle the state of the shift keys based on the state
         * of CapsLock so the correct character is generated.
         */
        if ((report_state & HID_KBD_OUTPUT_REPORT_CAPSLOCK) != 0
            && ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'))) {
            keyboard_info_t key_info = {0};

            key_info.state = info->state & ~(HID_MODIFER_LSHIFT | HID_MODIFER_RSHIFT);
            key_info.keys[0] = info->keys[0];

            if ((info->state & (HID_MODIFER_LSHIFT | HID_MODIFER_RSHIFT)) == 0) {
                key_info.state |= HID_MODIFER_LSHIFT;
            }

            key = keyboard_ascii_code(&key_info);
        }

        keypad_event_t keypad_event = {
            .key = KEYPAD_USB_KEYBOARD,
            .pressed = true,
            .keypad_state = ((uint16_t)info->keys[0] << 8) | key
        };
        keypad_inject_event(&keypad_event);
    }
}
