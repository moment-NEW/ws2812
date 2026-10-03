/*
 * Copyright (c) 2026, moment-NEW
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-03     moment-NEW   first version
 */

#include "ws2812.h"

#define DBG_TAG "ws2812"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef PKG_WS2812_USING_CLK_DEBUG
#include <board.h>  /* STM32 device/CMSIS headers for clock inspection */
#endif

/* Singleton device instance */
static struct ws2812_device _dev;
static rt_uint8_t _ws2812_inited = 0;
static rt_uint8_t _ws2812_spi_clock_logged = 0;

static struct rt_spi_configuration _ws2812_spi_cfg =
{
    .mode = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB | RT_SPI_NO_CS,
    .data_width = 8,
    .max_hz = WS2812_SPI_SPEED,
};

#ifdef PKG_WS2812_USING_BDMA_BUFFER
/* BDMA (D3 domain) can only access SRAM4. The BSP must reserve the
 * .ws2812_bdma section there in its linker scripts, e.g. stm32h723-DM-MC02. */
static rt_uint8_t _ws2812_bdma_buf[WS2812_TX_BUF_SIZE(PKG_WS2812_LED_NUM)]
    __attribute__((section(".ws2812_bdma"), aligned(32)));
#endif

/**
 * Initialize the WS2812 device
 * @return 0 on success, negative error code on failure
 */
int ws2812_hw_init(void)
{
    rt_err_t res;
    struct rt_spi_device *spi_dev;

    if (_ws2812_inited)
        return RT_EOK;

    spi_dev = (struct rt_spi_device *)rt_malloc(sizeof(struct rt_spi_device));
    if (spi_dev == RT_NULL)
    {
        LOG_E("Failed to malloc the spi device.");
        return -RT_ENOMEM;
    }

    if (rt_spi_bus_attach_device(spi_dev, "WS2812", WS2812_SPI_BUS, RT_NULL) != RT_EOK)
    {
        LOG_E("Failed to attach the spi device to bus %s.", WS2812_SPI_BUS);
        rt_free(spi_dev);
        return -RT_ERROR;
    }

    res = rt_spi_configure(spi_dev, &_ws2812_spi_cfg);
    if (res != RT_EOK)
    {
        LOG_E("Failed to configure WS2812 SPI, error: %d", res);
        rt_spi_bus_detach_device(spi_dev);
        rt_free(spi_dev);
        return res;
    }

    LOG_I("SPI max=%u Hz, encoding=%u-bit",
          (unsigned int)_ws2812_spi_cfg.max_hz,
          (unsigned int)WS2812_ENCODE_MODE_BITS);

#ifdef PKG_WS2812_USING_BDMA_BUFFER
    /* Use the static buffer placed in SRAM4 by the linker script instead of
     * the heap: a heap buffer is usually not reachable by BDMA. */
    _dev.buf = _ws2812_bdma_buf;
#else
    _dev.buf = (rt_uint8_t *)rt_malloc(ws2812_tx_buf_size(PKG_WS2812_LED_NUM));
#endif
    if (_dev.buf == RT_NULL)
    {
        LOG_E("Failed to allocate color buffer.");
        rt_spi_bus_detach_device(spi_dev);
        rt_free(spi_dev);
        return -RT_ENOMEM;
    }

    _dev.spi = spi_dev;
    _dev.led_num = PKG_WS2812_LED_NUM;
    rt_memset(_dev.buf, 0, ws2812_tx_buf_size(_dev.led_num));

    _ws2812_inited = 1;
    return RT_EOK;
}
INIT_DEVICE_EXPORT(ws2812_hw_init);

/**
 * Get the singleton WS2812 device instance
 * @return Pointer to the WS2812 device structure, or NULL if not initialized
 */
struct ws2812_device *ws2812_get_device(void)
{
    if (!_ws2812_inited)
        return RT_NULL;
    return &_dev;
}

rt_size_t ws2812_data_buf_size(rt_uint16_t led_num)
{
    return (rt_size_t)led_num * WS2812_ENCODED_BYTES_PER_LED;
}

rt_size_t ws2812_reset_buf_size(void)
{
    return WS2812_RESET_BYTES;
}

rt_size_t ws2812_tx_buf_size(rt_uint16_t led_num)
{
    return ws2812_data_buf_size(led_num) + ws2812_reset_buf_size();
}

void ws2812_dump_pixel(struct ws2812_device *ws, rt_uint16_t index)
{
    rt_uint8_t *src;
    rt_size_t i;

    if (!_ws2812_inited || ws == RT_NULL || ws->buf == RT_NULL || index >= ws->led_num)
        return;

    src = &ws->buf[index * WS2812_ENCODED_BYTES_PER_LED];

    rt_kprintf("ws2812 pixel[%u], encode=%u-bit, bytes=%u: ",
               (unsigned int)index,
               (unsigned int)WS2812_ENCODE_MODE_BITS,
               (unsigned int)WS2812_ENCODED_BYTES_PER_LED);
    for (i = 0; i < WS2812_ENCODED_BYTES_PER_LED; i++)
    {
        rt_kprintf("%02X ", src[i]);
    }
    rt_kprintf("\n");
}

int ws2812_transmit(struct ws2812_device *ws)
{
    rt_ssize_t length;
    rt_size_t frame_size;

    if (!_ws2812_inited || ws == RT_NULL || ws->spi == RT_NULL || ws->buf == RT_NULL)
        return -RT_ERROR;

    frame_size = ws2812_tx_buf_size(ws->led_num);
    length = rt_spi_transfer(ws->spi, ws->buf, RT_NULL, frame_size);

    if (length != (rt_ssize_t)frame_size)
    {
        LOG_E("SPI transfer failed, expected=%u bytes, sent=%d bytes",
              (unsigned int)frame_size, (int)length);
        return -RT_ERROR;
    }

#if defined(PKG_WS2812_USING_CLK_DEBUG) && defined(SPI_CFG1_MBR_Msk)
    /* STM32H7 SPI6 kernel clock inspection, for timing validation */
    if (!_ws2812_spi_clock_logged)
    {
        rt_uint32_t spi_clock;
        rt_uint32_t mbr;
        rt_uint32_t actual_hz;

        spi_clock = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI6);
        mbr = (SPI6->CFG1 & SPI_CFG1_MBR_Msk) >> SPI_CFG1_MBR_Pos;
        actual_hz = spi_clock / (1UL << (mbr + 1U));

        LOG_I("SPI6 kernel=%u Hz, MBR=/%u, SCK=%u Hz, driver=%u Hz",
              (unsigned int)spi_clock,
              (unsigned int)(1UL << (mbr + 1U)),
              (unsigned int)actual_hz,
              (unsigned int)ws->spi->config.usage_freq);
        _ws2812_spi_clock_logged = 1;
    }
#endif

    return RT_EOK;
}

int ws2812_encode_byte_3bit(rt_uint8_t *dst, rt_uint8_t byte)
{
    rt_uint32_t packed = 0;
    rt_uint8_t i;

    if (dst == RT_NULL)
        return -RT_EINVAL;

    for (i = 0; i < 8U; i++)
    {
        packed <<= 3;
        packed |= (byte & (1U << (7U - i))) ?
                  WS2812_3BIT_SPI_CODE_1 : WS2812_3BIT_SPI_CODE_0;
    }

    dst[0] = (rt_uint8_t)(packed >> 16);
    dst[1] = (rt_uint8_t)(packed >> 8);
    dst[2] = (rt_uint8_t)packed;

    return RT_EOK;
}

int ws2812_encode_byte_4bit(rt_uint8_t *dst, rt_uint8_t byte)
{
    rt_uint32_t packed = 0;
    rt_uint8_t i;

    if (dst == RT_NULL)
        return -RT_EINVAL;

    for (i = 0; i < 8U; i++)
    {
        packed <<= 4;
        packed |= (byte & (1U << (7U - i))) ?
                  WS2812_4BIT_SPI_CODE_1 : WS2812_4BIT_SPI_CODE_0;
    }

    dst[0] = (rt_uint8_t)(packed >> 24);
    dst[1] = (rt_uint8_t)(packed >> 16);
    dst[2] = (rt_uint8_t)(packed >> 8);
    dst[3] = (rt_uint8_t)packed;

    return RT_EOK;
}

int ws2812_encode_byte_8bit(rt_uint8_t *dst, rt_uint8_t byte)
{
    rt_uint8_t index;

    if (dst == RT_NULL)
        return -RT_EINVAL;

    for (index = 0; index < 8U; index++)
    {
        dst[index] = (byte & (1U << (7U - index))) ?
                     WS2812_8BIT_SPI_CODE_1 : WS2812_8BIT_SPI_CODE_0;
    }

    return RT_EOK;
}

int ws2812_set_pixel(struct ws2812_device *ws,
                     rt_uint16_t index,
                     ws2812_color_t color)
{
    rt_uint8_t *dst;

    if (!_ws2812_inited || ws == RT_NULL || index >= ws->led_num)
        return -RT_ERROR;

    dst = &ws->buf[index * WS2812_ENCODED_BYTES_PER_LED];

#if WS2812_ENCODE_MODE_BITS == 3U
    ws2812_encode_byte_3bit(dst + 0, color.green);
    ws2812_encode_byte_3bit(dst + WS2812_ENCODED_BYTES_PER_COLOR, color.red);
    ws2812_encode_byte_3bit(dst + 2 * WS2812_ENCODED_BYTES_PER_COLOR, color.blue);
#elif WS2812_ENCODE_MODE_BITS == 4U
    ws2812_encode_byte_4bit(dst + 0, color.green);
    ws2812_encode_byte_4bit(dst + WS2812_ENCODED_BYTES_PER_COLOR, color.red);
    ws2812_encode_byte_4bit(dst + 2 * WS2812_ENCODED_BYTES_PER_COLOR, color.blue);
#elif WS2812_ENCODE_MODE_BITS == 8U
    ws2812_encode_byte_8bit(dst + 0, color.green);
    ws2812_encode_byte_8bit(dst + WS2812_ENCODED_BYTES_PER_COLOR, color.red);
    ws2812_encode_byte_8bit(dst + 2 * WS2812_ENCODED_BYTES_PER_COLOR, color.blue);
#endif

    return RT_EOK;
}

int ws2812_build(struct ws2812_device *ws, const ws2812_color_t *data)
{
    rt_uint16_t index;

    if (!_ws2812_inited || ws == RT_NULL || ws->buf == RT_NULL || data == RT_NULL)
        return -RT_ERROR;

    for (index = 0; index < ws->led_num; index++)
    {
        ws2812_set_pixel(ws, index, data[index]);
    }

    return RT_EOK;
}

int ws2812_refresh(struct ws2812_device *ws)
{
    if (!_ws2812_inited || ws == RT_NULL)
        return -RT_ERROR;

    return ws2812_transmit(ws);
}
