#ifndef INTERVALS_H
#define INTERVALS_H

#include <stdint.h>

/**
 * Augmented Binary Search Tree for Intervals
 *
 * This library will keep track of non-overlapping intervals in the uint32 range
 *
 * It supports insert, remove, minimum, maximum, next, previous, search, and
 * traverse operations. All updates occur in-place.
 *
 * All memory allocation must be handled by the user through the alloc_interval
 * and free_interval functions. These are not called by any library functions.
 *
 * To create an empty set of intervals, simply store an Interval pointer. The
 * address of this pointer will be passed to the insert and remove operations,
 * which will update the pointer if the root node of the tree changes.
 */

/* Return values for insert_interval */

/**
 * The interval was inserted
 */
#define INTERVAL_SUCCESS  0

/**
 * The interval overlapped and wasn't inserted
 */
#define INTERVAL_OVERLAP  -1

/**
 * A range of integers with an upper and lower bound.
 */
typedef struct interval_node Interval;

/**
 * Signature for function pointer parameter to traverse_interval()
 */
typedef void (*Visitor)(Interval *);

struct interval_node {
  uint32_t low;
  uint32_t high;
  uint32_t max;
  void *data;
  Interval *parent;
  Interval *left_child;
  Interval *right_child;
};

/**
 * Allocates, initializes, and returns a pointer to an Interval.
 *
 * @param start The first integer in the interval
 *
 * @param count The number of integers in the interval
 *
 * @return An initialized Interval, or NULL if the allocator fails.
 */
Interval *alloc_interval(uint32_t start, uint32_t count);

/**
 * Deallocates an Interval.
 *
 * @note Be sure to call this after removing an interval from a set
 *
 * @param node The Interval to deallocate.
 */
void free_interval(Interval *node);

/**
 * Insert an Interval into the set of tracked Intervals.
 *
 * @param root The address of the Interval pointer that is the root of the set
 *
 * @param node The Interval to attempt to insert in the set
 *
 * @return INTERVAL_SUCCESS if the node inserted to the set without overlap,
 * INTERVAL_OVERLAP if the node was not inserted due to overlap with an existing
 * Interval.
 */
int insert_interval(Interval **root, Interval *node);

/**
 * Remove an Interval from the set of tracked Intervals.
 *
 * @WARNING: Because the actual node removed from the tree might not be the same
 * as the one passed in, the return value MUST be stored and used to free the
 * Interval that was removed from the set.
 *
 * @param root The address of the pointer to the root of the set of Intervals
 *
 * @param node The address of the Interval to remove from the set
 *
 * @return The address of the Interval storage that should be freed
 */
Interval *remove_interval(Interval **root, Interval *node);

/**
 * Find the minimum Interval from a set of Intervals.
 *
 * @param root The address of the root of the set of Intervals
 *
 * @return The address of the Interval with the smallest 'low' value in the set,
 * or NULL if the set is empty.
 */
Interval *minimum_interval(Interval *root);

/**
 * Find the maximum Interval from a set of Intervals.
 *
 * @param root The address of the root of the set of Intervals
 *
 * @return The address of the Interval with the largest 'low' value in the set,
 * or NULL if the set is empty.
 */
Interval *maximum_interval(Interval *root);

/**
 * Get the next Interval in the set assuming an ordering based on the 'low' value.
 *
 * @param node The Interval to find the successor of.
 *
 * @return The successor of the passed-in Interval, or NULL if there is none.
 */
Interval *next_interval(Interval *node);

/**
 * Get the previous Interval in the set assuming an ordering based on the 'low'
 * value.
 *
 * @param node The Interval to find the predecessor of.
 *
 * @return The predecessor of the passed-in Interval, or NULL if there is none.
 */
Interval *prev_interval(Interval *node);

/**
 * Find the first Interval in a set that overlaps with the given interval.
 *
 * @param root The root Interval of the set to search.
 *
 * @param start The first integer in the interval to search for.
 *
 * @param count The number of integers in the interval to search for.
 *
 * @return The first overlapping Interval, or NULL if there is none.
 */
Interval *search_interval(Interval *root, uint32_t start, uint32_t count);

/**
 * Traverse the Interval set, performing an action on each Interval.
 *
 * @note Intervals will be visited in increasing order of their 'low' values.
 *
 * @param root The root Interval of the set to traverse.
 *
 * @param action The action to invoke on each Interval.
 */
void traverse_interval(Interval *root, Visitor action);

#endif
