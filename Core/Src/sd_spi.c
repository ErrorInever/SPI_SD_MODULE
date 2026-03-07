#include "main.h"
#include "sd_spi.h"
#include "stm32f446xx.h"
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
    // CS -> 0. Start session
    CS.port->BSRR = CS_LOW;
    // Send index command (1 byte)
    SPI_transfer_data(cmd);
    // Send 4 byte args (2-5 bytes)
    SPI_transfer_data((uint8_t)(arg >> 24)); 
    SPI_transfer_data((uint8_t)(arg >> 16));
    SPI_transfer_data((uint8_t)(arg >> 8));
    SPI_transfer_data((uint8_t)arg);
    // Send CRC + stop (6 byte)
    SPI_transfer_data(crc | 0x01);
    // Wait response from SD
    uint8_t timeout = 100;
    uint8_t response = 0;
    do {
        response = SPI_transfer_data(SD_DUMMY_BYTE);
        timeout--;
    } while((response == SD_DUMMY_BYTE) && timeout != 0);
    // 6. CS -> 1. stop session
    while (SPI1->SR & SPI_SR_BSY); // waiting for the last bits to physically arrive
    CS.port->BSRR = CS_HIGH;
    SPI_transfer_data(SD_DUMMY_BYTE); // delay 8 tics
    return response;
}

bool SD_init(void) {
    // 1. APB2 = 50mhz. For init SD need 100–400kgz. divider 256.
    SPI1->CR1 |= (0x7 << 3);
    // 2. Power On Sequence: CS = 1, send 10 times 0xFF
    CS.port->BSRR = CS_HIGH;
    for(uint8_t i = 0; i < 10; i++) SPI_transfer_data(SD_DUMMY_BYTE);
    // 3. CMD0: arg = 0, crc = 0x95
    // Expected response: R1_IDLE_STATE (0x01)
    uint8_t res = 0;
    res = SPI_send_cmd(SD_CMD0_GO_IDLE_STATE, SD_DUMMY_ARGS, 0x95);
    if(res != SD_R1_IDLE_STATE) return 1;
    // 4. CMD8: arg = 0x1AA, crc = 0x87
    // This is a test: SD "I work on 3.3V and understand SDHC/SDXC"
    res = SPI_send_cmd(SD_CMD8_SEND_IF_COND, 0x1AA, 0x87);
    // TODO remove 4 byte from SPI
    if(res != SD_R1_IDLE_STATE) return 1;
    // 5. Initialization loop (ACMD41):
    uint16_t timeout = 1000;
    do {
        SPI_send_cmd(SD_CMD55_APP_CMD, SD_DUMMY_ARGS, SD_DUMMY_BYTE); // the next command will be specific
        // IMPORTANT for 64GB: in ACMD41 the argument must be 0x40000000 (HCS bit)
        res = SPI_send_cmd(SD_ACMD41_SD_SEND_OP_COND, 0x40000000, SD_DUMMY_BYTE);
        timeout--;
    } while((!(res == SD_R1_READY_STATE)) && timeout > 0); // Wait until the answer becomes SD_R1_READY_STATE (0x00)
    if (timeout == 0) return 1; // SD not response
    // 6. If successful, switch SPI to high speed (divider 2)
    SPI1->CR1 &= ~(0x7 << SPI_CR1_BR_Pos);
    SPI1->CR1 = (0x0 << 3); 

    while(SPI1->SR & SPI_SR_BSY);
    CS.port->BSRR = CS_LOW;

    return 0;
}

// --- Уровень 4: Работа с данными ---

uint8_t SD_ReadSector(uint32_t sector, uint8_t *buffer) {
    // 1. Отправить CMD17. Аргумент = номер сектора.
    // Помни: для SDXC (твоя 64Гб) адрес — это номер блока (0, 1, 2...), 
    // а не адрес байта.
    
    // 2. Ждать "Start Block Token" (0xFE) от карты
    
    // 3. Прочитать 512 байт в буфер
    
    // 4. Прочитать (пропустить) 2 байта CRC
    
    // 5. Поднять CS
    
    return 0;
}