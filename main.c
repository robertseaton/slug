#include <curl/curl.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "includes.h"

#define RANDOM_MAX 9999999999999

int main () {

     /* house keeping */
     srandom(time(NULL));
     curl_global_init(CURL_GLOBAL_ALL);
     double peer_id = rand() % RANDOM_MAX + pow(10, 13);
     mkdir("/tmp/slug", S_IRWXU);
     signal(SIGPIPE, SIG_IGN);

     start_torrent("ubuntu.torrent", peer_id, PORT);
     return 0;
}
