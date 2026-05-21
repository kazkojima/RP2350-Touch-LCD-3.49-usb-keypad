#include "DEV_Config.h"
#include "sdc-spi.h"
#include <stdio.h>
#include <string.h>

#define SDC_DEBUG 0
#if SDC_DEBUG
#define DBG printf
#else
#define DBG(format, ...)
#endif

// Initialize SPI
// Send command to SD card
uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
  uint8_t crc;
  switch (cmd)
    {
    case CMD0: crc = 0x95; break;
    case CMD8: crc = 0x87; break;
    case CMD55: crc = 0x65; break;
    case ACMD41: crc = 0x77; break;
    default: crc = 0xff; break;
    }

  // Command format is always 6 bytes (cmd + 4 bytes arg + checksum)
  uint8_t frame[6];
  frame[0] = 0x40 | cmd;
  frame[1] = (arg >> 24) & 0xFF;
  frame[2] = (arg >> 16) & 0xFF;
  frame[3] = (arg >> 8) & 0xFF;
  frame[4] = arg & 0xFF;
  frame[5] = crc; // (crc7(frame, 5) << 1) | 0x01;

  spi_write_blocking(SPI_PORT, frame, 6);

  // Wait for the R1 response
  uint8_t response;
  for (int i = 0; i < 16; i++) {
    spi_read_blocking(SPI_PORT, 0xFF, &response, 1);
    DBG("cmd %d response(%d)  %02x\n", cmd, i, response);
    if (!(response & 0x80)) {
      break;
    }
  }

  return response;
}

bool sd_init_spi_mode(void)
{
  spi_init(SPI_PORT, 400 * 1000);

  gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SD_SCK_PIN,  GPIO_FUNC_SPI);
  gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);

  gpio_set_slew_rate(SD_SCK_PIN, GPIO_SLEW_RATE_FAST);
  gpio_set_drive_strength(SD_MOSI_PIN, GPIO_DRIVE_STRENGTH_4MA);
  gpio_set_drive_strength(SD_SCK_PIN, GPIO_DRIVE_STRENGTH_12MA);

  gpio_init(SD_CS_PIN);
  gpio_put(SD_CS_PIN, 1);
  gpio_set_dir(SD_CS_PIN, GPIO_OUT);
  gpio_set_drive_strength(SD_CS_PIN, GPIO_DRIVE_STRENGTH_2MA);
  sleep_ms(1);

  // send 74+ startup clocks to the CMD line with CS high
  uint8_t ones[10];
  memset(ones, 0xFF, sizeof(ones));
  spi_write_blocking(SPI_PORT, ones, sizeof(ones));

  sd_select();
  sd_send_cmd(CMD0, 0x0);

  uint8_t res;
  uint8_t r4[4];
  sd_send_cmd(CMD8, 0x1aa);
  for (int j=0; j < 4; j++) {
    spi_read_blocking(SPI_PORT, 0xFF, &r4[j], 1);
    DBG("CMD8(%d) %02x\n", j, r4[j]);
  }

  for (int i=0; i < 4; i++)
    {
      sd_send_cmd(CMD55, 0x0);
      for (int j=0; j < 4; j++) {
	spi_read_blocking(SPI_PORT, 0xFF, &res, 1);
	DBG("waiting D0 high after6 CMD55 %02x\n", res);
	if (0xff == res)
	  break;
      }
      spi_read_blocking(SPI_PORT, 0xFF, &res, 1);
      if (0xff != res)
	DBG("??? D0 should be high after CMD55 %02x\n", res);
      res = sd_send_cmd(ACMD41, 1 << 30);
      if ((res & 0x1) == 0) {
	//DEV_Digital_Write(DEBUG_LED, 1);
	break;
      }
      sleep_ms(200);
    }

  // Change spi clock speed to 10MHz
  sd_deselect();
  spi_set_baudrate(SPI_PORT, 10*1000000);
  sleep_ms(1);
  sd_select();

  DBG("send CMD16\n");
  res = sd_send_cmd(CMD16, 512);
  return (res == 0);
}

// Read a 512-byte block to a buffer
bool sd_read_block(uint32_t sector_addr, uint8_t *buffer)
{
  uint8_t response = sd_send_cmd(CMD17, sector_addr);
    
  if (response != 0x00) {
    return false; // Command rejected
  }
 
  // Wait for the data token (0xFE indicates start of block)
  uint8_t token;
  for (int i = 0; i < 10000; i++) {
    spi_read_blocking(SPI_PORT, 0xFF, &token, 1);
    if (token == 0xFE)
      break;
  }

  if (token != 0xFE) {
    return false; // Data token not found
  }

  // Read 512 bytes into the buffer
  spi_read_blocking(SPI_PORT, 0xFF, buffer, 512);

  // Read 2-byte CRC (ignore it, just clear the bus)
  uint8_t crc[2];
  spi_read_blocking(SPI_PORT, 0xFF, crc, 2);

  return true;
}
