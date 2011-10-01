#include <stdlib.h>
#include <string.h>
#ifdef DEBUG
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "includes.h"

/* prints a string to stderr then exits 
 * TODO: replace with logging lib
 * * */
void 
error (char* s)
{
     fprintf(stderr, "%s\n", s);
     exit(1);
}

/* takes a key and a BEncoded dictionary node and searches for
 * the corresponding value
 * * */
struct BEncode* 
find_value (char* key, struct BDictNode* d)
{
     struct BDictNode* pt = d;
     struct BDictNode* next = d;
     
     while (next) {
          next = pt->next;
          if (strncmp(pt->key, key, strlen(key)) == 0)
               return pt->value;
          pt = next;
     }
     return NULL;
}

void 
freeBList (struct BListNode* l)
{
     struct BListNode* pt = l;
     struct BListNode* next = l;

     while (next) {
          next = pt->next;
          freeBEncode(pt->cargo);
          free(pt);
          pt = next;
     }
}

void 
freeBDict (struct BDictNode* d)
{
     struct BDictNode* pt = d;
     struct BDictNode* next = d;

     while (next) {
          next = pt->next;
          free(pt->key);
          freeBEncode(pt->value);
          free(pt);
          pt = next;
     }
     return;
}
void 
freeBEncode (struct BEncode* b)
{
     if (b->type == BInt) {
          free(b);

          return;
     }

     if (b->type == BString) {
          free(b->cargo.bStr);
          free(b);

          return;
     }

     if (b->type == BList) {
          freeBList(b->cargo.bList);
          free(b);

          return;
     }

     if (b->type == BDict) {
          freeBDict(b->cargo.bDict);
          free(b);

          return;
     }
}

void 
print_sha1 (uint8_t* sha1)
{
     int i;
     for (i = 0; i < 20; i++)
               printf("%d ", sha1[i]);
     printf("\n");
}

#ifdef DEBUG
/* saves piece from known-good file to a file
 * for later comparison with the output of 
 * the file from write_incorrect_piece
 * * */
void 
write_correct_piece (uint32_t piece_length, uint32_t index)
{
     FILE* known_good = fopen("known_good.iso", "r+");
     FILE* correct_piece = fopen("correct_piece.out", "w+");

     /* get file size for MMAPing */
     struct stat st;
     stat("known_good.iso", &st);
     long size = st.st_size;

     void* addr = mmap(0,
                       size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED,
                       fileno(known_good),
                       0);

     write(fileno(correct_piece), addr + piece_length * index, piece_length);
     munmap(addr, size);
     fclose(known_good);
     fclose(correct_piece);
}


/* saves piece that failed verification to a file
 * for later inspection with the likes of hexdump.
 * only available if built with -DDEBUG
 * * */
void 
write_incorrect_piece (void* piece, uint32_t piece_length, uint32_t index)
{
     FILE* failed_piece = fopen("failed_piece.out", "w+");
     
     write(fileno(failed_piece), piece, piece_length);
     fclose(failed_piece);
     printf("ERROR: Wrote piece #%d to failed_piece.out for inspection with hexdump.\n",
            index);

     write_correct_piece(piece_length, index);
     exit(1);
}
#endif
