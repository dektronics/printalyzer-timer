#include "usb_hid_keyboard.h"

#include <cmsis_os.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "keypad.h"
#include "display.h"

#define LOG_TAG "usb_kbd"
#include <elog.h>

#define HID_BUF_SIZE 16
#define KEY_SET_SIZE 6

typedef enum {
    USB_KBD_ATTACH = 0,
    USB_KBD_DETACH,
    USB_KBD_DATA,
    USB_KBD_SUBMIT_URB
} usb_keyboard_event_type_t;

typedef struct {
    uint8_t state;
    uint8_t keys[KEY_SET_SIZE];
} keyboard_info_t;

typedef struct {
    uint8_t attached;
    uint8_t report_state;
    uint8_t key_state[KEY_SET_SIZE];
    USB_MEM_ALIGNX uint8_t hid_in_buffer[HID_BUF_SIZE];
    USB_MEM_ALIGNX uint8_t hid_out_buffer[HID_BUF_SIZE];
} usb_hid_keyboard_handle_t;

typedef struct {
    usb_keyboard_event_type_t event_type;
    struct usbh_hid *hid_class;
    uint32_t event_time;
    keyboard_info_t info;
} usb_hid_keyboard_event_t;

static usb_hid_keyboard_handle_t keyboard_handles[CONFIG_USBHOST_MAX_HID_CLASS];

static osThreadId_t keyboard_task = NULL;
static const osThreadAttr_t keyboard_task_attrs = {
    .name = "hid_kbd_task",
    .stack_size = 2048,
    .priority = osPriorityNormal
};

/* Semaphore to synchronize startup and shutdown of the USB keyboard task */
static osSemaphoreId_t usb_hid_keyboard_semaphore = NULL;
static const osSemaphoreAttr_t usb_hid_keyboard_semaphore_attrs = {
    .name = "usb_hid_kbd_semaphore"
};

/* Queue for communication with the USB HID keyboard task */
static osMessageQueueId_t usb_hid_keyboard_queue = NULL;
static const osMessageQueueAttr_t usb_hid_keyboard_queue_attributes = {
    .name = "usb_hid_kbd_queue"
};

static void usbh_hid_keyboard_thread(void *argument);
static void usbh_hid_keyboard_control_attach(struct usbh_hid *hid_class);
static void usbh_hid_keyboard_control_detach(struct usbh_hid *hid_class);
static void usbh_hid_keyboard_callback(void *arg, int nbytes);
static uint8_t keyboard_ascii_code(uint8_t report_state, uint8_t state, uint8_t keycode);
static void keyboard_process_event(struct usbh_hid *hid_class, uint32_t event_time, const keyboard_info_t *info);
static int hid_keyboard_set_protocol(struct usbh_hid *hid_class, uint8_t protocol);
static int hid_keyboard_set_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen);

static const uint8_t keyboard_keys[] = {
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

static const uint8_t keyboard_shift_keys[] = {
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


bool usbh_hid_keyboard_init()
{
    /* Create the semaphore used to synchronize keyboard task startup and shutdown */
    usb_hid_keyboard_semaphore = osSemaphoreNew(1, 0, &usb_hid_keyboard_semaphore_attrs);
    if (!usb_hid_keyboard_semaphore) {
        log_e("usb_hid_keyboard_semaphore create error");
        return false;
    }

    /* Create the keyboard task event queue */
    usb_hid_keyboard_queue = osMessageQueueNew(10, sizeof(usb_hid_keyboard_event_t), &usb_hid_keyboard_queue_attributes);
    if (!usb_hid_keyboard_queue) {
        log_e("Unable to create USB HID keyboard event queue");
        return false;
    }

    memset(keyboard_handles, 0, sizeof(keyboard_handles));
    keyboard_task = NULL;

    return true;
}

void usbh_hid_keyboard_attached(struct usbh_hid *hid_class)
{
    /* If the keyboard task isn't running, then start it */
    if (!keyboard_task) {
        keyboard_task = osThreadNew(usbh_hid_keyboard_thread, NULL, &keyboard_task_attrs);
        if (!keyboard_task) {
            log_w("keyboard_task create error");
            return;
        }
    }

    /* Send a message informing the task of the connection event */
    usb_hid_keyboard_event_t event = {
        .event_type = USB_KBD_ATTACH,
        .hid_class = hid_class,
        .event_time = osKernelGetTickCount(),
        .info = {0}
    };
    osMessageQueuePut(usb_hid_keyboard_queue, &event, 0, 0);

    /* Block until the attach has been processed */
    osSemaphoreAcquire(usb_hid_keyboard_semaphore, portMAX_DELAY);
}

void usbh_hid_keyboard_detached(struct usbh_hid *hid_class)
{
    if (!keyboard_task) {
        log_w("Keyboard task not running");
        return;
    }

    /* Send a message informing the task of the detach event */
    usb_hid_keyboard_event_t event = {
        .event_type = USB_KBD_DETACH,
        .hid_class = hid_class,
        .event_time = osKernelGetTickCount(),
        .info = {0}
    };
    osMessageQueuePut(usb_hid_keyboard_queue, &event, 0, 0);

    /* Block until the detach has been processed */
    osSemaphoreAcquire(usb_hid_keyboard_semaphore, portMAX_DELAY);
}

void usbh_hid_keyboard_thread(void *argument)
{
    for (;;) {
        usb_hid_keyboard_event_t event;
        if(osMessageQueueGet(usb_hid_keyboard_queue, &event, NULL, portMAX_DELAY) == osOK) {
            struct usbh_hid *hid_class = event.hid_class;

            if (event.event_type == USB_KBD_ATTACH) {
                usbh_hid_keyboard_control_attach(hid_class);
            } else if (event.event_type == USB_KBD_DETACH) {
                usbh_hid_keyboard_control_detach(hid_class);
            } else if (event.event_type == USB_KBD_DATA) {
                usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];
                if (handle->attached) {
                    keyboard_process_event(event.hid_class, event.event_time, &event.info);
                }
            } else if (event.event_type == USB_KBD_SUBMIT_URB) {
                usbh_submit_urb(&hid_class->intin_urb);
            }

            if (event.event_type != USB_KBD_DATA && event.event_type != USB_KBD_SUBMIT_URB) {
                /* Check if anything is still connected */
                bool devices_connected = false;
                for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_HID_CLASS; i++) {
                    if (keyboard_handles[i].attached) {
                        devices_connected = true;
                        break;
                    }
                }

                if (devices_connected) {
                    /* If devices are still connected, release the semaphore here */
                    osSemaphoreRelease(usb_hid_keyboard_semaphore);
                } else {
                    /* Otherwise, exit the task loop then release the semaphore */
                    break;
                }

            }
        }
    }

    keyboard_task = NULL;
    osSemaphoreRelease(usb_hid_keyboard_semaphore);
    osThreadExit();
}

void usbh_hid_keyboard_control_attach(struct usbh_hid *hid_class)
{
    int ret;
    usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];

    if (handle->attached) {
        log_w("Keyboard %d already attached", hid_class->minor);
        return;
    }

    /* Set keyboard to "Boot Protocol" mode */
    ret = hid_keyboard_set_protocol(hid_class, 0);
    if (ret < 0) {
        log_d("hid_keyboard_set_protocol error: %d", ret);
        return;
    }

    memset(handle, 0, sizeof(usb_hid_keyboard_handle_t));
    handle->attached = 1;

    /* Submit the initial request */
    usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
        handle->hid_in_buffer, MIN(hid_class->intin->wMaxPacketSize, HID_BUF_SIZE),
        0, usbh_hid_keyboard_callback, hid_class);
    ret = usbh_submit_urb(&hid_class->intin_urb);
    if (ret < 0) {
        log_d("usbh_submit_urb error: %d", ret);
    }
}

void usbh_hid_keyboard_control_detach(struct usbh_hid *hid_class)
{
    usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];

    if (!handle->attached) {
        log_w("Keyboard %d already detached", hid_class->minor);
        return;
    }

    /* Inject release events for all currently pressed keys */
    for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
        if (handle->key_state[i] == 0) { continue; }
        uint8_t key = keyboard_ascii_code(handle->report_state, 0, handle->key_state[i]);

        keypad_event_t keypad_event = {
            .key = KEYPAD_USB_KEYBOARD,
            .pressed = false,
            .keypad_state = ((uint16_t) handle->key_state[i] << 8) | key
        };
        keypad_inject_event(&keypad_event);
    }

    handle->attached = 0;
}

void usbh_hid_keyboard_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;
    usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];
    bool submit_urb = false;

    if (!handle->attached) {
        return;
    }

    if (nbytes > 0) {
        if (nbytes >= 8) {
            usb_hid_keyboard_event_t event = {
                .event_type = USB_KBD_DATA,
                .hid_class = hid_class,
                .event_time = osKernelGetTickCount(),
            };

            /*
             * If a device returns more than 8 bytes, then the first byte is a report ID and the
             * next 8 bytes must still be the standard boot protocol report.
             * As long as we stick to only supporting keyboards that implement the boot protocol,
             * and never send the request to change them out of that mode, these assumptions
             * should hold.
             */
            uint8_t offset = 0;
            if (nbytes > 8) {
                offset = 1;
            }

            event.info.state = handle->hid_in_buffer[offset];

            for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
                event.info.keys[i] = handle->hid_in_buffer[offset + i + 2];
            }

            osMessageQueuePut(usb_hid_keyboard_queue, &event, 0, 0);

        } else {
            log_w("Unexpected byte count: %d", nbytes);
        }

        submit_urb = true;

    } else if (nbytes == -USB_ERR_NAK) {
        submit_urb = true;
    } else if (nbytes < 0) {
        log_w("usbh_hid_keyboard_callback[dev=%d] error: %d", hid_class->minor, nbytes);
    } else {
        /* Sometimes a device may return 0 bytes */
        submit_urb = true;
    }

    if (submit_urb) {
        /* Reassign URB structure fields since the library may have modified them */
        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin,
            handle->hid_in_buffer, MIN(hid_class->intin->wMaxPacketSize, HID_BUF_SIZE),
            0, usbh_hid_keyboard_callback, hid_class);

        usb_hid_keyboard_event_t event = {
            .event_type = USB_KBD_SUBMIT_URB,
            .hid_class = hid_class
        };
        osMessageQueuePut(usb_hid_keyboard_queue, &event, 0, 0);
    }
}

uint8_t keyboard_ascii_code(uint8_t report_state, uint8_t state, uint8_t keycode)
{
    uint8_t key = keyboard_keys[keyboard_codes[keycode]];

    if (((report_state & HID_KBD_OUTPUT_REPORT_CAPSLOCK) != 0
        && ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'))) ||
        (state & (HID_MODIFIER_LSHIFT | HID_MODIFIER_RSHIFT)) != 0) {
        key = keyboard_shift_keys[keyboard_codes[keycode]];
    }
    return key;
}

void keyboard_process_event(struct usbh_hid *hid_class, uint32_t event_time, const keyboard_info_t *info)
{
    usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];
    uint8_t report_state = keyboard_handles[hid_class->minor].report_state;
    uint8_t active_keys[KEY_SET_SIZE] = {0};
    uint8_t pressed_keys[KEY_SET_SIZE] = {0};
    uint8_t released_keys[KEY_SET_SIZE] = {0};

    /* Figure out what keys were pressed, released, or still active */
    if(info->keys[0] != HID_KBD_USAGE_ERRORROLLOVER) {
        uint8_t k = 0;
        uint8_t l = 0;
        uint8_t m = 0;
        for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
            if (info->keys[i] > 0) {
                bool is_active = false;
                for (uint8_t j = 0; j < KEY_SET_SIZE; j++) {
                    if (info->keys[i] == handle->key_state[j]) {
                        is_active = true;
                        break;
                    }
                }
                if (!is_active) {
                    pressed_keys[k++] = info->keys[i];
                }
                active_keys[l++] = info->keys[i];
            }
            if (handle->key_state[i] > 0) {
                bool is_pressed = false;
                for (uint8_t j = 0; j < KEY_SET_SIZE; j++) {
                    if (handle->key_state[i] == info->keys[j]) {
                        is_pressed = true;
                        break;
                    }
                }
                if (!is_pressed) {
                    released_keys[m++] = handle->key_state[i];
                }
            }
        }

        /* Copy the active set back to the key state */
        memcpy(handle->key_state, active_keys, KEY_SET_SIZE);
    }

#if 0
    log_d("Pressed: {%02X,%02X,%02X,%02X,%02X,%02X}",
        pressed_keys[0], pressed_keys[1], pressed_keys[2],
        pressed_keys[3], pressed_keys[4], pressed_keys[5]);
    log_d("Released: {%02X,%02X,%02X,%02X,%02X,%02X}",
        released_keys[0], released_keys[1], released_keys[2],
        released_keys[3], released_keys[4], released_keys[5]);
#endif

    /* Update the report state of the locking modifier keys */
    for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
        if (pressed_keys[i] == HID_KBD_USAGE_KPDNUMLOCK) {
            report_state ^= HID_KBD_OUTPUT_REPORT_NUMLOCK;
        } else  if (pressed_keys[i] == HID_KBD_USAGE_CAPSLOCK) {
            report_state ^= HID_KBD_OUTPUT_REPORT_CAPSLOCK;
        } else if (pressed_keys[i] == HID_KBD_USAGE_SCROLLLOCK) {
            report_state ^= HID_KBD_OUTPUT_REPORT_SCROLLLOCK;
        }
    }

#if 1
    if(info->keys[0] == HID_KBD_USAGE_ERRORROLLOVER) {
        log_d("Keyboard: ERROR ROLLOVER");
    } else {
        uint8_t key = keyboard_ascii_code(report_state, info->state, info->keys[0]);
        if (key >= 32 && key <= 126) {
            log_d("Keyboard: key='%c', mod=0x%02X, code={%02X,%02X,%02X,%02X,%02X,%02X}",
                key, info->state, info->keys[0], info->keys[1], info->keys[2],
                info->keys[3], info->keys[4], info->keys[5]);
        } else {
            log_d("Keyboard: key=0x%02X, mod=0x%02X, code={%02X,%02X,%02X,%02X,%02X,%02X}",
                key, info->state, info->keys[0], info->keys[1], info->keys[2],
                info->keys[3], info->keys[4], info->keys[5]);
        }
    }
#endif

    /* If the locking modifier key state changed, send a report to update keyboard indicators */
    if (report_state != handle->report_state) {
        int ret = hid_keyboard_set_report(hid_class, HID_REPORT_OUTPUT, 0x00, &report_state, 1);
        if (ret >= 0) {
            handle->report_state = report_state;
        } else {
            log_w("usbh_hid_set_report error: %d", ret);
        }
    }

    /* Check for Ctrl-PrintScreen */
    if ((info->state & (HID_MODIFIER_LCTRL | HID_MODIFIER_RCTRL)) != 0
        && info->keys[0] == HID_KBD_USAGE_PRINTSCN) {
        log_d("Triggering display screenshot");
        display_save_screenshot();
    }

    /* Inject the pressed keys */
    for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
        if (pressed_keys[i] == 0) { continue; }
        uint8_t key = keyboard_ascii_code(report_state, info->state, pressed_keys[i]);

        keypad_event_t keypad_event = {
            .key = KEYPAD_USB_KEYBOARD,
            .pressed = true,
            .keypad_state = ((uint16_t) pressed_keys[i] << 8) | key
        };
        keypad_inject_event(&keypad_event);
    }

    /* Inject the released keys */
    for (uint8_t i = 0; i < KEY_SET_SIZE; i++) {
        if (released_keys[i] == 0) { continue; }
        uint8_t key = keyboard_ascii_code(report_state, info->state, released_keys[i]);

        keypad_event_t keypad_event = {
            .key = KEYPAD_USB_KEYBOARD,
            .pressed = false,
            .keypad_state = ((uint16_t) released_keys[i] << 8) | key
        };
        keypad_inject_event(&keypad_event);
    }
}

int hid_keyboard_set_protocol(struct usbh_hid *hid_class, uint8_t protocol)
{
    struct usb_setup_packet *setup = hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_PROTOCOL;
    setup->wValue = protocol;
    setup->wIndex = 0;
    setup->wLength = 0;

    return usbh_control_transfer(hid_class->hport, setup, NULL);
}

int hid_keyboard_set_report(struct usbh_hid *hid_class, uint8_t report_type, uint8_t report_id, uint8_t *buffer, uint32_t buflen)
{
    /*
     * To send an output report, we need an aligned static buffer that lasts
     * the duration of the transaction. Its easier to implement this code here,
     * where we can be sure unrelated HID device drivers won't be trying to
     * share the same buffer.
     */
    struct usb_setup_packet *setup = hid_class->hport->setup;
    usb_hid_keyboard_handle_t *handle = &keyboard_handles[hid_class->minor];

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_REPORT;
    setup->wValue = (uint16_t)(((uint32_t)report_type << 8U) | (uint32_t)report_id);
    setup->wIndex = 0;
    setup->wLength = buflen;

    memcpy(handle->hid_out_buffer, buffer, buflen);
    return usbh_control_transfer(hid_class->hport, setup, handle->hid_out_buffer);
}
