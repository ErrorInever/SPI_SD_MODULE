#include "uart.h"
#include "stm32f446xx.h"
#include "ringbuffer.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

volatile bool needs_log = false;

#define UART_TX_BUF_SIZE 128
static uint8_t tx_storage[UART_TX_BUF_SIZE];
static GenericCB  uart_tx_cb; // ring buffer

#define APB1_SPEED 0xD9  // bus speed 42Mhz
#define AF7 0x07


void USART2_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // RCC usart2
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // RCC port A
  (void)RCC->APB1ENR;
  // init ring buffer
  cb_init(&uart_tx_cb, tx_storage, sizeof(uint8_t), 
  UART_TX_BUF_SIZE, true);
  // Set mode AF for pa2 and pa3
  GPIOA->MODER &= ~GPIO_MODER_MODER2_Msk;
  GPIOA->MODER &= ~GPIO_MODER_MODER3_Msk;
  GPIOA->MODER |= GPIO_MODER_MODER2_1;
  GPIOA->MODER |= GPIO_MODER_MODER3_1;
  // Set AF USART2 for pa2 and pa3
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2_Msk;
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL3_Msk;
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL2_Pos);
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL3_Pos);

  USART2->BRR = APB1_SPEED; // Default UART mode, OVER8=0
  USART2->CR1 |= USART_CR1_TE |         // Enable transmitter
                  USART_CR1_RE |        // Enable receiver
                  USART_CR1_RXNEIE |    // Enable interrupt on receive
                  USART_CR1_UE;         // Enable USART
  
  // add usart2 interrupt and set priority to NVIC
  NVIC_SetPriority(USART2_IRQn, 2);
  NVIC_EnableIRQ(USART2_IRQn);

  (void)USART2->DR; // reset flags
  (void)USART2->SR;
}

void USART2_IRQHandler(void) {
  // if register is empty i.e. TXE flag is 1 and if TXE interrupt Enable i.e. we want to send something
  if((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {
    uint8_t out_data; // pointer to data from pop ring buffer
    if(cb_pop(&uart_tx_cb, &out_data)) {
      USART2->DR = out_data;
    } else {
      // buffer is empty therefore we need disable TXEIE
      USART2->CR1 &= ~USART_CR1_TXEIE; 
    }
  } 
  // if recieve 
  if(USART2->SR & USART_SR_RXNE) {
    char cmd = (char)USART2->DR; // reading from DR automaticly reset flag RXNE
    if (cmd == '+') {

      needs_log = true;
    }
    if (cmd == '-') {
      
      needs_log = true;
    }
  }
}

// send raw byte
void USART2_send_byte(const char byte) {
  while(!(USART2->SR & USART_SR_TXE)); // wait TXE becomes 1 i.e. buffer becomes empty
  USART2->DR = byte;
}

// send string
void USART2_log(const char *str) {
  while(*str) {
    cb_push_safe(&uart_tx_cb, (void*)str++);
  }
  // "Kick" the UART TX interrupt to start transmission
  USART2->CR1 |= USART_CR1_TXEIE;
}

/**
 * @brief Formatted print to UART using the circular buffer.
 * @param format String format (like printf)
 * @param ... Arguments
 */
void UART_Printf(const char *format, ...) {
    char temp_str[128]; // Temporary buffer for formatted string
    va_list args;
    
    va_start(args, format);
    // Formatting the string into our temporary buffer
    int len = vsnprintf(temp_str, sizeof(temp_str), format, args);
    va_end(args);

    if (len > 0) {
        // Push each character into the ring buffer safely
        for (int i = 0; i < len; i++) {
            cb_push_safe(&uart_tx_cb, &temp_str[i]);
        }
        
        // "Kick" the UART TX interrupt to start transmission
        USART2->CR1 |= USART_CR1_TXEIE;
    }
}