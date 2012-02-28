/*
 * scheduler.c 
 * handle scheduling of pieces of torrent to be downloaded 
 */
#include <arpa/inet.h>

#include <stdlib.h>
#include <syslog.h>
#include <inttypes.h>

#include "includes.h"

void
queue_pieces(struct Torrent *t, struct Peer *peer)
{
     /* don't request pieces from a choked peer */
     if (peer == NULL || peer->tstate.peer_choking)
          return ;

     struct MinBinaryHeap *tmp = malloc(sizeof(struct MinBinaryHeap));
     struct Piece *piece;

     heap_initialize(tmp, t->num_pieces);

     while (peer->pieces_requested <= PEER_PIPELINE_LEN) {
               piece = heap_extract_min(t->pieces, compare_priority);
               if (piece != NULL && peer->bitfield[piece->index])
                    download_piece(piece, peer);
               else if (piece != NULL) {
                    heap_insert(tmp, *piece, &compare_priority);
               } else
                    break;
     }
     
     /* move pieces from tmp heap back into t->pieces */
     piece = heap_extract_min(tmp, compare_priority);
     while (piece != NULL) {
          heap_insert(t->pieces, *piece, &compare_priority);
          piece = heap_extract_min(tmp, &compare_priority);
     }
}

int 
in_queue(struct Peer **queue, struct Peer *cargo)
{
     uint64_t i;
     for (i = 0; i < MAX_ACTIVE_PEERS; i++)
          if (queue[i] == cargo)
               return 1;

     return 0;
}

/* helper function for __optimistic_unchoke */
static void
swap_peers(struct Peer *x, struct Peer *y)
{
     struct Peer *z;

     z = x;
     x = y;
     y = z;
}

/* helper function for __optimistic_unchoke, finds the first peer that
 * has never been downloaded from */
struct Peer*
find_unused_peer(struct PeerNode *list)
{
     struct PeerNode *head = list;
     struct Peer *out = NULL;

     while (list != NULL) {
          if (list->cargo->speed == -1) {
               out = list->cargo;
          }
          list = list->next;
     }

     list = head;

     return out;
}

int 
compare_speed(const void *p, const void *q)
{
     struct Peer *x = p;
     struct Peer *y = q;

     if (x == NULL)
          return -1;
     else if (y == NULL)
          return 1;
     else if (x > y)
          return 1;
     else if (x < y)
          return -1;
 
          return 0;
}

/* every 30s, we unchoke a peer that we haven't tried yet */
void
__optimistic_unchoke(evutil_socket_t fd, short what, void *arg)
{
     struct Torrent *t = arg;
     struct Peer *p;

     p = extract_element(t->peer_list, find_unused_peer(t->peer_list));

     /* if the old optimistically unchoked peer is faster than the slowest
      * of the peers in our current crop of active peers, we swap them and
      * choke the slower one
      * * */
     if (t->optimistic_unchoke != NULL && p != NULL) {
          qsort(t->active_peers, MAX_ACTIVE_PEERS, sizeof(struct Peer *), &compare_speed);
          if (t->active_peers[0] != NULL && t->optimistic_unchoke->speed > t->active_peers[0]->speed)
               swap_peers(t->optimistic_unchoke, t->active_peers[0]);
          // choke(t->optimistic_unchoke);
          insert_head(&t->peer_list, t->optimistic_unchoke);
     }

     syslog(LOG_DEBUG, "OPTIMISTIC UNCHOKE: %s\n", inet_ntoa(p->addr.sin_addr));
     t->optimistic_unchoke = p;
     unchoke(p);
}

/* helper function for __calculate_speed() */
void
calculate_speed(struct Peer *p)
{
     if (p->amount_downloaded) {
          p->speed = p->amount_downloaded / CALC_SPEED_INTERVAL;
          p->amount_downloaded = 0;
     }
}

void 
__calculate_speed(evutil_socket_t fd, short what, void *arg)
{
     struct Torrent *t = arg;

     uint64_t i;
     for (i = 0; i < MAX_ACTIVE_PEERS; i++) {
          if (t->active_peers[i] != NULL)
               calculate_speed(t->active_peers[i]);
     }

     if (t->optimistic_unchoke != NULL)
          calculate_speed(t->optimistic_unchoke);
}

void 
__schedule(evutil_socket_t fd, short what, void *arg)
{
     struct Torrent *t = arg;

     /* FIXME: some kind of pipelining heuristic needs to be 
      * implemented in order to saturate high bandwidth 
      * connections
      * * */

     uint64_t i;
     for (i = 0; i < MAX_ACTIVE_PEERS; i++) {
          if (t->active_peers[i] == NULL)
               t->active_peers[i] = extract_element(t->peer_list, find_unchoked(t->peer_list));

          if (t->active_peers[i] == NULL)
               t->active_peers[i] = extract_element(t->peer_list, find_seed(t->peer_list, t->num_pieces));

          /* still possible it's null */
          if (t->active_peers[i] != NULL)
               queue_pieces(t, t->active_peers[i]);
     }
         
     queue_pieces(t, t->optimistic_unchoke);
}

void 
schedule(struct Torrent *t, struct event_base *base)
{
     struct event *schedule_ev, *op_unchoke_ev, *calc_speed_ev;
     struct timeval schedule_interval = {SCHEDULE_INTERVAL, 0};
     struct timeval op_unchoke_interval = {OP_UNCHOKE_INTERVAL, 0};
     struct timeval calc_speed_interval = {CALC_SPEED_INTERVAL, 0};
     
     schedule_ev = event_new(base, -1, EV_PERSIST, __schedule, t);
     op_unchoke_ev = event_new(base, -1, EV_PERSIST, __optimistic_unchoke, t);
     calc_speed_ev = event_new(base, -1, EV_PERSIST, __calculate_speed, t);
     evtimer_add(schedule_ev, &schedule_interval);
     evtimer_add(op_unchoke_ev, &op_unchoke_interval);
     evtimer_add(calc_speed_ev, &calc_speed_interval);
}

/* possible we requested a piece and haven't received any data
 * for a long time, in which case it needs to be re-added to
 * the priority queue.
 * * */
void
__timeout(evutil_socket_t fd, short what, void *arg)
{
     struct Torrent *t = arg;
     struct Piece *p = heap_min(t->downloading);

     while (p != NULL && time(NULL) - p->started > TIMEOUT) {
          p = heap_extract_min(t->downloading, &compare_age);
          syslog(LOG_DEBUG, "TIMEOUT: Piece #%"PRIu64" from %s.\n", 
                 p->index, 
                 inet_ntoa(p->downloading_from->addr.sin_addr));
          p->downloading_from->state = Dead;
          p->amount_downloaded = 0;
          
          uint64_t i;
          for (i = 0; i < MAX_ACTIVE_PEERS; i++)
               if (t->active_peers[i] == p->downloading_from)
                    t->active_peers[i] = NULL;

          heap_insert(t->pieces, *p, &compare_priority);
          p = heap_min(t->downloading);
     }
}

void
timeout(struct Torrent *t, struct event_base *base)
{
     struct event *timeout_ev;
     struct timeval timeout_interval = {TIMEOUT_INTERVAL, 0};
     
     timeout_ev = event_new(base, -1, EV_PERSIST, __timeout, t);
     evtimer_add(timeout_ev, &timeout_interval);
}

void 
__interest(evutil_socket_t fd, short what, void *arg)
{
     struct Torrent *t = arg;

     struct PeerNode *peer_list = t->peer_list;

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
}

void 
update_interest(struct Torrent *t, struct event_base *base)
{
     struct event *interest_ev;
     struct timeval interest_interval = {INTEREST_INTERVAL, 0};

     interest_ev = event_new(base, -1, EV_PERSIST, __interest, t);
     evtimer_add(interest_ev, &interest_interval);
}
