#include <stdlib.h>

#include "includes.h"

struct Peer
*extract_element(struct PeerNode *head, struct Peer *peer)
{
     if (peer == NULL)
          return NULL;

     /* check if peer is the head */
     if (head->cargo == peer) {
          head = head->next;
          
          return peer;
     }

     struct PeerNode *prev = head;
     struct PeerNode *current = head->next;

     while (current != NULL) {
          if (current->cargo == peer) {
               prev->next = current->next;

               return peer;
          }
          
          prev = current;
          current = current->next;
     }

     /* if we reach this far, peer wasn't in linked list */
     return NULL;
}

void
insert_head(struct PeerNode **head, struct Peer *peer)
{
     struct PeerNode *new_head = malloc(sizeof(struct PeerNode));

     new_head->cargo = peer;
     new_head->next = *head;
     *head = new_head;
}

void
insert_tail(struct PeerNode *list, struct Peer *peer)
{
     struct PeerNode *head = list;
     struct PeerNode *tail = malloc(sizeof(struct PeerNode));
     tail->cargo = peer;
     tail->next = NULL;

     while (list->next != NULL)
          list = list->next;

     list->next = tail;
     list = head;
}
