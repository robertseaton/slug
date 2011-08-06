#include <stdlib.h>
#include <string.h>

#include "includes.h"

char* init_bitfield (uint64_t num_pieces, char* message, uint64_t* global_bitfield)
{

     if (num_pieces == 0)
          error("Failed to initialize peer's bitfield.");

     char* bitfield;
     bitfield = malloc(num_pieces);

     uint32_t i, j;
     for (i = 0; i < num_pieces; i++) {
          j = i / 8 + 1;
          bitfield[i] = message[j] & (1 << (i % 8));
     }

     return bitfield;
}

uint64_t* init_global_bitfield (uint64_t num_pieces)
{

     if (num_pieces == 0)
          error("Failed to initialize global bitfield.");

     uint64_t* bitfield = malloc(sizeof(uint64_t) * num_pieces);

     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          bitfield[i] = 0;

     return bitfield;
}

char* init_have_bitfield (uint64_t num_pieces)
{

     if (num_pieces == 0)
          error("Failed to initalize have bitfield.");
     char* bitfield = malloc(num_pieces);
     
     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          bitfield[i] = 0;

     return bitfield;
}

void update_bitfield (uint64_t num_pieces, char* message, uint64_t* global_bitfield, char* bitfield)
{
     uint32_t have;
     memcpy(&have, message[1], sizeof(uint32_t));
     have = ntohl(have);
     bitfield[have] = 1;
     global_bitfield[have]++;
}

void update_global_bitfield (uint64_t num_pieces, char* bitfield, uint64_t* global_bitfield)
{
     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          if (peer->bitfield[i] == 1)
               global_bitfield[i]++;
}
