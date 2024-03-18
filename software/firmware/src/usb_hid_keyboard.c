#include "usb_hid_keyboard.h"

#include <cmsis_os.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "keypad.h"

#define LOG_TAG "usb_kbd"
#include <elog.h>

#define EVENT_ATTACH 0
#define EVENT_DETACH 1
#define EVENT_DATA   2

typedef struct {
    uint8_t state;
    uint8_t keys[6];
} keyboard_info_t;

typedef struct {
    uint8_t connected;
    uint8_t report_state;
    keyboard_info_t last_event_info;
    uint32_t last_event_time;
} usb_hid_keyboard_handle_t;

typedef struct {
    struct usbh_hid *hid_class;
    uint8_t event_type;
    uint32_t event_time;
    keyboard_info_t info;
} usb_hid_keyboard_event_t;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_buffer[128];
static usb_hid_keyboard_handle_t keyboard_handles[CONFIG_USBHOST_MAX_HID_CLASS];
static volatile uint8_t keyboard_count = 0;
static usb_osal_thread_t keyboard_task = 0;

/* Queue for communication with the USB HID keyboard task */
static osMessageQueueId_t usb_hid_keyboard_queue = NULL;
static const osMessageQueueAttr_t usb_hid_keyboard_queue_attributes = {
    .name = "usb_hid_keyboard_queue"
};

static void usbh_hid_keyboard_thread(void *argument);
static void usbh_hid_keyboard_callback(void *arg, int nbytes);
static uint8_t keyboard_ascii_code(const keyboard_info_t *info);
static void keyboard_process_event(struct usbh_hid *hid_class, uint32_t event_time, const keyboard_info_t *info);

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


void usbh_hid_keyboard_init()
{
    memset(hid_buffer, 0, sizeof(hid_buffer));
    memset(keyboard_handles, 0, sizeof(keyboard_handles));
    keyboard_count = 0;
    keyboard_task = 0;

    usb_hid_keyboard_queue = osMessageQueueNew(10, sizeof(usb_hid_keyboard_event_t), &usb_hid_keyboard_queue_attributes);
    if (!usb_hid_keyboard_queue) {
        log_e("Unable to create USB HID keyboard event queue");
        return;
    }
}

void usbh_hid_keyboard_attached(struct usbh_hid *hid_class)
{
    //TODO add a mutex to make this safer
    //TODO check return values
    //TODO create a context handle to keep track of which device was attached

    if (!keyboard_task) {
        keyboard_task = usb_osal_thread_create("usbh_hid_keyboard", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_hid_keyboard_thread, 0);
    }

    usb_hid_keyboard_event_t event = {
        .hid_class = hid_class,
        .event_type = EVENT_ATTACH,
        .event_time = osKernelGetTickCount(),
        .info = {0}
    };
    osMessageQueuePut (usb_hid_keyboard_queue, &event, 0, 0);
}

void usbh_hid_keyboard_detached(struct usbh_hid *hid_class)
{
    if (keyboard_task) {
        usb_hid_keyboard_event_t event = {
            .hid_class = hid_class,
            .event_type = EVENT_DETACH,
            .event_time = osKernelGetTickCount(),
            .info = {0}
        };
        osMessageQueuePut (usb_hid_keyboard_queue, &event, 0, 0);
    }
}

void usbh_hid_keyboard_thread(void *argument)
{
    for (;;) {
        usb_hid_keyboard_event_t event;
        if(osMessageQueueGet(usb_hid_keyboard_queue, &event, NULL, portMAX_DELAY) == osOK) {
            if (event.event_type == EVENT_ATTACH) {
                int ret;
                struct usbh_hid *hid_class = event.hid_class;

                memset(&keyboard_handles[hid_class->minor], 0, sizeof(usb_hid_keyboard_handle_t));

                keyboard_handles[hid_class->minor].connected = 1;
                keyboard_count++;

                usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
                    hid_buffer, hid_class->intin->wMaxPacketSize, 0,
                    usbh_hid_keyboard_callback, hid_class);
                ret = usbh_submit_urb(&hid_class->intin_urb);
                if (ret < 0) {
                    log_d("usbh_submit_urb error: %d", ret);
                }

            } else if (event.event_type == EVENT_DETACH) {
                if (keyboard_count > 0) { keyboard_count--; }

                keyboard_handles[event.hid_class->minor].connected = 0;

                if (keyboard_count == 0) { break; }

            } else if (event.event_type == EVENT_DATA) {
                keyboard_process_event(event.hid_class, event.event_time, &event.info);
            }
        }
    }

    keyboard_task = NULL;
    usb_osal_thread_delete(NULL);
}

void usbh_hid_keyboard_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        if (nbytes == 8) {
            usb_hid_keyboard_event_t event = {
                .hid_class = hid_class,
                .event_type = EVENT_DATA,
                .event_time = osKernelGetTickCount(),
            };

            event.info.state = hid_buffer[0];

            for (uint8_t i = 0; i < 6; i++) {
                event.info.keys[i] = hid_buffer[i + 2];
            }

            osMessageQueuePut (usb_hid_keyboard_queue, &event, 0, 0);

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

void keyboard_process_event(struct usbh_hid *hid_class, uint32_t event_time, const keyboard_info_t *info)
{
    uint8_t key = keyboard_ascii_code(info);
    uint8_t report_state = keyboard_handles[hid_class->minor].report_state;

    /*
     * Sometimes we seem to receive the same event twice, in very quick
     * succession. This mostly happens with the CapsLock/NumLock keys
     * during the first press after startup, and it messes up our
     * handling of those keys. For now, the easy fix is to simply
     * detect and ignore such events.
     */
    if (memcmp(info, &keyboard_handles[hid_class->minor].last_event_info, sizeof(keyboard_info_t)) == 0
        && (event_time - keyboard_handles[hid_class->minor].last_event_time) < 20) {
        return;
    } else {
        memcpy(&keyboard_handles[hid_class->minor].last_event_info, info, sizeof(keyboard_info_t));
        keyboard_handles[hid_class->minor].last_event_time = event_time;
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


    if (report_state != keyboard_handles[hid_class->minor].report_state) {
        //FIXME These calls use a static buffer inside usbh_hid that should probably be protected
        int ret = usbh_hid_set_report(hid_class, HID_REPORT_OUTPUT, 0x00, &report_state, 1);
        if (ret >= 0) {
            keyboard_handles[hid_class->minor].report_state = report_state;
        } else {
            log_w("usbh_hid_set_report error: %d", ret);
        }
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
