#include <config.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <signal.h>

#include <wrtctl-net.h>
#include <stunnel.h>

#define STUNNEL_CONF_TEMPLATE \
    "cert       = %s\n" \
    "key        = %s\n" \
    "CAfile     = %s\n" \
    "pid        = %s\n" \
    "socket     = l:TCP_NODELAY=1\n" \
    "socket     = r:TCP_NODELAY=1\n" \
    "verify     = 2\n" \
    "client     = %s\n" \
    "foreground = yes\n\n" \
    "syslog     = yes\n\n" \
    "[wrtctl%s]\n" \
    "accept     = %s\n" \
    "connect    = %s\n" 


stunnel_ctx_t stunnel_child = NULL;

int alloc_stunnel_ctx( stunnel_ctx_t *ctx ){
    int rc = 0;
    int fd = -1;

    (*ctx) = NULL;
    
    if ( !((*ctx) = (stunnel_ctx_t)malloc(sizeof(struct stunnel_ctx))) )
        return errno;

    (*ctx)->cf = NULL;
    (*ctx)->conf_file_path = NULL;
    (*ctx)->pid_file_path = NULL;
    (*ctx)->pid = 0;

    if ( asprintf( &(*ctx)->conf_file_path, "/tmp/stunnel.conf-XXXXXX" ) == -1 ){
        rc = errno;
        fprintf(stderr, "asprintf: %s\n", strerror(errno));
        goto err;
    }

    if ( (fd = mkstemp( (*ctx)->conf_file_path )) == -1 ){
        rc = errno;
        fprintf(stderr, "mkstemp: %s\n", strerror(errno));
        goto err; 
    }

    if ( asprintf(
            &(*ctx)->pid_file_path,
            "/tmp/stunnel.pid-%s",
            (*ctx)->conf_file_path+strlen("/tmp/stunnel.conf-") ) == -1 ){
        rc = errno;
        fprintf(stderr, "asprintf: %s\n", strerror(errno));
        goto err;
    }

    if ( !( (*ctx)->cf = fdopen(fd, "w")) ){
        rc = errno;
        fprintf(stderr, "fdopen: %s\n", strerror(errno));
        goto err;
    }

    goto done;

err:
    if ( (*ctx)->conf_file_path )   free( (*ctx)->conf_file_path );
    if ( (*ctx)->pid_file_path )    free( (*ctx)->pid_file_path );
    if ( (*ctx) )                   { free( (*ctx) ); (*ctx) = NULL; }

done:
    return rc;
}

void free_stunnel_ctx( stunnel_ctx_t *ctx ){
    if ( (*ctx)->cf )
        fclose( (*ctx)->cf );
    if ( (*ctx)->pid_file_path != NULL )
        free( (*ctx)->pid_file_path );
    if ( (*ctx)->conf_file_path != NULL ){
        if ( !access( (*ctx)->conf_file_path, F_OK ) )
            unlink( (*ctx)->conf_file_path );
        free( (*ctx)->conf_file_path );
    }
    free( (*ctx) );
    (*ctx) = NULL;
    return;
}

int write_stunnel_conf(
        stunnel_ctx_t   ctx,
        bool            is_client,
        char *          hostname,
        char *          key_path,
        char *          port,
        char *          wrtctl_port,
        char *          wrtctld_port ){

    int rc              = 0;
    char *accept_str    = NULL;
    char *connect_str   = NULL;
    
    if ( is_client && !hostname ){
        fprintf(stderr, "write_stunnel_conf:  is_client but no hostname set.\n");
        return EINVAL;
    }

    if ( !key_path ) {
        fprintf(stderr, "write_stunnel_conf:  no key_path specified.\n");
        return EINVAL;
    }

    if ( !ctx->cf ){
        fprintf(stderr, "write_stunnel_conf:  no fd for config file.\n");
        return EINVAL;
    }

    if ( access(key_path, R_OK) ){
        fprintf(stderr, "write_stunnel_conf:  unable to read %s.\n", key_path);
        return EPERM;
    }

    if ( is_client ){
        if ( asprintf(&accept_str, "localhost:%s", 
                wrtctl_port ? wrtctl_port : WRTCTL_SSL_PORT ) == -1 ){
            rc = errno;
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            goto done;
        }
        if ( asprintf(&connect_str, "%s:%s", 
                hostname,
                wrtctld_port ? wrtctld_port : WRTCTLD_SSL_PORT) == -1 ){
            rc = errno;
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            goto done;
        }
    } else {
        if ( asprintf(&accept_str, "%s",  
                wrtctld_port ? wrtctld_port : WRTCTLD_SSL_PORT ) == -1 ){
            rc = errno;
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            goto done;
        }
        if ( asprintf(&connect_str, "localhost:%s",
                port ? port : WRTCTLD_DEFAULT_PORT) == -1 ){
            rc = errno;
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            goto done;
        }
    }
 
    fprintf(ctx->cf, STUNNEL_CONF_TEMPLATE,
        /* cert     */ key_path,
        /* key      */ key_path,
        /* CAfile   */ key_path,
        /* pidfile  */ ctx->pid_file_path,
        /* client   */ is_client ? "yes" : "no",
        /* service  */ is_client ? "" : "d",
        /* accept   */ accept_str,
        /* connect  */ connect_str );

done:
    fclose(ctx->cf);
    ctx->cf = NULL;
    if (connect_str)    free(connect_str);
    if (accept_str)     free(accept_str);
    return rc;
}

void sigchld_handler( int s ){
    int status;
    int w = -1;

    w = waitpid(stunnel_child->pid, &status, WNOHANG);
    if ( stunnel_child && w > 0 ){ 
       if (WIFEXITED(status)) {
            fprintf(stderr, "Unexpected stunnel exit: status=%d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Unexpected stunnel exit: killed by signal %d\n", WTERMSIG(status));
        } else {
            fprintf(stderr, "Unexpected stunnel signal.\n");
        }
        exit(EXIT_FAILURE);
    }
}

void exit_handler( int s ){
    struct sigaction sa;
    fprintf(stderr, "Caught signal %d, exiting.\n", s);
    if ( stunnel_child )
       kill_stunnel( &stunnel_child );

    sa.sa_handler = SIG_DFL;
    sigaction( SIGTERM, &sa, NULL );
    sigaction( SIGINT, &sa, NULL );
    kill( getpid(), s);
}

        
int fork_stunnel( stunnel_ctx_t ctx ){
    int rc = 0;
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_NOCLDSTOP;

    if ( sigaction( SIGCHLD, &sa, NULL ) == -1 ){
        rc = errno;
        fprintf(stderr, "sigaction: %s\n", strerror(errno));
        goto done;
    }

    sa.sa_handler = exit_handler;
    sa.sa_flags = 0;
    if ( sigaction( SIGTERM, &sa, NULL ) == -1 ){
        rc = errno;
        fprintf(stderr, "sigaction: %s\n", strerror(errno));
        goto done;
    }
    if ( sigaction( SIGINT, &sa, NULL ) == -1 ){
        rc = errno;
        fprintf(stderr, "sigaction: %s\n", strerror(errno));
        goto done;
    }
    stunnel_child = ctx;

    if ( (ctx->pid = fork()) == -1 ){
        rc = errno;
        fprintf(stderr, "fork: %s\n", strerror(errno));
        goto done;
    }

    if ( !ctx->pid ){
        char *argv[] = { STUNNEL_PATH, ctx->conf_file_path, NULL };
        char *envir[] = { NULL };

        /* Hide output */
        freopen( "/dev/null", "r", stdin);
        freopen( "/dev/null", "w", stdout);
        freopen( "/dev/null", "w", stderr);
        execve(STUNNEL_PATH, argv, envir);
        exit(EXIT_FAILURE);
    } else {
        do {
            sched_yield();
        } while ( access(ctx->pid_file_path, F_OK) != 0);
    }

done:
    return rc;
}

int kill_stunnel( stunnel_ctx_t *ctx ){
    int w = -1;
    int rc = 0;
    int status;
    struct sigaction sa;

    sa.sa_handler = SIG_DFL;
    sigaction( SIGCHLD, &sa, NULL );

    if ( kill( (*ctx)->pid, SIGTERM ) == -1 ){
        rc = errno;
        fprintf(stderr, "Failed to kill %u: %s\n", strerror(errno));
        return rc;
    }
  
    for (;;) {
        w = waitpid((*ctx)->pid, &status, WNOHANG);
        if ( w == (*ctx)->pid || w == -1 || w == 0 )
            break;
        sched_yield();
    }

    if ( !access( (*ctx)->conf_file_path, W_OK ) )
        unlink( (*ctx)->conf_file_path );
    if ( !access( (*ctx)->pid_file_path, F_OK ) )
        unlink( (*ctx)->pid_file_path );
    free_stunnel_ctx( ctx );
    stunnel_child = NULL;
    return rc;
}

int start_stunnel_client( 
        stunnel_ctx_t * ctx,
        char *          server,
        char *          key_path,
        char *          port,
        char *          wrtctl_port,
        char *          wrtctld_port ){

    int rc = 0;

    if ( (rc = alloc_stunnel_ctx( ctx )) != 0 ){
        fprintf(stderr, "Failed to allocate an stunnel context.\n");
        return rc;
    }

    rc = write_stunnel_conf(
        (*ctx),
        true,
        server,
        key_path,
        port,
        wrtctl_port,
        wrtctld_port);
    if ( rc != 0 ){
        fprintf(stderr, "Failed to write the stunnel client config file.\n");
        return rc;
    }

    if ( (rc = fork_stunnel( (*ctx) ) != 0) ){
        fprintf(stderr, "Failed to fork the stunnel client.\n");
        return rc;
    }

    return rc;
}


int start_stunnel_server( 
        stunnel_ctx_t * ctx,
        char *          key_path,
        char *          port,
        char *          wrtctl_port,
        char *          wrtctld_port ){

    int rc = 0;

    if ( (rc = alloc_stunnel_ctx( ctx )) != 0 ){
        fprintf(stderr, "Failed to allocate an stunnel context.\n");
        return rc;
    }

    rc = write_stunnel_conf(
        (*ctx),
        false,
        NULL,
        key_path,
        port,
        wrtctl_port,
        wrtctld_port);
    if ( rc != 0 ){
        fprintf(stderr, "Failed to write the stunnel server config file.\n");
        return rc;
    }

    if ( (rc = fork_stunnel( (*ctx) ) != 0) ){
        fprintf(stderr, "Failed to fork the stunnel server.\n");
        return rc;
    }

    return rc;
}
