/*
 * heap.c
 * implements a min binary heap for use as a priority queue 
 * (t->pieces, t->downloading)
 * * */
#include <stdlib.h>
#include <string.h>

#include "includes.h"

#define LEFT(x)   (2 * x)
#define RIGHT(x)  ((2 * x) + 1)
#define PARENT(x) (x / 2)

void 
swap(struct Piece x, struct Piece y)
{
     struct Piece tmp;

     tmp = x;
     x = y;
     y = tmp;
}

int8_t 
compare_priority(struct Piece x, struct Piece y)
{
     if (x.priority < y.priority)
          return 1;
     else
          return 0;
}

int8_t
compare_age(struct Piece x, struct Piece y)
{
     if (x.started < y.started)
          return 1;
     else
          return 0;
}

/* pq = priority queue */
void 
heapify(struct MinBinaryHeap* pq, uint64_t i, int8_t (*compare)(struct Piece, struct Piece))
{
     uint64_t minimum;
     uint64_t l = LEFT(i);
     uint64_t r = RIGHT(i);
     
     /* check left child node */
     if (compare(pq->elements[l], pq->elements[i]))
          minimum = l;
     else
          minimum = i;

     /* check right child node */
     if (compare(pq->elements[r], pq->elements[minimum]))
          minimum = r;

     if (minimum != i) {
          swap(pq->elements[i], pq->elements[minimum]);
          heapify(pq, minimum, compare);
     }     
}

struct Piece
*heap_min(struct MinBinaryHeap *pq)
{
     if (pq->heap_size > 0)
          return &(pq->elements[1]);
     else
          return NULL;
}

struct Piece
*heap_extract_min(struct MinBinaryHeap *pq, int8_t (*compare)(struct Piece, struct Piece))
{
     struct Piece *min = malloc(sizeof(struct Piece));
     
     if (pq->heap_size >= 1) {
          memcpy(min, &(pq->elements[1]), sizeof(struct Piece));
          pq->elements[1] = pq->elements[(pq->heap_size)--];
          heapify(pq, 1, compare);
     } else {
          free(min);
          min = NULL;
     }

     return min;
}

int8_t 
heap_insert(struct MinBinaryHeap *pq, struct Piece key, int8_t (*compare)(struct Piece, struct Piece))
{
     uint64_t i;
     
     if (pq->heap_size >= pq->max_elements)
          return -1;

     i = ++(pq->heap_size);
     while (i > 1 && compare(key, pq->elements[PARENT(i)])) {
          pq->elements[i] = pq->elements[PARENT(i)];
          i = PARENT(i);
     }

     pq->elements[i] = key;

     return 0;
}

int8_t 
heap_delete(struct MinBinaryHeap *pq, uint64_t i, int8_t (*compare)(struct Piece, struct Piece))
{
     if (i < pq->heap_size || i < 1)
          return -1;

     pq->elements[i] = pq->elements[(pq->heap_size)--];
     heapify(pq, i, compare);

     return 0;
}

void 
heap_initialize(struct MinBinaryHeap *pq, uint64_t i)
{
     pq->heap_size = 0;
     pq->max_elements = i;
     pq->elements = malloc(sizeof(struct Piece) * ((pq->max_elements) + 1));
}

struct Piece
*find_by_index(struct MinBinaryHeap *pq, uint64_t index)
{
     uint64_t i;
     for (i = 1; i < pq->heap_size + 1; i++)
          if (pq->elements[i].index == index)
               return &(pq->elements[i]);

     /* invalid index */
     return NULL;
}

struct Piece 
*extract_by_index (struct MinBinaryHeap *pq, uint64_t index, int8_t (*compare)(struct Piece, struct Piece))
{
     struct Piece *out = malloc(sizeof(struct Piece));

     uint64_t i;
     for (i = 1; i < pq->heap_size + 1; i++)
          if (pq->elements[i].index == index) {
               memcpy(out, &(pq->elements[i]), sizeof(struct Piece));
               pq->elements[i] = pq->elements[(pq->heap_size)--];
               heapify(pq, i, compare);

               return out;
          }

     /* invalid index */
     free(out);
     return NULL;
}
