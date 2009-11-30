#include <config.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <uci.h>
#include "wrtctl-net.h"

#ifdef NDEBUG
#define TIMEOUT 10
#else
#define TIMEOUT 1000
#endif

#define MAX_LINE 1024

bool verbose = false;

int client_loop(nc_t nc, FILE *cmds_fp) {
    char *          line = NULL;
    ssize_t         line_len = 0;
    int             rc = 0;
    packet_t        sp, rp;
    struct timeval  to = { TIMEOUT, 0 };
    struct net_cmd  ncmd;
    int             line_cnt = 0;
    size_t          n;

        
    memset( &ncmd, 0, sizeof(struct net_cmd) );

    while ( (line_len = getline(&line, &n, cmds_fp)) > 0 ){
        line_cnt += 1;
        if ( line_len == MAX_LINE ){
            fprintf(stderr, "Line %d too long.\n", line_cnt);
            rc = EINVAL;
            break;
        }

        if ( line[line_len-1] == '\n' )
            line[line_len-1] = '\0';

        if ( (rc = line_to_packet(line, &sp) ) != NET_OK ){
            fprintf(stderr, "Failed to parse line %d\n", line_cnt);
            rc = EINVAL;
            break;
        }
        
        nc_add_packet(nc, sp);
    
        if ( (rc = wait_on_response(nc, &to, true)) != NET_OK ){
            fprintf(stderr, "Timeout while sending command: %s\n", line);
            fprintf(stderr, "%s\n", net_strerror(rc));
            rc = ETIMEDOUT;
            break;
        }

        if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
            fprintf(stderr, "No response from %s\n", nc->dd->host);
            rc = ETIMEDOUT;
            break;
        }

        if ( (rc = unpack_net_cmd_packet(&ncmd, rp )) != NET_OK ){
            fprintf(stderr, "unpack_str_cmd_packet: %s\n", net_strerror(rc));
            return ENOMEM;
        }

        STAILQ_REMOVE( &(nc->dd->recvq), rp, packet, packet_queue );
        free_packet(rp);

        if ( ncmd.id != (uint16_t)0 ){
            fprintf(stderr, "Server Error:  %u, %s\n",
                ncmd.id, ncmd.value ? ncmd.value : "(null errmsg)");
            rc = ncmd.id;
            break;
        } else if ( ncmd.value ){
            if ( verbose )
                printf("%-40s --> ", line);
            printf("%s\n", ncmd.value );
        }

        if (ncmd.value)
            free(ncmd.value);
        if (ncmd.subsystem)
            free(ncmd.subsystem);
        memset( &ncmd, 0, sizeof(struct net_cmd) );

        free(line);
        line = NULL;
    }

    if (line)
        free(line);
    if (ncmd.value)
        free(ncmd.value);
    if (ncmd.subsystem)
        free(ncmd.subsystem);

    if ( feof(cmds_fp) == 0 ){
        fprintf(stderr, "Did not finish processing all commands.\n");
        if ( rc == 0 )
            rc = 1;
    }

    return rc;
}
void usage() {

    printf("%s\n", PACKAGE_STRING);
    printf("wrtctl [ARGS]\n");
    printf("Optional Arguments:\n");
    printf("\t-h,--help                     This screen.\n");
    printf("\t-v,--verbose                  Toggle more verbose messages.\n");
    printf("\t-p,--port <port>              Port to connect to [%s].\n", WRTCTLD_DEFAULT_PORT);
    printf("\nRequired Arguments:\n");
    printf("\t-t,--target <target>          Address to connect to.\n");
    printf("\t-f,--file <file>              File containing commands to process (- for stdin)\n");
#ifdef ENABLE_STUNNEL
    printf("\nSSL Optional Arguments:\n");
    printf("\t-n,--no_ssl                   Do not use ssl for encryption/verification\n");
    printf("\t-C,--ssl_client <port>        Port for local stunnel wrapper [%s].\n", WRTCTL_SSL_PORT);
    printf("\t-S,--ssl_server <port>        Port for remote stunnel wrapper [%s].\n", WRTCTLD_SSL_PORT);
    printf("\t-k,--key_path <path>          Path to shared SSL certificate [%s].\n", DEFAULT_KEY_PATH);
#endif
    return;
}
    
int main(int argc, char **argv){
    nc_t    nc = NULL;
    int     rc = 0;
    bool    use_ssl = false;
    char    *target = NULL, *port = NULL;
    FILE    *cmdfd = NULL;


#ifdef ENABLE_STUNNEL
    char *key_path = NULL, *client_ssl_port = NULL, *server_ssl_port = NULL;
    stunnel_ctx_t stunnel_ctx = NULL;

    use_ssl = true;
#endif

    while ( true ){
        int c;
        int oi = 0;
        static struct option lo[] = {
            { "port",       required_argument,  NULL,   'p'},
            { "target",     required_argument,  NULL,   't'},
            { "file",       required_argument,  NULL,   'f'},
            { "verbose",    no_argument,        NULL,   'v'},
            { "help",       no_argument,        NULL,   'h'},
#ifdef ENABLE_STUNNEL
            { "ssl_client", required_argument,  NULL,   'C'},
            { "ssl_server", required_argument,  NULL,   'S'},
            { "key_path",   required_argument,  NULL,   'k'},
            { "no_ssl",     no_argument,        NULL,   'n'},
#endif
            { 0,            0,                  0,      0}
        };

#ifdef ENABLE_STUNNEL
        c = getopt_long(argc, argv, "p:t:f:vhC:S:k:n", lo, &oi);
#else
        c = getopt_long(argc, argv, "p:t:f:vh", lo, &oi);
#endif
        if ( c == -1 ) break;

        switch (c) {
            case 'f':
                if ( optarg[0] == '-')
                    cmdfd = stdin;
                else {
                    if ( !(cmdfd = fopen(optarg, "r") ) ){
                        rc = errno;
                        perror("fopen: ");
                    }
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 't':
                if ( !(target = strdup(optarg)) ){
                    perror("strdup: ");
                    rc = errno;
                }
                break;
            case 'p':
                port = optarg;
                break;
            case 'h':
                usage();
                goto done;
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
            case 'n':
                use_ssl = false;
                break;
#endif
            default:
                fprintf(stderr, "Unknown command -%c%s.\n",
                    c, optarg ? optarg : "");
                rc = EINVAL;
                break;
        }
        if ( rc != 0 ) break;
    }
    
    if ( rc != 0 ){
        fprintf(stderr, "Unknown switches on command line.\n");
        exit(EXIT_FAILURE);
    }

    if ( !target ){
        fprintf(stderr, "No target host (-t, --target) specified.\n");
        rc = EINVAL;
        goto done;
    }

    if ( !port )
        port = WRTCTLD_DEFAULT_PORT;

    if ( !cmdfd ){
        fprintf(stderr, "No command file specified.\n");
        rc = EINVAL;
        goto done;
    }

    if ( (rc = alloc_client(&nc, false, verbose)) != NET_OK ){
        fprintf(stderr, "alloc_client failed: %s.\n", net_strerror(rc));
        goto done;
    }

#ifdef ENABLE_STUNNEL
    if ( use_ssl ){
        if ( !key_path )
            key_path = DEFAULT_KEY_PATH;

        if ( (rc = start_stunnel_client(
                &stunnel_ctx,
                target,
                key_path,
                port,
                client_ssl_port,
                server_ssl_port)) != 0 ){
            fprintf(stderr, "Failed to start the stunnel wrapper.\n");
            goto done;
        }
    
        if ( (rc = create_conn(
                nc,
                "localhost",
                client_ssl_port ? client_ssl_port : WRTCTL_SSL_PORT)) != NET_OK ){
            fprintf(stderr, "create_conn failed: %s.\n", net_strerror(rc));
            goto done;
        }
    } else 
#endif
    {  
        if ( (rc = create_conn(
                nc,
                target,
                port)) != NET_OK ){
            fprintf(stderr, "create_conn failed: %s.\n", net_strerror(rc));
            goto done;
        }
    }
    
    free(target); target = NULL;

    rc = client_loop(nc, cmdfd);
done:
    if (cmdfd && cmdfd != stdin)
        fclose(cmdfd);
    if ( target )       free(target);
    if (nc) {
        close_conn(nc);
        free(nc);
    }
#ifdef ENABLE_STUNNEL
    if ( (stunnel_ctx) )   kill_stunnel( &stunnel_ctx );
#endif
     exit( rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE );
}



