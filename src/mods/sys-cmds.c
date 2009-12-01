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
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "wrtctl-net.h"

#define SYS_CMDS_MODVER 1
#define SYS_CMDS_NAME "sys-cmds"
#define CTX_CAST(x,y) sysh_ctx_t x = (sysh_ctx_t)y

char mod_name[] = SYS_CMDS_NAME;
char mod_magic_str[MOD_MAGIC_LEN] = "SYS";
int  mod_version = SYS_CMDS_MODVER;
char mod_errstr[MOD_ERRSTR_LEN];

int     mod_init        (void **ctx);
void    mod_destroy     (void *ctx);
int     mod_handler     (void *ctx, net_cmd_t cmd, packet_t *outp);


typedef struct sysh_ctx {
    char    *initd_dir;
} *sysh_ctx_t;

int     sys_cmd_initd   (sysh_ctx_t syshc, char *value, uint16_t *out_rc, char **out_str);

int mod_init(void **mod_ctx){
    int rc = MOD_OK;
    sysh_ctx_t ctx = NULL;
    char *initd_dir = NULL;

    *mod_ctx = NULL;
    
    if ( !(ctx = (sysh_ctx_t)malloc(sizeof(struct sysh_ctx))) ){
        rc = MOD_ERR_MEM;
        goto err;
    }
    ctx->initd_dir = NULL;

    initd_dir = getenv("WRTCTL_SYS_INITD_DIR");
    if ( !initd_dir )
        initd_dir = "/etc/init.d/";
    if ( !(ctx->initd_dir = strdup(initd_dir)) ){
        rc = MOD_ERR_MEM;
        goto err;
    }

    (*mod_ctx) = ctx;
    return rc;

err:
    if( ctx ) mod_destroy((void*)ctx);
    return rc;
}

void mod_destroy(void *ctx){
    CTX_CAST(syshc, ctx);
    if ( ctx ){
        if ( syshc->initd_dir ) 
            free(syshc->initd_dir);
        free(syshc);
    }
    return;
}

int mod_handler(void *ctx, net_cmd_t cmd, packet_t *outp){
    int rc = MOD_OK;
    CTX_CAST(syshc, ctx);
    uint16_t out_rc;
    char *out_str = NULL;

    info("sys-cmds_handler in: cmd=%u, args='%s'\n",
        cmd->id, cmd->value ? cmd->value : "(null)");

    switch ( cmd->id ){
        case SYS_CMD_INITD:
            rc = sys_cmd_initd(syshc, cmd->value, &out_rc, &out_str);
            break;
        default:
            err("sys-cmds_handler:  Unknown command '%u'\n", cmd->id);
            out_rc = NET_ERR_INVAL;
            if ( asprintf(&out_str, "Unknown command") == -1 ){
                err("asprintf: %s\n", strerror(errno));
                out_str = NULL;
            }
            break;
    }
    rc = create_net_cmd_packet(outp, out_rc, SYS_CMDS_MAGIC, out_str);
    if ( out_rc != NET_OK )
        err("sys-cmds_handler returned %u, %s\n",
            out_rc, out_str ? out_str : "-" );
    if ( out_str )
        free(out_str);
    return rc;
}

int sys_cmd_initd(sysh_ctx_t syshc, char *value, uint16_t *out_rc, char **out_str){
    int sys_rc = MOD_OK;
    int rc = 0;
    pid_t pid;
    char *dpath, *daemon, *command, *p;
    static char *valid_commands[] = {"restart", "stop", "start", NULL};
    size_t len;
    bool valid = false;
    int i;
    
    dpath = daemon = command = p = NULL;

    if ( !value || !(p = strchr(value, ' '))  ){
        sys_rc = EINVAL;
        if ( asprintf(out_str, "Invalid argument list.") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    *p = '\0';
    daemon = strdup(value);
    *p = ' ';
    if ( asprintf(&dpath, "%s/%s", syshc->initd_dir, basename(daemon)) < 0 ){
        sys_rc = ENOMEM;
        goto done;
    }
    if ( access(dpath, X_OK) != 0 ){
        sys_rc = EPERM;
        if ( asprintf(out_str, "access:  %s", strerror(errno)) == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }


    command = strdup(p+1);
    for (i=0; valid_commands[i]; i++){
        len = strlen(valid_commands[i]);
        if ( strnlen(command, len+1) != len )
            continue;
        if ( !strncmp(command, valid_commands[i], len) ){
            valid = true;
            break;
        }
    }

    if ( !valid ){
        sys_rc = EINVAL;
        if ( asprintf(out_str, "Invalid init command.") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    pid = fork();
    if ( pid == -1 ){
        sys_rc = errno;
        if ( asprintf(out_str, "fork:  %s", strerror(errno)) == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    
    if ( pid == 0 ){
        char *argv[] = { dpath, command, NULL };
        char *envir[] = { NULL };
        rc = execve(dpath, argv, envir);
        exit(EXIT_FAILURE);
    } else {
        int status;
        /* Wait for the child to exit and grab the rc. */
        if ( waitpid( pid, &status, 0) != pid ){
            sys_rc = errno;
            if ( asprintf(out_str, "waitpid:  %s", strerror(errno)) ){
                err("asprintf: %s\n", strerror(errno));
                *out_str = NULL;
            }
            goto done;
        }
        if ( !(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) ){
            sys_rc = ECANCELED;
            if ( asprintf(out_str, "%s exited with failure.\n", daemon) == -1 ){
                err("asprintf: %s\n", strerror(errno));
                *out_str = NULL;
            }
        }
    }

    if ( asprintf(out_str, "%s %s success.\n", daemon, command) == -1 ){
        err("asprintf: %s\n", strerror(errno));
        *out_str = NULL;
    }

done:
    if (daemon) free(daemon);
    if (dpath) free(dpath);
    if (command) free(command);
    (*out_rc) = (uint16_t)sys_rc;
    return rc;
}
