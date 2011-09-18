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

/* check if a peer is already in our queue */
int8_t in_queue (struct Torrent* t, struct Peer* p)
{
     int i;
     for (i = 0; i < QUEUE_SIZE; i++) {
          if (t->download_queue[i] != NULL && t->download_queue[i]->peer->addr.sin_addr.s_addr == p->addr.sin_addr.s_addr)
               return 1;
     }
     
     return 0;
}

struct QueueObject* get_rarest_unchoked (struct Torrent* t)
{
     struct QueueObject* qo = malloc(sizeof(struct QueueObject));
     struct PeerNode* pn = t->peer_list;

     

     int i;
     for (i = 0; i < t->num_pieces; i++) {
          while (pn != NULL && t->pieces[0].state == Need) {
               if (!t->have_bitfield[t->pieces[0].index] && 
                   has_piece(&t->pieces[0], pn->cargo) && 
                   !pn->cargo->tstate.peer_choking &&
                   !in_queue(t, pn->cargo)) {
                    t->pieces[0].state = Queued;
                    qo->peer = pn->cargo;
                    qo->piece = &t->pieces[0];

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
     int i;
     for (i = 0; i < QUEUE_SIZE; i++)
          if (t->download_queue[i] == NULL || t->download_queue[i]->piece->state == Have)
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
     for (i = 0; i < QUEUE_SIZE; i++)
          if (t->download_queue[i] != NULL && t->download_queue[i]->piece->state == Queued)
               download_piece(t->download_queue[i]->piece, t->download_queue[i]->peer);
}

void __interest (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;

     struct PeerNode* pn = t->peer_list;
     
     while (pn != NULL) {
          if (has_needed_piece(pn->cargo->bitfield, t->have_bitfield, t->num_pieces) 
              && !pn->cargo->tstate.interested)
               interested(pn->cargo);
          else if (!has_needed_piece(pn->cargo->bitfield, t->have_bitfield, t->num_pieces) 
                   && pn->cargo->tstate.interested)
               not_interested(pn->cargo);

          pn = pn->next;
     }
}

void schedule (struct Torrent* t, struct event_base* base)
{
     struct event* schedule_ev;
     struct timeval schedule_interval = {SCHEDULE_INTERVAL, 0};
     
     schedule_ev = event_new(base, -1, EV_PERSIST, __schedule, t);
     evtimer_add(schedule_ev, &schedule_interval);
}

void update_interest (struct Torrent* t, struct event_base* base)
{
     struct event* interest_ev;
     struct timeval interest_interval = {INTEREST_INTERVAL, 0};

     interest_ev = event_new(base, -1, EV_PERSIST, __interest, t);
     evtimer_add(interest_ev, &interest_interval);
}
