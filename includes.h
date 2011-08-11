#ifndef INCLUDES_H
#define INCLUDES_H

#include <curl/curl.h>
#include <event2/event.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>

/* start forward declarations */
struct Piece;
struct BEncode;
struct PeerNode;
/* end forward declarations */

struct Torrent {
     char* peer_id;
     char* name;
     char* tracker_id;
     char* url;
     char* have_bitfield;
     unsigned char* info_hash;
     uint64_t length;
     uint64_t piece_length;
     uint64_t num_pieces;
     uint64_t port;
     uint64_t uploaded;
     uint64_t downloaded;
     uint64_t left;
     uint64_t announce_interval;
     uint64_t* global_bitfield;
     int8_t private;
     int8_t compact;
     struct Piece* pieces;
     struct PeerNode* peer_list;
};

struct Piece {
     uint64_t index;
     uint64_t amount_downloaded;
     uint64_t amount_requested;
     uint64_t rarity;
     unsigned char sha1[20];
     int8_t state;
};

struct Peer {
     struct bufferevent* bufev;
     struct sockaddr_in addr;
     int32_t connectionfd;
     uint64_t message_length;
     uint64_t amount_pending;
     uint64_t amount_downloaded;
     uint64_t tstate;
     time_t started;
     unsigned char* message;
     unsigned char* bitfield;
     struct Torrent* torrent;
     enum {
          NotConnected,
          Connected,
          Handshaking,
          HavePartial,
          HaveMessage,
          HavePartialMessage,
          Dead
     } state;
#ifdef DEBUG
     /* human readable ip address */
     char* dot_ip;
#endif
};

/* peers linked list */
struct PeerNode {
     struct Peer* cargo;
     struct PeerNode* next;
};

typedef enum BType {
     BString,
     BInt,
     BList,
     BDict
} BType;

struct BListNode {
     struct BEncode* cargo;
     struct BListNode* next;
};

struct BDictNode {
     char* key;
     struct BEncode* value;
     struct BDictNode* next;
};

struct BEncode {
     BType type;
     union {
          int64_t bInt;
          char* bStr;
          struct BListNode* bList;
          struct BDictNode* bDict;
     } cargo;
};

#define PORT 6784
#define DEFAULT_ANNOUNCE 1800
#define STARTED 0
#define STOPPED 1
#define COMPLETED 2
#define VOID 8
/* tstate identifiers for peer */
#define CHOKED       (1 << 0)
#define INTERESTED   (1 << 1)
/* state identifiers for piece */
#define NEED          (1 << 0)

/* from announce.c */
void first_announce(struct Torrent*, int8_t, CURL*, struct event_base*);

/* from bitfield.c */
char* init_bitfield(uint64_t, unsigned char*);
uint64_t* init_global_bitfield(uint64_t);
char* init_have_bitfield(uint64_t);
void update_bitfield(unsigned char*, uint64_t*, unsigned char*);
void update_global_bitfield(uint64_t, char*, uint64_t*);

/* from metadata.c */
struct Torrent* init_torrent(FILE*, double, double);

/* from network.c */
void init_connections(struct PeerNode*, unsigned char*, struct event_base*);

/* from parser.c */
struct BEncode* parseBEncode(char*, int64_t*);

/* from peer.c */
struct Peer* init_peer(char*, char*, struct Torrent*);
struct PeerNode* init_peer_node(struct Peer*, struct PeerNode*);
void add_peers (struct Torrent*, struct PeerNode*);

/* from piece.c */
void init_piece(struct Piece, uint64_t);
void free_pieces(struct Piece*, uint64_t);

/* from torrent.c */
void start_torrent(char*, double, double);

/* from url.c */
char* construct_url(struct Torrent*, int8_t);

/* from util.c */
void error(char*);
struct BEncode* find_value(char* key, struct BDictNode* d);
void freeBEncode(struct BEncode*);
#endif
