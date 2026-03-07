#ifndef SD_SPI_H_
#define SD_SPI_H_

#include "stm32f446xx.h"
#include <stdint.h>
#include <stdbool.h>

// CS BSRR sessions for 6 PIN
#define CS_HIGH                     GPIO_BSRR_BS6
#define CS_LOW                      GPIO_BSRR_BR6

// Data token
#define SD_TOKEN_START_BLOCK        0xFE        // start transfer data
// Data response token
#define SD_DATA_RESP_MASK           0x1F
#define SD_DATA_RESP_ACCEPTED       0x05
#define SD_DATA_RESP_CRC_ERROR      0x0B
#define SD_DATA_RESP_WRITE_ERROR    0x0D
// Busy
#define SD_BUSY_TOKEN               0x00
// Idle / empty byte SPI
#define SD_DUMMY_BYTE               0xFF
// CRC empty byte
#define SD_DUMMY_CRC                0x01
// CMD args empty byte
#define SD_DUMMY_ARGS               0x00
/* SD type response */
#define SD_R1_READY_STATE       0x00
#define SD_R1_IDLE_STATE        (1 << 0)
#define SD_R1_ERASE_RESET       (1 << 1)
#define SD_R1_ILLEGAL_COMMAND   (1 << 2)
#define SD_R1_CRC_ERROR         (1 << 3)
#define SD_R1_ERASE_SEQ_ERROR   (1 << 4)
#define SD_R1_ADDRESS_ERROR     (1 << 5)
#define SD_R1_PARAMETER_ERROR   (1 << 6)
/* SD commands */
#define SD_CMD0_GO_IDLE_STATE        0x40  // CMD0 Reset
#define SD_CMD8_SEND_IF_COND         0x48  // CMD8 Check VCC (important for SDXC)
#define SD_CMD13_SEND_STATUS         0x4D  // CMD13
#define SD_CMD17_READ_SINGLE_BLOCK   0x51  // CMD17 Read sector
#define SD_CMD24_WRITE_BLOCK         0x58  // CMD24 Write sector
#define SD_CMD55_APP_CMD             0x77  // CMD55 Prefix for ACMD
#define SD_CMD58_READ_OCR            0x7A  // CMD58
#define SD_ACMD41_SD_SEND_OP_COND    0x69  // ACMD41 (after CMD55) Initializing

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} SPI_t ;


bool SPI_init(void);
bool SD_init(void);
static uint8_t SPI_transfer_data(uint8_t data);
static uint8_t SPI_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
uint8_t SD_ReadSector(uint32_t sector, uint8_t *buffer);

#endif
