#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "wrtctl-net.h"
#include "config.h"

bool verbose = false;
bool daemonize = true;

void usage() {
    printf("%s\n", PACKAGE_STRING);
    printf("wrtctld [ARGS]\n");
    printf("Optional Arguments:\n");
    printf("\t-h,--help                     This screen.\n");
    printf("\t-v,--verbose                  Toggle more verbose messages.\n");
    printf("\t-f,--foreground               Don't fork to the background.\n");
    printf("\t-p,--port <port>              Port to listen on.  Default: %s.\n", WRTCTLD_DEFAULT_PORT);
    printf("\t-m,--modules mod1,mod2...     Modules to load.\n");
    printf("\t-M,--modules_dir <path>       Directory containing modules.  Default: %s.\n",
        DEFAULT_MODULE_DIR);
    return;
}


 
/* server, port, modules, debug, daemonize, module_dir, verbose */
int main(int argc, char **argv){
    int rc = 0;
    char *modules = NULL;
    char *port = NULL;
    char *module_path = NULL;

    ns_t ns = NULL;

    while ( true ){
        int c;
        int oi = 0;
        static struct option lo[] = {
            { "help",       no_argument,        NULL,   'h'},
            { "port",       required_argument,  NULL,   'p'},
            { "modules",    required_argument,  NULL,   'm'},
            { "verbose",    no_argument,        NULL,   'v'},
            { "foreground", no_argument,        NULL,   'f'},
            { "modules_dir",required_argument,  NULL,   'M'},
            { 0,            0,                  0,      0}
        };

        c = getopt_long(argc, argv, "p:m:vfM:h", lo, &oi);
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
                if ( setenv("WRTCTL_MODULE_DIR", optarg, 1) != 0){
                    perror("setenv: ");
                    rc = errno;
                }
                break;
            case 'h':
                usage();
                goto shutdown;
                break;
            default:
                rc = EINVAL;
                break;
        }
        if ( rc != 0 ) break;
    }

    if ( !port )
        port = WRTCTLD_DEFAULT_PORT;
    
    if ( rc != 0 )
        exit(EXIT_FAILURE);
    
    if ( daemonize )
        openlog("wrtctld", LOG_PID, LOG_DAEMON);
    
    if ( (rc = create_ns(&ns, port, modules, daemonize, verbose && !daemonize)) != NET_OK ){
        fprintf(stderr, "create_ns: %s\n", net_strerror(rc));
        rc = EXIT_FAILURE;
        goto shutdown;
    }

    if ( modules )
        free(modules);
    if ( port )
        free(port);

    log("Daemon started.\n");
    rc = ns->server_loop(ns);
    err_rc(rc, "Daemon exiting, server_loop returned: %s\n", net_strerror(rc));
   

shutdown:
    if ( ns )
        free_ns(ns);
    if ( daemonize )
        closelog();
    exit(rc == NET_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}

