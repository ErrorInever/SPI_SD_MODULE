#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "ringbuffer.h"
#include "stm32f4xx.h"


#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL()  __enable_irq()

bool cb_init(GenericCB *cb, void *storage, size_t element_size, size_t size, bool override) {
    if (cb == NULL || storage == NULL) return false;
    if (size == 0 || (size & (size - 1)) != 0) {
        return false; 
    }
    cb->buffer = (uint8_t *)storage;
    cb->element_size = element_size;
    cb->mask = size - 1;
    cb->override = override;
    cb->head = 0;
    cb->tail = 0;
    return true;
}

bool cb_push(GenericCB *cb, const void *data) {
    if (is_full(cb)) {
        if (cb->override == true) cb->tail = (cb->tail + 1) & cb->mask;
        else return false;
    }
    void *dest = &cb->buffer[cb->head * cb->element_size];
    memcpy(dest, data, cb->element_size);
    cb->head = (cb->head + 1) & cb->mask;
    return true;
}

bool cb_push_safe(GenericCB *cb, const void *data) {
    ENTER_CRITICAL();
    bool result = cb_push(cb, data);
    EXIT_CRITICAL();
    return result;
}

bool cb_pop(GenericCB *cb, void *out_data) {
    if(is_empty(cb)) return false;
    const void *const src = &cb->buffer[cb->tail * cb->element_size];
    memcpy(out_data, src, cb->element_size);
    cb->tail = (cb->tail + 1) & cb->mask;
    return true;
}

bool cb_peek(const GenericCB *__restrict cb, void *__restrict out_data) {
    if (is_empty(cb)) return false;
    const void *const src = &cb->buffer[cb->tail * cb->element_size];
    memcpy(out_data, src, cb->element_size);
    return true;
}