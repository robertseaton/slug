#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "includes.h"

struct Peer* init_peer (char* addr, char* port)
{
     struct Peer* p = malloc(sizeof(struct Peer));
     memcpy((void *)&p->addr.sin_addr.s_addr, addr, sizeof(p->addr.sin_addr.s_addr));
     memcpy((void *)&p->addr.sin_port, port, sizeof(p->addr.sin_port));
     p->state = NOT_CONNECTED | CHOKED;
     p->bitfield = NULL;
     p->addr.sin_family = AF_INET;
     p->amount_downloaded = 0;
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
     /* handle the first peer in the linked list */
     if (current->cargo->addr.sin_addr.s_addr == peer->addr.sin_addr.s_addr) {
          
          return ;
     }
     /* check against the rest of the peers in the linked list */
     while (current->next != NULL) {
          current = current->next;
          if (current->cargo->addr.sin_addr.s_addr == peer->addr.sin_addr.s_addr)
               return ;
     } 
     
     /* if we made it this far, that means our peer isn't already contained
      * in the linked list
      * * */
     current->next = init_peer_node(peer, NULL);
}

void add_peers (struct Torrent* t, struct PeerNode* peer_list)
{
     struct PeerNode* current = t->peer_list;
   
     /* possible this is called from the first announce */
     if (t->peer_list == NULL) {
          current = init_peer_node(peer_list->cargo, NULL);
          peer_list = peer_list->next;
     }

     /* iterate over all the peers */
     while (peer_list != NULL) {
          add_peer(current, peer_list->cargo);
          peer_list = peer_list->next;
     }
}
