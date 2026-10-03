# ws2812

## 1. 简介

**ws2812** 是一个基于 SPI 总线的 RT-Thread WS2812/WS2812B RGB LED 驱动软件包。

WS2812 使用单线归零码（Return-to-Zero）时序，对脉冲宽度要求严格，常规 GPIO
位操作难以在带_cache_的 Cortex-M7 上保证精度。本软件包利用 SPI MOSI 以固定
占空比的编码模拟 WS2812 时序：每个 WS2812 位由 N 个 SPI 位编码而成，
`0` 编码为 `100`，`1` 编码为 `110`（即高电平时间区分 0/1），传输完成后
保持低电平超过复位时间即可锁存一帧。

WS2812 关键时序参数（数据手册典型值）：

| 脉冲 | 定义 | 时间 | 容差 |
| :--: | :-- | :--: | :--: |
| T0H | 0 码高电平时间 | 0.4us | ±150ns |
| T1H | 1 码高电平时间 | 0.85us | ±150ns |
| T0L | 0 码低电平时间 | 0.85us | ±150ns |
| T1L | 1 码低电平时间 | 0.4us | ±150ns |
| RES | 复位（锁存）低电平时间 | >50us | - |

设 SPI 时钟为 `f`，每个 SPI 位时间为 `1/f`。以默认 8-bit 编码
（每个 WS2812 位占 8 个 SPI 位）为例，在 SCK = 6.25MHz 时，
每个 WS2812 位周期为 `8 / 6.25MHz = 1.28us`：
`0` 码（`0b01100000`）高电平 2 拍 = 0.32us，`1` 码（`0b01111000`）
高电平 4 拍 = 0.64us，满足器件实际容差（已在 DM-MC02 开发板上长时间验证）。

## 2. 特性

- 支持轮询 / DMA / BDMA 传输（取决于 BSP 中 SPI 总线的配置模式）
- 支持 3-bit / 4-bit / 8-bit 三档编码密度（`WS2812_ENCODE_MODE_BITS`），
  便于根据实际 SCK 调整时序
- 支持静态 BDMA 缓冲区：自动放入 BSP 链接脚本预留的 `.ws2812_bdma`
  段（SRAM4），规避 STM32H7 BDMA 只能访问 D3 域 SRAM4 的限制
- 提供 `ws2812_example` FinSH/MSH 示例命令
- API 简洁：`ws2812_set_pixel` + `ws2812_refresh` 两步刷新

## 3. 依赖

- RT-Thread 4.0+（使用 `rt_spi_*` 设备框架与自动初始化机制）
- SPI 总线已由 BSP 启用并注册（如 `BSP_USING_SPI6`）
- 使用 BDMA 模式时：BSP 需启用 SPI 的 BDMA 传输选项，并在链接脚本中
  预留 `.ws2812_bdma` 段到 SRAM4（参考 `stm32h723-DM-MC02` BSP 的
  `link.lds` / `link.sct` / `link.icf`）

## 4. 配置

在 BSP 目录下执行 `menuconfig`，路径：

```
RT-Thread online packages
    peripheral libraries and drivers
        ws2812: WS2812 RGB LED driver using SPI
```

| 选项 | 类型 | 默认值 | 说明 |
| :-- | :--: | :--: | :-- |
| `PKG_WS2812_SPI_BUS_NAME` | string | `spi6` | WS2812 挂接的 SPI 总线名 |
| `PKG_WS2812_SPI_FREQUENCY` | int | `4000000` | 请求的 SPI 时钟（Hz），实际 SCK 由总线分频决定 |
| `PKG_WS2812_LED_NUM` | int | `1` | 级联 LED 数量 |
| `PKG_WS2812_USING_BDMA_BUFFER` | bool | `n` | 使用 `.ws2812_bdma` 段中的静态缓冲（BDMA 场景，见下） |
| `PKG_WS2812_USING_CLK_DEBUG` | bool | `n` | 运行时打印 SPI6 实际 SCK（STM32H7 专用调试选项） |
| `PKG_WS2812_USING_EXAMPLE` | bool | `n` | 编译示例命令 `ws2812_example` |

## 5. 使用示例

```c
#include "ws2812.h"

void led_demo(void)
{
    struct ws2812_device *ws = ws2812_get_device();
    if (ws == RT_NULL)
        return;

    ws2812_set_pixel(ws, 0, WS2812_RGB2COLOR(0xFF0000)); /* red */
    ws2812_set_pixel(ws, 1, WS2812_RGB2COLOR(0x00FF00)); /* green */
    ws2812_refresh(ws);                                   /* latch the frame */
}
```

或在 MSH 中运行示例：

```
msh> ws2812_example
```

## 6. API 说明

| API | 说明 |
| :-- | :-- |
| `ws2812_hw_init()` | 初始化 SPI 设备并分配缓冲（`INIT_DEVICE_EXPORT` 自动调用） |
| `ws2812_get_device()` | 获取单例设备句柄，未初始化返回 `RT_NULL` |
| `ws2812_set_pixel(ws, index, color)` | 设置第 `index` 颗 LED 颜色（GRB 自动编码） |
| `ws2812_build(ws, data)` | 批量写入整帧颜色数组 |
| `ws2812_refresh(ws)` | 编码缓冲整帧发送并锁存 |
| `ws2812_transmit(ws)` | 仅发送当前缓冲 |
| `ws2812_dump_pixel(ws, index)` | 打印某颗 LED 的编码字节（调试用） |
| `ws2812_encode_byte_3/4/8bit()` | 单字节编码原语 |
| `ws2812_tx_buf_size(led_num)` 等 | 缓冲大小计算 |

## 7. 注意事项

- **BDMA 与 SRAM4**：STM32H7 的 BDMA 位于 D3 域，只能访问 SRAM4
  （`0x38000000`）。SPI 总线配置为 BDMA 传输时，必须开启
  `PKG_WS2812_USING_BDMA_BUFFER`，且 BSP 链接脚本需包含
  `.ws2812_bdma` 段定义；否则请保持关闭，使用堆缓冲。
- **频率选择**：请求频率与实际 SCK 存在分频误差。以 DM-MC02
  （SPI6 内核时钟 100MHz）为例，请求 4MHz 实际得到 6.25MHz SCK，
  配合默认 8-bit 编码实测稳定。可开启 `PKG_WS2812_USING_CLK_DEBUG`
  观察实际 SCK 后再调整。
- **CubeMX**：不建议在 CubeMX 中为该 SPI 开启 DMA/BDMA，统一由
  RT-Thread 设备框架管理。

## 8. 许可证

采用 [Apache-2.0](LICENSE) 许可证，版权所有 (c) 2026 moment-NEW。
