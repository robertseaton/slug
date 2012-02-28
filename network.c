#include <arpa/inet.h>
#include <assert.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "includes.h"

void parse_msg(struct Peer*);
void get_msg(struct bufferevent*, struct Peer*);
void read_prefix(struct bufferevent*, struct Peer*);
void get_prefix(struct bufferevent*, struct Peer*);

/* FIXME: You should implement this. */
void 
handle_cancel(struct Peer *p)
{
     syslog(LOG_DEBUG, "CANCEL: %s\n", inet_ntoa(p->addr.sin_addr));
     return ;
}

/* FIXME: You should implement this. */
void 
handle_request(struct Peer *p)
{
     syslog(LOG_DEBUG, "REQUEST: %s\n", inet_ntoa(p->addr.sin_addr));
     return ;
}

void
piece_complete(struct Peer *peer, struct Piece *piece, uint32_t index)
{
     void *addr;
     uint8_t *sha1 = piece->sha1;

     if (peer->torrent->is_single)
          addr = peer->torrent->torrent_files.single.file.mmap + index * peer->torrent->piece_length;
     else
          ; /* TODO */

     if (verify_piece(addr, peer->torrent->piece_length, sha1)) {
          syslog(LOG_DEBUG, "Successfully downloaded piece: #%d of %"PRIu64"", index, peer->torrent->num_pieces);
          have(piece, peer->torrent);

               /*
               if (peer->tstate.peer_choking) {
                    uint64_t i;
                    for (i = 0; i < MAX_ACTIVE_PEERS; i++)
                         if (peer->torrent->active_peers[i] == p) {
                              peer->torrent->active_peers[i] = NULL;
                              insert_head(&peer->torrent->peer_list, p);
                         }
                         } */
     } else {
          /* If we failed to verify the piece, we have to assume the whole thing
             is bad and re-download the entire piece. To accomplish this, we 
             reset the piece's state and add the piece back to the heap, the 
             scheduler will handle the rest. */
          syslog(LOG_WARNING, "Failed to verify piece: #%d", index);
          piece->state = Need;
          heap_insert(peer->torrent->pieces, *piece, &compare_priority);
     }
     peer->pieces_requested--;
}

void 
handle_piece(struct Peer *p)
{
     uint32_t index, off;
     uint64_t length;

    /* The piece message takes the form: <prefix><index><offset><block>
       <prefix> is handled by other code
       <index> is the piece's index, four bytes, big endian
       <offset> is the offset of the block within the piece specified by <index>
       <offset> is also four bytes and big endian
       <block> is actual torrent data, a subset of the piece */
     memcpy(&index, &p->message[1], sizeof(index));
     memcpy(&off, &p->message[5], sizeof(off));
     
     index = ntohl(index);
     off = ntohl(off);
     /* The block's offset within the file: */
     length = index * p->torrent->piece_length + off;


     if (off == 0)
          syslog(LOG_DEBUG, "PIECE: #%d from %s", index, inet_ntoa(p->addr.sin_addr));

     memcpy(p->torrent->mmap + length, &p->message[9], REQUEST_LENGTH);

     

     struct Piece *piece = extract_by_index(p->torrent->downloading, index, &compare_age);

     /* sometimes a peer will send us a piece that we don't need */
     if (piece == NULL)
          return ;

     piece->amount_downloaded += REQUEST_LENGTH;

     if (piece->amount_downloaded >= p->torrent->piece_length) {
          piece_complete(p, piece, index);
     } else
          /* put piece back in heap */
          heap_insert(p->torrent->downloading, *piece, &compare_age);
}

void 
get_msg(struct bufferevent *bufev, struct Peer *p)
{
     uint64_t amount_read = p->message_length - p->amount_pending;
     int64_t message_length;
     uint32_t index, off;
     struct Piece* piece;

     if (p->message_length == 16393 && p->amount_pending < 16395 - 9) {
          memcpy(&index, &p->message[1], sizeof(index));
          memcpy(&off, &p->message[5], sizeof(off));
          index = ntohl(index);
          off = ntohl(off);
          piece = find_by_index(p->torrent->downloading, index);

          message_length = bufferevent_read(bufev, 
                                            &p->message[amount_read],
                                            p->amount_pending);
     } else
          message_length = bufferevent_read(bufev, 
                                            &p->message[amount_read],
                                            p->amount_pending);

     /* possible bufferevent_read found nothing */
     if (message_length < 0)
          message_length = 0;

     p->amount_pending = p->amount_pending - message_length;
     p->amount_downloaded += message_length;

     if (p->amount_pending == 0)
          p->state = HaveMessage;
     else 
          p->state = HavePartialMessage;

}

void 
read_prefix(struct bufferevent *bufev, struct Peer *p)
{
     if (p->message != NULL)
          free(p->message); /* free old message */

#define PREFIX_LEN 4
     p->amount_pending = PREFIX_LEN;
     p->message_length = PREFIX_LEN;
     p->message = malloc(sizeof(uint8_t) * PREFIX_LEN);
     int64_t message_length = bufferevent_read(bufev, 
                                               &p->message_length,
                                               p->amount_pending);

     /* possible bufferevent_read found nothing */
     if (message_length < 0)
          message_length = 0;

     p->amount_pending = p->amount_pending - message_length;

     /* It's possible that bufferevent_read will only return a partial prefix,
        but it's very rare, at least on a decent connection. It's almost 
        certainly a bug that we don't handle this. FIXME */
     assert(p->amount_pending <= 0);
}

void 
get_prefix(struct bufferevent *bufev, struct Peer *p)
{
     p->message_length = ntohl(p->message_length);
     p->amount_pending = p->message_length;
     
     if (p->message != NULL)
          free(p->message);

     p->message = malloc(sizeof(uint8_t) * p->message_length);
     get_msg(bufev, p);
}

void 
parse_msg(struct Peer *p)
{
     p->state = Connected;

     switch(p->message_length) {
     case 0:
          /* keep alive message, nothing needs to be done */
          return ;
     case 1:
          /* choke, unchoke, interested, and not interested messages all have a
             fixed length of 1 */
          switch ((int)*(p->message)) {
          case 0: /* choke */
               syslog(LOG_DEBUG, "CHOKE: %s", inet_ntoa(p->addr.sin_addr));
               p->tstate.peer_choking = 1;
               return ;
          case 1: /* unchoke */
               syslog(LOG_DEBUG, "UNCHOKE: %s", inet_ntoa(p->addr.sin_addr));
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
               return ;
          else
               break;
     case 5: /* have message */
          if (p->message[0] == 4) {
               update_bitfield(p->message, 
                               p->torrent->global_bitfield, 
                               p->bitfield);
               return ;
          } else
               break;
     case 13:
          switch ((int)*p->message) {
          case 6: /* request */
               handle_request(p);
               return ;
          case 8: /* cancel */
               handle_cancel(p);
               return ;
          } break;
     case 68: /* handshake */
          syslog(LOG_DEBUG, "HANDSHAKE: %s", inet_ntoa(p->addr.sin_addr));
          return ;
#define PIECE_PREFIX 16393
     case PIECE_PREFIX: /* piece message */
          if (p->message[0] == 7) {
               handle_piece(p);
               return;
          } else
               break;
     default:
          if (p->message_length == (int)(p->torrent->num_pieces / 8.0 + 1.9)) { /* full bitfield */
               syslog(LOG_DEBUG, "BITFIELD: %s", inet_ntoa(p->addr.sin_addr));
               p->bitfield = init_bitfield(p->torrent->num_pieces,
                                           p->message);

               return ;
          } else
               syslog(LOG_WARNING, "Received malformed message.");
     }
}

void 
handle_peer_response(struct bufferevent *bufev, void *payload)
{
     struct Peer *p = payload;
     
     if (p->state == Handshaking) {
          p->message = malloc(sizeof(uint8_t) * p->message_length);
          get_msg(bufev, p);
     } else if (p->state == HavePartialMessage)
          get_msg(bufev, p);
     else if (p->state == Connected) {
          read_prefix(bufev, p);
          get_prefix(bufev, p);
     } 

     if (p->state == HaveMessage)
          parse_msg(p);
}

void 
init_connection(struct Peer *p, uint8_t *handshake, struct event_base *base)
{
     if (p->state != NotConnected)
          return ;

     p->connectionfd = socket(AF_INET, SOCK_STREAM, 0);
     fcntl(p->connectionfd, F_SETFL, O_NONBLOCK);
     p->bufev = bufferevent_socket_new(base, 
                                       p->connectionfd,
                                       BEV_OPT_CLOSE_ON_FREE);
     bufferevent_socket_connect(p->bufev, 
                                (struct sockaddr *) &p->addr,
                                sizeof(p->addr));
     syslog(LOG_DEBUG, "CONNECTED: %s\n", inet_ntoa(p->addr.sin_addr));
     bufferevent_setcb(p->bufev, handle_peer_response, NULL, NULL, p);
     bufferevent_enable(p->bufev, EV_READ);

#define HANDSHAKE_LEN 68
     bufferevent_write(p->bufev, handshake, HANDSHAKE_LEN);

     p->state = Handshaking;
     p->message_length = HANDSHAKE_LEN;
     p->amount_pending = HANDSHAKE_LEN;
}

void 
init_connections(struct PeerNode *head, uint8_t *handshake, struct event_base *base)
{
     struct PeerNode *current;

     for (current = head; current != NULL; current = current->next)
          init_connection(current->cargo, handshake, base);
}
