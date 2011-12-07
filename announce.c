/*
 * announce.c
 * handle announcing to tracker
 * * */

#include <ctype.h>
#include <curl/curl.h>
#include <event2/event.h>
#include <stdlib.h>
#include <string.h>

#include "includes.h"

static int8_t 
failed(struct BDictNode *b)
{
     struct BEncode *output_value = find_value("failure reason", b);
     
     if (output_value != NULL)
          return 1;
     
     return 0;
}

static void 
pfailure_reason(struct BDictNode *b)
{
     struct BEncode *output_value = find_value("failure reason", b);
     fprintf(stderr, "Announce failed: %s\n", output_value->cargo.bStr);
}

struct PeerNode 
*get_peers(struct Torrent *t, char *data)
{
     int64_t i = 0;
     char *j;

#define PEERS_STR "5:peers"
     j = strstr(data, PEERS_STR);
     j += strlen(PEERS_STR);
     
     freeBEncode(parseBEncode(j, &i));

     uint64_t num_peers = 0;
     while (isdigit(*j)) {
          num_peers = num_peers * 10 + ((*j) - '0');
          ++j; --i;
     }

     num_peers /= 6;
     /* skip the ':' */
     j++; i--;

     /* There's a possibility we didn't find any peers.
      * Not sure if this works, might need to check earlier.
      */
     if (num_peers == 0)
          return NULL;

#ifdef DEBUG
     printf("ANNOUNCE: %"PRIu64" peers.\n", num_peers);
#endif

     struct PeerNode *current;
     current = init_peer_node(init_peer(j, j + 4, t), NULL);
     struct PeerNode *first = current;
     j += 6;

     uint64_t k;
     for (k = 1; k < num_peers; k++) { 
          current->next = init_peer_node(init_peer(j, j + 4, t), NULL);
          current = current->next;
          j += 6;
     }

     return first;
}

static uint64_t 
get_interval(struct BDictNode *b)
{
     struct BEncode *output_value = find_value("interval", b);
     return output_value->cargo.bInt;
}

static void 
__announce(evutil_socket_t fd, short what, void *arg)
{
     /* TODO */
}

void 
announce(struct Torrent *t, int8_t event, CURL *connection, struct event_base *base)
{
     int32_t length = strlen(t->name) + strlen("/tmp/slug/-announce") + 1;
     char *filepath = malloc(length);
     char *url = construct_url(t, event);
     snprintf(filepath, length, "/tmp/slug/%s-announce", t->name);
     FILE *announce_data = fopen(filepath, "w");

     curl_easy_setopt(connection, CURLOPT_URL, url);
     curl_easy_setopt(connection, CURLOPT_WRITEDATA, announce_data);
     curl_easy_perform(connection);

     fclose(announce_data);
     announce_data = fopen(filepath, "r");

     /* read announce response into array */
     fseek(announce_data, 0, SEEK_END);
     int32_t pos = ftell(announce_data);
     fseek(announce_data, 0, SEEK_SET);
     char *data = malloc(pos);
     fread(data, pos, 1, announce_data);
     
     int64_t x = 0;
     struct BEncode *announceBEncode = parseBEncode(data, &x);

     if (announceBEncode->type != BDict)
          error("Announce returned invalid BEncode.");
     else if (failed(announceBEncode->cargo.bDict))
          pfailure_reason(announceBEncode->cargo.bDict);
     else {
          add_peers(t, get_peers(t, data));
          t->announce_interval = get_interval(announceBEncode->cargo.bDict);
     }



     free(data);
     free(filepath);
     freeBEncode(announceBEncode);
}
