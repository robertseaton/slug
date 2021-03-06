#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "includes.h"

struct BEncode
*initBInt(int64_t i)
{
     struct BEncode *b = malloc(sizeof(struct BEncode));
     b->type = BInt;
     b->cargo.bInt = i;

     return b;
}

struct BEncode
*initBList(struct BListNode *l)
{
     struct BEncode *b = malloc(sizeof(struct BEncode));
     b->type = BList;
     b->cargo.bList = l;

     return b;
}

struct BEncode
*initBString(char *s)
{
     struct BEncode *b = malloc(sizeof(struct BEncode));
     b->type = BString;
     b->cargo.bStr = s;

     return b;
}

struct BEncode 
*initBDict(struct BDictNode *d)
{
     struct BEncode *b = malloc(sizeof(struct BEncode));
     b->type = BDict;
     b->cargo.bDict = d;
     
     return b;
}

struct BEncode
*parseBInt(char *data, int *position)
{
     int i = 0;

     assert(data[*position] == 'i');
     (*position)++;

     /* discard leading zeroes */
     while (data[*position] == '0')
          (*position)++;

     while (isdigit(data[*position]))
          i = i * 10 + (data[(*position)++] - '0');
     assert(data[*position] == 'e');
     (*position)++;

     return initBInt(i);
}

struct BEncode 
*parseBList(char *data, int *position)
{
    struct BListNode l;
    struct BListNode *pt = &l;
    
    assert(data[*position] == 'l');
    (*position)++;
    while (data[*position] != 'e') {
         pt->next = malloc(sizeof(struct BListNode));
         pt = pt->next;
         pt->cargo = parseBEncode(data, position);
         pt->next = 0;
    }
    assert(data[*position] == 'e');
    (*position)++;
        
    return initBList(l.next);
}

struct BEncode
*parseBString(char *data, int *position)
{
    int64_t l = 0;
    
    while (isdigit(data[*position])) 
         l = l * 10 + (data[(*position)++] - '0');

    assert(data[*position] == ':');
    (*position)++;

    char *s = malloc(l + 1);

    int i;
    for (i = 0; i < l; i++)
         s[i] = data[(*position)++];

    s[l] = '\0';

    return initBString(s);
}

struct BEncode
*parseBDict(char *data, int *position)
{
     struct BDictNode d;
     struct BDictNode *pt = &d;

     assert(data[*position] == 'd');
     (*position)++;
     while (data[*position] != 'e') {
          pt->next = malloc(sizeof(struct BDictNode));
          pt = pt->next;
          struct BEncode* s = parseBString(data, position);
          pt->key = s->cargo.bStr;
          pt->value = parseBEncode(data, position);
          pt->next = 0;

          free(s);
     }
     assert(data[*position] == 'e');
     (*position)++;

     return initBDict(d.next);
}

struct BEncode 
*parseBEncode(char *data, int *position)
{
     switch (data[*position]) {
     case 'd':
          return parseBDict(data, position);
     case 'l':
          return parseBList(data, position);
     case 'i':
          return parseBInt(data, position);
     default:
          return parseBString(data, position);
     }
}
