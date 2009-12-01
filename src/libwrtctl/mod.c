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
#include <dlfcn.h>
#include <errno.h>

#include <wrtctl-log.h>
#include "wrtctl-int.h"

char * load_module(mlh_t ml, md_t *mdp, char *module_path){
    md_t md = NULL;
    char *errstr = NULL;
    int (*init)(void **);

    if ( !(md = (md_t)malloc(sizeof(struct mod_data))) ){
        errstr = "Insufficient Memory.";
        goto err;
    }
    memset(md, 0, sizeof(struct mod_data));
    md->dlp=NULL;
    
    dlerror();
    if ( !(md->dlp = dlopen(module_path, RTLD_LAZY|RTLD_LOCAL)) ){
        errstr = dlerror();
        goto err;
    }
   
    init = dlsym(md->dlp, "mod_init");
    if ( (errstr = dlerror()) )
        goto err;

    if ( init( &md->mod_ctx ) != MOD_OK ){
        if ( asprintf(&errstr, "Failed to initialize %s.\n", module_path) == -1 )
            err("asprintf: %s\n", strerror(errno));
        goto err;
    }

    md->mod_name = dlsym(md->dlp, "mod_name");
    if ( (errstr = dlerror()) )
        goto err;

    md->mod_magic_str = dlsym(md->dlp, "mod_magic_str");
    if ( (errstr = dlerror()) )
        goto err;

    md->mod_version = *(int*)dlsym(md->dlp, "mod_version");
    if ( (errstr = dlerror()) )
        goto err;

    md->mod_handler = dlsym(md->dlp, "mod_handler");
    if ( (errstr = dlerror()) )
        goto err;
   
    md->mod_errstr = dlsym(md->dlp, "mod_errstr");
    if ( (errstr = dlerror()) )
        goto err;
 
    if ( ml )
        STAILQ_INSERT_TAIL(ml, md, mod_data_list);
    if ( mdp )
        (*mdp) = md;
    return NULL;

err:
    errstr = strdup(errstr);
    if ( md )
        unload_module(NULL, md);

    return errstr;
}

void unload_module(mlh_t ml, md_t md){
    int (*destroy)(void *) = NULL;

    if ( md->dlp ){
        destroy = dlsym(md->dlp, "mod_destroy");
        if ( !dlerror() )
            destroy(md->mod_ctx);
        /* Useful with valgrind */
        if ( !getenv("WRTCTL_NO_DLCLOSE") )
            dlclose(md->dlp);
    }
    if ( ml )
        STAILQ_REMOVE( ml, md, mod_data, mod_data_list );
    free(md);
}

static char *mod_errstr_table[] = {
    /* MOD_OK */        "Success",
    /* MOD_ERR_MEM */   "Insufficient memory",
    /* MOD_ERR_FORK */  "Fork failed",
    /* MOD_ERR_SIG */   "Failed to send signal",
    /* MOD_ERR_PERM */  "Insufficient permission",
    /* MOD_ERR_INVAL */ "Invalid arguments",
    /* MOD_ERR_TPL */   "TPL failed",
    /* MOD_ERR_INT */   "Internal module failure",
    /* MOD_ERR_LOAD */  "Error loading module(s)",
    /* MOD_ERR_MAX */   "Unknown error number"
};

inline char * mod_strerror(int err){
    return mod_errstr_table[err < MOD_ERR_MAX ? err : MOD_ERR_MAX];
}
