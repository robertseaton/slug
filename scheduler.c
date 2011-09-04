/*
 * scheduler.c 
 * handle scheduling of pieces of torrent to be downloaded 
 * * */
#include <stdlib.h>

#include "includes.h"

int rarest (const void* x, const void* y)
{
     const struct Piece* a = (struct Piece *)x;
     const struct Piece* b = (struct Piece *)y;

     return (a->rarity - b->rarity);
}

struct QueueObject* get_rarest_unchoked (struct Torrent* t)
{
     struct QueueObject* qo = malloc(sizeof(struct QueueObject));
     struct PeerNode* pn = t->peer_list;

     

     int i;
     for (i = 0; i < t->num_pieces; i++) {
          while (pn != NULL && t->pieces[i].state == Need) {
               if (!t->have_bitfield[t->pieces[i].index] && 
                   has_piece(&t->pieces[i], pn->cargo) && 
                   !pn->cargo->tstate.peer_choking) {
                    t->pieces[i].state = Queued;
                    qo->peer = pn->cargo;
                    qo->piece = &t->pieces[i];

                    return qo;
               } else
                    pn = pn->next;
          }
          pn = t->peer_list;
     }

     /* if we reach this far, we found no unchoked torrents */

     return NULL;
}

void build_queue (struct Torrent* t)
{
     int i = 0;
     while (t->download_queue[i] != NULL && i < QUEUE_SIZE)
          i++;

     for ( ; i < QUEUE_SIZE; i++)
          t->download_queue[i] = get_rarest_unchoked(t);
}

void __schedule (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;
     uint64_t i;
     for (i = 0; i < t->num_pieces; ++i)
          t->pieces[i].rarity = t->global_bitfield[t->pieces[i].index];
 
     qsort(t->pieces, t->num_pieces, sizeof(struct Piece), rarest);
     
     build_queue(t);
}

void schedule (struct Torrent* t, struct event_base* base)
{
     struct event* schedule_ev;
     struct timeval schedule_interval = {SCHEDULE_INTERVAL, 0};
     
     schedule_ev = evtimer_new(base, __schedule, t);
     evtimer_add(schedule_ev, &schedule_interval);
}
