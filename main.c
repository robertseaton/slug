#include <curl/curl.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>

#include "includes.h"

#define RANDOM_MAX 9999999999999

int 
main(int argc, char* argv[]) 
{
     if (argc != 2) {
          printf("usage: slug [file]\n");
          exit(0);
     }

     /* house keeping */
     srandom(time(NULL));
     curl_global_init(CURL_GLOBAL_ALL);
     double peer_id = rand() % RANDOM_MAX + pow(10, 13);
     mkdir("/tmp/slug", S_IRWXU);
     signal(SIGPIPE, SIG_IGN);
     openlog("slug", LOG_PERROR, 0);

     start_torrent(argv[1], peer_id, PORT);
     return 0;
}
