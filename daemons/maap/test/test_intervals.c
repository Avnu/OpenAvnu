#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "intervals.h"

int last_high = 0;
int total = 0;

void print_node(Interval *node) {
  fprintf(stdout, "[%d,%d] ", node->low, node->high);
  if (node->low <= last_high ||
      node->high < node->low) {
    fprintf(stderr, "\nERROR: <%d,%d>\n", node->low, node->high);
  }
  last_high = node->high;
  total++;
}

int main(void) {
  Interval *set = NULL, *inter, *over;
  int i, rv, count;

  time((time_t *)&i);
  srandom(i);

  printf("Testing duplicate values\n");
  inter = alloc_interval(1, 10);
  rv = insert_interval(&set, inter);
  if (rv != INTERVAL_SUCCESS) {
    printf("Insert of [%d,%d] failed unexpectedly\n", inter->low, inter->high);
  } else {
    printf("Inserted [%d,%d]\n", inter->low, inter->high);
  }
  inter = alloc_interval(1, 10);
  rv = insert_interval(&set, inter);
  if (rv != INTERVAL_OVERLAP) {
    printf("Insert of [%d,%d] should have failed, but didn't\n", inter->low, inter->high);
  } else {
    printf("Repeat insert of [%d,%d] failed, so the test passed\n", inter->low, inter->high);
  }

  while (set) {
    inter = remove_interval(&set, set);
    free_interval(inter);
  }

  count = 1000;
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

  count = 100000;
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

  inter = minimum_interval(set);
  printf("\nMinimum Interval:  [%d,%d]\n", inter->low, inter->high);
  inter = maximum_interval(set);
  printf("Maximum Interval:  [%d,%d]\n", inter->low, inter->high);

  printf("\nFinal set:\n");
  traverse_interval(set, print_node);
  printf("\n");
  fprintf(stderr, "\nTotal members: %d\n", total);

  while (set) {
    inter = remove_interval(&set, set);
    free_interval(inter);
  }

  return 0;
}
