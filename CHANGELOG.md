# Changelog

## v1.0.0 - 2026-10-03

- Initial release
- SPI-based WS2812/WS2812B driver with 3/4/8-bit encodable density
- Poll / DMA / BDMA transfer support (buffer placement in SRAM4 via the
  `.ws2812_bdma` linker section, for STM32H7 BDMA)
- `ws2812_example` MSH example command
