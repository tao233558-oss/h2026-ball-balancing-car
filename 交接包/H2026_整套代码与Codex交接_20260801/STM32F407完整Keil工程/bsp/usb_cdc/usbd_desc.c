/* Minimal USB CDC descriptors for the H2026 online tuner. */
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define H2026_USB_VID       0x0483U
#define H2026_USB_PID       0x5740U
#define H2026_LANG_ID       0x0409U

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed,
                                  uint16_t *length);
static uint8_t *language_descriptor(USBD_SpeedTypeDef speed,
                                    uint16_t *length);
static uint8_t *short_string_descriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length);

USBD_DescriptorsTypeDef VCP_Desc =
{
    device_descriptor,
    language_descriptor,
    short_string_descriptor,
    short_string_descriptor,
    short_string_descriptor,
    short_string_descriptor,
    short_string_descriptor
};

__ALIGN_BEGIN static uint8_t g_device_descriptor[USB_LEN_DEV_DESC]
    __ALIGN_END =
{
    USB_LEN_DEV_DESC,
    USB_DESC_TYPE_DEVICE,
    0x00U, 0x02U,                 /* USB 2.00 */
    0x02U, 0x02U, 0x00U,         /* CDC class/subclass/protocol */
    USB_MAX_EP0_SIZE,
    LOBYTE(H2026_USB_VID), HIBYTE(H2026_USB_VID),
    LOBYTE(H2026_USB_PID), HIBYTE(H2026_USB_PID),
    0x00U, 0x01U,                 /* device release 1.00 */
    1U,                           /* manufacturer string */
    2U,                           /* product string */
    0U,                           /* no serial string */
    USBD_MAX_NUM_CONFIGURATION
};

__ALIGN_BEGIN static uint8_t g_language_descriptor[USB_LEN_LANGID_STR_DESC]
    __ALIGN_END =
{
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(H2026_LANG_ID),
    HIBYTE(H2026_LANG_ID)
};

/* "H2026" needs 12 bytes as an UTF-16 USB string descriptor. */
__ALIGN_BEGIN static uint8_t g_string_descriptor[16] __ALIGN_END;

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed,
                                  uint16_t *length)
{
    UNUSED(speed);
    *length = (uint16_t)sizeof(g_device_descriptor);
    return g_device_descriptor;
}

static uint8_t *language_descriptor(USBD_SpeedTypeDef speed,
                                    uint16_t *length)
{
    UNUSED(speed);
    *length = (uint16_t)sizeof(g_language_descriptor);
    return g_language_descriptor;
}

static uint8_t *short_string_descriptor(USBD_SpeedTypeDef speed,
                                        uint16_t *length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)"H2026", g_string_descriptor, length);
    return g_string_descriptor;
}
