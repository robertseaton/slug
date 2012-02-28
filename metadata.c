#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>

#include "includes.h"

/* takes a metadata BEncoded dictionary and returns the url */ 
char
*get_url(struct BDictNode *b)
{
     char *url;
     struct BEncode *output_value = find_value("announce", b);
     
     assert(output_value != NULL && output_value->type == BString);
     url = malloc(strlen(output_value->cargo.bStr) + 1); 
     strcpy(url, output_value->cargo.bStr);

     return url;          
}

/* takes a dictionary of BEncoded metadata and returns the piece length */
uint64_t 
get_piece_length(struct BDictNode *b)
{
     struct BEncode *output_value = find_value("piece length", b);

     assert(output_value != NULL && output_value->type == BInt);
 
     return output_value->cargo.bInt;
          
}

/* takes the metadata as a string and returns the pieces */
void 
get_pieces(struct MinBinaryHeap *pieces, uint64_t *num_pieces, char *data)
{
     char *j;
     *num_pieces = 0;
     
#define PIECE_STR "6:pieces"
     j = strstr(data, PIECE_STR);
     assert(j != NULL);

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
     assert(*num_pieces);

     heap_initialize(pieces, *num_pieces);
     
     uint64_t i;
     for (i = 0; i < *num_pieces; ++i) {
          struct Piece *piece = malloc(sizeof(struct Piece));

          uint64_t k;
          for (k = 0; k < CHARS_IN_PIECE; ++k) {
               piece->sha1[k] = (*j); 
               j++;
          }
          init_piece(piece, i);
          heap_insert(pieces, *piece, &compare_priority);
     }
}

/* Takes a BEncoded metadata dictionary and returns 1 if private, 0 otherwise. */
uint8_t
is_private(struct BDictNode *b)
{
     struct BEncode *output_value;

     output_value = find_value("private", b);
     
     if (output_value != NULL && output_value->type == BInt && output_value->cargo.bInt == 1)
          return 1;
     else
          return 0;
}

char 
*get_name(struct BDictNode *b)
{
     char *name;
     struct BEncode *output_value;

     output_value = find_value("name", b);
     assert(output_value != NULL && output_value->type == BString);
     name = malloc(strlen(output_value->cargo.bStr) + 1);
     strcpy(name, output_value->cargo.bStr);

     return name;
}

uint64_t 
get_length(struct BDictNode *b)
{
     struct BEncode *output_value;

     output_value = find_value("length", b);

      if (output_value == NULL) {
          /* multi-file torrent */
           output_value = find_value("files", b);
          
          /* Debugging this is great because you get to inspect values like :
             output_value.cargo.bList.cargo.cargo.bDict.value.cargo.bInt 
             I HAVE MADE A POOR DECISION SOMEWHERE, COMPUTER   */
           uint64_t length = (find_value("length", output_value->cargo.bList->cargo->cargo.bDict))->cargo.bInt;
           struct BListNode *next = output_value->cargo.bList->next;
           while (next != NULL) {
                length += (find_value("length", next->cargo->cargo.bDict))->cargo.bInt;
                next = next->next;
           }
           
           return length;
      } else 
           return output_value->cargo.bInt;
}

char 
*get_path(struct BDictNode *b, char* folder)
{
     char *path;
     struct BEncode *output_value = find_value("path", b);

     assert(output_value != NULL && output_value->type == BList);
    
     struct BListNode *head = output_value->cargo.bList;
     struct BListNode *next = output_value->cargo.bList;
     size_t i;

     if (folder != NULL)
          i = strlen(folder) + 1;
     else
          i = 0;

     while (next) {
          if (next->cargo->type == BString)
               i += sizeof(char) * (strlen(next->cargo->cargo.bStr) + 2);
          next = next->next;
     }
          
     /* make sure we don't malloc 0 */
     assert(i);

     path = malloc(i);
     *path = '\0'; /* for strcat */
     next = head;

     if (folder != NULL) {
          strcat(path, folder);
          strcat(path, "/");
     }
 
     while (next) {
          if (next->cargo->type == BString) {
               strcat(path, next->cargo->cargo.bStr);
               strcat(path, "/");
          }
          next = next->next;
     }

     /* remove the final trailing / */
     path[strlen(path) - 1] = '\0';
     return path;
}

/* Checks if a torrent is a single file torrent. */
uint8_t
is_single_file_torrent(char *data)
{
     /* only multi-file metadata dictionaries contain the files key */
     if (strstr(data, "5:files") != NULL)
          return 0;
     else
          return 1;
}

/* returns the SHA1 hash of the info dictionary */
uint8_t
*get_info_hash(char *data)
{
     SHA_CTX sha;
     int64_t i = 0;
     char *j;
     uint8_t *info_hash;

     info_hash = malloc(sizeof(uint8_t) * 20);
#define INFO_STR "4:info"
     j = strstr(data, INFO_STR);
     
     assert(j != NULL);

     j += strlen(INFO_STR);
     freeBEncode(parseBEncode(j, &i));
     SHA1_Init(&sha);
     SHA1_Update(&sha, j, (int)i);
     SHA1_Final(info_hash, &sha);

     return info_hash;
}

/* returns the randomly generated peerID for the torrent */
char
*set_peer_id(double rand)
{
     static char peer_id[20];

     strcpy(peer_id, "SL");
     strcpy(&peer_id[2], "0000");
     gcvt(rand, 14, &peer_id[6]);

     return peer_id;
}

struct TorrentFile
*init_files(struct BDictNode *b, char* folder)
{
     struct BEncode *value = find_value("files", b);
     struct BListNode *next;
     struct TorrentFile *files;

     int num_files;
     for (num_files = 0, next = value->cargo.bList; next != NULL; next = next->next)
          num_files++;

     files = malloc(sizeof(struct TorrentFile) * num_files);

     int i;
     void *addr = NULL;
     FILE *f;
     char *p;
     for (i = 0, next = value->cargo.bList; next != NULL; next = next->next, i++) {
          files[i].path = get_path(next->cargo->cargo.bDict, folder);
          p = strrchr(files[i].path, '/');
          *p = '\0';
          mkpath(files[i].path, S_IRWXU);
          *p = '/';
          files[i].length = get_length(next->cargo->cargo.bDict);
          if ((f = fopen(files[i].path, "w+")) == NULL)
               perror('\0');
          files[i].fd = fileno(fopen(files[i].path, "w+"));
          fseek(fdopen(files[i].fd, "w+"), files[i].length - 1, SEEK_SET);
          write(files[i].fd, "", 1);
          files[i].mmap = mmap(0, files[i].length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, 
                               files[i].fd, 0);
          if (addr == NULL)
               addr = files[i].mmap;
          addr += files[i].length;
     }

     return files;
}

struct Torrent
*init_torrent(FILE *stream, double peer_id, double port)
{
     static struct Torrent t;
     
     /* This reads a file into an array. If it looks retarded, that's because it
        is. */
     fseek(stream, 0, SEEK_END);
     long pos = ftell(stream);
     fseek(stream, 0, SEEK_SET);
     char *data = malloc(pos);
     fread(data, pos, 1, stream);

     int64_t x = 0;
     struct BEncode *bEncodedDict = parseBEncode(data, &x);

     if (bEncodedDict->type == BDict) {
          struct BEncode *output_value;
          
          output_value = find_value("info", bEncodedDict->cargo.bDict);
          assert(output_value != NULL && output_value->type == BDict);
          
          t.pieces = malloc(sizeof(struct MinBinaryHeap));
          t.downloading = malloc(sizeof(struct MinBinaryHeap));
          t.is_single = is_single_file_torrent(data);
          t.announce_interval = DEFAULT_ANNOUNCE;
          t.name = get_name(output_value->cargo.bDict);
          t.length = get_length(output_value->cargo.bDict);
          t.piece_length = get_piece_length(output_value->cargo.bDict);
          get_pieces(t.pieces, &t.num_pieces, data);
          heap_initialize(t.downloading, t.num_pieces);
          t.url = get_url(bEncodedDict->cargo.bDict);
          t.info_hash = (uint8_t *)get_info_hash(data);
          t.peer_id = set_peer_id(peer_id);
          t.port = port;
          t.compact = 1;
          t.uploaded = 0;
          t.downloaded = 0;
          t.left = t.num_pieces * t.piece_length;
          t.global_bitfield = init_global_bitfield(t.num_pieces);
          t.have_bitfield = init_have_bitfield(t.num_pieces);
          t.peer_list = NULL; /* got to initialize this */
          
          if (t.is_single) {
               struct TorrentFile *f = &t.torrent_files.single.file;
               f->fd = fileno(fopen(t.name, "w+"));
               f->length = t.length;
               /* write a dummy byte for MMAPing */
               fseek(fdopen(f->fd, "w+"), t.length - 1, SEEK_SET);
               write(f->fd, "", 1);

               f->mmap = mmap(0, t.length, PROT_READ | PROT_WRITE, MAP_SHARED, 
                              f->fd, 0);
               t.mmap = f->mmap;
          } else {
               t.torrent_files.multi.files = init_files(output_value->cargo.bDict, t.name);
               t.mmap = t.torrent_files.multi.files[0].mmap;
          }
     } else {
          syslog(LOG_ERR, "Failed to parse metadata.");
          exit(1);
     }
     
     free(data);
     freeBEncode(bEncodedDict);
     return &t;
}
