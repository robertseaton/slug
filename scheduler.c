/*
 * scheduler.c 
 * handle scheduling of pieces of torrent to be downloaded 
 * * */
#include <stdlib.h>

#include "includes.h"

int rarest (const void* x, const void* y)
{
     const struct Piece* a = (struct Piece*)x;
     const struct Piece* b = (struct Piece*)y;

     return (a->rarity - b->rarity);
}

void __schedule (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;
     uint64_t i;
     for (i = 0; i < t->num_pieces; ++i)
          t->pieces[i].rarity = t->global_bitfield[t->pieces[i].index];
 
     qsort(t->pieces, t->num_pieces, sizeof(struct Piece), rarest);
     
     i = 0;
     while (i < t->num_pieces && (t->pieces[i].rarity == 0 || all_choked(t->peer_list, i)))
          i++;

     if (i == t->num_pieces)
          /* everyone is choking us! */
          return ;
     
     /* if we made it this far, there exists a peer which:
      * 1) has us unchoked
      * 2) has a piece we want
      * * */
     struct PeerNode* avail_peers = find_unchoked(t->peer_list, i);
     unchoke(avail_peers->cargo);
     interested(avail_peers->cargo);
     download_piece(&t->pieces[i], avail_peers->cargo);
}

void schedule (struct Torrent* t, struct event_base* base)
{
     struct event* schedule_ev;
     struct timeval schedule_interval = {SCHEDULE_INTERVAL, 0};
     
     schedule_ev = evtimer_new(base, __schedule, t);
     evtimer_add(schedule_ev, &schedule_interval);
}
