#include <assert.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "includes.h"

void parse_msg(struct Peer*);
void get_msg(struct bufferevent*, struct Peer*);
void read_prefix(struct bufferevent*, struct Peer*);
void get_prefix(struct bufferevent*, struct Peer*);

void handle_cancel (struct Peer* p)
{
#ifdef DEBUG
     printf("CANCEL: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
     return ;
}

void handle_request (struct Peer* p)
{
#ifdef DEBUG
     printf("REQUEST: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
     return ;
}

void handle_piece (struct Peer* p)
{
     uint32_t index, off;
     unsigned long long length;

     memcpy(&index, &p->message[1], sizeof(index));
     memcpy(&off, &p->message[5], sizeof(off));
     
     index = ntohl(index);
     off = ntohl(off);
     length = index * p->torrent->piece_length + off;

#ifdef DEBUG
     if (off == 0)
     printf("PIECE: #%d from %s\n", index, inet_ntoa(p->addr.sin_addr));
#endif
     
     memcpy(p->torrent->mmap + length, &p->message[9], REQUEST_LENGTH);
     p->torrent->pieces[index].amount_downloaded += REQUEST_LENGTH;

     if (p->torrent->pieces[index].amount_downloaded >= p->torrent->piece_length)
          if (verify_piece(p->torrent->mmap + index * p->torrent->piece_length, p->torrent->piece_length, p->torrent->pieces[index].sha1)) {
               printf("Successfully downloaded piece: #%d\n", index);
               p->torrent->pieces[index].state = Have;
               have(&p->torrent->pieces[index], p->torrent->peer_list, p->torrent->have_bitfield);
          } else {
               printf("Failed to verify piece: #%d\n, length: %llu", index, length);
               //unqueue(&peer->torrent->pieces[index]);
               p->torrent->pieces[index].state = Need;
               p->torrent->pieces[index].amount_downloaded = 0;
          }
}

void get_msg (struct bufferevent* bufev, struct Peer* p)
{
     uint64_t amount_read = p->message_length - p->amount_pending;
     int64_t message_length = bufferevent_read(bufev, &p->message[amount_read], p->amount_pending);
     
     /* possible bufferevent_read found nothing */
     if (message_length < 0)
          message_length = 0;

     p->amount_pending = p->amount_pending - message_length;
     
     if (p->amount_pending > 0)
          p->state = HavePartialMessage;
     else
          p->state = HaveMessage;
}

void read_prefix (struct bufferevent* bufev, struct Peer* p)
{
     if (p->message != NULL)
          free(p->message); /* free old message */
#define PREFIX_LEN 4
     p->amount_pending = PREFIX_LEN;
     p->message_length = PREFIX_LEN;
     p->message = malloc(sizeof(unsigned char) * PREFIX_LEN);
     int64_t message_length = bufferevent_read(bufev, &p->message_length, p->amount_pending);

     /* possible bufferevent_read found nothing */
     if (message_length < 0)
          message_length = 0;

     p->amount_pending = p->amount_pending - message_length;

     if (p->amount_pending > 0)
          printf("Read partial prefix!\n");
}

void get_prefix (struct bufferevent* bufev, struct Peer* p)
{
     p->message_length = ntohl(p->message_length);
     p->amount_pending = p->message_length;
     
     if (p->message != NULL)
          free(p->message);

     p->message = malloc(sizeof(unsigned char) * p->message_length);
     get_msg(bufev, p);
}

void parse_msg (struct Peer* p)
{
     p->state = Connected;

     switch(p->message_length) {
     case 0:
          /* keep alive message, nothing needs to be done */
          return ;
     case 1:
          /* choke, unchoke, interested, and not interested messages all have a fixed length of 1 */
          switch ((int)*p->message) {
          case 0: /* choke */
#ifdef DEBUG
               printf("CHOKE: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
               p->tstate.peer_choking = 1;
               return ;
          case 1: /* unchoke */
#ifdef DEBUG
               printf("UNCHOKE: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
               p->tstate.peer_choking = 0;
               return ;
          case 2: /* interested */
               p->tstate.peer_interested = 1;
               return ;
          case 3: /* uninterested */
               p->tstate.peer_interested = 0;
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
     case 68: /* handshake */
#ifdef DEBUG
          printf("HANDSHAKE: %s\n", inet_ntoa(p->addr.sin_addr));
          return;
#endif
#define PIECE_PREFIX 16393
     case PIECE_PREFIX: /* piece message */
          if (p->message[0] == 7) {
               handle_piece(p);
               return;
          } else
               break;
     default:
          if (p->message_length == (int)(p->torrent->num_pieces / 8.0 + 1.9)) { /* full bitfield */
#ifdef DEBUG
               printf("BITFIELD: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
               p->bitfield = init_bitfield(p->torrent->num_pieces, p->message);

               return ;
          } else
                 printf("Received malformed message.");
     }
}

void handle_peer_response (struct bufferevent* bufev, void* payload)
{
     struct Peer* p = payload;

     if (p->state == Handshaking) {
          p->message = malloc(sizeof(unsigned char) * p->message_length);
          get_msg(bufev, p);
     }     
     else if (p->state == HavePartialMessage)
          get_msg(bufev, p);
     else if (p->state == Connected) {
          read_prefix(bufev, p);
          get_prefix(bufev, p);
     } 

     if (p->state == HaveMessage)
          parse_msg(p);
}

void init_connection (struct Peer* p, unsigned char* handshake, struct event_base* base)
{
     if (p->state != NotConnected)
          return ;

     p->connectionfd = socket(AF_INET, SOCK_STREAM, 0);
     fcntl(p->connectionfd, F_SETFL, O_NONBLOCK);
     p->bufev = bufferevent_socket_new(base, p->connectionfd, BEV_OPT_CLOSE_ON_FREE);
     bufferevent_socket_connect(p->bufev, (struct sockaddr *) &p->addr, sizeof(p->addr));
#ifdef DEBUG
     printf("CONNECTED: %s\n", inet_ntoa(p->addr.sin_addr));
#endif
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
     while (head != NULL) {
          init_connection(head->cargo, handshake, base);
          head = head->next;
     }
}
