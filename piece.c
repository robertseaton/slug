#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "includes.h"

void 
init_piece (struct Piece* p, uint64_t index)
{
     p->index = index;
     p->amount_downloaded = 0;
     p->amount_requested = 0;
     p->priority = 0;
     p->state = Need;
     p->priority = 0;
}

void 
download_piece (struct Piece* piece, struct Peer* peer)
{
     uint64_t offset = 0;
     piece->amount_requested = 0;
     piece->state = Downloading;
     piece->started = time(NULL);
     piece->downloading_from = peer;
     peer->pieces_requested++;
     heap_insert(&peer->torrent->downloading, *piece, &compare_age);

     while (peer->torrent->piece_length - piece->amount_requested > 0) {
          request(peer, piece, offset);
          offset += REQUEST_LENGTH;
          piece->amount_requested += REQUEST_LENGTH;
     }
}

uint8_t 
verify_piece (void* addr, uint64_t piece_length, uint8_t* piece_sha)
{
     SHA_CTX sha;
     uint8_t sha1result[20];

     SHA1_Init(&sha);
     SHA1_Update(&sha, addr, piece_length);
     SHA1_Final(sha1result, &sha);

     if (memcmp(sha1result, piece_sha, 20)) {
          printf("Expected: ");
          print_sha1(piece_sha);
          printf("Received: ");
          print_sha1(sha1result);
          return 0;
     } else
          return 1;
}

uint8_t 
has_piece (struct Piece* piece, struct Peer* peer)
{
     if (peer->bitfield == NULL)
          return 0;
     
     return peer->bitfield[piece->index];
}

uint64_t 
pieces_remaining (char* have_bitfield, uint64_t num_pieces)
{
     uint64_t num_remain = 0;

     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          if (have_bitfield[i] == 0)
               num_remain++;

     return num_remain;
}

void
print_pieces_remaining (char* have_bitfield, uint64_t num_pieces)
{
     printf("Pieces remaining: ");
     
     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          if (have_bitfield[i] == 0)
               printf("%lu, ", i);
     printf("\n");
}
