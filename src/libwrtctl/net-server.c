#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

#include <wrtctl-log.h>
#include "wrtctl-int.h"
#include "config.h"

int accept_connection( ns_t ns );
int load_modules(mlh_t ml, char *modules);
void unload_modules(mlh_t ml);
int daemon_mod_handler(void *ctx, net_cmd_t cmd, packet_t *outp);

int create_ns(ns_t *ns, char *port, char *module_list, bool enable_log, bool verbose){
    int rc = NET_OK;
    md_t daemon_mod = NULL;
    char *shutdown_path = NULL;
    struct addrinfo hints, *res = NULL;
    int t=1;
    
    (*ns) = NULL;
    if ( !((*ns) = (ns_t)malloc(sizeof(struct net_server))) )
        return NET_ERR_MEM;

    (*ns)->shutdown = false;
    (*ns)->ctx = NULL;
    (*ns)->server_loop = default_server_loop;
    (*ns)->handler = default_handler;
    (*ns)->shutdown_dd = default_shutdown_dd;
    
    wrtctl_enable_log = enable_log;
    wrtctl_verbose = verbose;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ( (rc = getaddrinfo(NULL, port, &hints, &res)) != 0 ){
        err("getaddrinfo: %s\n", gai_strerror(rc));
        rc = NET_ERR_FD;
        goto err;
    }
 
    if( ((*ns)->listen_fd = socket( res->ai_family, res->ai_socktype, res->ai_protocol)) < 0){
        rc = NET_ERR_FD;
        goto err;
    }

    if ( (rc = setsockopt((*ns)->listen_fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(int)) == -1) ){
        err("setsockopt: %s\n", strerror(rc));
        rc = NET_ERR_FD;
        goto err;
    }

    if( bind((*ns)->listen_fd, res->ai_addr, res->ai_addrlen)  < 0 ){
        rc = NET_ERR_FD;
        goto err;
    }

    if( listen( (*ns)->listen_fd, 10 ) < 0 ){
        rc = NET_ERR_FD;
        goto err;
    }

    shutdown_path = getenv("WRTCTL_SYS_SHUTDOWN_PATH");
    if ( !shutdown_path )
        shutdown_path = "/sbin/shutdown";
    if ( !((*ns)->shutdown_path = strdup(shutdown_path)) ){
        rc = NET_ERR_MEM;
        goto err;
    }

    STAILQ_INIT( &((*ns)->dd_list) );
    STAILQ_INIT( &((*ns)->mod_list) );
    if ( !(daemon_mod = (md_t)malloc(sizeof(struct mod_data))) ){
        rc = NET_ERR_MEM;
        goto err;
    }

    daemon_mod->mod_name = DAEMON_MOD_NAME;
    daemon_mod->mod_magic_str = DAEMON_CMD_MAGIC;
    daemon_mod->mod_version = DAEMON_MODVER;
    daemon_mod->mod_ctx = (void*)(*ns);
    daemon_mod->dlp = NULL;
    daemon_mod->mod_handler = daemon_mod_handler;
    daemon_mod->mod_errstr = mod_errstr;
    STAILQ_INSERT_TAIL(&((*ns)->mod_list), daemon_mod, mod_data_list);

    if ( module_list ){
        if ( (rc = load_modules(&((*ns)->mod_list), module_list)) != MOD_OK ){
            err("load_modules failed, %s\n", mod_strerror(rc));
            rc = NET_ERR;
            goto err;
        }
    }

    goto done;

err:
    if ( res ) freeaddrinfo(res);
    if ( (*ns)->listen_fd != -1 ) close((*ns)->listen_fd);
    unload_modules(&((*ns)->mod_list));
    if ( (*ns) ) free_ns((*ns));
    *ns = NULL;

done:
    return rc;
}

int accept_connection(ns_t ns){
    dd_t dd;
    int fd, rc;


    if ( (fd = accept( ns->listen_fd, NULL, (socklen_t)0)) < 0 ){
        rc = NET_ERR;
        if ( errno == ECONNABORTED )
            rc = NET_ERR_CONNRESET;
        return rc;
    }
    
    if( (rc = create_dd( &dd, fd )) != NET_OK ){
        shutdown(fd, SHUT_RDWR);
        close(fd);
        return rc;
    }
   
    info("Accepted new connection from %s (%d)\n", dd->host, dd->fd);
    STAILQ_INSERT_TAIL( &(ns->dd_list), dd, dd_queue );
    return NET_OK;
}

int default_server_loop(ns_t ns){
    int tfd, rc;
    fd_set incoming_fd, outgoing_fd1, outgoing_fd2;
    dd_t dd_iter, dd_tmp;
    struct timeval timeout;

    info("Starting %s\n", __func__);
    while( !ns->shutdown ){
        rc = NET_OK;
        FD_ZERO(&incoming_fd);
        FD_ZERO(&outgoing_fd1);
        FD_ZERO(&outgoing_fd2);
        FD_SET(ns->listen_fd, &incoming_fd);
        tfd = ns->listen_fd;

        STAILQ_FOREACH(dd_iter, &(ns->dd_list), dd_queue){
            if ( !STAILQ_EMPTY(&(dd_iter->sendq)) )
                FD_SET(dd_iter->fd, &outgoing_fd1);
            FD_SET(dd_iter->fd, &incoming_fd);
            if ( dd_iter->fd > tfd )
                tfd = dd_iter->fd;
        }

        if ( select((tfd)+1, &incoming_fd, &outgoing_fd1, NULL, NULL) == -1 ){
            rc = NET_ERR_FD;
            break;
        }

        if ( FD_ISSET(ns->listen_fd, &incoming_fd) ){
            if ( (rc = accept_connection(ns)) != NET_OK ){
                break;
            }
        }

        STAILQ_FOREACH_SAFE(dd_iter, &(ns->dd_list), dd_queue, dd_tmp){
            if ( FD_ISSET(dd_iter->fd, &incoming_fd) ){
                while( (rc = recv_packet(dd_iter)) == NET_OK ){;}
                switch (rc){
                    case NET_OK:
                        break;
                    case NET_ERR_CONNRESET:
                        if ( !STAILQ_EMPTY(&(dd_iter->sendq)) || !STAILQ_EMPTY(&(dd_iter->recvq)) )
                            break;
                    default:
                        info("Closing connection to %s due to empty recv()\n", dd_iter->host);
                        dd_iter->shutdown = true;
                        break;
                }
            }

            if ( dd_iter->shutdown ){
                ns->shutdown_dd(ns, dd_iter);
                continue;
            }

            if ( (rc = ns->handler(ns, dd_iter)) != NET_OK ){
                info("Closing connection to %s due to handler error: %s\n",
                    dd_iter->host, net_strerror(rc));
                dd_iter->shutdown = true;
            }
            FD_SET(dd_iter->fd, &outgoing_fd2);
        }

        timeout.tv_sec = (long)0;
        timeout.tv_usec = (long)500;
        if ( select((tfd)+1, NULL, &outgoing_fd2, NULL, &timeout) == -1 ){
            rc = NET_ERR_FD;
            break;
        }

        STAILQ_FOREACH(dd_iter, &(ns->dd_list), dd_queue){
            if ( FD_ISSET(dd_iter->fd, &outgoing_fd1) || FD_ISSET(dd_iter->fd, &outgoing_fd2) ){
                if ( (rc = flush_sendq(dd_iter)) != NET_OK ){
                    info("Closing connection to %s due to send error: %s\n",
                        dd_iter->host, net_strerror(rc));
                    dd_iter->shutdown = true;
                }
            }
        }

        STAILQ_FOREACH_SAFE(dd_iter, &(ns->dd_list), dd_queue, dd_tmp){
            if ( dd_iter->shutdown )
                ns->shutdown_dd(ns, dd_iter);
        }
    }

    STAILQ_FOREACH_SAFE(dd_iter, &(ns->dd_list), dd_queue, dd_tmp){
        ns->shutdown_dd(ns, dd_iter);
    }

    shutdown(ns->listen_fd, SHUT_RDWR);
    close(ns->listen_fd);
 
    return rc;
}

int default_handler( ns_t ns, dd_t dd ){
    md_t md;
    packet_t p, p_tmp, out_packet;
    size_t data_len;
    bool handled;
    int hrc, nrc;

    STAILQ_FOREACH_SAFE(p, &(dd->recvq), packet_queue, p_tmp){
        handled = false;
        data_len = p->len - sizeof(uint32_t) - CMD_ID_LEN;

        if ( !strncmp(p->cmd_id, NET_CMD_MAGIC, MOD_MAGIC_LEN-1) ){
            struct net_cmd nc = {0, NULL, NULL};

            if ( (nrc = unpack_net_cmd_packet(&nc, p)) != NET_OK ){
                err("unpack_net_cmd_packet: %s\n", net_strerror(nrc));
                break;
            }

            STAILQ_FOREACH(md, &(ns->mod_list), mod_data_list){
                if ( strncmp(nc.subsystem, md->mod_magic_str, MOD_MAGIC_LEN-1) )
                    continue;

                handled = true;

                hrc = md->mod_handler(
                    md->mod_ctx,
                    &nc,
                    &out_packet);

                if ( hrc != MOD_OK ){
                    err("%s handler error: %s.\n", md->mod_name, mod_strerror(hrc) );
                    break;
                }

                STAILQ_INSERT_TAIL( &(dd->sendq), out_packet, packet_queue );

                if ( nc.subsystem )
                    free(nc.subsystem);
                if ( nc.value )
                    free(nc.value);
            }
            if ( !handled ){
                err("Unhandled net command for subsystem %s\n", nc.subsystem);
            }
        } else {
            err("Unhandled packet of type %s\n", p->cmd_id);
        }

        STAILQ_REMOVE( &(dd->recvq), p, packet, packet_queue );
        free_packet(p);

    }
    return NET_OK;
}
    

void default_shutdown_dd( ns_t ns, dd_t dd ){
    shutdown(dd->fd, SHUT_RDWR);
    close(dd->fd);
    STAILQ_REMOVE(&(ns->dd_list), dd, d_data, dd_queue);
    return;
}

int load_modules(mlh_t ml, char *modules){
    char mod_path[MAXPATHLEN];
    char *mod_dir = NULL;
    int rc = MOD_OK;
    char *tok = NULL;
    char *ret = NULL;

    if ( !(mod_dir = getenv("WRTCTL_MODULE_DIR")) )
        mod_dir = DEFAULT_MODULE_DIR;
                    
    tok = strtok(modules, " ,");
    while ( tok ){
        info("Loading %s.so from %s\n", tok, mod_dir);
        if ( snprintf(mod_path, MAXPATHLEN, "%s/%s.so", mod_dir, tok) >= MAXPATHLEN ){
            err("Module path too long, %s/%s.so\n", mod_dir, tok);
            rc = MOD_ERR_LOAD;
        } else if ( (ret = load_module(ml, NULL, mod_path)) ) {
            err("Error loading %s/%s.so, %s\n", mod_dir, tok, ret);
            rc = MOD_ERR_LOAD;
            free(ret);
            ret = NULL;
        }
        tok = strtok(NULL, " ,");
    }
    return rc;
}


void unload_modules(mlh_t ml){
    md_t md, md_tmp;
    STAILQ_FOREACH_SAFE(md, ml, mod_data_list, md_tmp){
        info("Unloading %s\n", md->mod_name);
        unload_module(ml, md);
    }
    return;
}  


int daemon_mod_handler(void *ctx, net_cmd_t cmd, packet_t *outp){
    int rc = MOD_OK;
    DAE_CTX_CAST(ns, ctx);
    uint16_t out_rc = MOD_OK;
    char *out_str = NULL;

    info("daemon_mod_handler in: cmd=%u, args='%s'\n", 
        cmd->id, cmd->value ? cmd->value : "(null)");

    switch ( cmd->id ){
        case DAEMON_CMD_PING:
            rc = daemon_cmd_ping(ns, NULL, &out_rc, &out_str);
            break;
        case DAEMON_CMD_SHUTDOWN:
            rc = daemon_cmd_reboot(ns, NULL, &out_rc, &out_str);
            break;
        default:
            err("daemon_mod_handler:  Unknown command '%u'\n", cmd->id);
            out_rc = NET_ERR_INVAL;
            asprintf(&out_str, "Unknown command");
            break;
    }
    rc = create_net_cmd_packet(outp, out_rc, DAEMON_CMD_MAGIC, out_str);
    if ( out_rc != NET_OK )
        err("daemon_mod_hander returned %u, %s\n",
            out_rc, out_str ? out_str : "-");
    if ( out_str )
        free(out_str);
    return rc;
}

int daemon_cmd_ping(ns_t ns, char *unused, uint16_t *out_rc, char **out_str){
    int sys_rc = 0;
    int rc = -1;
    time_t t;

    if( time(&t) == ((time_t)-1) ){
        asprintf(out_str, "time:  %s", strerror(errno));
        sys_rc = errno;
        goto done;
    }

    if ( asprintf(out_str, "%lu", (uint32_t)t) == -1 ){
        sys_rc = ENOMEM;
        goto done;
    }
    rc = 0;

done:
    (*out_rc) = (uint16_t)sys_rc;
    return rc;
}

int daemon_cmd_reboot(ns_t ns, char *unused, uint16_t *out_rc, char **out_str){
    int sys_rc = 0;
    int rc = 0;
    pid_t pid;

    
    if ( access(ns->shutdown_path, X_OK) != 0 ){
        asprintf(out_str, "access:  %s", strerror(errno));
        sys_rc = errno;
        goto done;
    }

    pid = fork();
    if ( pid == -1 ){
        asprintf(out_str, "fork:  %s", strerror(errno));
        sys_rc = errno;
        goto done;
    }

    if ( pid == 0 ){
        char *argv[] = { ns->shutdown_path, "-r", "now", NULL };
        char *envir[] = { NULL };
        extern int errno;
        /* We separate and sleep for 5 seconds.  This should be plenty of
         * time for the daemon to respond to the client and shutdown cleanly.
         */
        setsid();
        sleep(5);
        return execve(ns->shutdown_path, argv, envir);
    } else {
        ns->shutdown = true;
        asprintf(out_str, "Rebooting...");
        sys_rc = MOD_OK;
    }


done:
    (*out_rc) = (uint16_t)sys_rc;
    return rc;
}
