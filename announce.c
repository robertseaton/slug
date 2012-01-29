/*
 * announce.c
 * handle announcing to tracker
 * * */

#include <ctype.h>
#include <curl/curl.h>
#include <event2/event.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <syslog.h>
#include <assert.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <math.h>

#include "includes.h"

struct Tracker {
     char *protocol;
     char *host;
     int port;
     int socket;
     struct sockaddr *addr;
     socklen_t addrlen;
     uint64_t connection_id;
};

struct AnnounceRequest {
     int64_t connection_id;
     int32_t action;
     int32_t transaction_id;
     uint8_t info_hash[20];
     char peer_id[20];
     uint64_t downloaded;
     uint64_t left;
     uint64_t uploaded;
     uint32_t event;
     uint32_t ip;
     uint32_t key;
     uint32_t num_want;
     uint16_t port;
};

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
     syslog(LOG_WARNING, "Announce failed: %s\n", output_value->cargo.bStr);
}

void 
get_peers(struct Torrent *t, char *data)
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

     syslog(LOG_DEBUG, "ANNOUNCE: %"PRIu64" peers.\n", num_peers);

     uint32_t ip;
     uint16_t port;

     int k;
     for (k = 0; k < num_peers; k++) {
          memcpy(&ip, j, sizeof(ip));
          j += sizeof(ip);
          memcpy(&port, j, sizeof(port));
          j += sizeof(port);

          if (t->peer_list == NULL)
               t->peer_list = init_peer_node(init_peer(ip, port, t), NULL);
          else
               add_peer(&t->peer_list, init_peer(ip, port, t));
     }
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
http_announce(struct Torrent *t, int8_t event, CURL *connection, struct event_base *base)
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

     assert(announceBEncode->type == BDict);
     if (failed(announceBEncode->cargo.bDict))
          pfailure_reason(announceBEncode->cargo.bDict);
     else {
          get_peers(t, data);
          t->announce_interval = get_interval(announceBEncode->cargo.bDict);
     }

     free(data);
     free(filepath);
     freeBEncode(announceBEncode);
}

struct Tracker
*construct_tracker(char* url)
{
     struct Tracker *t;
     char *urlcp, *port, *p;
     uint8_t protocol_length, url_length;
     struct addrinfo hints, *res;

     /* This is some gnarly string mangling. 
        Example input: "udp://tracker.openbittorrent.com:80/announce"
        Output: t->protocol = "udp"
                t->url = "tracker.openbittorrent.com"
                t->port = 80 */
     t = malloc(sizeof(struct Tracker));
     t->port = 0;
     urlcp = malloc(strlen(url) + 1);
     strcpy(urlcp, url);
     p = strstr(urlcp, ":");
     protocol_length = p - urlcp;
     *p = '\0';
     t->protocol = urlcp;
     urlcp += protocol_length + 3;
     p = strstr(urlcp + protocol_length, ":");
     url_length = p - urlcp;
     *p = '\0';
     t->host = urlcp;
     urlcp += url_length + 1;
     p = urlcp;
     port = urlcp;

     while (isdigit(*p))
          t->port = t->port * 10 + (*p++ - '0');

     *p = '\0';

     if (t->port == 0)
          t->port = 80;

     if (strcmp(t->protocol, "udp") == 0) {
          memset(&hints, 0, sizeof(struct addrinfo));
          hints.ai_family = AF_INET;
          hints.ai_socktype = SOCK_DGRAM;
          hints.ai_flags = 0;
          hints.ai_protocol = IPPROTO_UDP;
          hints.ai_canonname = NULL;
          hints.ai_addr = NULL;
          hints.ai_next = NULL;

          if (getaddrinfo(t->host, port, &hints, &res) != 0) {
               syslog(LOG_ERR, "Failed to resolve address of tracker.\n");
               exit(EXIT_FAILURE);
          }

          t->socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
          t->addr = res->ai_addr;
          t->addrlen = res->ai_addrlen;
     }

     return t;
}

int
udp_send_receive(struct Tracker *t, void *message, int message_len, void *response, int response_len, int *amount_read)
{
     int attempts;
     fd_set *set = malloc(sizeof(fd_set));
     struct timeval timeout;

     FD_ZERO(set);
     FD_SET(t->socket, set);

     for (attempts = 0; attempts <= MAX_RETRIES; attempts++) {
          timeout.tv_sec = 15 * pow(2, attempts);
          timeout.tv_usec = 0;
          if (message_len != 0) {
               if (sendto(t->socket, message, message_len, 0, t->addr, t->addrlen) == -1)
                    return UDP_SEND_FAILURE;
          }
          if (select(t->socket + 1, set, NULL, NULL, &timeout) == 1) {
               if (amount_read != NULL)
                    *amount_read = recv(t->socket, response, response_len, 0);
               else
                    recv(t->socket, response, response_len, 0);
               return SUCCESS;
          }
     }

     return UDP_RECEIVE_FAILURE;
}

int
udp_tracker_connect(struct Tracker *t)
{     
     int8_t payload[16];
     int8_t response[16];
     int64_t connection_id = htobe64(0x41727101980);
     int32_t action = htonl(0);
     int32_t transaction_id = htonl(rand());

     /* construct the message */
     memcpy(&payload[0], &connection_id, sizeof(connection_id));
     memcpy(&payload[8], &action, sizeof(action));
     memcpy(&payload[12], &transaction_id, sizeof(transaction_id));

     fcntl(t->socket, F_SETFL, O_NONBLOCK);

     if (udp_send_receive(t, payload, 16, response, 16, NULL) != SUCCESS)
          return ANNOUNCE_FAILURE;

     int32_t recv_action, recv_transaction_id;
     int64_t recv_connection_id;

     memcpy(&recv_action, &response[0], sizeof(recv_action));
     memcpy(&recv_transaction_id, &response[4], sizeof(recv_transaction_id));
     memcpy(&recv_connection_id, &response[8], sizeof(recv_connection_id));

     if (transaction_id != recv_transaction_id || action != recv_action)
          return ANNOUNCE_FAILURE;

     t->connection_id = recv_connection_id;

     return SUCCESS;
}

struct AnnounceRequest
*udp_construct_announce(struct Torrent *t, int32_t action, int32_t event, int32_t ip, int32_t num_want)
{
     struct AnnounceRequest *ar = malloc(sizeof(struct AnnounceRequest));

     ar->connection_id = t->tracker->connection_id;
     ar->action = htonl(action);
     ar->transaction_id = htonl(rand());
     memcpy(ar->peer_id, t->peer_id, 20);
     memcpy(ar->info_hash, t->info_hash, 20);
     ar->downloaded = htobe64(t->downloaded);
     ar->uploaded = htobe64(t->uploaded);
     ar->left = htobe64(t->left);
     ar->key = 0;
     ar->port = htons(t->port);
     ar->event = htonl(event);

     if (ip == 0)
          ar->ip = 0;
     else
          ar->ip = htonl(ip);

     if (num_want == 0)
          ar->num_want = DEFAULT_PEERS_WANTED;
     else
          ar->num_want = htonl(num_want);

     return ar;
}

void
udp_get_peers(int8_t* data, int length, struct Torrent* t)
{
     uint32_t ip;
     uint16_t port;
     int num_peers;

     num_peers = length / (sizeof(ip) + sizeof(port));

     int i;
     int j = 0;
     for (i = 0; i < num_peers; i++) {
          memcpy(&ip, &data[j], sizeof(ip));
          j += sizeof(ip);
          memcpy(&port, &data[j], sizeof(port));
          j += sizeof(port);

          if (t->peer_list == NULL)
               t->peer_list = init_peer_node(init_peer(ip, port, t), NULL);
          else
               add_peer(&t->peer_list, init_peer(ip, port, t));
     }
}

int
udp_announce(struct Torrent *t)
{
     struct AnnounceRequest *ar = udp_construct_announce(t, ANNOUNCE, 0, 0, 0);
     int8_t announce_response[4096];
     int amount_read;

     if (udp_send_receive(t->tracker, ar, sizeof(announce_response), announce_response, 4096, &amount_read) != SUCCESS) {
          syslog(LOG_WARNING, "Announce failure.");
          exit(EXIT_FAILURE);
     }

     int32_t recv_action, recv_transaction_id;
     int32_t interval, leechers, seeders;
     memcpy(&recv_action, &announce_response[0], sizeof(recv_action));
     memcpy(&recv_transaction_id, &announce_response[4], sizeof(recv_transaction_id));
     memcpy(&interval, &announce_response[8], sizeof(interval));
     memcpy(&leechers, &announce_response[12], sizeof(leechers));
     memcpy(&seeders, &announce_response[16], sizeof(seeders));
     
     if (recv_action != ar->action || recv_transaction_id != ar->transaction_id) {
          syslog(LOG_WARNING, "Announce failed.");
          return ANNOUNCE_FAILURE;
     }
     
     seeders = ntohl(seeders);
     leechers = ntohl(leechers);
     interval = ntohl(interval);
     udp_get_peers(&announce_response[20], amount_read - 20, t);
     syslog(LOG_WARNING, "amount read: %d", amount_read);
     syslog(LOG_WARNING, "Seeders: %d, Leechers: %d, Interval: %d\n", seeders, leechers, interval);
}

void 
announce(struct Torrent *t, int8_t event, CURL *connection, struct event_base *base)
{
     t->tracker = construct_tracker(t->url);

     if (strncmp(t->url, "udp:", 4) == 0) {
          if (udp_tracker_connect(t->tracker) == ANNOUNCE_FAILURE) {
               syslog(LOG_ERR, "Failed to connect to tracker.");
               exit(EXIT_FAILURE);
          }
          syslog(LOG_WARNING, "success");
          udp_announce(t);
     } else if (strncmp(t->url, "http:", 5) == 0)
          http_announce(t, event, connection, base);
     else {
          syslog(LOG_WARNING, "Unrecognized tracker protocol. Trying HTTP.");
          http_announce(t, event, connection, base);
     }
}
