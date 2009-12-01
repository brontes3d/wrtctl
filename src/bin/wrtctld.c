/*
 * Copyright (c) 2009, 3M
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the 3M nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Justin Bronder <jsbronder@brontes3d.com>
 */

#include <config.h>

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include "wrtctl-net.h"

bool verbose = false;
bool daemonize = true;

void usage() {
    printf("%s\n", PACKAGE_STRING);
    printf("wrtctld [ARGS]\n");
    printf("Optional Arguments:\n");
    printf("\t-h,--help                     This screen.\n");
    printf("\t-v,--verbose                  Toggle more verbose messages.\n");
    printf("\t-f,--foreground               Don't fork to the background.\n");
    printf("\t-p,--port <port>              Port to listen on [%s].\n", WRTCTLD_DEFAULT_PORT);
    printf("\t-m,--modules mod1,mod2...     Modules to load.\n");
    printf("\t-M,--modules_dir <path>       Directory containing modules [%s].\n",
        DEFAULT_MODULE_DIR);
#ifdef ENABLE_STUNNEL
    printf("\nSSL Optional Arguments:\n");
    printf("\t-C,--ssl_client <port>        Port for local stunnel wrapper [%s].\n", WRTCTL_SSL_PORT);
    printf("\t-S,--ssl_server <port>        Port for remote stunnel wrapper [%s].\n", WRTCTLD_SSL_PORT);
    printf("\t-k,--key_path <path>          Path to shared SSL certificate [%s].\n", DEFAULT_KEY_PATH);
#endif
 
    return;
}


 
/* server, port, modules, debug, daemonize, module_dir, verbose */
int main(int argc, char **argv){
    int rc = 0;
    char *modules = NULL;
    char *port = NULL;
    ns_t ns = NULL;

#ifdef ENABLE_STUNNEL
    char *key_path = NULL, *client_ssl_port = NULL, *server_ssl_port = NULL;
    stunnel_ctx_t stunnel_ctx = NULL;
#endif

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
#ifdef ENABLE_STUNNEL
            { "ssl_client", required_argument,  NULL,   'C'},
            { "ssl_server", required_argument,  NULL,   'S'},
            { "key_path",   required_argument,  NULL,   'k'},
#endif
            { 0,            0,                  0,      0}
        };

#ifdef ENABLE_STUNNEL
        c = getopt_long(argc, argv, "p:m:vfM:hC:S:k:", lo, &oi);
#else
        c = getopt_long(argc, argv, "p:m:vfM:h", lo, &oi);
#endif
        if ( c == -1 ) break;

        switch (c) {
            case 'p':
                port = optarg;
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
#ifdef ENABLE_STUNNEL
            case 'C':
                client_ssl_port = optarg;
                break;
            case 'S':
                server_ssl_port = optarg;
                break;
            case 'k':
                key_path = optarg;
                break;
#endif
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
#ifdef ENABLE_STUNNEL
    if ( !key_path )
        key_path = DEFAULT_KEY_PATH;

    if ( (rc = start_stunnel_server(
            &stunnel_ctx,
            key_path,
            port,
            client_ssl_port,
            server_ssl_port)) != 0 ){
        rc = EXIT_FAILURE;
        fprintf(stderr, "Failed to start the stunnel wrapper.\n");
        goto shutdown;
    }
#endif

    log("Daemon started.\n");
    rc = ns->server_loop(ns);
    err_rc(rc, "Daemon exiting, server_loop returned: %s\n", net_strerror(rc));
   

shutdown:
    if ( ns )
        free_ns(&ns);
    if ( daemonize )
        closelog();
    exit(rc == NET_OK ? EXIT_SUCCESS : EXIT_FAILURE);
}

