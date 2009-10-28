#ifndef __STUNNEL_H_
#define __STUNNEL_H_

#include <config.h>

#include <stdlib.h>
#include <unistd.h>

typedef struct stunnel_ctx {
    char *  conf_file_path;
    char *  pid_file_path;
    FILE *  cf;
    pid_t   pid;
} * stunnel_ctx_t;

/*  The following two functions wrap the configuration and forking of a stunnel
 *  instance.
 *      - hostname:     Host to for the client to connect to.
 *      - key_path:     Path to the shared certificate to accept on both sides.
 *      - port:         Port that the unencrypted daemon listens on.  Default is WRTCTL_DEFAULT_PORT.
 *      - wrtctl_port   Port for encrypted client to connect to locally.  Default is WRTCTL_SSL_PORT.
 *      - wrtctld_port  Port the encrypted daemon listens on remotely.  Default is WRTCTLD_SSL_PORT.
 */
int start_stunnel_client(
    stunnel_ctx_t * ctx,
    char *          hostname,
    char *          key_path,
    char *          port,
    char *          wrtctl_port,
    char *          wrtctld_port );

int start_stunnel_server(
    stunnel_ctx_t * ctx,
    char *          key_path,
    char *          port,
    char *          wrtctl_port,
    char *          wrtctld_port );

/*
 * Stop the stunnel process and cleanup all memory.
 */
int kill_stunnel( stunnel_ctx_t *ctx );
#endif
