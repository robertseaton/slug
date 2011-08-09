#include <stdlib.h>
#include <string.h>

#include "includes.h"

/* prints a string to stderr then exits */
void error (char* s)
{
     fprintf(stderr, "%s\n", s);
     exit(1);
}

/* takes a key and a BEncoded dictionary node and searches for
 * the corresponding value
 * * */
struct BEncode* find_value (char* key, struct BDictNode* d)
{
     struct BDictNode* pt = d;
     struct BDictNode* next = d;
     
     while (next) {
          next = pt->next;
          if (strncmp(pt->key, key, strlen(key)) == 0)
               return pt->value;
          pt = next;
     }
     return NULL;
}

void freeBList (struct BListNode* l)
{
     struct BListNode* pt = l;
     struct BListNode* next = l;

     while (next) {
          next = pt->next;
          freeBEncode(pt->cargo);
          free(pt);
          pt = next;
     }
}

void freeBDict (struct BDictNode* d)
{
     struct BDictNode* pt = d;
     struct BDictNode* next = d;

     while (next) {
          next = pt->next;
          free(pt->key);
          freeBEncode(pt->value);
          free(pt);
          pt = next;
     }
     return;
}
void freeBEncode (struct BEncode* b)
{
     if (b->type == BInt) {
          free(b);

          return;
     }

     if (b->type == BString) {
          free(b->cargo.bStr);
          free(b);

          return;
     }

     if (b->type == BList) {
          freeBList(b->cargo.bList);
          free(b);

          return;
     }

     if (b->type == BDict) {
          freeBDict(b->cargo.bDict);
          free(b);

          return;
     }
}
