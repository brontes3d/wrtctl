#include <config.h>

#include <stdlib.h>
#include <endian.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "tpl.h"
#include "wrtctl-int.h"

int     create_packet   (packet_t *p, char *cmd_id, void *data, uint32_t data_len);
int     send_packet     (dd_t dd, packet_t p);

//TODO:   Accept sockaddr_in pointer or handle null.
int create_dd(dd_t *dd, int fd){
    struct sockaddr_in sa;
    socklen_t socklen = sizeof(struct sockaddr_in);

    (*dd) = NULL;
    if ( !((*dd) = (dd_t)malloc(sizeof(struct d_data))) )
        return NET_ERR_MEM;

    (*dd)->host = NULL;
    (*dd)->fd = fd;
   
    STAILQ_INIT( &((*dd)->sendq) ); 
    STAILQ_INIT( &((*dd)->recvq ) );

    (*dd)->shutdown = false;
    (*dd)->dd_errno = 0;

    if ( getpeername(fd, (struct sockaddr *)&sa, &socklen) < 0
        || socklen > sizeof(struct sockaddr_in)){

        err("getpeername: %s\n", strerror(errno));
        free_dd(dd);
        if ( errno == ENOTCONN )
            return NET_ERR_CONNRESET;
        return NET_ERR_MEM;
    } else {
        char buf[512];
        int rc;

        if ( (rc = getnameinfo( (struct sockaddr *)&sa, socklen, buf, 512, NULL, 0, 0)) != 0 ){
            err("getnameinfo: %s\n", gai_strerror(rc));
            free_dd(dd);
            return NET_ERR_NS;
        }
        if ( !((*dd)->host = strdup(buf)) ){
            free_dd(dd);
            return NET_ERR_MEM;
        }
    }
    return NET_OK;
}

void free_dd(dd_t *dd){
    if ( (*dd) ){
        packet_t p, t;
        STAILQ_FOREACH_SAFE(p, &((*dd)->sendq), packet_queue, t){
            STAILQ_REMOVE(&((*dd)->sendq), p, packet, packet_queue);
            free_packet(p);
        }
        STAILQ_FOREACH_SAFE(p, &((*dd)->recvq), packet_queue, t){
            STAILQ_REMOVE(&((*dd)->recvq), p, packet, packet_queue);
            free_packet(p);
        }
        if( (*dd)->host )
            free( (*dd)->host );
        free( (*dd) );
        *dd = NULL;
    }
    return;
}

int create_packet(packet_t *p, char *cmd_id, void *data, uint32_t data_len){
    uint32_t be_len, p_len;
    void *dp;
    
    (*p) = NULL;
    p_len = sizeof(uint32_t) + CMD_ID_LEN + data_len;
    if ( p_len > MAX_PACKET_SIZE )
        return NET_ERR_PKTSZ;

    if ( strnlen(cmd_id, CMD_ID_LEN) != CMD_ID_LEN-1 )
        return NET_ERR_INVAL;
    
    if ( !((*p) = (packet_t)malloc(sizeof(struct packet))) )
        return NET_ERR_MEM;
    
    (*p)->len = p_len;

    if ( !((*p)->data = malloc(p_len)) ){
        free_packet(*p);
        return NET_ERR_MEM;
    }

//    be_len = htobe32(p_len);
    be_len = htonl(p_len);
    dp = (*p)->data;
    memcpy(dp, &be_len, sizeof(uint32_t));
    dp += sizeof(uint32_t);
    memcpy(dp, cmd_id, sizeof(char)*CMD_ID_LEN);
    dp += sizeof(char)*CMD_ID_LEN;
    memcpy(dp, data, data_len);
    return NET_OK;
}

int flush_sendq(dd_t dd){
    packet_t cp, tmp = NULL;
    int rc = NET_OK;

    STAILQ_FOREACH_SAFE(cp, &(dd->sendq), packet_queue, tmp){
        STAILQ_REMOVE(&(dd->sendq), cp, packet, packet_queue);
        rc = send_packet(dd, cp);
        free_packet(cp);
        if (rc != NET_OK)
            break;
    }
    return rc;
}

int send_packet(dd_t dd, packet_t p){
    uint32_t sent, remain;
    int n;
    void *dp;

    sent = 0;
    remain = p->len;
    dd->dd_errno = NET_OK;
    dp = p->data;

    while ( remain > 0 ){
        n = send(dd->fd, dp + sent, remain, 0);
        if ( n < 0 ){
            dd->shutdown = true;
            dd->dd_errno = NET_ERR;
            if ( errno == ECONNRESET )
                dd->dd_errno = NET_ERR_CONNRESET;
            err("send_packet(send): %s\n", strerror(errno));
            break;
        }
        sent += (uint32_t)n;
        remain -= (uint32_t)n;
    }
    return dd->dd_errno;
}

int recv_packet(dd_t dd){
    void        *buf = NULL, *bp = NULL;
    uint32_t    bremain, p_len = 0;
    int         n, rc;
    packet_t    p = NULL;

    /* Get the packet length first */
    bremain = sizeof(uint32_t);
    bp = &p_len;
    while( (n = recv(dd->fd, bp, bremain, MSG_DONTWAIT)) > 0 && bremain > 0){
        bremain -= (uint32_t)n;
        bp += (size_t)n;
    }
        
    if ( bremain != 0 ){
        rc = NET_ERR_CONNRESET;
        if ( n < 0 ){
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
                goto err;
            err("recv_packet(recv): %s\n", strerror(errno));
        }
        if ( bremain == sizeof(uint32_t) )
            goto err;
        err("recv_packet: Could not recv full packet length.\n");
        goto err;
    }

//    p_len = be32toh(p_len);
    p_len = ntohl(p_len);
    if ( p_len > MAX_PACKET_SIZE ){
        rc = NET_ERR_PKTSZ;
        goto err;
    }

    if( !(buf = malloc((size_t)p_len)) ){
        rc = NET_ERR_MEM;
        goto err;
    }

    memcpy(buf, &p_len, sizeof(uint32_t));
    bremain = p_len - sizeof(uint32_t);
    bp = buf + sizeof(uint32_t);

    while( bremain > 0 && (n = recv(dd->fd, bp, bremain, 0)) > 0 ){
        bremain -= (uint32_t)n;
        bp += (size_t)n;
    }

    if ( bremain != 0 && n <= 0 ){
        err("recv_packet:  Did not recv full packet of length %u\n", p_len);
        if ( n < 0 )
            err("recv_packet(recv): %s\n", strerror(errno));
        rc = NET_ERR_CONNRESET;
        goto err;
    }

    if ( !(p = (packet_t)malloc(sizeof(struct packet))) ){
        rc = NET_ERR_MEM;
        goto err;
    }

    p->len = p_len;
    memcpy(p->cmd_id, buf+sizeof(uint32_t), CMD_ID_LEN);
    p->cmd_id[CMD_ID_LEN-1] = '\0';
    p->data = buf;

    STAILQ_INSERT_TAIL( &(dd->recvq), p, packet_queue );
    rc = NET_OK;
    goto done;

err:
    if (buf) free(buf);
    if (p) free_packet(p);
    dd->dd_errno = rc;

done:
    return rc;
} 

static char *net_errstr_table[] = {
    /* NET_OK */            "Success",
    /* NET_ERR_MEM */       "Insufficient memory",
    /* NET_ERR_FD */        "Socket/file descriptor error",
    /* NET_ERR_INVAL */     "Invalid argument",
    /* NET_ERR_CONNRESET */ "Connection closed",
    /* NET_ERR_PKTSZ */     "Invalid packet (length)",
    /* NET_ERR_NS */        "Nameservice failure",
    /* NET_ERR_TIMEOUT */   "Timeout",
    /* NET_ERR_TPL */       "TPL pack/unpack failure",
    /* NET_ERR */           "Generic network stack error."
};

inline char * net_strerror(int err){
    return net_errstr_table[err < NET_ERR ? err : NET_ERR];
}

int create_net_cmd_packet(packet_t *p, uint16_t id, char *subsystem, char *value ){
    tpl_node *tn = NULL;
    struct net_cmd cmd;
    void *data = NULL;
    size_t dl;
    int rc;

    cmd.id = id;
    cmd.subsystem = NULL;
    cmd.value = NULL;
    if ( subsystem ){
        if ( !(cmd.subsystem = strdup(subsystem)) ){
            rc = NET_ERR_MEM;
            goto done;
        }
    }
    if ( value ){
        if ( !(cmd.value = strdup(value)) ){
            rc = NET_ERR_MEM;
            goto done;
        }
    }

    if ( !(tn = tpl_map("S(vss)", &cmd)) )
        return NET_ERR_MEM;
    tpl_pack(tn,0);
    if( tpl_dump(tn, TPL_MEM, &data, &dl) != 0 ){
        rc = NET_ERR_TPL;
        goto done;
    }

    rc = create_packet(p, NET_CMD_MAGIC, data, dl);

done:   
    if ( tn )
        tpl_free(tn);
    if ( data )
        free(data);
    return rc;
}

int unpack_net_cmd_packet(net_cmd_t cmd, packet_t p){
    int rc = NET_OK;
    tpl_node *tn = NULL;
    void *data;
    size_t dl;

    data = p->data + sizeof(uint32_t) + CMD_ID_LEN;
    dl = p->len - sizeof(uint32_t) - CMD_ID_LEN;

    if ( !(tn = tpl_map("S(vss)", cmd)) )
        return NET_ERR_MEM;
    if ( tpl_load(tn, TPL_MEM, data, dl) != 0 ){
        rc = NET_ERR_TPL;
        goto done;
    }
    tpl_unpack(tn, 0);

done:
    if ( tn )
        tpl_free(tn);
    return rc;
}

int wrtctl_tpl_oops(const char *format, ... ){
    int rc = 0;
    if ( wrtctl_verbose )
        rc = fprintf(stderr, format);
    if ( wrtctl_enable_log )
        syslog(LOG_ERR, format);
    return rc > 0 ? rc : 1;
}

void init_tpl_hook(){
    extern tpl_hook_t tpl_hook;
    tpl_hook.oops = wrtctl_tpl_oops;
}
