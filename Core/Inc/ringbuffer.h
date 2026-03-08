/**
 * @file circular_buffer.h
 * @brief Universal thread-safe Ring Buffer implementation for STM32.
 * * Design features:
 * 1. Power-of-two sizing for fast index wrapping via bitwise AND mask.
 * 2. Generic element size support using memcpy.
 * 3. Atomic push operations for thread safety (main loop vs interrupts).
 * 4. Configurable overflow policy (override or reject).
 */

 /** * @brief Critical section macros to prevent race conditions.
 */
#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @struct GenericCB
 * @brief Circular Buffer control structure.
 */
typedef struct {
    uint8_t *buffer;
    size_t element_size;
    size_t mask;
    volatile size_t head;
    volatile size_t tail;
    bool override;
} GenericCB;

/**
 * @brief Initializes the circular buffer.
 * * @param cb Pointer to the GenericCB structure.
 * @param storage Pointer to the pre-allocated memory block.
 * @param element_size Size of each element (e.g., sizeof(uint8_t)).
 * @param size Number of elements (MUST be a power of two).
 * @param override Set true to allow overwriting old data when the buffer is full.
 * @return true if initialization succeeded, false if size is not a power of two.
 */
bool cb_init(GenericCB *cb, void *storage, size_t element_size, size_t mask, bool override);

/**
 * @brief Checks if the buffer is full.
 * @note One slot is always left empty to distinguish from the 'empty' state.
 */
static inline bool is_full(const GenericCB *cb) {
    return ((cb->head + 1) & cb->mask) == cb->tail;
}

/**
 * @brief Checks if the buffer is empty.
 */
static inline bool is_empty(const GenericCB *cb) {
    return cb->head == cb->tail;
}

/**
 * @brief Pushes data into the buffer.
 * @warning Not thread-safe if called from main while interrupts are active.
 * * @param cb Pointer to the buffer.
 * @param data Pointer to the source data.
 * @return true if pushed, false if full (and override is disabled).
 */
 bool cb_push(GenericCB *cb, const void *data);

 /**
 * @brief Thread-safe version of cb_push.
 * Disables interrupts during index manipulation to prevent race conditions.
 */
 bool cb_push_safe(GenericCB *cb, const void *data);

 /**
 * @brief Pops data out of the buffer.
 * * @param cb Pointer to the buffer.
 * @param out_data Pointer to the destination where data will be copied.
 * @return true if data was retrieved, false if the buffer was empty.
 */
bool cb_pop(GenericCB *cb, void *out_data);
/**
 * @brief Returns data from the tail without removing it from the buffer.
 */
bool cb_peek(const GenericCB *cb, void *out_data);

#endif