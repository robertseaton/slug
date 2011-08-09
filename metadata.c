#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#include "includes.h"

/* takes a metadata BEncoded dictionary and returns the url */ 
char* get_url (struct BDictNode* b)
{
     char* url;
     char input_key[9] = "announce";
     struct BEncode* output_value = find_value(input_key, b);
     
     if (output_value == NULL || output_value->type != BString)
          error("Failed to get URL from metadata.");
     else {
          url = malloc(strlen(output_value->cargo.bStr) + 1);
          strcpy(url, output_value->cargo.bStr);
     }

     return url;          
}

/* takes a dictionary of BEncoded metadata and returns the piece length */
uint64_t get_piece_length (struct BDictNode* b)
{
     char input_key[13] = "piece length";
     struct BEncode* output_value = find_value(input_key, b);

     if (output_value == NULL || output_value->type != BInt)
          error("Failed to get piece length from metadata.");
 
          return output_value->cargo.bInt;
          
}

/* takes the metadata as a string and returns the pieces */
void get_pieces (struct Piece* pieces, uint64_t* num_pieces, char* data)
{
     char* j;
     *num_pieces = 0;
     
#define PIECE_STR "6:pieces"
     j = strstr(data, PIECE_STR);
     if (j == NULL)
          error("Failed to get pieces from metadata");

     j += strlen(PIECE_STR);
     while (isdigit(*j))  {
          *num_pieces = *num_pieces * 10 + ((*j) - '0');
          j++;
     }
     /* skip the ':' */
     j++;

#define CHARS_IN_PIECE 20
     *num_pieces = *num_pieces / CHARS_IN_PIECE;

     /* make sure we don't malloc 0 */
     if (*num_pieces == 0)
          error("Parsing metadata returned an invalid number of pieces.");

     pieces = malloc(sizeof(struct Piece) * *num_pieces);
     
     uint64_t i;
     for (i = 0; i < *num_pieces; ++i) {
          
          uint64_t k;
          for (k = 0; k < CHARS_IN_PIECE; ++k) {
               pieces[i].sha1[k] = (*j);
               j++;
          }
          init_piece(pieces[i], i);
     }
}

/* takes a BEncoded metadata dictionary and returns 1 if private, 0 otherwise */
int is_private (struct BDictNode* b)
{
     char input_key[8] = "private";
     struct BEncode* output_value = find_value(input_key, b);
     
     if (output_value != NULL && output_value->type == BInt && output_value->cargo.bInt == 1)
          return 1;
     else
          return 0;
}

char* get_name (struct BDictNode* b)
{
     char* name;
     char input_key[5] = "name";
     struct BEncode* output_value = find_value(input_key, b);
     
     if (output_value == NULL || output_value->type != BString)
          error("Failed to get torrent's name from metadata.");
     else {
          name = malloc(strlen(output_value->cargo.bStr) + 1);
          strcpy(name, output_value->cargo.bStr);
     }

     return name;
}

uint64_t get_length (struct BDictNode* b)
{
     char input_key[7] = "length";
     struct BEncode* output_value = find_value(input_key, b);

     if (output_value == NULL || output_value->type != BInt)
          error("Failed to get torrent's length from metadata.");
     
          return output_value->cargo.bInt;
}

char* get_path (struct BDictNode* b)
{
     char* path;
     char input_key[5] = "path";
     struct BEncode* output_value = find_value(input_key, b);

     if (output_value == NULL || output_value->type != BList)
          error("Failed to get torrent's path from metadata");
     else {
          struct BListNode* head = output_value->cargo.bList;
          struct BListNode* next = output_value->cargo.bList;
          size_t i = 0;

          while (next) {
               if (next->cargo->type == BString)
                    i += sizeof(char) * (strlen(next->cargo->cargo.bStr) + 1);
               next = next->next;
          }
          
          /* make sure we don't malloc 0 */
          if (i == 0)
               error("Parsing metadata returned an invalid value for path length.");

          path = malloc(i);
          next = head;

          while (next) {
               if (next->cargo->type == BString) {
                    strcat(path, next->cargo->cargo.bStr);
                    strcat(path, "/");
               }
               next = next->next;
          }
     }

     return path;
}

/* checks if a torrent is a single file torrent from the metadata */
int is_single_file_torrent (char* data)
{
     /* only multi-file metadata dictions contain the files key */

     if (strstr(data, "5:files") != NULL)
          return 0;
     else
          return 1;
}

/* returns the SHA1 hash of the info dictionary */
unsigned char* get_info_hash (char* data)
{
     SHA_CTX sha;
     int64_t i = 0;
     char* j;
     unsigned char* info_hash;
     info_hash = malloc(sizeof(unsigned char) * 20);

#define INFO_STR "4:info"
     j = strstr(data, INFO_STR);
     
     if (j == NULL)
          error("Failed to get info hash from torrent's metadata.");

     j += strlen(INFO_STR);

     freeBEncode(parseBEncode(j, &i));
     SHA1_Init(&sha);
     SHA1_Update(&sha, j, (int)i);
     SHA1_Final(info_hash, &sha);

     return info_hash;
}

/* returns the randomly generated peerID for the torrent */
char* set_peer_id (double rand)
{
     static char peer_id[20];
     strcpy(peer_id, "SL");
     strcpy(&peer_id[2], "0000");
     gcvt(rand, 14, &peer_id[6]);

     return peer_id;
}

struct Torrent* init_torrent (FILE* stream, double peer_id, double port)
{
     struct Torrent* t = malloc(sizeof(struct Torrent));
     
     fseek(stream, 0, SEEK_END);
     long pos = ftell(stream);
     fseek(stream, 0, SEEK_SET);

     char* data = malloc(pos);
     fread(data, pos, 1, stream);

     int64_t x = 0;
     struct BEncode* bEncodedDict = parseBEncode(data, &x);

     if (bEncodedDict->type == BDict) {
          t->url = get_url(bEncodedDict->cargo.bDict);

          char input_key[5] = "info";
          struct BEncode* output_value = find_value(input_key, bEncodedDict->cargo.bDict);

          if (output_value == NULL || output_value->type != BDict)
               error("Failed to get torrent's metadata.");
          
          t->announce_interval = DEFAULT_ANNOUNCE;
          t->name = get_name(output_value->cargo.bDict);
          t->length = get_length(output_value->cargo.bDict);
          t->piece_length = get_piece_length(output_value->cargo.bDict);
          get_pieces(t->pieces, &t->num_pieces, data);
          t->url = get_url(bEncodedDict->cargo.bDict);
          t->info_hash = (unsigned char *)get_info_hash(data);
          t->peer_id = set_peer_id(peer_id);
          t->port = port;
          t->compact = 1;
          t->uploaded = 0;
          t->downloaded = 0;
          t->left = t->num_pieces * t->piece_length;
          t->global_bitfield = init_global_bitfield(t->num_pieces);
          t->have_bitfield = init_have_bitfield(t->num_pieces);
          freeBEncode(bEncodedDict);
          free(data);
     } else
          error("Failed to parse metadata.");
     
     return t;
}
