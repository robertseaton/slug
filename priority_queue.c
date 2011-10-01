#include <stdlib.h>
#include "includes.h"

#define LEFT(x) (2 * x)
#define RIGHT(x) ((2 * x) + 1)
#define PARENT(x) (x / 2)

void 
swap (struct QueueObject x, struct QueueObject y)
{
     struct QueueObject tmp;

     tmp = x;
     x = y;
     y = tmp;
}

int8_t 
compare_priority (struct QueueObject x, struct QueueObject y)
{
     if (x.priority < y.priority)
          return 1;
     else
          return 0;
}

void 
heapify (struct MinBinaryHeap* pq, uint64_t i)
{
     uint64_t minimum;
     uint64_t l = LEFT(i);
     uint64_t r = RIGHT(i);
     
     /* check left child node */
     if (compare_priority(pq->elements[l], pq->elements[i]))
          minimum = l;
     else
          minimum = i;

     /* check right child node */
     if (compare_priority(pq->elements[r], pq->elements[minimum]))
          minimum = r;

     if (minimum != i) {
          swap(pq->elements[i], pq->elements[minimum]);
          heapify(pq, minimum);
     }     
}

struct QueueObject* 
heap_min (struct MinBinaryHeap* pq)
{
     return &(pq->elements[1]);
}

struct QueueObject 
heap_extract_min (struct MinBinaryHeap* pq)
{
     struct QueueObject min;
     
     if (pq->heap_size >= 1) {
          min = pq->elements[1];
          pq->elements[1] = pq->elements[(pq->heap_size)--];
          heapify(pq, 1);
     }

     return min;
}

int8_t 
heap_insert (struct MinBinaryHeap* pq, struct QueueObject key)
{
     uint64_t i;
     
     if (pq->heap_size >= pq->max_elements)
          return -1;

     i = ++(pq->heap_size);
     while (i > 1 && compare_priority(key, pq->elements[PARENT(i)])) {
          pq->elements[i] = pq->elements[PARENT(i)];
          i = PARENT(i);
     }

     pq->elements[i] = key;
     return 0;
}

int8_t 
heap_delete (struct MinBinaryHeap* pq, uint64_t i)
{
     struct QueueObject deleted;

     if (i < pq->heap_size || i < 1)
          return -1;

     deleted = pq->elements[i];
     pq->elements[i] = pq->elements[(pq->heap_size)--];

     heapify(pq, i);
     return 0;
}

void 
heap_initialize (struct MinBinaryHeap* pq, uint64_t i)
{
     pq->heap_size = 0;
     pq->max_elements = i;
     pq->elements = malloc(sizeof(struct QueueObject) * ((pq->max_elements) + 1));
}

struct QueueObject 
qo_create (struct Peer* peer, struct Piece* piece, uint64_t priority)
{
     struct QueueObject qo;

     qo.peer = peer;
     qo.piece = piece;
     qo.priority = priority;

     return qo;
}
