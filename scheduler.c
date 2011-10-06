/*
 * scheduler.c 
 * handle scheduling of pieces of torrent to be downloaded 
 * * */
#include <arpa/inet.h>
#include <stdlib.h>

#include "includes.h"

void
queue_pieces (struct Torrent* t, struct Peer* peer)
{
     while (peer->pieces_requested <= PEER_PIPELINE_LEN) {
          struct Piece* piece = heap_extract_min(&t->pieces, &compare_priority);
               if (piece != NULL && peer->bitfield[piece->index])
                    download_piece(piece, peer);
               else if (piece != NULL) {
                    heap_insert(&t->pieces, *piece, &compare_priority);
                    insert_tail(t->peer_list, peer);
               } else
                    break;
     }
}

void 
__schedule (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;

     /* FIXME: some kind of pipelining heuristic needs to be 
      * implemented in order to saturate high bandwidth 
      * connections
      * * */
     while (t->unchoked_peers != NULL) {
          struct Peer* peer = extract_element(&t->unchoked_peers, t->unchoked_peers->cargo);
          queue_pieces(t, peer);
     }
}

void 
schedule (struct Torrent* t, struct event_base* base)
{
     struct event* schedule_ev;
     struct timeval schedule_interval = {SCHEDULE_INTERVAL, 0};
     
     schedule_ev = event_new(base, -1, EV_PERSIST, __schedule, t);
     evtimer_add(schedule_ev, &schedule_interval);
}

/* possible we requested a piece and haven't received any data
 * for a long time, in which case it needs to be re-added to
 * the priority queue.
 * * */
void
__timeout (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;
     struct Piece* p = heap_min(&t->downloading);

     while (p != NULL && time(NULL) - p->started > TIMEOUT) {
          p = heap_extract_min(&t->downloading, &compare_age);
#ifdef DEBUG
          printf("TIMEOUT: Piece #%lu from %s.\n", 
                 p->index, 
                 inet_ntoa(p->downloading_from->addr.sin_addr));
#endif
          p->downloading_from->state = Dead;
          p->amount_downloaded = 0;
          extract_element(&t->active_peers, p->downloading_from);
          heap_insert(&t->pieces, *p, &compare_priority);
          p = heap_min(&t->downloading);
     }
}

void
timeout (struct Torrent* t, struct event_base* base)
{
     struct event* timeout_ev;
     struct timeval timeout_interval = {TIMEOUT_INTERVAL, 0};
     
     timeout_ev = event_new(base, -1, EV_PERSIST, __timeout, t);
     evtimer_add(timeout_ev, &timeout_interval);
}

void 
__interest (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;

     struct PeerNode* peer_list = t->peer_list;
     struct PeerNode* unchoked_peers = t->unchoked_peers;

     while (peer_list != NULL) {
          if (has_needed_piece(peer_list->cargo->bitfield, 
                               t->have_bitfield, 
                               t->num_pieces) && 
              !peer_list->cargo->tstate.interested)
               interested(peer_list->cargo);
          else if (!has_needed_piece(peer_list->cargo->bitfield, 
                                     t->have_bitfield, 
                                     t->num_pieces) &&
                   peer_list->cargo->tstate.interested)
               not_interested(peer_list->cargo);

          peer_list = peer_list->next;
     }

     while (unchoked_peers != NULL) {
          if (has_needed_piece(unchoked_peers->cargo->bitfield, 
                               t->have_bitfield, 
                               t->num_pieces) && 
              !unchoked_peers->cargo->tstate.interested)
               interested(unchoked_peers->cargo);
          else if (!has_needed_piece(unchoked_peers->cargo->bitfield, 
                                     t->have_bitfield, 
                                     t->num_pieces) &&
                   unchoked_peers->cargo->tstate.interested)
               not_interested(unchoked_peers->cargo);

          unchoked_peers = unchoked_peers->next;
     }
}

void 
update_interest (struct Torrent* t, struct event_base* base)
{
     struct event* interest_ev;
     struct timeval interest_interval = {INTEREST_INTERVAL, 0};

     interest_ev = event_new(base, -1, EV_PERSIST, __interest, t);
     evtimer_add(interest_ev, &interest_interval);
}
