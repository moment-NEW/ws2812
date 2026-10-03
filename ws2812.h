/*
 * Copyright (c) 2026, moment-NEW
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-03     moment-NEW   first version
 */

#ifndef __WS2812_H__
#define __WS2812_H__

#include <stdint.h>

#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration macros, overridable from Kconfig (rtconfig.h) or board headers */
#ifndef PKG_WS2812_SPI_BUS_NAME
#define PKG_WS2812_SPI_BUS_NAME     "spi6"
#endif

#ifndef PKG_WS2812_SPI_FREQUENCY
#define PKG_WS2812_SPI_FREQUENCY    4000000
#endif

#ifndef PKG_WS2812_LED_NUM
#define PKG_WS2812_LED_NUM          1
#endif

#define WS2812_SPI_BUS      PKG_WS2812_SPI_BUS_NAME
#define WS2812_SPI_SPEED    PKG_WS2812_SPI_FREQUENCY

/* Color macros (GRB order) */
#define WS2812_COLOR_RED        0xFF0000
#define WS2812_COLOR_GREEN      0x00FF00
#define WS2812_COLOR_BLUE       0x0000FF
#define WS2812_COLOR_WHITE      0xFFFFFF
#define WS2812_COLOR_BLACK      0x000000

/* SPI encoding: 0 -> 100, 1 -> 110. */
#define WS2812_3BIT_SPI_CODE_0              0x04U /* 0b100 */
#define WS2812_3BIT_SPI_CODE_1              0x06U /* 0b110 */
#define WS2812_3BIT_ENCODED_BYTES_PER_COLOR 3U
#define WS2812_3BIT_ENCODED_BYTES_PER_LED   9U
#define WS2812_4BIT_SPI_CODE_0              0x08U /* 0b1000 */
#define WS2812_4BIT_SPI_CODE_1              0x0CU /* 0b1100 */
#define WS2812_4BIT_ENCODED_BYTES_PER_COLOR 4U
#define WS2812_4BIT_ENCODED_BYTES_PER_LED   12U
#define WS2812_8BIT_SPI_CODE_0              0x60U /* 0b01100000 */
#define WS2812_8BIT_SPI_CODE_1              0x78U /* 0b01111000 */
#define WS2812_8BIT_ENCODED_BYTES_PER_COLOR 8U
#define WS2812_8BIT_ENCODED_BYTES_PER_LED   24U

/* WS2812 data frame configuration */
#ifndef WS2812_ENCODE_MODE_BITS
#define WS2812_ENCODE_MODE_BITS       8U
#endif
#if WS2812_ENCODE_MODE_BITS == 3U
#define WS2812_ENCODED_BYTES_PER_COLOR WS2812_3BIT_ENCODED_BYTES_PER_COLOR
#define WS2812_ENCODED_BYTES_PER_LED   WS2812_3BIT_ENCODED_BYTES_PER_LED
#elif WS2812_ENCODE_MODE_BITS == 4U
#define WS2812_ENCODED_BYTES_PER_COLOR WS2812_4BIT_ENCODED_BYTES_PER_COLOR
#define WS2812_ENCODED_BYTES_PER_LED   WS2812_4BIT_ENCODED_BYTES_PER_LED
#elif WS2812_ENCODE_MODE_BITS == 8U
#define WS2812_ENCODED_BYTES_PER_COLOR WS2812_8BIT_ENCODED_BYTES_PER_COLOR
#define WS2812_ENCODED_BYTES_PER_LED   WS2812_8BIT_ENCODED_BYTES_PER_LED
#else
#error "Unsupported WS2812_ENCODE_MODE_BITS"
#endif

#define WS2812_RESET_TIME_US          300U
#define WS2812_RESET_MIN_BYTES        235U
#define WS2812_RESET_BYTES_BY_FREQ(freq) \
    (((freq) * WS2812_RESET_TIME_US + 7999999U) / 8000000U)
#define WS2812_RESET_BYTES            \
    ((WS2812_RESET_BYTES_BY_FREQ(PKG_WS2812_SPI_FREQUENCY) > WS2812_RESET_MIN_BYTES) ? \
     WS2812_RESET_BYTES_BY_FREQ(PKG_WS2812_SPI_FREQUENCY) : WS2812_RESET_MIN_BYTES)
#define WS2812_DATA_BUF_SIZE(led_num) \
    ((led_num) * WS2812_ENCODED_BYTES_PER_LED)
#define WS2812_TX_BUF_SIZE(led_num) \
    (WS2812_DATA_BUF_SIZE(led_num) + WS2812_RESET_BYTES)

/* WS2812 GRB color */
typedef struct
{
    rt_uint8_t red;
    rt_uint8_t green;
    rt_uint8_t blue;
} ws2812_color_t;

/* Convert RGB value (0xRRGGBB) to ws2812_color_t */
#define WS2812_RGB2COLOR(rgb)          \
    ((ws2812_color_t)                  \
    {                                  \
        .red = (rt_uint8_t)(((rgb) >> 16) & 0xFFU), \
        .green = (rt_uint8_t)(((rgb) >> 8) & 0xFFU), \
        .blue = (rt_uint8_t)((rgb) & 0xFFU), \
    })

/** WS2812 device instance */
struct ws2812_device
{
    struct rt_spi_device *spi;
    rt_uint8_t  *buf;           /* encoded color buffer */
    rt_uint16_t led_num;        /* number of LEDs */
};

/* Auto-initialised at DEVICE stage (callable manually too) */
int ws2812_hw_init(void);

/* Get the singleton device after init */
struct ws2812_device *ws2812_get_device(void);

/* Encode one color byte as packed SPI data: 0 -> 100, 1 -> 110. */
int ws2812_encode_byte_3bit(rt_uint8_t *dst, rt_uint8_t byte);
int ws2812_encode_byte_4bit(rt_uint8_t *dst, rt_uint8_t byte);
int ws2812_encode_byte_8bit(rt_uint8_t *dst, rt_uint8_t byte);

/* Buffer size helpers */
rt_size_t ws2812_data_buf_size(rt_uint16_t led_num);
rt_size_t ws2812_reset_buf_size(void);
rt_size_t ws2812_tx_buf_size(rt_uint16_t led_num);

/* Pixel operations */
int ws2812_set_pixel(struct ws2812_device *ws, rt_uint16_t index, ws2812_color_t color);
int ws2812_build(struct ws2812_device *ws, const ws2812_color_t *data);
int ws2812_transmit(struct ws2812_device *ws);
int ws2812_refresh(struct ws2812_device *ws);
void ws2812_dump_pixel(struct ws2812_device *ws, rt_uint16_t index);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H__ */
