#include <curl/curl.h>
#include <event2/event.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "includes.h"

/* handshake: <pstrlen><pstr><reserved><info_hash><peer_id> */
uint8_t
*construct_handshake(struct Torrent* t)
{
#define PSTRLEN_LEN 1
#define PSTR_LEN 19
#define RESERVED_LEN 8
#define INFOHASH_LEN 20
#define PEERID_LEN 20

     uint8_t *handshake = malloc(PSTRLEN_LEN + 
                                 PSTR_LEN + 
                                 RESERVED_LEN + 
                                 INFOHASH_LEN + 
                                 PEERID_LEN);
     memset(handshake, 
            0, 
            PSTRLEN_LEN + 
            PSTR_LEN + 
            RESERVED_LEN + 
            INFOHASH_LEN + 
            PEERID_LEN);

     handshake[0] = PSTR_LEN;
     memcpy(handshake + PSTRLEN_LEN, 
            "BitTorrent protocol", 
            PSTR_LEN);
     memcpy(handshake + PSTRLEN_LEN + PSTR_LEN + RESERVED_LEN, 
            t->info_hash, 
            INFOHASH_LEN);
     memcpy(handshake + PSTRLEN_LEN + PSTR_LEN + RESERVED_LEN + INFOHASH_LEN, 
            t->peer_id, 
            PEERID_LEN);

     return handshake;
}

void 
start_torrent(char *file, double peer_id, double port)
{
     FILE *stream = fopen(file, "r");
     struct Torrent *t = init_torrent(stream, peer_id, port);
     struct event_base *base = event_base_new();
     uint8_t *handshake = construct_handshake(t);
     CURL *connection = curl_easy_init();

     t->started = time(NULL);
     
     announce(t, STARTED, connection, base);
     init_connections(t->peer_list, handshake, base);
     update_interest(t, base);
     schedule(t, base);
     timeout(t, base);
     event_base_dispatch(base);
}

void
complete(struct Torrent *t)
{
     time_t seconds_elapsed = time(NULL) - t->started;
     uint64_t kb_downloaded = t->length / 1024;

     printf("Torrent %s complete, took %"PRId64" minutes, %"PRId64" seconds (avg speed: %"PRId64" kbps)\n", 
            t->name, 
            (uint64_t)seconds_elapsed / 60, 
            (uint64_t)seconds_elapsed % 60,
            (uint64_t)kb_downloaded / seconds_elapsed);

     exit(0);
}
