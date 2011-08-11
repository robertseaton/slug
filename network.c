#include <event2/bufferevent.h>
#include <event2/event.h>
#include <fcntl.h>
#include <stdlib.h>

#include "includes.h"

void parse_msg(struct Peer*);
void get_msg(struct bufferevent*, struct Peer*);
void read_prefix(struct bufferevent*, struct Peer*);
void get_prefix(struct bufferevent*, struct Peer*);

void handle_cancel (struct Peer* peer)
{
     return ;
}

void handle_request (struct Peer* peer)
{
     return ;
}

void get_msg (struct bufferevent* bufev, struct Peer* p)
{
     uint64_t amount_read = p->message_length - p->amount_pending;
     uint64_t message_length = bufferevent_read(bufev, &p->message[amount_read], p->amount_pending);
     p->amount_pending = p->amount_pending - message_length;
     
     if (p->amount_pending > 0 && message_length > 0) {
          if (p->state == Connected)
               p->state = HavePartialMessage;
          else if (p->state != Connected)
               p->state = HavePartial;
     }
     else if (p->state == HavePartialMessage)
          p->state = HaveMessage;
     else
          p->state = Connected;
}

void read_prefix (struct bufferevent* bufev, struct Peer* p)
{
     if (p->message != NULL)
          free(p->message); /* free old message */
#define PREFIX_LEN 4
     p->amount_pending = PREFIX_LEN;
     p->message_length = PREFIX_LEN;
     p->message = malloc(sizeof(unsigned char) * PREFIX_LEN);
     uint64_t message_length = bufferevent_read(bufev, &p->message_length, p->amount_pending);
     p->amount_pending = p->amount_pending - message_length;

     if (p->amount_pending > 0 && message_length > 0) {
          if (p->state == Connected)
               p->state = HavePartialMessage;
          else
               p->state = HavePartial;
     }
     else if (p->state == HavePartialMessage)
          p->state = HaveMessage;
     else
          p->state = Connected;
}

void get_prefix (struct bufferevent* bufev, struct Peer* p)
{
     p->message_length = ntohl(p->message_length);
     p->amount_pending = p->message_length;
     
     if (p->message != NULL)
          free(p->message);

     p->message = malloc(sizeof(unsigned char) * p->message_length);
     get_msg(bufev, p);
     
     if (p->state == Connected)
          p->state = HaveMessage;
}

void parse_msg (struct Peer* p)
{
     switch(p->message_length) {
     case 0:
          /* keep alive message, nothing needs to be done */
          return ;
     case 1:
          /* choke, unchoke, interested, and not interested messages all have a fixed length of 1 */
          switch ((int)*p->message) {
          case 0: /* choke */
               p->tstate = p->tstate & CHOKED;
               return ;
          case 1: /* unchoke */
               p->tstate = p->tstate & ~CHOKED;
               return ;
          case 2: /* interested */
               p->tstate = p->tstate & INTERESTED;
               return ;
          case 3: /* uninterested */
               p->tstate = p->tstate & ~INTERESTED;
               return ;
          } break;
     case 3: /* port */
          if (p->message[0] == 9)
               /* DHT NOT IMPLEMENTED */
               return;
          else
               break;
     case 5: /* have message */
          if (p->message[0] == 4) {
               update_bitfield(p->message, p->torrent->global_bitfield, p->bitfield);
               return ;
          } else
               break;
     case 13:
          switch ((int)*p->message) {
          case 6: /* request */
               handle_request(p);
               return;
          case 8: /* cancel */
               handle_cancel(p);
               return;
          } break;
#define PIECE_PREFIX 16393
     case PIECE_PREFIX: /* piece message */
          if (p->message[0] == 7) {
               /* handle_piece(p); */
               return;
          } else
               break;
     default: /* full bitfield */
          init_bitfield(p->torrent->num_pieces, p->message);
          return ;
     }
}

void handle_peer_response (struct bufferevent* bufev, void* payload)
{
     struct Peer* p = payload;

     if (p->state == Handshaking)
          p->message = malloc(sizeof(unsigned char) * p->message_length);
     else if (p->state == Connected)
          read_prefix(bufev, p);
     if (p->state == Connected && p->amount_pending == 0)
          get_prefix(bufev, p);
     if (p->state == HaveMessage)
          parse_msg(p);
     
     get_msg(bufev, p);
}

void init_connection (struct Peer* p, unsigned char* handshake, struct event_base* base)
{
     if (p->state != NotConnected)
          return ;

     p->connectionfd = socket(AF_INET, SOCK_STREAM, 0);
     fcntl(p->connectionfd, F_SETFL, O_NONBLOCK);
     p->bufev = bufferevent_socket_new(base, p->connectionfd, BEV_OPT_CLOSE_ON_FREE);
     bufferevent_socket_connect(p->bufev, (struct sockaddr *) &p->addr, sizeof(p->addr));
     bufferevent_setcb(p->bufev, handle_peer_response, NULL, NULL, p);
     bufferevent_enable(p->bufev, EV_READ);

#define HANDSHAKE_LEN 68
     bufferevent_write(p->bufev, handshake, HANDSHAKE_LEN);

     p->state = Handshaking;
     p->message_length = HANDSHAKE_LEN;
     p->amount_pending = HANDSHAKE_LEN;
}

void init_connections (struct PeerNode* head, unsigned char* handshake, struct event_base* base)
{
     /* first peer */
     init_connection(head->cargo, handshake, base);
     head = head->next;

     /* rest */
     while (head->next != NULL) {
          head = head->next;
          init_connection(head->cargo, handshake, base);
     }
}
