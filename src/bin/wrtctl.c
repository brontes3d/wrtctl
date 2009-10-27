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

int sys_cmds(nc_t nc, char *cmdline) {
    packet_t sp = NULL, rp = NULL;
    struct timeval to = { TIMEOUT, 0 };
    struct net_cmd ncmd;
    char *daemon = NULL, *command = NULL, *send = NULL;

    uint16_t id = (uint16_t)SYS_CMD_NONE;
    char *subsystem = SYS_CMDS_MAGIC;


    int rc = NET_OK;

    if ( !cmdline  ){
        fprintf(stderr, "Invalid command line.\n");
        return EINVAL;
    }

    if ( !strncmp(cmdline, "initd ", strlen("initd ")) ){
        id = SYS_CMD_INITD;
        if ( !(daemon = index(cmdline, ' ')) || !(command = index(daemon+1, ' ')) ){
            fprintf(stderr, "Invalid sys.initd command line.\n");
            return EINVAL;
        }
        command++;

        /* Yes, this does let crap through, like 'restartblah'.  I've left this
         * here on purpose to allow me to test the comparisons in sys_cmd_initd on
         * the server side, where we correctly and fully check the arguments.
         */
        if ( strncmp(command, "restart", strlen("restart")) \
            && strncmp(command, "stop", strlen("stop")) \
            && strncmp(command, "start", strlen("start")) ){
            fprintf(stderr, "Invalid argument to sys.initd.\n");
            return EINVAL;
        }
        send = daemon+1;
    } else {
        fprintf(stderr, "Invalid sys command line.\n");
        return EINVAL;
    }

    if ( (rc = create_net_cmd_packet(&sp, id, subsystem, send)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    nc_add_packet(nc, sp);

    if ( (rc = wait_on_response(nc, &to, true)) != NET_OK ){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ETIMEDOUT;
    }

    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
        fprintf(stderr, "No response from %s\n", nc->dd->host);
        return ETIMEDOUT;
    }

    if ( (rc = unpack_net_cmd_packet(&ncmd, rp )) != NET_OK ){
        fprintf(stderr, "unpack_str_cmd_packet: %s\n", net_strerror(rc));
        return ENOMEM;
    }

    rc = 0;
    if ( ncmd.id != (uint16_t)0 ){
        fprintf(stderr, "Server SysCmds Error:  %u, %s\n",
            ncmd.id, ncmd.value ? ncmd.value : "(null errmsg)");
        rc = EINVAL;
    } else if ( ncmd.value ){
        printf("%s\n", ncmd.value );
    }

    if ( ncmd.value )
        free(ncmd.value);
    if ( ncmd.subsystem )
        free(ncmd.subsystem);
    return rc;
}


int daemon_cmds(nc_t nc, char *cmdline){
    packet_t sp = NULL, rp = NULL;
    struct timeval to = { TIMEOUT, 0 };
    struct net_cmd ncmd;

    uint16_t id = (uint16_t)DAEMON_CMD_NONE;
    char *subsystem = DAEMON_CMD_MAGIC;

    int rc = NET_OK;

    if ( !cmdline ){
        fprintf(stderr, "No command specified.\n");
        return EINVAL;
    }

    if ( !strncmp(cmdline, "ping", 5) )
        id = DAEMON_CMD_PING;
    else if ( !strncmp(cmdline, "reboot", 9) )
        id = DAEMON_CMD_SHUTDOWN;
    else {
        fprintf(stderr, "Invalid daemon command line.\n");
        return EINVAL;
    }

    if ( (rc = create_net_cmd_packet(&sp, id, subsystem, NULL)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    nc_add_packet(nc, sp);

    if ( (rc = wait_on_response(nc, &to, true)) != NET_OK ){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ETIMEDOUT;
    }

    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
        fprintf(stderr, "No response from %s\n", nc->dd->host);
        return ETIMEDOUT;
    }

    if ( (rc = unpack_net_cmd_packet(&ncmd, rp )) != NET_OK ){
        fprintf(stderr, "unpack_str_cmd_packet: %s\n", net_strerror(rc));
        return ENOMEM;
    }

    rc = 0;
    if ( ncmd.id != (uint16_t)0 ){
        fprintf(stderr, "Server Daemon Error:  %u, %s\n",
            ncmd.id, ncmd.value ? ncmd.value : "(null errmsg)");
        rc = EINVAL;
    } else if ( ncmd.value ){
        printf("%s\n", ncmd.value );
    }

    if ( ncmd.value )
        free(ncmd.value);
    if ( ncmd.subsystem )
        free(ncmd.subsystem);
    return rc;
}

 



int uci_cmds(nc_t nc, char *cmdline){
    packet_t sp = NULL, rp = NULL;
    char *cmd = NULL, *option = NULL, *value = NULL;
    struct timeval to = {TIMEOUT, 0};
    size_t cl;
    struct net_cmd ncmd;

    uint16_t id = (uint16_t)UCI_CMD_NONE;
    char *subsystem = UCI_CMDS_MAGIC;
    char *nc_value = NULL;

    int rc = NET_OK;


    cmd = option = value = NULL;
    cl = cmdline ? strlen(cmdline) : 0;

    if ( !cmdline || !(cmd = strtok(cmdline, " ")) ){
        fprintf(stderr, "No command specified.\n");
        return EINVAL;
    }

    option = strtok(NULL, "=");
    if ( option && cl > (strlen(option)+strlen(cmd)+1))
        value = option + strlen(option) + 1;

    /* UCI_CMD_GET */
    if ( !strncmp(cmd, "get", 4) ){
        if ( !option || value ){
            fprintf(stderr, "Invalid UCI element string.\n");
            return EINVAL;
        }
        id = (uint16_t)UCI_CMD_GET;
        nc_value = option;

    /* UCI_CMD_SET */
    } else if ( !strncmp(cmd, "set", 4) ){
        if ( !option || !value ){
            fprintf(stderr, "Invalid UCI element/value string.\n");
            return EINVAL;
        }
        id = (uint16_t)UCI_CMD_SET;
        *(option+strlen(option)) = '=';
        nc_value = option;

    /* UCI_CMD_COMMIT */
    } else if ( !strncmp(cmd, "commit", 7) ){
        if ( value ){
            fprintf(stderr, "Invalid arguments to commit.\n");
            return EINVAL;
        }
        id = (uint16_t)UCI_CMD_COMMIT;
        nc_value = option;

    /* UCI_CMD_REVERT */
    } else if ( !strncmp(cmd, "revert", 7) ){
        if ( value ){
            fprintf(stderr, "Invalid arguments to revert.\n");
            return EINVAL;
        }
        id = (uint16_t)UCI_CMD_REVERT;
        nc_value = option;
    } else {
        fprintf(stderr, "Invalid UCI command line.\n");
        return EINVAL;
    }

    if ( (rc = create_net_cmd_packet(&sp, id, subsystem, nc_value)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    nc_add_packet(nc, sp);

    if ( (rc = wait_on_response(nc, &to, true)) != NET_OK ){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ETIMEDOUT;
    }

    if ( !(rp = STAILQ_FIRST(&(nc->dd->recvq))) ){
        fprintf(stderr, "No response from %s\n", nc->dd->host);
        return ETIMEDOUT;
    }

    if ( (rc = unpack_net_cmd_packet(&ncmd, rp )) != NET_OK ){
        fprintf(stderr, "unpack_str_cmd_packet: %s\n", net_strerror(rc));
        return ENOMEM;
    }

    rc = 0;
    if ( ncmd.id != (uint16_t)UCI_OK ){
        fprintf(stderr, "Server UCI Error:  %u, %s\n",
            ncmd.id, ncmd.value ? ncmd.value : "(null errmsg)");
        rc = EINVAL;
    } else if ( ncmd.value ){
        printf("%s\n", ncmd.value );
    }

    if ( ncmd.value )
        free(ncmd.value);
    if ( ncmd.subsystem )
        free(ncmd.subsystem);
    return rc;
}

void usage() {
    char *valid_subsystems = "uci, daemon, sys";

    printf("%s\n", PACKAGE_STRING);
    printf("wrtctl [ARGS]\n");
    printf("Optional Arguments:\n");
    printf("\t-h,--help                     This screen.\n");
    printf("\t-v,--verbose                  Toggle more verbose messages.\n");
    printf("\t-p,--port <port>              Port to connect to.  Default: %s\n", WRTCTLD_DEFAULT_PORT);
    printf("\nRequired Arguments:\n");
    printf("\t-t,--target <target>          Address to connect to.\n");
    printf("\t-s,--subsystem <subsystem>    Command type.  (%s)\n", valid_subsystems);
    printf("\t-c,--command '<command>'      Command to be sent.\n");
    return;
}
    
bool verbose = false;

int main(int argc, char **argv){
    nc_t nc = NULL;;
    int rc = 0;

    char *command = NULL, *subsystem = NULL, *target = NULL, *port = NULL;

    while ( true ){
        int c;
        int oi = 0;
        static struct option lo[] = {
            { "port",       required_argument,  NULL,   'p'},
            { "target",     required_argument,  NULL,   't'},
            { "subsystem",  required_argument,  NULL,   's'},
            { "command",    required_argument,  NULL,   'c'},
            { "verbose",    no_argument,        NULL,   'v'},
            { "help",       no_argument,        NULL,   'h'},
            { 0,            0,                  0,      0}
        };

        c = getopt_long(argc, argv, "p:t:s:c:vh", lo, &oi);
        if ( c == -1 ) break;

        switch (c) {
            case 's':
                if ( !(subsystem = strdup(optarg)) ){
                    perror("strdup: ");
                    rc = errno;
                }
                break;
            case 'c':
                if ( !(command = strdup(optarg)) ){
                    perror("strdup: ");
                    rc = errno;
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

    if ( (rc = alloc_client(&nc, false, verbose)) != NET_OK ){
        fprintf(stderr, "alloc_client failed: %s.\n", net_strerror(rc));
        goto done;
    }

    if ( (rc = create_conn(nc, target, port)) != NET_OK ){
        fprintf(stderr, "create_conn failed: %s.\n", net_strerror(rc));
        goto done;
    }
   
    free(target); target = NULL;

    if ( ! subsystem || ! command ){
        fprintf(stderr, "Invalid command line, no subsystem and/or command specified.\n");
        rc = EINVAL;
        goto done;
    }

    if ( verbose )
        printf("Processing subsystem='%s' command='%s'.\n", subsystem, command);

    if ( !strncmp(subsystem, "uci", 4) ){
        rc = uci_cmds(nc, command);
    } else if ( !strncmp(subsystem, "daemon", 7) ){
        rc = daemon_cmds(nc, command);
    } else if ( !strncmp(subsystem, "sys", 4) ){
        rc = sys_cmds(nc, command);
    } else {
        fprintf(stderr, "Unknown subsystem '%s'\n", subsystem);
        rc = EINVAL;
        goto done;
    }

done:
    if ( command )      free(command);
    if ( subsystem )    free(subsystem);
    if ( target )       free(target);
    if (nc) {
        close_conn(nc);
        free(nc);
    }
    exit( rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE );
}



