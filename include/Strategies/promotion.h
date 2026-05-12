#ifndef PROMOTION_H
#define PROMOTION_H

#include "Infrastructure/handle.h"

/**
 * @brief Promotes a handle from the Nursery to the General Arena.
 * 
 * This function performs the "heavy lifting" of the generational transition:
 * - Calculates the required size from the HandleEntry.
 * - Requests a new allocation from the General Arena (TLSF).
 * - Performs a memory copy (memcpy) of the payload.
 * - Updates the HandleEntry's 'data_offset' and 'strategy' pointer.
 * 
 * @param handle The handle to be promoted.
 * @return int 1 if the promotion was successful, 
 *             0 if the General Arena is full (promotion failed).
 */
int nursery_promotion(Handle handle);

#endif /* PROMOTION_H */