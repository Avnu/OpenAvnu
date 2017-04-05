/*************************************************************************************
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "intervals.h"

#ifdef _WIN32
/* Windows-specific header values */
#define random() rand()
#define srandom(s) srand(s)
#endif

#define INTERVALS_TO_ADD     1000
#define INTERVALS_TO_REPLACE 100000
#define INTERVALS_TO_SEARCH  10000

uint32_t last_high = 0;
int total = 0;

void print_node(Interval *node) {
	printf("[%d,%d] ", node->low, node->high);
	if (node->low <= last_high ||
		node->high < node->low) {
		fprintf(stderr, "\nError:  <%d,%d>\n", node->low, node->high);
		exit(1);
	}
	last_high = node->high;
	total++;
}

int main(void) {
	Interval *set = NULL, *inter, *over, *prev;
	int i, rv, count;

	srandom((unsigned int) time(NULL));

	printf("Testing duplicate values\n");
	inter = alloc_interval(1, 10);
	rv = insert_interval(&set, inter);
	if (rv != INTERVAL_SUCCESS) {
		fprintf(stderr, "Error:  Insert of [%d,%d] failed unexpectedly\n", inter->low, inter->high);
		return 1; /* Error */
	} else {
		printf("Inserted [%d,%d]\n", inter->low, inter->high);
	}
	inter = alloc_interval(1, 10);
	rv = insert_interval(&set, inter);
	if (rv != INTERVAL_OVERLAP) {
		fprintf(stderr, "Error:  Insert of [%d,%d] should have failed, but didn't\n", inter->low, inter->high);
		return 1; /* Error */
	} else {
		printf("Repeat insert of [%d,%d] failed, so the test passed\n", inter->low, inter->high);
	}

	while (set) {
		inter = remove_interval(&set, set);
		free_interval(inter);
	}

	count = INTERVALS_TO_ADD;
	printf("\nInserting %d random intervals into a set\n", count);

	for (i = 0; i < count;) {
		inter = alloc_interval(random() % 0xfffff, random() % 128 + 1);
		rv = insert_interval(&set, inter);
		if (rv == INTERVAL_OVERLAP) {
			over = search_interval(set, inter->low, inter->high - inter->low + 1);
			printf("[%d,%d] overlapped existing entry [%d,%d]\n",
				inter->low, inter->high, over->low, over->high);
			free_interval(inter);
		} else {
			printf("Inserted [%d,%d]:\n", inter->low, inter->high);
			i++;
		}
	}

	count = INTERVALS_TO_REPLACE;
	printf("\nReplacing %d random intervals\n", count);

	for (i = 0, over = NULL; i < count;) {
		if (over) {
			inter = alloc_interval(random() % 0xfffff, random() % 128 + 1);
			rv = insert_interval(&set, inter);
			if (rv == INTERVAL_SUCCESS) {
				printf("Replaced [%d,%d] with [%d,%d]\n",
					   over->low, over->high, inter->low, inter->high);
				free_interval(over);
				over = NULL;
				i++;
			} else {
				printf("Overlapping replacement interval\n");
				free_interval(inter);
			}
		} else {
			over = search_interval(set, random() % 0xfffff, random() % 128 + 1);
			if (over) over = remove_interval(&set, over);
		}
	}

	/* Test that searches always return the first match */
	for (i = 0; i < INTERVALS_TO_SEARCH; i++) {
		uint32_t search_base = random() % 0xfffff;
		uint32_t search_size = random() % 2048 + 1;
		inter = search_interval(set, search_base, search_size);
		if (inter && !interval_check_overlap(inter, search_base, search_size)) {
			fprintf(stderr, "Error:  Search compare failure\n");
			return 1; /* Error */
		}
		if (inter && (prev = prev_interval(inter)) != NULL) {
			if (prev->high >= search_base) {
				fprintf(stderr, "Error:  Search lowest item failure\n");
				return 1; /* Error */
			}
			if (interval_check_overlap(prev, search_base, search_size)) {
				fprintf(stderr, "Error:  interval_check_overlap compare failure\n");
				return 1; /* Error */
			}
		}
	}
	printf("\n" "search_interval testing passed\n");

	/* Test next_interval and search_interval */
	i = 0;
	count = INTERVALS_TO_ADD;
	inter = minimum_interval(set);
	prev = NULL;
	while (inter) {
		i++;
		if (prev && prev->high >= inter->low) {
			fprintf(stderr, "Error:  Overlapping or out-of-order interval detected\n");
			return 1; /* Error */
		}
		if (search_interval(set, inter->low, 1) != inter) {
			fprintf(stderr, "Error:  Search for interval [%d,%d] failed\n", inter->low, inter->high);
			return 1; /* Error */
		}
		prev = inter;
		inter = next_interval(inter);
	}
	if (i != count) {
		fprintf(stderr, "Error:  Found %d intervals during next_interval interation\n", i);
		return 1; /* Error */
	}
	if (prev != maximum_interval(set)) {
		fprintf(stderr, "Error:  next_interval iteration didn't end at maximum_interval\n");
		return 1; /* Error */
	}
	printf("\n" "next_interval testing passed\n");

	/* Test previous_interval and search_interval */
	i = 0;
	count = INTERVALS_TO_ADD;
	inter = maximum_interval(set);
	prev = NULL;
	while (inter) {
		i++;
		if (prev && prev->low <= inter->high) {
			fprintf(stderr, "Error:  Overlapping or out-of-order interval detected\n");
			return 1; /* Error */
		}
		if (search_interval(set, inter->high, 1) != inter) {
			fprintf(stderr, "Error:  Search for interval [%d,%d] failed\n", inter->low, inter->high);
			return 1; /* Error */
		}
		prev = inter;
		inter = prev_interval(inter);
	}
	if (i != count) {
		fprintf(stderr, "Error:  Found %d intervals during next_interval interation\n", i);
		return 1; /* Error */
	}
	if (prev != minimum_interval(set)) {
		fprintf(stderr, "Error:  prev_interval iteration didn't end at minimum_interval\n");
		return 1; /* Error */
	}
	printf("\n" "previous_interval testing passed\n");

	inter = minimum_interval(set);
	printf("\nMinimum Interval:  [%d,%d]\n", inter->low, inter->high);
	inter = maximum_interval(set);
	printf("Maximum Interval:  [%d,%d]\n", inter->low, inter->high);

	printf("\nFinal set:\n");
	traverse_interval(set, print_node);
	printf("\n\nTotal members: %d\n\n", total);

	if (total != INTERVALS_TO_ADD) {
		fprintf(stderr, "Error:  Had %d intervals, rather an the expected %d\n", total, INTERVALS_TO_ADD);
		return 1; /* Error */
	}

	while (set) {
		inter = remove_interval(&set, set);
		free_interval(inter);
	}

	fprintf(stderr, "Tests passed.\n");
	return 0;
}
