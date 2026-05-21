#ifndef _SDC_SPI_H_
#define _SDC_SPI_H_

// minimal SD Card Commands
#define CMD0     0
#define CMD1     1
#define CMD8     8
#define CMD16   16
#define CMD55   55
#define ACMD41 41
#define CMD17   17 // Read Single Block

static inline void sd_select() { gpio_put(SD_CS_PIN, 0); asm volatile("nop \n"); }
static inline void sd_deselect() { asm volatile("nop \n"); gpio_put(SD_CS_PIN, 1); }

uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg);
bool sd_init_spi_mode(void);
bool sd_read_block(uint32_t sector_addr, uint8_t *buffer);

#endif
