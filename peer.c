#include <event2/bufferevent.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include "includes.h"

struct Peer
*init_peer(uint32_t addr, uint16_t port, struct Torrent *t)
{
     struct Peer *p = malloc(sizeof(struct Peer));

     memcpy((void *)&p->addr.sin_addr.s_addr, &addr, sizeof(p->addr.sin_addr.s_addr));
     memcpy((void *)&p->addr.sin_port, &port, sizeof(p->addr.sin_port));
     p->state = NotConnected;
     p->tstate.peer_choking = 1;
     p->tstate.interested = 0;
     p->bitfield = NULL;
     p->addr.sin_family = AF_INET;
     p->amount_downloaded = 0;
     p->torrent = t;
     p->pieces_requested = 0;
     p->speed = -1;
     time(&p->started);

     return p;
}

struct PeerNode
*init_peer_node(struct Peer *cargo, struct PeerNode *next)
{
     struct PeerNode *pn = malloc(sizeof(struct PeerNode));

     pn->cargo = cargo;
     pn->next = next;
     
     return pn;
}

void 
add_peer(struct PeerNode **head, struct Peer *peer)
{
     struct PeerNode *current;
     
     for (current = *head; current->next != NULL; current = current->next) {
          if (current->cargo->addr.sin_addr.s_addr == peer->addr.sin_addr.s_addr)
               return ;
     } 
     
     /* If we made it this far, that means our peer isn't already contained
        in the linked list. */
     current->next = init_peer_node(peer, NULL);
}

void 
add_peers(struct Torrent *t, struct PeerNode *peer_list)
{
     /* possible this is called from the first announce */
     if (t->peer_list == NULL) {
          t->peer_list = init_peer_node(peer_list->cargo, NULL);
          peer_list = peer_list->next;
     }
     struct PeerNode *head = peer_list;
     /* iterate over all the peers */
     while (peer_list != NULL) {
          add_peer(&t->peer_list, peer_list->cargo);
          peer_list = peer_list->next;
     }

     t->peer_list = head;
}

struct Peer
*find_unchoked(struct PeerNode *list)
{
     struct PeerNode *head = list;
     struct Peer *out = NULL;

     while (list != NULL) {
          if (!list->cargo->tstate.peer_choking)
               out = list->cargo;
          list = list->next;
     }

     list = head;

     return out;
}

void 
unchoke(struct Peer *p)
{
     uint32_t unchoke_prefix = htonl(1);
     uint8_t unchoke_id = 1;

     bufferevent_write(p->bufev, (const void *)&unchoke_prefix, sizeof(unchoke_prefix));
     bufferevent_write(p->bufev, (const void *)&unchoke_id, sizeof(unchoke_id));
     p->tstate.choked = 0;
}

void 
interested(struct Peer *p)
{
     uint32_t interested_prefix = htonl(1);
     uint8_t interested_id = 2;
     
     bufferevent_write(p->bufev, (const void *)&interested_prefix, sizeof(interested_prefix));
     bufferevent_write(p->bufev, (const void *)&interested_id, sizeof(interested_id));
     p->tstate.interested = 1;
}

void 
not_interested(struct Peer *p)
{
     uint32_t not_interested_prefix = htonl(1);
     uint8_t not_interested_id = 3;

     bufferevent_write(p->bufev, (const void *)&not_interested_prefix, sizeof(not_interested_prefix));
     bufferevent_write(p->bufev, (const void *)&not_interested_id, sizeof(not_interested_id));
     p->tstate.interested = 0;
}

void 
request(struct Peer *peer, struct Piece *piece, off_t off)
{
     uint32_t request_prefix = htonl(13);
     uint8_t request_id = 6;
     uint32_t index = htonl(piece->index);
     uint32_t request_length = htonl(REQUEST_LENGTH);
     uint32_t offset = htonl(off);

     bufferevent_write(peer->bufev, (const void *)&request_prefix, sizeof(request_prefix));
     bufferevent_write(peer->bufev, (const void *)&request_id, sizeof(request_id));
     bufferevent_write(peer->bufev, (const void *)&index, sizeof(index));
     bufferevent_write(peer->bufev, (const void *)&offset, sizeof(offset));
     bufferevent_write(peer->bufev, (const void *)&request_length, sizeof(request_length));

     if (off == 0)
          syslog(LOG_DEBUG, "Requested piece %"PRIu64" from peer %s.\n", piece->index, inet_ntoa(peer->addr.sin_addr));
}

void 
have(struct Piece *piece, struct Torrent *t)
{
     uint32_t have_prefix = htonl(5);
     uint8_t have_id = 4;
     uint32_t index = htonl(piece->index);
     struct PeerNode *head = t->peer_list;

     while (head != NULL) {
          bufferevent_write(head->cargo->bufev, (const void *)&have_prefix, sizeof(have_prefix));
          bufferevent_write(head->cargo->bufev, (const void *)&have_id, sizeof(have_id));
          bufferevent_write(head->cargo->bufev, (const void *)&index, sizeof(index));
          head = head->next;
     }

     t->have_bitfield[piece->index] = 1;

     uint64_t i = pieces_remaining(t->have_bitfield, t->num_pieces);

     /* if 0 pieces remain, the torrent is complete */
     if (i == 0) {
          complete(t);
     }

#ifdef DEBUG
     if (i < 5) {
          print_pieces_remaining(t->have_bitfield, t->num_pieces);
     }
#endif
}

int8_t 
has_needed_piece(uint8_t *peer_bitfield, char *host_bitfield, uint64_t num_pieces)
{
     /* possible the bitfield hasn't been initialized yet */
     if (peer_bitfield == NULL)
          return 0;

     uint64_t i;
     for (i = 0; i < num_pieces; i++)
          if (peer_bitfield[i] == 1 && host_bitfield[i] == 0)
               return 1;

     return 0;
}

uint64_t
num_peers(struct PeerNode *list)
{
     struct PeerNode *head = list;
     
     uint64_t i = 0;
     while (list != NULL) {
          i++;
          list = list->next;
     }

     list = head;

     return i;
}
