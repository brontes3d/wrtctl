#include <config.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <wrtctl-log.h>
#include "wrtctl-int.h"

int parse_uci_cmd       (char *cmdline, packet_t *sp);
int parse_sys_cmd       (char *cmdline, packet_t *sp);
int parse_daemon_cmd    (char *cmdline, packet_t *sp);

int alloc_client(nc_t *nc, bool enable_log, bool verbose){
    (*nc) = NULL;
    if ( !((*nc) = (nc_t)malloc(sizeof(struct net_client))) ){
        return NET_ERR_MEM;
    }
    wrtctl_enable_log = enable_log;
    wrtctl_verbose = verbose;
    (*nc)->dd = NULL;
    init_tpl_hook();
    return NET_OK;
}

int create_conn(nc_t nc, char *to, char *port){
    int fd, rc;
    struct addrinfo hints;
    struct addrinfo *res = NULL;


    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ( (rc = getaddrinfo( to, port, &hints, &res )) != 0 ){
        err("getaddrinfo %s\n", gai_strerror(rc));
        rc = NET_ERR;
        goto done;
    }
        
    if( (fd = socket( AF_INET, SOCK_STREAM, res->ai_protocol)) == -1 ){
        err("socket %s\n", strerror(errno));
        rc = NET_ERR_FD;
        goto done;
    }

    if( connect( fd, res->ai_addr, res->ai_addrlen ) == -1 ){
        err("connect %s\n", strerror(errno));
        rc = NET_ERR;
        goto done;
    }

    rc = create_dd(&(nc->dd), fd);

done:
    if (res) freeaddrinfo(res);
    return rc;
}

int wait_on_response(nc_t nc, struct timeval *timeout, bool send_packets){
    int rc;
    fd_set incoming_fd;

    if ( send_packets && (rc = flush_sendq(nc->dd)) != NET_OK ){
        err("flush_sendq returned %d\n", rc);
        return rc;
    }
    
    FD_ZERO(&incoming_fd);
    FD_SET(nc->dd->fd, &incoming_fd);

    if ( select(nc->dd->fd+1, &incoming_fd, NULL, NULL, timeout) == -1 ){
        err("select %s\n", strerror(errno));
        rc = NET_ERR_FD;
        return rc;
    }

    if ( !FD_ISSET(nc->dd->fd, &incoming_fd) ){
        err("Timeout waiting for response from %s.\n", nc->dd->host);
        return NET_ERR_TIMEOUT;
    }

    while ( (rc = recv_packet(nc->dd)) == NET_OK ){;}
    switch (rc) {
        case NET_OK:
            break;
        case NET_ERR_CONNRESET:
            if ( !STAILQ_EMPTY(&nc->dd->recvq) )
                rc = NET_OK;
            else
                rc = NET_ERR_CONNRESET;
            break;
        default:
            rc = NET_ERR;
    }
    return rc;
}

void close_conn(nc_t nc){
    if ( nc && nc->dd ){
        shutdown(nc->dd->fd, SHUT_RDWR);
        close(nc->dd->fd);
        free_dd(&nc->dd);
    }
}

int line_to_packet(char * line, packet_t *sp) {
    int         rc;
    char *      subsystem = NULL;
    char *      cmd = NULL;

    if ( !(subsystem = strtok(line, ":")) 
            || !(cmd = strtok(NULL, ":")))
        return EINVAL;
    if ( !strncmp(subsystem, "uci", 4) ){
        rc = parse_uci_cmd(cmd, sp);
    } else if ( !strncmp(subsystem, "daemon", 7) ){
        rc = parse_daemon_cmd(cmd, sp);
    } else if ( !strncmp(subsystem, "sys", 4) ){
        rc = parse_sys_cmd(cmd, sp);
    } else {
        fprintf(stderr, "Unknown subsystem '%s'\n", subsystem);
        return EINVAL;
    }
    line[strlen(line)] = ':';

    return rc;
}


int parse_sys_cmd(char *cmdline, packet_t *sp) {
    int rc              = NET_OK;
    uint16_t id         = (uint16_t)SYS_CMD_NONE;
    char *subsystem     = SYS_CMDS_MAGIC;
    char *daemon = NULL, *command = NULL, *send = NULL;

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

    if ( (rc = create_net_cmd_packet(sp, id, subsystem, send)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    
    return rc;
}


int parse_daemon_cmd(char *cmdline, packet_t *sp){
    int rc              = NET_OK;
    uint16_t id         = (uint16_t)DAEMON_CMD_NONE;
    char *subsystem     = DAEMON_CMD_MAGIC;
    
    if ( !strncmp(cmdline, "ping", 5) )
        id = DAEMON_CMD_PING;
    else if ( !strncmp(cmdline, "reboot", 9) )
        id = DAEMON_CMD_SHUTDOWN;
    else {
        fprintf(stderr, "Invalid daemon command line.\n");
        return EINVAL;
    }

    if ( (rc = create_net_cmd_packet(sp, id, subsystem, NULL)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    return rc;
}

 
int parse_uci_cmd(char *cmdline, packet_t *sp){
    int rc              = NET_OK;
    uint16_t id         = (uint16_t)UCI_CMD_NONE;
    char *subsystem     = UCI_CMDS_MAGIC;
    char *nc_value      = NULL;

    char *cmd = NULL, *option = NULL, *value = NULL;
    size_t cl;

    cl = cmdline ? strlen(cmdline) : 0;

    if ( !(cmd = strtok(cmdline, " ")) ){
        fprintf(stderr, "No UCI command specified.\n");
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

    if ( (rc = create_net_cmd_packet(sp, id, subsystem, nc_value)) != NET_OK){
        fprintf(stderr, "%s\n", net_strerror(rc));
        return ENOMEM;
    }
    return rc;
}

