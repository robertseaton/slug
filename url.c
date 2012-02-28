#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "includes.h"

static char 
from_hex(char c)
{
     return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

static char 
to_hex(char h)
{
     static char hex[] = "0123456789abcdef";
     return hex[h & 15];
}

char
*encode_url(char *s, char *end)
{
     char *str_pt = s;
     char *buffer = malloc((end - s) * 4 + 1);
     char *buffer_pt = buffer;

     while (str_pt <= end) {
          if (isalnum(*str_pt) || 
              *str_pt == '-'   || 
              *str_pt == '_'   || 
              *str_pt == '.'   || 
              *str_pt == '~')
               *buffer_pt++ = *str_pt;
          else if (*str_pt == ' ')
               *buffer_pt++ = '+';
          else {
               *buffer_pt++ = '%';
               *buffer_pt++ = to_hex(*str_pt >> 4);
               *buffer_pt++ = to_hex(*str_pt & 15);
          }
          str_pt++;
     }
     *buffer_pt = '\0';

     return buffer;
}


char
*construct_url(struct Torrent *t, int8_t event)
{
     char *peer_id = encode_url(t->peer_id, &t->peer_id[19]);
     char *info_hash = encode_url((char *)t->info_hash, (char *)&t->info_hash[19]);
     int32_t url_length = strlen(t->url) + strlen("?info_hash=") + 
          strlen(info_hash) + strlen("&peer_id=") + strlen(peer_id) + 
          strlen("&port=") + strlen("&uploaded=") + strlen("&downloaded=") + 
          strlen("&left=") + strlen("&compact=") + strlen("&event=completed") + 100;
     char *url = malloc(url_length);

#define NUMWANT 50   
     snprintf(url, url_length,
              "%s"
              "?info_hash=%s"
              "&peer_id=%s"
              "&port=%"PRIu64
              "&uploaded=%"PRIu64
              "&downloaded=%"PRIu64
              "&left=%"PRIu64
              "&compact=%d"
              "&numwant=%d",
              t->url,
              info_hash,
              peer_id,
              t->port,
              t->uploaded,
              t->downloaded,
              t->left,
              t->compact,
              50);

     if (event == STARTED)
          strcat(url, "&event=started");
     else if (event == STOPPED)
          strcat(url, "&event=stopped");
     else if (event == COMPLETED)
          strcat(url, "&event=completed");

     free(peer_id);
     free(info_hash);
     return url;
}

