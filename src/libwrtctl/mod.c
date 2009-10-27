#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

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
    
    dlerror();
    if ( !(md->dlp = dlopen(module_path, RTLD_LAZY|RTLD_LOCAL)) ){
        errstr = dlerror();
        goto err;
    }
   
    init = dlsym(md->dlp, "mod_init");
    if ( (errstr = dlerror()) )
        goto err;

    if ( init( &md->mod_ctx ) != MOD_OK ){
        asprintf(&errstr, "Failed to initialize %s.\n", module_path);
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
