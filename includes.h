#ifndef INCLUDES_H
#define INCLUDES_H

#include <sys/queue.h>
#include <curl/curl.h>
#include <event2/event.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define PORT                6784
#define DEFAULT_ANNOUNCE    1800
#define SCHEDULE_INTERVAL   1
#define INTEREST_INTERVAL   10
#define TIMEOUT_INTERVAL    10
#define OP_UNCHOKE_INTERVAL 30
#define CALC_SPEED_INTERVAL 5
#define TIMEOUT             15
#define REQUEST_LENGTH      16384
#define PEER_PIPELINE_LEN   4
#define MAX_ACTIVE_PEERS    4

#define STARTED             0
#define STOPPED             1
#define COMPLETED           2
#define VOID                8

#define SUCCESS             1
#define ANNOUNCE_FAILURE    0
#define ANNOUNCE_SUCCESS    1
#define UDP_SEND_FAILURE    0
#define UDP_RECEIVE_FAILURE 2
#define DEFAULT_PEERS_WANTED 50
#define MAX_RETRIES          8

#define ANNOUNCE 1

/* start forward declarations */
struct Piece;
struct BEncode;
struct Peer;
struct MinBinaryHeap;
/* end forward declarations */

/* pieces priority queue is implemented as a min binary heap */
struct MinBinaryHeap {
     uint64_t heap_size;
     uint64_t max_elements;
     struct Piece *elements;
};

struct TorrentFile {
     uint64_t length;
     char *path;
     void *mmap;
     int fd;
};

struct Torrent {
     char *peer_id;
     char *name;
     char *tracker_id;
     char *url;
     char *have_bitfield;
     uint8_t *info_hash;
     uint8_t is_single;
     uint64_t piece_length;
     uint64_t num_pieces;
     uint16_t port;
     uint64_t uploaded;
     uint64_t downloaded;
     uint64_t left;
     uint64_t length;
     uint64_t announce_interval;
     uint64_t *global_bitfield;
     int8_t private;
     int8_t compact;
     time_t started;
     void *mmap;
     struct Peer *optimistic_unchoke;
     struct Peer *active_peers[MAX_ACTIVE_PEERS];
     struct MinBinaryHeap *pieces;
     struct MinBinaryHeap *downloading;
     struct PeerNode *peer_list;
     struct Tracker *tracker;
     union {
          struct {
               struct TorrentFile file;
          } single;

          struct {
               struct TorrentFile *files;
               uint64_t length;
          } multi;
     } torrent_files;
};

struct Piece {
     uint64_t started;
     uint64_t index;
     uint64_t amount_downloaded;
     uint64_t amount_requested;
     uint64_t priority;
     uint8_t sha1[20];
     struct Peer *downloading_from;
     enum {
          Need,
          Queued,
          Downloading,
          Have
     } state;
};

struct State {
     uint8_t choked : 1;
     uint8_t interested : 1;
     uint8_t peer_choking : 1;
     uint8_t peer_interested : 1;
};

struct Peer {
     struct bufferevent *bufev;
     struct sockaddr_in addr;
     int32_t connectionfd;
     uint64_t message_length;
     uint64_t amount_pending;
     uint64_t amount_downloaded;
     int64_t speed;
     time_t started;
     uint8_t pieces_requested;
     uint8_t *message;
     uint8_t *bitfield;
     struct State tstate;
     struct Torrent *torrent;
     enum {
          NotConnected,
          Connected,
          Handshaking,
          HavePartial,
          HaveMessage,
          HavePartialMessage,
          Dead
     } state;
};

/* simple linked list */
struct PeerNode {
     struct Peer *cargo;
     struct PeerNode *next;
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
     struct BEncode *value;
     struct BDictNode *next;
};

struct BEncode {
     BType type;
     union {
          int64_t bInt;
          char *bStr;
          struct BListNode *bList;
          struct BDictNode *bDict;
     } cargo;
};

/* from announce.c */
void             announce                  (struct Torrent*, int8_t, CURL*, struct event_base*);

/* bitfield.c */
uint8_t         *init_bitfield             (uint64_t, uint8_t*);
uint64_t        *init_global_bitfield      (uint64_t);
char            *init_have_bitfield        (uint64_t);
void             update_bitfield           (uint8_t*, uint64_t*, uint8_t*);
void             update_global_bitfield    (uint64_t, char*, uint64_t*);

/* heap.c */
int              compare_priority          (struct Piece, struct Piece);
int              compare_age               (struct Piece, struct Piece);
void             heap_initialize           (struct MinBinaryHeap*, uint64_t);
int              heap_insert               (struct MinBinaryHeap*, struct Piece, int (*)(struct Piece, struct Piece));
struct Piece    *find_by_index             (struct MinBinaryHeap*, uint64_t);
struct Piece    *heap_min                  (struct MinBinaryHeap*);
struct Piece    *heap_extract_min          (struct MinBinaryHeap*, int (*)(struct Piece, struct Piece));
struct Piece    *extract_by_index          (struct MinBinaryHeap*, uint64_t, int (*)(struct Piece, struct Piece));

/* list.c */
struct Peer     *extract_element           (struct PeerNode*, struct Peer*);
void             insert_head               (struct PeerNode**, struct Peer*);
void             insert_tail               (struct PeerNode*, struct Peer*);

/* metadata.c */
struct Torrent  *init_torrent              (FILE*, double, double);

/* network.c */
void             init_connections          (struct PeerNode*, uint8_t*, struct event_base*);

/* parser.c */
struct BEncode  *parseBEncode              (char*, int64_t*);

/* peer.c */
struct Peer     *init_peer                 (uint32_t, uint16_t, struct Torrent*);
struct PeerNode *init_peer_node            (struct Peer*, struct PeerNode*);
void             add_peer                  (struct PeerNode**, struct Peer*);
void             add_peers                 (struct Torrent*, struct PeerNode*);
struct Peer     *find_unchoked             (struct PeerNode*);
struct Peer     *find_seed                 (struct PeerNode*, uint64_t);
void             unchoke                   (struct Peer*);
void             interested                (struct Peer*);
void             not_interested            (struct Peer*);
void             request                   (struct Peer*, struct Piece*, off_t);
void             have                      (struct Piece*, struct Torrent*);
int              has_needed_piece          (uint8_t*, char*, uint64_t);
uint64_t         num_peers                 (struct PeerNode*);

/* piece.c */
void             init_piece                (struct Piece*, uint64_t);
void             free_pieces               (struct Piece*, uint64_t);
void             download_piece            (struct Piece*, struct Peer*);
int              verify_piece              (void*, uint64_t, uint8_t*);
int              has_piece                 (struct Piece*, struct Peer*);
int              pieces_remaining          (char*, uint64_t);
void             print_pieces_remaining    (char*, uint64_t);

/* scheduler.c */
void             queue_pieces              (struct Torrent*, struct Peer*);
void             update_interest           (struct Torrent*, struct event_base*);
void             schedule                  (struct Torrent*, struct event_base*);
void             timeout                   (struct Torrent*, struct event_base*);

/* torrent.c */
void             start_torrent             (char*, double, double);
void             complete                  (struct Torrent*);

/* url.c */
char            *construct_url             (struct Torrent*, int8_t);

/* util.c */
void             error                     (char*);
struct BEncode  *find_value                (char*, struct BDictNode*);
void             freeBEncode               (struct BEncode*);
void             print_sha1                (uint8_t*);
int              mkpath                    (const char*, mode_t);
#endif
