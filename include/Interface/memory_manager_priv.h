#ifndef MM_PRIV_H
#define MM_PRIV_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Requests a contiguous block of memory from the System/OS.
 * 
 * This is called by the Arenas (Nursery/General) during initial setup 
 * or when a massive expansion is required.
 * 
 * @param size The total number of bytes to request.
 * @return void* Pointer to the start of the reserved region, or NULL if 
 *               the OS denied the request.
 */
void* mm_request_region(size_t size);

/**
 * @brief Resizes an existing system memory region.
 * 
 * Used primarily when a Bump Allocator or TLSF Arena reaches its limit 
 * and needs to expand. This function handles the low-level logic of 
 * growing the region or moving it to a new location if contiguous 
 * space is unavailable.
 * 
 * @param old_ptr Current base pointer of the region.
 * @param old_size Current size of the region in bytes.
 * @param new_size Desired new size in bytes.
 * @param max_allowed_size The hard limit defined in the system config.
 * @return void* Pointer to the (potentially moved) region, or NULL on failure.
 */
void* mm_resize_region(void* old_ptr, uint32_t old_size, uint32_t new_size, uint32_t max_allowed_size);

#endif /* MM_PRIV_H */