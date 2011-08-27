#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>

#include "includes.h"

void free_pieces (struct Piece* pieces, uint64_t num_pieces)
{
     free(pieces);
}

void init_piece (struct Piece* p, uint64_t index)
{
     p->index = index;
     p->amount_downloaded = 0;
     p->amount_requested = 0;
     p->rarity = 0;
     p->state = Need;
     p->rarity = 0;
}

void download_piece (struct Piece* piece, struct Peer* peer)
{
     off_t offset = 0;
     piece->amount_requested = 0;
     piece->state = Downloading;

     while (peer->torrent->piece_length - piece->amount_requested > 0) {
          request(peer, piece, offset);
          offset += REQUEST_LENGTH;
          piece->amount_requested += REQUEST_LENGTH;
     }
}

int verify_piece (void* addr, uint64_t piece_length, char* piece_sha)
{
     SHA_CTX sha;
     unsigned char sha1result[20];
     
     SHA1_Init(&sha);
     SHA1_Update(&sha, addr, piece_length);
     SHA1_Final(sha1result, &sha);

     if (strncmp(sha1result, piece_sha, 20))
          return 0;
     else
          return 1;
}
