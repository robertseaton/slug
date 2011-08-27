#include <event2/bufferevent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "includes.h"

struct Peer* init_peer (char* addr, char* port, struct Torrent* t)
{
     struct Peer* p = malloc(sizeof(struct Peer));
     memcpy((void *)&p->addr.sin_addr.s_addr, addr, sizeof(p->addr.sin_addr.s_addr));
     memcpy((void *)&p->addr.sin_port, port, sizeof(p->addr.sin_port));
     p->state = NotConnected;
     p->bitfield = NULL;
     p->addr.sin_family = AF_INET;
     p->amount_downloaded = 0;
     p->torrent = t;
     time(&p->started);
#ifdef DEBUG
     p->dot_ip = inet_ntoa(p->addr.sin_addr);
#endif

     return p;
}

struct PeerNode* init_peer_node (struct Peer* cargo, struct PeerNode* next)
{
     struct PeerNode* pn = malloc(sizeof(struct PeerNode));
     pn->cargo = cargo;
     pn->next = next;
     
     return pn;
}

void add_peer (struct PeerNode* current, struct Peer* peer)
{
     while (current != NULL) {
          if (current->cargo->addr.sin_addr.s_addr == peer->addr.sin_addr.s_addr)
               return ;
          current = current->next;
     } 
     
     /* if we made it this far, that means our peer isn't already contained
      * in the linked list
      * * */
     current = init_peer_node(peer, NULL);
}

void add_peers (struct Torrent* t, struct PeerNode* peer_list)
{
     /* possible this is called from the first announce */
     if (t->peer_list == NULL) {
          t->peer_list = init_peer_node(peer_list->cargo, NULL);
          peer_list = peer_list->next;
     }
     struct PeerNode* first = peer_list;
     /* iterate over all the peers */
     while (peer_list != NULL) {
          add_peer(t->peer_list, peer_list->cargo);
          peer_list = peer_list->next;
     }

     t->peer_list = first;
}

struct PeerNode* find_unchoked (struct PeerNode* head, uint64_t index)
{
     struct PeerNode* unchoked_peers = NULL;
     struct PeerNode* unchoked_head = unchoked_peers;

     while (head != NULL) {
          if (head->cargo->bitfield[index] && head->cargo->tstate.peer_choking == 0 && unchoked_peers == NULL) {
               unchoked_peers = malloc(sizeof(struct PeerNode));
               unchoked_head = unchoked_peers;
               unchoked_peers->cargo = head->cargo;
               unchoked_peers->next = NULL;
          } else if (head->cargo->bitfield[index] && head->cargo->tstate.peer_choking == 0) {
               unchoked_peers->next = malloc(sizeof(struct PeerNode));
               unchoked_peers = unchoked_peers->next;
               unchoked_peers->next = NULL;
               unchoked_peers->cargo = head->cargo; 
          }
          head = head->next;
     }

     return unchoked_head;
               
}

/* check if all the peers with a piece are choking us */
uint8_t all_choked (struct PeerNode* head, uint64_t index)
{
     struct PeerNode* unchoked_peers = find_unchoked(head, index);

     if (unchoked_peers != NULL)
          return 0;
     else
          return 1;
}

void unchoke (struct Peer* p)
{
     uint32_t unchoke_prefix = htonl(1);
     uint8_t unchoke_id = 1;

     bufferevent_write(p->bufev, (const void *) &unchoke_prefix, sizeof(unchoke_prefix));
     bufferevent_write(p->bufev, (const void *) &unchoke_id, sizeof(unchoke_id));
     p->tstate.choked = 0;
}

void interested (struct Peer* p)
{
     uint32_t interested_prefix = htonl(1);
     uint8_t interested_id = 2;
     
     bufferevent_write(p->bufev, (const void *) &interested_prefix, sizeof(interested_prefix));
     bufferevent_write(p->bufev, (const void *) &interested_id, sizeof(interested_id));
     p->tstate.interested = 1;
}

void request (struct Peer* peer, struct Piece* piece, off_t off)
{
     uint32_t request_prefix = htonl(5);
     uint8_t request_id = 6;
     uint32_t index = htonl(piece->index);
     uint32_t request_length = htonl(REQUEST_LENGTH);
     uint32_t offset = htonl(off);

     bufferevent_write(peer->bufev, (const void *) &request_prefix, sizeof(request_prefix));
     bufferevent_write(peer->bufev, (const void *) &request_id, sizeof(request_prefix));
     bufferevent_write(peer->bufev, (const void *) &index, sizeof(index));
     bufferevent_write(peer->bufev, (const void *) &offset, sizeof(offset));
     bufferevent_write(peer->bufev, (const void *) &request_length, sizeof(request_length));
}
