#include "main.h"
#include "sd_spi.h"
#include "stm32f446xx.h"
#include "uart.h"
#include <stdint.h>
#include <stdbool.h>

/*
--------SPI PINS--------
VCC     5V
GND
SCK     PA5
MISO    PA6
MOSI    PA7
CS      PB6
------------------------
*/
/*SPI interface*/
static const SPI_t SCK = {GPIOA, GPIO_PIN_5};      // Serial clock: Clock signal. Generated only by the master (STM32)
static const SPI_t MISO = {GPIOA, GPIO_PIN_6};     // Master In Slave Out: Data from the device to the STM32
static const SPI_t MOSI = {GPIOA, GPIO_PIN_7};     // Master Out Slave In: Data from STM32 to the device
static const SPI_t CS = {GPIOB, GPIO_PIN_6};       // Chip Select: The pin we press to ground (0) to tell a particular device: "I'm talking to you"


bool SPI_init(void) {
    // RCC
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    // CS output
    CS.port->MODER &= ~GPIO_MODER_MODER6_Msk;
    CS.port->MODER |= GPIO_MODER_MODER6_0;
    CS.port->BSRR = CS_HIGH; // when init it must be 1, because device select by low front 0. setup CS = 1 (High front)
    // Setup pins SCK, MISO and MOSI to AF5
    SCK.port->MODER &= ~GPIO_MODER_MODER5_Msk;
    MISO.port->MODER &= ~GPIO_MODER_MODER6_Msk;
    MOSI.port->MODER &= ~GPIO_MODER_MODER7_Msk;
    SCK.port->MODER  |= GPIO_MODER_MODER5_1;
    MISO.port->MODER |= GPIO_MODER_MODER6_1;
    MOSI.port->MODER |= GPIO_MODER_MODER7_1;
    SCK.port->AFR[0] &= ~GPIO_AFRL_AFSEL5_Msk;
    SCK.port->AFR[0] |= (5UL << GPIO_AFRL_AFSEL5_Pos);
    MISO.port->AFR[0] &= ~GPIO_AFRL_AFSEL6_Msk;
    MISO.port->AFR[0] |= (5UL << GPIO_AFRL_AFSEL6_Pos);
    MOSI.port->AFR[0] &= ~GPIO_AFRL_AFSEL7_Msk;
    MOSI.port->AFR[0] |= (5UL << GPIO_AFRL_AFSEL7_Pos);
    // Very high speed
    SCK.port->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR5;
    SCK.port->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR5;
    MISO.port->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR6;
    MISO.port->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6;
    MOSI.port->OSPEEDR &= ~GPIO_OSPEEDER_OSPEEDR7;
    MOSI.port->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR7;
    // SPI Logic
    SPI1->CR1 = 0;
    SPI1->CR1 |= (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI);
    SPI1->CR1 |= (0x7 << 3); // APB2 = 50mhz. For init SD need 100–400kgz, therefore devider = 256. 
    // Enable
    SPI1->CR1 |= SPI_CR1_SPE;
    return 0;
}

static uint8_t SPI_transfer_data(uint8_t data) {
    while(!(SPI1->SR & SPI_SR_TXE)); // wait empty buffer for transfer next byte
    SPI1->DR = data; // transfer data
    while(!(SPI1->SR & SPI_SR_RXNE)); // wait response 1 (byte in register)
    return (uint8_t)SPI1->DR; // must be read DR for reset flag RXNE
}


static uint8_t SPI_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    // Send index command (1 byte)
    SPI_transfer_data(cmd);
    // Send 4 byte args (2-5 bytes)
    SPI_transfer_data((uint8_t)(arg >> 24)); 
    SPI_transfer_data((uint8_t)(arg >> 16));
    SPI_transfer_data((uint8_t)(arg >> 8));
    SPI_transfer_data((uint8_t)arg);
    // Send CRC + stop (6 byte)
    SPI_transfer_data(crc | 0x01);
    SD_delay();
    // Wait response from SD
    uint8_t timeout = 100;
    uint8_t res = 0;
    do {
        res = SPI_transfer_data(SD_DUMMY_BYTE);
    } while(res == SD_DUMMY_BYTE && --timeout > 0);
    
    return res;
}

static inline void SD_delay(void) {
    SPI_transfer_data(SD_DUMMY_BYTE);
}

static inline void SD_start_session(void) {
    CS.port->BSRR = CS_LOW;
}

static void SD_stop_session(void) {
    while (SPI1->SR & SPI_SR_BSY);
    CS.port->BSRR = CS_HIGH;
    SD_delay(); // delay()
}

// static uint8_t SD_wait_ready(void) {
//     uint32_t timeout = 500000UL;
//     while (SPI_transfer_data(0xFF) != 0xFF && --timeout > 0);
//     return (timeout > 0) ? 0 : 1; // 0 - ready, 1 - timeout
// }

static uint8_t SD_wait_ready(void) {
    uint32_t timeout = 1000000UL; // Увеличим для 64Гб
    uint8_t last_byte = 0;
    
    while (timeout > 0) {
        last_byte = SPI_transfer_data(0xFF);
        if (last_byte == 0xFF) return 0; // Успех! Карта готова
        timeout--;
    }
    
    // Если дошли сюда — беда.
    UART_Printf("WaitReady Timeout! Last byte: 0x%02X\r\n", last_byte);
    return 1;
}

bool SD_init(void) {
    // 1. APB2 = 50mhz. For init SD need 100–400kgz. divider 256.
    SPI1->CR1 |= (0x7 << 3);

    // 2. Power On Sequence: CS = 1, send 10 times 0xFF
    SD_stop_session();
    for(uint8_t i = 0; i < 10; i++) SPI_transfer_data(SD_DUMMY_BYTE);

    // 3. CMD0: arg = 0, crc = 0x95
    // Expected response: R1_IDLE_STATE (0x01)
    SD_start_session();
    uint8_t res = 0;
    res = SPI_send_cmd(SD_CMD0_GO_IDLE_STATE, SD_DUMMY_ARGS, 0x95);
    if(res != SD_R1_IDLE_STATE) return 1;
    UART_Printf("init() CMD0 response: %X\texpected 0x01\r\n", res);
    SD_stop_session();

    // 4. CMD8: arg = 0x1AA, crc = 0x87
    // This is a test: SD "I work on 3.3V and understand SDHC/SDXC"
    SD_start_session();
    res = SPI_send_cmd(SD_CMD8_SEND_IF_COND, 0x1AA, 0x87);
    if(res == SD_R1_IDLE_STATE) { // clear 4 byte
        uint32_t echo = 0;
        for(int i=0; i<4; i++) {
            echo = (echo << 8) | SPI_transfer_data(0xFF);
            }
        UART_Printf("init() CMD8 Echo: 0x%08lX\r\n", echo);
    }
    SD_stop_session();

    // 5. Initialization loop (ACMD41):
    uint16_t timeout = 1000;
    UART_Printf("init() Waiting for card to be ready...\r\n");
    do {
        SD_start_session(); 
        SPI_send_cmd(SD_CMD55_APP_CMD, SD_DUMMY_ARGS, SD_DUMMY_BYTE); // the next command will be specific
        // IMPORTANT for 64GB: in ACMD41 the argument must be 0x40000000 (HCS bit)
        res = SPI_send_cmd(SD_ACMD41_SD_SEND_OP_COND, 0x40000000, SD_DUMMY_BYTE);
         SD_stop_session();
    } while((!(res == SD_R1_READY_STATE)) && --timeout > 0); // Wait until the answer becomes SD_R1_READY_STATE (0x00)
    if (timeout == 0) return 1; // SD not response

    UART_Printf("init() ACMD41 response: %X\texpected 0x00\r\n", res);

    SD_start_session();
    res = SPI_send_cmd(SD_CMD58_READ_OCR, 0, 0xFF);
    UART_Printf("init() CMD58 response: %X\r\n", res);
    if (res == 0x00) {
        uint8_t ocr[4];
        for(int i=0; i<4; i++) ocr[i] = SPI_transfer_data(0xFF);
        
        // 30-й бит первого байта (OCR[0]) показывает тип карты
        if (ocr[0] & 0x40) {
            UART_Printf("Card Type: SDHC/SDXC (Block addressing)\r\n");
        } else {
            UART_Printf("Card Type: SDSC (Byte addressing)\r\n");
        }
    }
    SD_stop_session();

    // 6. If successful, switch SPI to high speed (divider 2)
    
    // SPI1->CR1 &= ~(0x7 << SPI_CR1_BR_Pos); 25Mhz

    // SPI1->CR1 &= ~(0x7 << SPI_CR1_BR_Pos); 
    // SPI1->CR1 |= (0x3 << SPI_CR1_BR_Pos); // 3.125Mhz

    SPI1->CR1 &= ~SPI_CR1_SPE;

    SPI1->CR1 &= ~(0x7 << SPI_CR1_BR_Pos);
    SPI1->CR1 |= (0x4 << SPI_CR1_BR_Pos); // 1.5 Mhz

    SPI1->CR1 |= SPI_CR1_SPE;

    return 0;
}

// --- Уровень 4: Работа с данными ---

uint8_t SD_ReadSector(uint32_t sector, uint8_t *buffer) {
    // 1. Send CMD17. Argument = sector number.
    // For SDXC (your 64GB) the address is the block number (0, 1, 2...)
    uint8_t res;
    SD_start_session();
    if (SD_wait_ready() != 0) { // wait SD wakeup
        SD_stop_session();
        return 5; 
    }

    res = SPI_send_cmd(SD_CMD17_READ_SINGLE_BLOCK, sector, SD_DUMMY_CRC);
    UART_Printf("CMD17 res: 0x%02X\r\n", res);
    if(res != SD_R1_READY_STATE) {
        SD_stop_session();
        return 1; // error
    }
    SD_delay();
    // 2. Wait for the "Start Block Token" (0xFE) from the SD
    uint32_t timeout = 100000UL;
    uint8_t token;
    do {
        token = SPI_transfer_data(SD_DUMMY_BYTE);
    } while(token == 0xFF && --timeout > 0);

    if(token != SD_TOKEN_START_BLOCK) {
        UART_Printf("Read_sector: 0xFE not received. token = %X\r\n", token);
        SD_stop_session();
        return 2; // error: token not received

    }

    // 3. Read 512 bytes into buffer
    for(uint16_t i = 0; i < 512; i++) {
        buffer[i] = SPI_transfer_data(SD_DUMMY_BYTE);
    }
    
    // 4. Read (skip) 2 bytes of CRC
    SPI_transfer_data(SD_DUMMY_BYTE); 
    SPI_transfer_data(SD_DUMMY_BYTE);

    SD_stop_session();
    
    return 0;
}

uint8_t SD_WriteSector(uint32_t sector, uint8_t *buffer) {
    if(sector < 2000) {
        UART_Printf("Danger: a sector closer to zero can erase the partition table(MBR)\r\n");
        return 1;
    }
    CS.port->BSRR = CS_HIGH;
    for(int i=0; i<10; i++) SPI_transfer_data(0xFF);

    SD_start_session();
    (void)SPI1->DR;

    if (SD_wait_ready() != 0) {
        UART_Printf("Still Busy. Sending CMD13...\r\n");
        SPI_send_cmd(SD_CMD13_SEND_STATUS, 0, 0xFF);
        SPI_transfer_data(0xFF);
        SPI_transfer_data(0xFF);
        
        if (SD_wait_ready() != 0) {
            SD_stop_session();
            return 5; 
        }
    }
    // 1. Send CMD24. Argument = sector number.
    // For SDXC (your 64GB) the address is the block number (0, 1, 2...)
    uint8_t res = SPI_send_cmd(SD_CMD24_WRITE_BLOCK, sector, SD_DUMMY_CRC);
    if(res != SD_R1_READY_STATE) {
        SD_stop_session();
        return 2; // error
    }
    SD_delay();

    // 2. Send token start block (0xFE)
    SPI_transfer_data(SD_TOKEN_START_BLOCK);
    for(uint16_t i = 0; i < 512; i++) {
        SPI_transfer_data(buffer[i]);
    }
    SPI_transfer_data(SD_DUMMY_BYTE); 
    SPI_transfer_data(SD_DUMMY_BYTE);

    // Receive a response from the card to the sent block
    uint8_t data_res = SPI_transfer_data(SD_DUMMY_BYTE);
    // Mask 0x1F: The response must be xxxx0101 (0x05) - "Data accepted"
    if ((data_res & 0x1F) != 0x05) {
        SD_stop_session();
        return 3; // Write error (rejected by card)
    }

    // 2. Waiting for writing to complete (Busy state)
    // While the card is writing data from the buffer to flash memory, it sets MISO to zero.
    // Send 0xFF and wait until NOT zero returns.
    uint32_t timeout = 1000000UL; 
    while (SPI_transfer_data(0xFF) != 0xFF && --timeout > 0);

    SD_stop_session();

    return (timeout == 0) ? 4 : 0;
}