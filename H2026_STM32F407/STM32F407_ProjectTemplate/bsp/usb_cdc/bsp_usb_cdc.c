#include "bsp_usb_cdc.h"

#include <string.h>

#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

#define USB_CDC_RX_BUFFER_SIZE  64U
#define USB_CDC_RX_RING_SIZE   256U
#define USB_CDC_TX_BUFFER_SIZE 256U

static USBD_HandleTypeDef g_usb_device;
static uint8_t g_usb_rx_packet[USB_CDC_RX_BUFFER_SIZE];
static uint8_t g_usb_tx_packet[USB_CDC_TX_BUFFER_SIZE];
/* USB CDC ACM line coding: 115200 baud, 8 data bits, no parity, 1 stop. */
static uint8_t g_usb_line_coding[7] =
{
    0x00U, 0xC2U, 0x01U, 0x00U,
    0x00U, 0x00U, 0x08U
};
static volatile uint16_t g_usb_rx_head;
static volatile uint16_t g_usb_rx_tail;
static volatile uint32_t g_usb_rx_overflow;
static uint8_t g_usb_rx_ring[USB_CDC_RX_RING_SIZE];

static int8_t usb_cdc_interface_init(void);
static int8_t usb_cdc_interface_deinit(void);
static int8_t usb_cdc_interface_control(uint8_t command,
                                        uint8_t *buffer,
                                        uint16_t length);
static int8_t usb_cdc_interface_receive(uint8_t *buffer,
                                        uint32_t *length);
static int8_t usb_cdc_interface_tx_complete(uint8_t *buffer,
                                            uint32_t *length,
                                            uint8_t endpoint);

static USBD_CDC_ItfTypeDef g_usb_cdc_interface =
{
    usb_cdc_interface_init,
    usb_cdc_interface_deinit,
    usb_cdc_interface_control,
    usb_cdc_interface_receive,
    usb_cdc_interface_tx_complete
};

void usb_cdc_init(void)
{
    g_usb_rx_head = 0U;
    g_usb_rx_tail = 0U;
    g_usb_rx_overflow = 0U;

    USBD_Init(&g_usb_device, &VCP_Desc, 0U);
    USBD_RegisterClass(&g_usb_device, &USBD_CDC);
    USBD_CDC_RegisterInterface(&g_usb_device, &g_usb_cdc_interface);
    USBD_Start(&g_usb_device);
}

uint8_t usb_cdc_is_configured(void)
{
    return (g_usb_device.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

uint8_t usb_cdc_write(const uint8_t *data, uint16_t length)
{
    USBD_CDC_HandleTypeDef *class_data;

    if((data == 0) || (length == 0U) ||
       (length > USB_CDC_TX_BUFFER_SIZE) ||
       (usb_cdc_is_configured() == 0U))
    {
        return 0U;
    }

    class_data =
        (USBD_CDC_HandleTypeDef *)g_usb_device.pClassDataCmsit[
            g_usb_device.classId];
    if((class_data == 0) || (class_data->TxState != 0U))
    {
        return 0U;
    }

    memcpy(g_usb_tx_packet, data, length);
    USBD_CDC_SetTxBuffer(&g_usb_device, g_usb_tx_packet, length);
    return (USBD_CDC_TransmitPacket(&g_usb_device) == USBD_OK) ? 1U : 0U;
}

uint8_t usb_cdc_write_string(const char *text)
{
    uint16_t length = 0U;

    if(text == 0)
    {
        return 0U;
    }
    while((text[length] != '\0') && (length < USB_CDC_TX_BUFFER_SIZE))
    {
        length++;
    }
    return usb_cdc_write((const uint8_t *)text, length);
}

uint8_t usb_cdc_read_byte(uint8_t *byte)
{
    uint16_t tail;

    if(byte == 0)
    {
        return 0U;
    }
    tail = g_usb_rx_tail;
    if(tail == g_usb_rx_head)
    {
        return 0U;
    }

    *byte = g_usb_rx_ring[tail];
    g_usb_rx_tail = (uint16_t)((tail + 1U) % USB_CDC_RX_RING_SIZE);
    return 1U;
}

uint32_t usb_cdc_rx_overflow_count(void)
{
    return g_usb_rx_overflow;
}

static int8_t usb_cdc_interface_init(void)
{
    USBD_CDC_SetTxBuffer(&g_usb_device, g_usb_tx_packet, 0U);
    USBD_CDC_SetRxBuffer(&g_usb_device, g_usb_rx_packet);
    return (int8_t)USBD_OK;
}

static int8_t usb_cdc_interface_deinit(void)
{
    return (int8_t)USBD_OK;
}

static int8_t usb_cdc_interface_control(uint8_t command,
                                        uint8_t *buffer,
                                        uint16_t length)
{
    if((buffer != 0) && (length >= sizeof(g_usb_line_coding)))
    {
        if(command == CDC_SET_LINE_CODING)
        {
            memcpy(g_usb_line_coding,
                   buffer,
                   sizeof(g_usb_line_coding));
        }
        else if(command == CDC_GET_LINE_CODING)
        {
            memcpy(buffer,
                   g_usb_line_coding,
                   sizeof(g_usb_line_coding));
        }
    }
    return (int8_t)USBD_OK;
}

static int8_t usb_cdc_interface_receive(uint8_t *buffer,
                                        uint32_t *length)
{
    uint32_t index;

    for(index = 0U; index < *length; index++)
    {
        uint16_t head = g_usb_rx_head;
        uint16_t next =
            (uint16_t)((head + 1U) % USB_CDC_RX_RING_SIZE);
        if(next == g_usb_rx_tail)
        {
            g_usb_rx_overflow++;
            break;
        }
        g_usb_rx_ring[head] = buffer[index];
        g_usb_rx_head = next;
    }

    USBD_CDC_SetRxBuffer(&g_usb_device, g_usb_rx_packet);
    USBD_CDC_ReceivePacket(&g_usb_device);
    return (int8_t)USBD_OK;
}

static int8_t usb_cdc_interface_tx_complete(uint8_t *buffer,
                                            uint32_t *length,
                                            uint8_t endpoint)
{
    (void)buffer;
    (void)length;
    (void)endpoint;
    return (int8_t)USBD_OK;
}
