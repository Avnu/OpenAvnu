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
#include "intervals.h"

static int check_overlap(Interval *a, Interval *b) {
	return (a->low <= b->high && b->low <= a->high);
}

Interval *alloc_interval(uint32_t start, uint32_t count) {
	Interval *i;
	i = calloc(1, sizeof (Interval));
	if (i) {
		i->low = start;
		i->high = start + count - 1;
	}
	return i;
}

void free_interval(Interval *node) {
	free(node);
}

int insert_interval(Interval **root, Interval *node) {
	Interval *current;

	if (*root == NULL) {
		*root = node;
		return INTERVAL_SUCCESS;
	}
	current = *root;
	while (1) {
		if (check_overlap(current, node)) {
			return INTERVAL_OVERLAP;
		}
		if (node->low < current->low) {
			if (current->left_child == NULL) {
				current->left_child = node;
				node->parent = current;
				break;
			} else {
				current = current->left_child;
			}
		} else {
			if (current->right_child == NULL) {
				current->right_child = node;
				node->parent = current;
				break;
			} else {
				current = current->right_child;
			}
		}
	}

	return INTERVAL_SUCCESS;
}

Interval *remove_interval(Interval **root, Interval *node) {
	Interval *snip, *child;

	/* If the node to remove does not have two children, we will snip it,
	   otherwise we will swap it with its successor and snip that one */
	if (!node->left_child || !node->right_child) {
		snip = node;
	} else {
		snip = next_interval(node);
	}

	/* If the node to snip has a child, make its parent link point to the snipped
	   node's parent */
	if (snip->left_child) {
		child = snip->left_child;
	} else {
		child = snip->right_child;
	}
	if (child) {
		child->parent = snip->parent;
	}

	/* If the snipped node has no parent, it is the root node and we use the
	   provided root pointer to point to the child. Otherwise, we find if the
	   snipped node was a left or right child and set the appropriate link in the
	   parent node */
	if (!snip->parent) {
		*root = child;
	} else if (snip == snip->parent->left_child) {
		snip->parent->left_child = child;
	} else {
		snip->parent->right_child = child;
	}

	/* Swap the contents of the node passed in to remove with the one chosen to be
	   snipped */
	if (snip != node) {
		void *old_data = node->data;
		node->low = snip->low;
		node->high = snip->high;
		node->data = snip->data;
		snip->data = old_data;
	}

	return snip;
}

Interval *minimum_interval(Interval *root) {
	Interval *current = root;

	while (1) {
		if (current && current->left_child) {
			current = current->left_child;
		} else {
			return current;
		}
	}
}

Interval *maximum_interval(Interval *root) {
	Interval *current = root;

	while (1) {
		if (current && current->right_child) {
			current = current->right_child;
		} else {
			return current;
		}
	}
}

Interval *next_interval(Interval *node) {
	Interval *parent, *child;

	if (node->right_child) {
		return minimum_interval(node->right_child);
	}

	child = node;
	parent = node->parent;
	while (parent && child == parent->right_child) {
		child = parent;
		parent = parent->parent;
	}
	return parent;
}

Interval *prev_interval(Interval *node) {
	Interval *parent, *child;

	if (node->left_child) {
		return maximum_interval(node->left_child);
	}

	child = node;
	parent = node->parent;
	while (parent && child == parent->left_child) {
		child = parent;
		parent = parent->parent;
	}
	return parent;
}

Interval *search_interval(Interval *root, uint32_t start, uint32_t count) {
	Interval *current, *test;
	Interval i;

	i.low = start;
	i.high = start + count - 1;
	current = root;

	while (current && !check_overlap(current, &i)) {
		if (current->low > i.low) {
			current = current->left_child;
		} else {
			current = current->right_child;
		}
	}

	/* Make sure we really are returning the first (lowest) matching interval. */
	if (current) {
		for (test = prev_interval(current);
			test != NULL && check_overlap(test, &i);
			test = prev_interval(test))
		{
			/* We found a lower interval that also matches the search parameters. */
			current = test;
		}
	}

	return current;
}

void traverse_interval(Interval *root, Visitor action) {
	if (root) {
		traverse_interval(root->left_child, action);
		action(root);
		traverse_interval(root->right_child, action);
	}
}
