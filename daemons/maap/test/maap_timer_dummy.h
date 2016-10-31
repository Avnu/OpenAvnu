/**
 * @file
 *
 * @brief Support for timer operations unit tests
 *
 * These functions support additional functions used by unit tests for timing operations.
 */

#ifndef MAAP_TIMER_DUMMY_H
#define MAAP_TIMER_DUMMY_H

#include <stdint.h>


/**
 * Advance the internal time by the number of nanoseconds.
 *
 * @param nsec Number of nanoseconds to advance the internal time
 */
void Time_increaseNanos(uint64_t nsec);

#endif
