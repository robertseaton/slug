
#include "includes.h"

void init_piece (struct Piece p, uint64_t index)
{
     p.index = index;
     p.state = NEED;
     p.rarity = 0;
}
