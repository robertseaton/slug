#include <stdlib.h>

#include "includes.h"

void free_pieces (struct Piece* pieces, uint64_t num_pieces)
{
     free(pieces);
}

void init_piece (struct Piece p, uint64_t index)
{
     p.index = index;
     p.amount_downloaded = 0;
     p.amount_requested = 0;
     p.rarity = 0;
     p.state = NEED;
     p.rarity = 0;
}
