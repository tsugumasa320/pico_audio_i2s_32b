/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Modified by Elehobica, 2021
// Enhanced with comprehensive documentation by Claude, 2025

/**
 * @file audio.cpp
 * @brief Audio Buffer Management and Sample Conversion for Pico Audio
 * 
 * This file implements the core audio buffer management system for Raspberry Pi Pico
 * audio applications. It provides:
 * 
 * - Thread-safe audio buffer pools
 * - Sample format conversion between different PCM formats
 * - Buffer allocation and recycling
 * - Connection management between audio producers and consumers
 * 
 * The buffer management system uses linked lists with careful locking to ensure
 * thread safety while maintaining low latency for real-time audio processing.
 */

// ============================================================================
// System Includes
// ============================================================================

#include <cstring>              // For memory operations
#include "pico/audio.h"         // Audio framework definitions
#include "pico/sample_conversion.h"  // Sample format conversion utilities

// ============================================================================
// Debug Configuration
// ============================================================================

/**
 * @brief Enable audio-specific assertions for debug builds
 * 
 * When enabled, provides additional validation checks for audio buffer
 * operations. Disable in release builds for optimal performance.
 */
#define ENABLE_AUDIO_ASSERTIONS

#ifdef ENABLE_AUDIO_ASSERTIONS
/**
 * @brief Audio-specific assertion macro
 * 
 * Provides assertion checking specifically for audio operations.
 * Can be independently controlled from general system assertions.
 */
#define audio_assert(x) assert(x)
#else
#define audio_assert(x) (void)0
#endif

// ============================================================================
// Internal Buffer List Management
// ============================================================================

/**
 * @brief Remove and return the first buffer from a linked list
 * 
 * This function atomically removes the head buffer from a singly-linked
 * list of audio buffers. The removed buffer's next pointer is set to NULL.
 * 
 * @param phead Pointer to the list head pointer
 * @return Removed buffer, or NULL if list was empty
 * 
 * @note This function is not thread-safe by itself - caller must provide
 *       appropriate synchronization.
 */
inline static audio_buffer_t *list_remove_head(audio_buffer_t **phead) 
{
    audio_buffer_t *ab = *phead;
    
    if (ab) {
        *phead = ab->next;  // Update head to next buffer
        ab->next = NULL;    // Disconnect removed buffer
    }
    
    return ab;
}

/**
 * @brief Remove head buffer from a list with tail tracking
 * 
 * Similar to list_remove_head(), but also maintains a tail pointer
 * for efficient append operations. When the last buffer is removed,
 * the tail pointer is also set to NULL.
 * 
 * @param phead Pointer to the list head pointer
 * @param ptail Pointer to the list tail pointer
 * @return Removed buffer, or NULL if list was empty
 * 
 * @note Tail pointer consistency is validated in debug builds
 */
inline static audio_buffer_t *list_remove_head_with_tail(audio_buffer_t **phead,
                                                        audio_buffer_t **ptail) 
{
    audio_buffer_t *ab = *phead;
    
    if (ab) {
        *phead = ab->next;  // Update head to next buffer
        
        if (!ab->next) {
            // Removing the last buffer - update tail pointer
            audio_assert(*ptail == ab);  // Verify tail consistency
            *ptail = NULL;
        } else {
            ab->next = NULL;  // Disconnect removed buffer
        }
    }
    
    return ab;
}

/**
 * @brief Add a buffer to the beginning of a linked list
 * 
 * Inserts the specified buffer at the head of the list. The buffer
 * must not already be part of any list (next pointer must be NULL).
 * 
 * @param phead Pointer to the list head pointer
 * @param ab    Buffer to prepend
 * 
 * @note Debug builds verify buffer is not already in a list
 */
inline static void list_prepend(audio_buffer_t **phead, audio_buffer_t *ab) 
{
    audio_assert(ab->next == NULL);  // Buffer must not be in a list
    audio_assert(ab != *phead);      // Buffer cannot be the current head
    
    ab->next = *phead;  // Point to current head
    *phead = ab;        // Update head to new buffer
}

/**
 * @brief Add a buffer to the end of a list with tail tracking
 * 
 * Efficiently appends a buffer to the end of a linked list using
 * a tail pointer to avoid traversing the entire list. If the list
 * is empty, both head and tail are set to the new buffer.
 * 
 * @param phead Pointer to the list head pointer
 * @param ptail Pointer to the list tail pointer  
 * @param ab    Buffer to append
 * 
 * @note This provides O(1) append operations when tail is maintained
 */
inline static void list_append_with_tail(audio_buffer_t **phead, audio_buffer_t **ptail,
                                        audio_buffer_t *ab) 
{
    audio_assert(ab->next == NULL);  // Buffer must not be in a list
    audio_assert(ab != *phead);      // Buffer cannot be current head
    audio_assert(ab != *ptail);      // Buffer cannot be current tail
    
    if (!*phead) {
        // List is empty - buffer becomes both head and tail
        audio_assert(!*ptail);  // Tail should also be NULL
        *ptail = ab;
        list_prepend(phead, ab);
    } else {
        // List not empty - append to end
        (*ptail)->next = ab;
        *ptail = ab;
    }
}

audio_buffer_t *get_free_audio_buffer(audio_buffer_pool_t *context, bool block) {
    audio_buffer_t *ab;

    do {
        uint32_t save = spin_lock_blocking(context->free_list_spin_lock);
        ab = list_remove_head(&context->free_list);
        spin_unlock(context->free_list_spin_lock, save);
        if (ab || !block) break;
        __wfe();
    } while (true);
    return ab;
}

/**
 * @brief Return a buffer to the free pool
 * 
 * Makes a previously allocated buffer available for reuse by adding it
 * back to the free list. This will wake any threads waiting for buffers.
 * 
 * @param context Buffer pool to return buffer to
 * @param ab      Buffer to return (must not be in any list)
 * 
 * @note Buffer must not be linked to other buffers (next pointer must be NULL)
 * @note This function is thread-safe and will signal waiting threads
 */
void queue_free_audio_buffer(audio_buffer_pool_t *context, audio_buffer_t *ab) 
{
    assert(!ab->next);  // Buffer must not be in a list
    
    // Atomically add buffer back to free list
    uint32_t save = spin_lock_blocking(context->free_list_spin_lock);
    list_prepend(&context->free_list, ab);
    spin_unlock(context->free_list_spin_lock, save);
    
    // Signal that a buffer is now available
    __sev();
}

/**
 * @brief Get a buffer filled with audio data
 * 
 * Retrieves a buffer that has been filled with audio data and is ready
 * for processing or output. This is typically used by audio consumers
 * to get the next buffer to play.
 * 
 * @param context Buffer pool to get buffer from
 * @param block   If true, wait for buffer availability; if false, return NULL immediately
 * @return Buffer with audio data, or NULL if none available and block=false
 * 
 * @note This function is thread-safe and uses spin locks for synchronization
 * @note Uses tail tracking for efficient O(1) removal from prepared list
 */
audio_buffer_t *get_full_audio_buffer(audio_buffer_pool_t *context, bool block) 
{
    audio_buffer_t *ab;
    
    do {
        // Atomically remove a buffer from the prepared list
        uint32_t save = spin_lock_blocking(context->prepared_list_spin_lock);
        ab = list_remove_head_with_tail(&context->prepared_list, &context->prepared_list_tail);
        spin_unlock(context->prepared_list_spin_lock, save);
        
        // Return buffer if found, or if non-blocking mode
        if (ab || !block) break;
        
        // Wait for event (buffer to become available)
        __wfe();
    } while (true);
    
    return ab;
}

void queue_full_audio_buffer(audio_buffer_pool_t *context, audio_buffer_t *ab) {
    assert(!ab->next);
    uint32_t save = spin_lock_blocking(context->prepared_list_spin_lock);
    list_append_with_tail(&context->prepared_list, &context->prepared_list_tail, ab);
    spin_unlock(context->prepared_list_spin_lock, save);
    __sev();
}

void producer_pool_give_buffer_default(audio_connection_t *connection, audio_buffer_t *buffer) {
    queue_full_audio_buffer(connection->producer_pool, buffer);
}

audio_buffer_t *producer_pool_take_buffer_default(audio_connection_t *connection, bool block) {
    return get_free_audio_buffer(connection->producer_pool, block);
}

void consumer_pool_give_buffer_default(audio_connection_t *connection, audio_buffer_t *buffer) {
    queue_free_audio_buffer(connection->consumer_pool, buffer);
}

audio_buffer_t *consumer_pool_take_buffer_default(audio_connection_t *connection, bool block) {
    return get_full_audio_buffer(connection->consumer_pool, block);
}

static audio_connection_t connection_default = {
        .producer_pool_take = producer_pool_take_buffer_default,
        .producer_pool_give = producer_pool_give_buffer_default,
        .consumer_pool_take = consumer_pool_take_buffer_default,
        .consumer_pool_give = consumer_pool_give_buffer_default,
        .producer_pool = nullptr,
        .consumer_pool = nullptr
};

audio_buffer_t *audio_new_buffer(audio_buffer_format_t *format, int buffer_sample_count) {
    audio_buffer_t *buffer = (audio_buffer_t *) calloc(1, sizeof(audio_buffer_t));
    audio_init_buffer(buffer, format, buffer_sample_count);
    return buffer;
}

void audio_init_buffer(audio_buffer_t *audio_buffer, audio_buffer_format_t *format, int buffer_sample_count) {
    audio_buffer->format = format;
    audio_buffer->buffer = pico_buffer_alloc(buffer_sample_count * format->sample_stride);
    audio_buffer->max_sample_count = buffer_sample_count;
    audio_buffer->sample_count = 0;
}

audio_buffer_pool_t *
audio_new_buffer_pool(audio_buffer_format_t *format, int buffer_count, int buffer_sample_count) {
    audio_buffer_pool_t *ac = (audio_buffer_pool_t *) calloc(1, sizeof(audio_buffer_pool_t));
    audio_buffer_t *audio_buffers = buffer_count ? (audio_buffer_t *) calloc(buffer_count,
                                                                                       sizeof(audio_buffer_t)) : 0;
    ac->format = format->format;
    for (int i = 0; i < buffer_count; i++) {
        audio_init_buffer(audio_buffers + i, format, buffer_sample_count);
        audio_buffers[i].next = i != buffer_count - 1 ? &audio_buffers[i + 1] : NULL;
    }
    // todo one per channel?
    ac->free_list_spin_lock = spin_lock_init(SPINLOCK_ID_AUDIO_FREE_LIST_LOCK);
    ac->free_list = audio_buffers;
    ac->prepared_list_spin_lock = spin_lock_init(SPINLOCK_ID_AUDIO_PREPARED_LISTS_LOCK);
    ac->prepared_list = NULL;
    ac->prepared_list_tail = NULL;
    ac->connection = &connection_default;
    return ac;
}

audio_buffer_t *audio_new_wrapping_buffer(audio_buffer_format_t *format, mem_buffer_t *buffer) {
    audio_buffer_t *audio_buffer = (audio_buffer_t *) calloc(1, sizeof(audio_buffer_t));
    if (audio_buffer) {
        audio_buffer->format = format;
        audio_buffer->buffer = buffer;
        audio_buffer->max_sample_count = buffer->size / format->sample_stride;
        audio_buffer->sample_count = 0;
        audio_buffer->next = 0;
    }
    return audio_buffer;

}

audio_buffer_pool_t *
audio_new_producer_pool(audio_buffer_format_t *format, int buffer_count, int buffer_sample_count) {
    audio_buffer_pool_t *ac = audio_new_buffer_pool(format, buffer_count, buffer_sample_count);
    ac->type = audio_buffer_pool::ac_producer;
    return ac;
}

audio_buffer_pool_t *
audio_new_consumer_pool(audio_buffer_format_t *format, int buffer_count, int buffer_sample_count) {
    audio_buffer_pool_t *ac = audio_new_buffer_pool(format, buffer_count, buffer_sample_count);
    ac->type = audio_buffer_pool::ac_consumer;
    return ac;
}

void audio_complete_connection(audio_connection_t *connection, audio_buffer_pool_t *producer_pool,
                               audio_buffer_pool_t *consumer_pool) {
    assert(producer_pool->type == audio_buffer_pool::ac_producer);
    assert(consumer_pool->type == audio_buffer_pool::ac_consumer);
    producer_pool->connection = connection;
    consumer_pool->connection = connection;
    connection->producer_pool = producer_pool;
    connection->consumer_pool = consumer_pool;
}

void give_audio_buffer(audio_buffer_pool_t *ac, audio_buffer_t *buffer) {
    buffer->user_data = 0;
    assert(ac->connection);
    if (ac->type == audio_buffer_pool::ac_producer)
        ac->connection->producer_pool_give(ac->connection, buffer);
    else
        ac->connection->consumer_pool_give(ac->connection, buffer);
}

audio_buffer_t *take_audio_buffer(audio_buffer_pool_t *ac, bool block) {
    assert(ac->connection);
    if (ac->type == audio_buffer_pool::ac_producer)
        return ac->connection->producer_pool_take(ac->connection, block);
    else
        return ac->connection->consumer_pool_take(ac->connection, block);
}

// todo rename this - this is s16 to s16
audio_buffer_t *mono_to_mono_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Mono<FmtS16>, Mono<FmtS16>>(connection, block);
}

// todo rename this - this is s16 to s16
audio_buffer_t *stereo_s16_to_stereo_s16_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Stereo<FmtS16>, Stereo<FmtS16>>(connection, block);
}

audio_buffer_t *stereo_s32_to_stereo_s32_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Stereo<FmtS32>, Stereo<FmtS32>>(connection, block);
}

// todo rename this - this is s16 to s16
audio_buffer_t *mono_to_stereo_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Stereo<FmtS16>, Mono<FmtS16>>(connection, block);
}

audio_buffer_t *mono_s8_to_mono_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Mono<FmtS16>, Mono<FmtS8>>(connection, block);
}

audio_buffer_t *mono_s8_to_stereo_consumer_take(audio_connection_t *connection, bool block) {
    return consumer_pool_take<Stereo<FmtS16>, Mono<FmtS8>>(connection, block);
}

void stereo_s16_to_stereo_s16_producer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    return producer_pool_blocking_give<Stereo<FmtS16>, Stereo<FmtS16>>(connection, buffer);
}

void stereo_s32_to_stereo_s32_producer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    return producer_pool_blocking_give<Stereo<FmtS32>, Stereo<FmtS32>>(connection, buffer);
}