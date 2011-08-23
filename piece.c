#include <stdlib.h>

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
     p->state = NEED;
     p->rarity = 0;
}

void download_piece (struct Piece* piece, struct Peer* peer)
{
     off_t offset = 0;
     piece->amount_requested = 0;

     while (peer->torrent->piece_length - piece->amount_requested > 0) {
          request(peer, piece, offset);
          offset += REQUEST_LENGTH;
          piece->amount_requested += REQUEST_LENGTH;
     }
}
