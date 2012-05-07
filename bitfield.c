#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "includes.h"

int
getbit(uint8_t *bitfield, uint32_t bit)
{
        uint32_t byte;

        /* which byte is this bit in (divide by 8) */
        byte = bit >> 3u;

        return ((bitfield[byte] & (1u << (7u - (bit & 7u)))) != 0);
}


/* Parses a bitfield message into a bitfield data structure. */
uint8_t
*init_bitfield(uint64_t num_pieces, uint8_t *message)
{
     uint8_t *bitfield;
     assert(num_pieces != 0);
     bitfield = malloc(num_pieces);

     uint32_t i, j;
     for (i = 0; i < num_pieces; i++) {
          j = i / 8 + 1;
          if (getbit(&bitfield[1], i))
               bitfield[i] = 1;
          else
               bitfield[i] = 0;
     }

     return bitfield;
}

/* Allocates and initializes a global bitfield. */
uint64_t
*init_global_bitfield(uint64_t num_pieces)
{
     assert(num_pieces != 0);
     uint64_t *bitfield = malloc(sizeof(uint64_t) * num_pieces);

     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          bitfield[i] = 0;

     return bitfield;
}

/* Allocates and initializes a bitfield of the pieces we have. */
char
*init_have_bitfield(uint64_t num_pieces)
{
     assert(num_pieces != 0);
     char *bitfield = malloc(num_pieces);
     
     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          bitfield[i] = 0;

     return bitfield;
}

/* Updates a bitfield with new information provided by a have message. */
void 
update_bitfield(uint8_t *message, uint64_t *global_bitfield, uint8_t *bitfield)
{
     uint32_t have;
     
     /* they haven't sent us a bitfield! */
     if (bitfield == NULL) 
          return ;

     memcpy(&have, &message[1], sizeof(uint32_t));
     have = ntohl(have);
     bitfield[have] = 1;
     global_bitfield[have]++;
}

/* Updates a global bitfield with information from a peer's bitfield. */
void 
update_global_bitfield(uint64_t num_pieces, char *bitfield, uint64_t *global_bitfield)
{
     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          if (bitfield[i] == 1)
               global_bitfield[i]++;
}
