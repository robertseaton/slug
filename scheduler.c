/*
 * scheduler.c 
 * handle scheduling of pieces of torrent to be downloaded 
 * * */
#include <stdlib.h>

#include "includes.h"

int 
rarest (const void* x, const void* y)
{
     const struct Piece* a = (struct Piece *)x;
     const struct Piece* b = (struct Piece *)y;

     return (a->priority - b->priority);
}

/* check if a peer is already in our queue */
int8_t 
peer_in_queue (struct Torrent* t, struct Peer* p)
{
     int i;
     for (i = 0; i < QUEUE_SIZE; i++) {
          struct QueueObject* qo = t->download_queue[i];

          if (qo != NULL) {
               unsigned long qo_addr = qo->peer->addr.sin_addr.s_addr;
               unsigned long p_addr = p->addr.sin_addr.s_addr;

               if (qo_addr == p_addr)
                    return 1;
          }
     }
     
     return 0;
}

int8_t 
piece_in_queue (struct Torrent* t, struct Piece* p)
{
     int i;
     for (i = 0; i < QUEUE_SIZE; i++) {
          if (t->download_queue[i] != NULL && t->download_queue[i]->piece == p)
               return 1;
     }
     
     return 0;
}

int8_t 
expired (struct QueueObject* qo) 
{
     if (time(NULL) - qo->started > 50) {
          qo->peer->state = Dead;
          qo->piece->state = Need;
          qo->piece->amount_downloaded = 0;
          return 1;
     } else
          return 0;
}

int8_t 
ready (struct Torrent* t, struct Piece piece, struct Peer* peer)
{
     if (!t->have_bitfield[piece.index] &&
         has_piece(&piece, peer) &&
         !peer->tstate.peer_choking &&
         !peer_in_queue(t, peer) &&
         !piece_in_queue(t, &piece) &&
         peer->state != Dead)
          return 1;
     else
          return 0;
     
}

struct QueueObject* get_rarest_unchoked (struct Torrent* t)
{
     struct QueueObject* qo = malloc(sizeof(struct QueueObject));
     struct PeerNode* pn = t->peer_list;
     struct Piece* piece; = find_by_index(t->peers)
     

     int i;
     for (i = 0; i < t->num_pieces; i++) {
          while (pn != NULL) {
               piece = find_by_index(t->peers, i);
               if (!t->have_bitfield[t->pieces[i].index] &&
                   has_piece(&t->pieces[i], pn->cargo) &&
                   !pn->cargo->tstate.peer_choking &&
                   !peer_in_queue(t, pn->cargo) &&
                   !piece_in_queue(t, &t->pieces[i]) &&
                    pn->cargo->state != Dead) {
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

/* TODO: replace naive implementation with min binary heap */
void 
build_queue (struct Torrent* t)
{
     int i;
     for (i = 0; i < QUEUE_SIZE; i++)
          if (t->download_queue[i] == NULL || 
              t->download_queue[i]->piece->state == Have || 
              expired(t->download_queue[i]))
               t->download_queue[i] = get_rarest_unchoked(t);
}

void 
__calculate_speed (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;
     struct PeerNode* head = t->peer_list;

     while (head != NULL) {
          head->cargo->speed = head->cargo->amount_downloaded / CALC_SPEED_INTERVAL;
          head->cargo->amount_downloaded = 0;
          head = head->next;
     }
}

void 
calculate_speed (struct Torrent* t, struct event_base* base)
{
     struct event* calc_speed_ev;
     struct timeval calc_speed_interval = {CALC_SPEED_INTERVAL, 0};
     
     calc_speed_ev = event_new(base, -1, EV_PERSIST, __calculate_speed, t);
     evtimer_add(calc_speed_ev, &calc_speed_interval);
}

void 
__schedule (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;

     uint64_t i;
     for (i = 0; i < t->num_pieces; ++i)
          t->pieces[i].rarity = t->global_bitfield[t->pieces[i].index];
     
     build_queue(t);
     for (i = 0; i < QUEUE_SIZE; i++) {
          struct QueueObject* qo = t->download_queue[i];
          if (qo != NULL && qo->piece->state == Queued) {
               download_piece(qo->piece, qo->peer);
               qo->begin_time = time(NULL);
          }
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

void 
__interest (evutil_socket_t fd, short what, void* arg)
{
     struct Torrent* t = arg;

     struct PeerNode* pn = t->peer_list;
     
     while (pn != NULL) {
          if (has_needed_piece(pn->cargo->bitfield, 
                               t->have_bitfield, 
                               t->num_pieces) && 
              !pn->cargo->tstate.interested)
               interested(pn->cargo);
          else if (!has_needed_piece(pn->cargo->bitfield, 
                                     t->have_bitfield, 
                                     t->num_pieces) &&
                   pn->cargo->tstate.interested)
               not_interested(pn->cargo);

          pn = pn->next;
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

void 
unqueue (struct Piece* piece, struct QueueObject** queue)
{
     piece->amount_downloaded = 0;

     int i;
     for (i = 0; i < QUEUE_SIZE; i++)
          if (queue[i] != NULL && piece->index == queue[i]->piece->index) {
               printf("Unqueued piece #%lu\n", piece->index);
               queue[i] = NULL;
               piece->state = Need;
          }
}
