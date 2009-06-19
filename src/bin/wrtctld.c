#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "wrtctl-net.h"

bool verbose = false;
bool daemonize = true;

/* server, port, modules, debug, daemonize, module_dir, verbose */
int main(int argc, char **argv){
    int rc = 0;
    char *modules = NULL;
    char *port = NULL;

    ns_t ns = NULL;

    while ( true ){
        int c;
        int oi = 0;
        static struct option lo[] = {
            { "port",       required_argument,  NULL,   'p'},
            { "modules",    required_argument,  NULL,   'm'},
            { "verbose",    no_argument,        NULL,   'v'},
            { "foreground", no_argument,        NULL,   'f'},
            { "modules_dir",required_argument,  NULL,   'M'},
            { 0,            0,                  0,      0}
        };

        c = getopt_long(argc, argv, "p:m:vfM:", lo, &oi);
        if ( c == -1 ) break;

        switch (c) {
            case 'p':
                if ( !(port = strdup(optarg)) ){
                    perror("strdup: ");
                    rc = errno;
                }
                break;
            case 'm':
                if ( !(modules = strdup(optarg)) ){
                    perror("strdup: ");
                    rc = errno;
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'f':
                daemonize = false;
                break;
            case 'M':
                if ( setenv("WRTCTL_MODULE_DIR", optarg, 1) != 0 ){
                    perror("setenv: ");
                    rc = errno;
                }
                break;
            default:
                rc = EINVAL;
                break;
        }
        if ( rc != 0 ) break;
    }

    if ( !port && !(port = strdup("1337")) ){
        perror("strdup: ");
        rc = errno;
    }
 
    if ( rc != 0 )
        exit(EXIT_FAILURE);
       
    if ( (rc = create_ns(&ns, port, modules)) != NET_OK ){
        fprintf(stderr, "create_ns: %s\n", net_strerror(rc));
        rc = EXIT_FAILURE;
        goto shutdown;
    }

    if ( modules )
        free(modules);
    if ( port )
        free(port);

    ns->logfd = stderr;
    rc = ns->server_loop(ns);
   

shutdown:
    if( ns )
        free_ns(ns);
    exit(rc == NET_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}



