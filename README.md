# STM32 RC522 Driver (SPI DMA, Non-Blocking)

STM32 RC522 (MFRC522) SPI driver for STM32 HAL using DMA and a non-blocking state-machine architecture.

This is an initial public version structured as a CubeIDE-style project.  
The driver is functional and will be cleaned, modularized, and documented in future revisions.

## Structure

src/
main.c
rc522.c
rc522_dma.c

include/
main.h
rc522.h
rc522_dma.h


## Notes

- Uses STM32 HAL
- Requires SPI + DMA configuration in CubeMX
- Currently uses global SPI handle (`hspi2`)

Documentation and refactoring in progress.

## License
MIT License

