#include "includes.h"

int failed (struct BDictNode* b)
{
     struct BEncode* output_value = find_value("failure reason", b);
     
     if (output_value != NULL)
          return 0;
     
     return -1;
}

void pfailure_reason (struct BDictNode* b)
{
     struct BEncode* output_value = find_value("failure reason", b);
     fprintf(stderr, "Announce failed: %s.\n", output_value->cargo.bStr);
}

struct Peer** get_peers (struct Torrent* t, char* data)
{
     int i = 0;
     char* j;
     uint64_t num_peers = 0;
#define PEERS_STR "5:peers"
     j = strstr(data, PEERS_STR);
     j += strlen(PEERS_STR);
     
     free(parseBEncode(j, &i));

     while (isdigit(*j)) {
          num_peers = num_peers * 10 + ((*j) - '0');
          ++j; --i;
     }
     num_peers /= 6;
     /* skip the ':' */
     j++; i--;

     struct Peer** list = malloc(sizeof(Peer*) * num_peers);

     uint64_t k;
     for (k = 0; k < num_peers; k++) { 
          list[k] = init_peer(t, j, j + 4);
          j += 6;
     }

     return list;
}

uint64_t get_interval (struct BDictNode* b)
{
     struct BEncode* output_value = find_value("interval", b);
     return output_value->cargo.bInt;
}

void announce (struct Torrent* t, int event, CURL* connection, struct event_base* base)
{
     int32_t length = strlen(t->name) + strlen("/tmp/slug/-announce") + 1;
     char* file_path = malloc(length);
     
     snprintf(file_path, length, "/tmp/slug/%s-announce", t->name);
     FILE* announce_data = fopen(file_path, "w");

     curl_easy_setopt(connection, CURLOPT_URL, url);
     curl_easy_setopt(connect, CURLOPT_WRITEDATA, announce_data);
     curl_easy_perform(connection);

     fclose(announce_data);
     announce_data = fopen(filepath, "r");

     /* read announce response into array */
     fseek(announce_data, 0, SEEK_END);
     int32_t pos = ftell(announce_data);
     fseek(announce_data, 0, SEEK_SET);
     char* data = malloc(pos);
     fread(data, pos, 1, announce_data);
     
     int x = 0;
     struct BEncode* announceBEncode = parseBEncode(data, &x);

     if (announceBencode->type != BDict)
          error("Announce returned invalid BEncode.");
     else if (failed(announceBEncode->cargo))
          pfailure_reason(announceBEncode->cargo);
     else {
          add_peers(t, get_peers(struct Torrent* t, char* data));
          t->announce_interval = get_interval(announceBEncode->cargo);
     }

     
     free(data);
     free(filepath);
     freeBEncode(announceBEncode);
}
