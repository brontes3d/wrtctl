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

extern int errno;

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
