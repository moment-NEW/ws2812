/*
 * Copyright (c) 2026, moment-NEW
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-03     moment-NEW   first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "ws2812.h"

static void ws2812_example(void)
{
    struct ws2812_device *ws = ws2812_get_device();
    static const struct
    {
        const char *name;
        rt_uint32_t rgb;
    } test_colors[] =
    {
        {"RED",   WS2812_COLOR_RED},
        {"GREEN", WS2812_COLOR_GREEN},
        {"BLUE",  WS2812_COLOR_BLUE},
        {"WHITE", WS2812_COLOR_WHITE},
        {"BLACK", WS2812_COLOR_BLACK},
    };
    rt_size_t index;

    if (ws == RT_NULL)
    {
        rt_kprintf("WS2812 device is not ready.\n");
        return;
    }

    for (index = 0; index < sizeof(test_colors) / sizeof(test_colors[0]); index++)
    {
        rt_kprintf("WS2812 test color: %s, rgb=0x%06X\n",
                   test_colors[index].name,
                   (unsigned int)test_colors[index].rgb);
        ws2812_set_pixel(ws, 0, WS2812_RGB2COLOR(test_colors[index].rgb));
        ws2812_dump_pixel(ws, 0);
        ws2812_refresh(ws);
        rt_thread_mdelay(500);
    }
}
MSH_CMD_EXPORT(ws2812_example, WS2812 RGB LED color cycle example);
