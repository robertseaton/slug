#include <ctype.h>
#include <stdlib.h>

#include "includes.h"

struct BEncode* initBInt (int64_t i)
{
     struct BEncode* b = malloc(sizeof(struct BEncode));
     b->type = BInt;
     b->cargo.bInt = i;

     return b;
}

struct BEncode* initBList (struct BListNode* l)
{
     struct BEncode* b = malloc(sizeof(struct BEncode));
     b->type = BList;
     b->cargo.bList = l;

     return b;
}

struct BEncode* initBString (char* s)
{
     struct BEncode* b = malloc(sizeof(struct BEncode));
     b->type = BString;
     b->cargo.bStr = s;

     return b;
}

struct BEncode* initBDict (struct BDictNode* d)
{
     struct BEncode* b = malloc(sizeof(struct BEncode));
     b->type = BDict;
     b->cargo.bDict = d;
     
     return b;
}

struct BEncode* parseBInt (char* data, int64_t* position)
{
     int64_t i = 0;

     if (data[(*position)++] != 'i')
          error("Failed to parse BInt.");

     while (isdigit(data[*position]))
          i = i * 10 + (data[(*position)++] - '0');

     if (data[(*position)++] != 'e')
          error("Failed to parse BEncoded int.");

     return initBInt(i);
}

struct BEncode* parseBList (char* data, int64_t* position)
{
    struct BListNode l;
    struct BListNode* pt = &l;
    
    if (data[(*position)++] != 'l')
         error("Failed to parse a BEncoded list.");
        
    while (data[*position] != 'e') {
         pt->next = malloc(sizeof(struct BListNode));
         pt = pt->next;
         pt->cargo = parseBEncode(data, position);
         pt->next = 0;
    }
    
    if (data[(*position)++] != 'e')
         error("Failed to parse a BEncoded list.");
        
    return initBList(l.next);
}

struct BEncode* parseBString (char* data, int64_t* position)
{
    int64_t l = 0;
    
    while (isdigit(data[*position])) 
         l = l * 10 + (data[(*position)++] - '0');

    if (data[(*position)++] != ':')
         error("Failed to parse a BEncoded string.");

    char* s = malloc(l + 1);

    int i;
    for (i = 0; i < l; i++)
         s[i] = data[(*position)++];

    s[l] = 0;

    return initBString(s);
}

struct BEncode* parseBDict (char* data, int64_t* position)
{
     struct BDictNode d;
     struct BDictNode* pt = &d;

     if (data[(*position)++] != 'd')
          error("Failed to parse a BEncoded dictionary.");

     while (data[*position] != 'e') {
          pt->next = malloc(sizeof(struct BDictNode));
          pt = pt->next;
          struct BEncode* s = parseBString(data, position);
          pt->key = s->cargo.bStr;
          pt->value = parseBEncode(data, position);
          pt->next = 0;

          free(s);
     }

     if (data[(*position)++] != 'e')
          error("Failed to parse a BEncoded dictionary.");

     return initBDict(d.next);
}

struct BEncode* parseBEncode (char* data, int64_t* position)
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
