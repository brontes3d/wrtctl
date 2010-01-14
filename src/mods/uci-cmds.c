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
#include <errno.h>
#include <uci.h>
#include "wrtctl-net.h"

/* Handles updating uci files.
 * Supported:
 *  - Updating existing options.
 *  - Fetching existing options.
 *  - Sections:
 *      - If section is not given (NULL or '\0'), the lookup for the section
 *        automatically returns the first section in the given package containing
 *        the desired options.
 *      - Extended lookup via uci_lookup_ptr is supported.
 *  - Commiting changes.
 *  - Reverting changes.
 * Not Supported:
 *  - Adding or removing packages, sections or options.
 */

typedef struct uci_context  * uci_context_t;
typedef struct uci_element  * uci_element_t;
typedef struct uci_package  * uci_package_t;
typedef struct uci_section  * uci_section_t;
typedef struct uci_option   * uci_option_t;

struct ucih_ctx;
typedef struct ucih_ctx * ucih_ctx_t;

struct ucih_ctx {
    uci_context_t uci_ctx;
    bool revert;
};

/* The following three are just useful in debugging.  They print to stdout. */
int uci_list_packages           (ucih_ctx_t ucih);
int uci_list_package_contents   (ucih_ctx_t ucih, char *package_name);
int uci_list_section            (ucih_ctx_t ucih, uci_section_t s);



#define UCI_CMDS_MODVER 1
#define UCI_CMDS_NAME "uci-cmds"
#define CTX_CAST(x,y) ucih_ctx_t x = (ucih_ctx_t)y

char mod_name[] = UCI_CMDS_NAME;
char mod_magic_str[MOD_MAGIC_LEN] = UCI_CMDS_MAGIC;
int  mod_version = UCI_CMDS_MODVER;
char mod_errstr[MOD_ERRSTR_LEN];

int     mod_init        (void **ctx);
void    mod_destroy     (void *ctx);
int     mod_handler     (void *ctx, net_cmd_t cmd, packet_t *outp);


/* Given a package and option, returns an allocated string in *s that points to
 * the first section in the given package that contains such an option.  It is
 * up to the caller to free the string when finished.
 */
int uci_find_section            (ucih_ctx_t ucih, char **s, char *p, char *o);

/* Wraps the loading of a package and returns the desiered uci_package pointer in *p.  */
int uci_load_package            (ucih_ctx_t ucih, uci_package_t *package, char *p);

/* Makes sure that pso contains a section value.  If not, the string is replaced
 * with the one that contains the first section in package that contains the given
 * option.
 */
int uci_fill_section            (ucih_ctx_t ucih, char **pso);

int uci_cmd_set     (ucih_ctx_t ucih, char *value, uint16_t *out_rc,  char **out_str);
int uci_cmd_get     (ucih_ctx_t ucih, char *value, uint16_t *out_rc,  char **out_str);
int uci_cmd_commit  (ucih_ctx_t ucih, char *value, uint16_t *out_rc,  char **out_str);
int uci_cmd_revert  (ucih_ctx_t ucih, char *value, uint16_t *out_rc,  char **out_str);

int mod_init(void **mod_ctx){
    int rc = MOD_OK;
    char *path = NULL;
    ucih_ctx_t ctx = NULL;
    
    if ( !(ctx = (ucih_ctx_t)malloc(sizeof(struct ucih_ctx))) ){
        rc = MOD_ERR_MEM;
        goto err;
    }
    *mod_ctx = NULL;

    ctx->revert = true;

    if ( !(ctx->uci_ctx = uci_alloc_context()) ){
        rc = MOD_ERR_MEM;
        goto err;
    }

    path = getenv("WRTCTL_UCI_CONFDIR");
    if ( !path ){
        path = "/etc/config/";
    }
    if ( uci_set_confdir(ctx->uci_ctx, path ) != UCI_OK ){
        rc = MOD_ERR_MEM;
        goto err;
    }

    path = getenv("WRTCTL_UCI_SAVEDIR");
    if ( !path ){
        path = "/tmp/.uci/";
    }
    if ( uci_set_savedir(ctx->uci_ctx, path ) != UCI_OK ){
        rc = MOD_ERR_MEM;
        goto err;
    }

    if ( getenv("WRTCTL_UCI_NO_REVERT") != NULL )
        ctx->revert = false;

    (*mod_ctx) = ctx;
    return MOD_OK;

err:
    if( ctx )
        mod_destroy((void*)ctx);
    return rc;
}

void mod_destroy(void *ctx){
    CTX_CAST(ucihc, ctx);
    if ( ctx ){
        if ( ucihc->revert )
            uci_cmd_revert(ucihc, NULL, NULL, NULL);
        uci_free_context(ucihc->uci_ctx);
        free(ucihc);
    }
    return;
}

int mod_handler(void *ctx, net_cmd_t cmd, packet_t *outp){
    int rc = MOD_OK;
    CTX_CAST(ucihc, ctx);
    uint16_t out_rc = UCI_OK;
    char *out_str = NULL;

    //UCIH_DEBUG("%s: cmd=%u, args='%s'\n",
        //__func__, cmd->id, cmd->value ? cmd->value : "(null)");

    switch ( cmd->id ){
        case UCI_CMD_SET:
            rc = uci_cmd_set(ucihc, cmd->value, &out_rc, &out_str);
            break;
        case UCI_CMD_GET:
            rc = uci_cmd_get(ucihc, cmd->value, &out_rc, &out_str);
            break;
        case UCI_CMD_COMMIT:
            rc = uci_cmd_commit(ucihc, cmd->value, &out_rc, &out_str);
            break;
        case UCI_CMD_REVERT:
            rc = uci_cmd_revert(ucihc, cmd->value, &out_rc, &out_str);
            break;
    }

    rc = create_net_cmd_packet(outp, out_rc, UCI_CMDS_MAGIC, out_str);
    if (out_str)
        free(out_str);
    
    return rc;
}

int uci_cmd_set(ucih_ctx_t ucihc, char *value, uint16_t *out_rc, char **out_str){
    int uci_rc;
    int rc = 0;
    struct uci_ptr ucip;
    char *pso = NULL, *v = NULL;
    size_t vlen;

    if ( !value || !(vlen = strlen(value)) ){
        uci_rc = UCI_ERR_INVAL;
        if ( asprintf(out_str, "uci_cmd_set:  Invalid command line.") == -1 ){
            err("asprintf: %s\n", strerror(errno))
            *out_str = NULL;
        }
        goto done;
    }

     if ( !(pso = strdup(value)) ){
        uci_rc = UCI_ERR_MEM;
        if ( asprintf(out_str, "uci_cmd_set:  Out of Memory.\n") == -1 ){
            err("asprintf: %s\n", strerror(errno))
            *out_str = NULL;
        }
        goto done;
    }
 
    if ( !(pso = strtok(pso, "="))
            || strlen(pso) >= vlen ){
        uci_rc = UCI_ERR_INVAL;
        if ( asprintf(out_str, "uci_cmd_set:  Invalid command line.") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    if ( !(v = strdup(value + strlen(pso) + 1))){
        uci_rc = UCI_ERR_MEM;
        if ( asprintf(out_str, "uci_cmd_set:  Out of Memory.\n") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    if ( (uci_rc = uci_fill_section(ucihc, &pso)) != UCI_OK ){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_cmd_set:uci_fill_section");
        goto done;
    }

    if ( (uci_rc = uci_lookup_ptr(ucihc->uci_ctx, &ucip, pso, true)) != UCI_OK ){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_lookup_ptr");
        goto done;
    }
    /* The above replaces the two separators '.' with '\0'' */

    ucip.value = v;
    
    if ( (uci_rc = uci_set(ucihc->uci_ctx, &ucip)) != UCI_OK){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_set");
        goto done;
    }

    if ( (uci_rc = uci_save(ucihc->uci_ctx, ucip.p)) != UCI_OK){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_save");
        goto done;
    }

    rc = 0;

    //UCIH_DEBUG("%s:  Set %s = %s\n", __func__, pso, v);
done:
    if ( v )
        free(v);
    if ( pso )
        free(pso);
    (*out_rc) = (uint16_t)uci_rc;
    return rc;
}

int uci_cmd_get(ucih_ctx_t ucihc, char *value, uint16_t *out_rc, char **out_str){
    int             uci_rc = UCI_OK;
    struct uci_ptr  ucip;
    bool            restore_pso = false;
    char *          val_str =  NULL;
    char *          full_pso = NULL;


    if ( !value ){
        uci_rc = UCI_ERR_INVAL;
        if ( asprintf(out_str, "uci_cmd_get:  Invalid command line.") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    /*
     * As value is part of the incoming packet, we can't mess with it in uci_fill_section
     * or uci_lookup_ptr.  So we're taking a copy of it here.
     */
    if ( !(full_pso = strdup(value)) ){
        uci_rc = UCI_ERR_MEM;
        if ( asprintf(out_str, "uci_cmd_get:  Memory allocation failure.") == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }

    if ( (uci_rc = uci_fill_section(ucihc, &full_pso)) != UCI_OK ){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_cmd_get:uci_fill_section");
        goto done;
    }
    
    if ( (uci_rc = uci_lookup_ptr(ucihc->uci_ctx, &ucip, full_pso, true)) != UCI_OK ){
        uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_lookup_ptr");
        goto done;
    }
    /* The above replaces the two separators '.' with '\0'' */

    if ( !(ucip.flags & UCI_LOOKUP_COMPLETE) ){
        full_pso[strlen(full_pso)] = '.';
        full_pso[strlen(full_pso)] = '.';
        uci_rc = UCI_ERR_NOTFOUND;
        if ( asprintf(out_str, "%s not found", full_pso) == -1 ){
            err("asprintf: %s\n", strerror(errno));
            *out_str = NULL;
        }
        goto done;
    }
    
    restore_pso = true;

    switch (ucip.o->type){
        case UCI_TYPE_STRING:
            if( ucip.o->v.string )
                val_str = ucip.o->v.string;
            break;
        case UCI_TYPE_LIST:
            {
                uci_element_t e;
                char *listbuf, *p = NULL;
                size_t listlen = 0;
                bool need_sep = false;

                uci_foreach_element( &ucip.o->v.list, e ){
                    listlen += strlen(e->name)+2;
                }

                if ( !(listbuf=(char*)malloc(sizeof(char)*(listlen+1))) ){
                    if ( asprintf(out_str, "Insufficient memory.") == -1 ){
                        err("asprintf: %s\n", strerror(errno));
                        *out_str = NULL;
                    }
                    uci_rc = UCI_ERR_MEM;
                    goto done;
                }
           
                p = listbuf;
                uci_foreach_element( &ucip.o->v.list, e ){
                    p += sprintf(p, "%s%s", e->name, need_sep ? ", " : "");
                    *p='\0';
                }
                val_str = listbuf;
            }
            break;
    }

done:

    /* Restore our pso from the damage done by uci_lookup_ptr */
    if ( restore_pso ){
        full_pso[strlen(full_pso)] = '.';
        full_pso[strlen(full_pso)] = '.';
    }

    if ( uci_rc == UCI_OK ){
        if ( asprintf(out_str, "%s=%s", full_pso, val_str ? val_str : "(null)") < 0 ){
            (*out_str) = NULL;
            uci_rc = UCI_ERR_MEM;
        }
    }

    if ( full_pso )
        free(full_pso);

    (*out_rc) = (uint16_t)uci_rc;
    //UCIH_DEBUG("%s:  Returning %s\n", __func__, (*out_str));
    return 0;
}

int uci_cmd_commit(ucih_ctx_t ucihc, char *value, uint16_t *out_rc, char **out_str){
    int uci_rc = UCI_OK;
    uci_package_t p;
    char *pn;

    /* The package to commit was specified. */
    if ( value){
        pn = strchr(value, '.');

        if ( pn ) *pn = '\0';

        if ( (uci_rc = uci_load_package(ucihc, &p, value)) != UCI_OK ){
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_load_package");
            goto done;
        }

        if ( (uci_rc = uci_commit(ucihc->uci_ctx, &p, true)) != UCI_OK ){
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_commit");
        }
        //UCIH_DEBUG("%s:  Committing %s %s.\n",
            //__func__, value, uci_rc == UCI_OK ? "succeeded" : "failed");

        if ( pn ) *pn = '.';
    
    } else {
    /* Commit everything */
        char **configs;
        int loop_rc;
        int i;


        if ( (uci_rc = uci_list_configs(ucihc->uci_ctx, &configs)) != UCI_OK ){
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_list_configs");
            goto done;
        }

        for ( i=0,pn=configs[i]; configs[i]; pn=configs[i],i++){
            if ( (loop_rc = uci_load_package(ucihc, &p, pn)) != UCI_OK ){
                uci_rc = loop_rc;
                continue;
            }

            if ( (loop_rc = uci_commit(ucihc->uci_ctx, &p, true)) != UCI_OK ){
                uci_rc = loop_rc;
            }

            //UCIH_DEBUG("%s:  Committing %s %s.\n",
                //__func__, pn, loop_rc == UCI_OK ? "succeeded" : "failed");
        }
        if ( uci_rc != UCI_OK )
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_commit_all");
    }

done:

    (*out_rc) = (uint16_t)uci_rc;
    return 0;
}

int uci_cmd_revert(ucih_ctx_t ucihc, char *value, uint16_t *out_rc, char **out_str){
    int uci_rc = UCI_OK;
    char *pn;
    struct uci_ptr ucip;
    bool revert_pso = false;

    if ( value ){
        pn = strchr(value, '.');
        
        if ( pn ){
            revert_pso = true;
            *pn = '\0';
        }

        if ( (uci_rc = uci_lookup_ptr(ucihc->uci_ctx, &ucip, value, false)) != UCI_OK ){
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_lookup_ptr");
            goto done;
        }

        if ( (uci_rc = uci_revert(ucihc->uci_ctx, &ucip )) != UCI_OK ){
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_revert");
            goto done;
        }

        //UCIH_DEBUG("%s:  Reverted %s.\n", __func__, value);

        if ( revert_pso ){
            revert_pso = false;
            *pn = '.';
        }

    } else {
        char **configs = NULL;
        int loop_rc;
        int i;


        /* XXX:  As of at least uci-0.7.4, this function leaks when a glob of the confdir
         * returns nonzero (empty directory).
         *
         * Patched in Brontes3d portage overlay.
         */
        if ( (uci_rc = uci_list_configs(ucihc->uci_ctx, &configs)) != UCI_OK ){
            if ( out_str )
                uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_list_configs");
            goto done;
        }

        for ( i=0,pn=configs[i]; configs[i]; i++,pn=configs[i] ){
            if ( (loop_rc = uci_lookup_ptr(ucihc->uci_ctx, &ucip, pn, false)) != UCI_OK ){
                uci_rc = loop_rc;
                continue;
            }

            if ( (loop_rc = uci_revert(ucihc->uci_ctx, &ucip)) != UCI_OK ){
                uci_rc = loop_rc;
                continue;
            }
            //UCIH_DEBUG("%s: Reverted %s\n", __func__, pn);
        }
        if ( uci_rc != UCI_OK && out_str )
            uci_get_errorstr(ucihc->uci_ctx, out_str, "uci_revert_all");
        free(configs);
    }

done:
    if ( revert_pso && pn )
        *pn = '.';
    if ( out_rc )
        (*out_rc) = (uint16_t)uci_rc;
    return 0;
}

int uci_list_packages(ucih_ctx_t ucihc){
    int rc, i;
    char **packages = NULL;

    if ( (rc = uci_list_configs(ucihc->uci_ctx, &packages)) != UCI_OK ){
        uci_perror(ucihc->uci_ctx, "uci_list_packages:uci_list_configs:  ");
        return rc;
    }

    for (i=0; packages[i] != NULL; i++){
        printf("%s\n", packages[i]);
        uci_list_package_contents(ucihc, packages[i]);
    }
    free(packages);
    return UCI_OK;
}

int uci_list_package_contents(ucih_ctx_t ucihc, char *package_name){
    uci_package_t package = NULL;
    uci_section_t section = NULL;
    uci_element_t e = NULL;
    int rc, i;

    if ( (rc = uci_load_package(ucihc, &package, package_name)) != UCI_OK ){
        return rc;
    }

    i=0;
    uci_foreach_element( &package->sections, e ){
        section = uci_to_section(e);
        if (section->anonymous){
            printf("\t%s%d\n", package_name, i);
            i+=1;
        } else
            printf("\t%s\n", e->name);
        uci_list_section(ucihc, section);
    }
    uci_unload(ucihc->uci_ctx, package);
    return 0;
}

int uci_list_section(ucih_ctx_t ucihc, uci_section_t s){
    uci_element_t e, _e = NULL;
    uci_option_t o = NULL;

    uci_foreach_element(&s->options, e ){
        o = uci_to_option(e);
        printf("\t\t%s:  ", e->name);
        switch (o->type){
            case UCI_TYPE_STRING:
                printf("%s\n", o->v.string);
                break;
            case UCI_TYPE_LIST:
                printf("LIST:  ");
                uci_foreach_element( &o->v.list, _e ){
                    printf("%s ", _e->name);
                }
                printf("\n");
                break;
        }
    }
    return 0;
}

// Caller must free the returned string.
int uci_find_section(ucih_ctx_t ucihc, char **s, char *p, char *o ){
    uci_element_t se, oe = NULL;
    uci_package_t package = NULL;
    uci_section_t section = NULL;
    uci_option_t option = NULL;
    int rc;
    bool found = false;

    *s = NULL;
  
    if ( (rc = uci_load_package(ucihc, &package, p)) != UCI_OK ){
        return rc;
    }

    rc = UCI_ERR_NOTFOUND;
    uci_foreach_element( &package->sections, se){
        section = uci_to_section(se);
        if ( !section->anonymous )
            continue;
        uci_foreach_element( &section->options, oe){
            option = uci_to_option(oe);
            if ( !strcmp(o, oe->name) ){
                found = true;
                if ( !((*s) = strdup(se->name)) )
                    rc = UCI_ERR_MEM;
                else
                    rc = UCI_OK;
                break; 
            }
        }
        if (found)
            return UCI_OK;
    }

    //UCIH_DEBUG("%s: rc=%d p=%s, o=%s => s=%s\n",
        //__func__, rc, p, o, (*s) ? *s : "(null)" );
    return UCI_ERR_NOTFOUND;
}


int uci_load_package(ucih_ctx_t ucihc, uci_package_t *package, char *p ){
    int rc;
    uci_element_t e = NULL; 

    (*package) = NULL;

    /* I used to unload at the end of any function that loaded.  However,
     * that didn't seem to actually remove the package.  Also, it's pretty
     * likely I'll need to load the package again anyways.  So here's
     * a search to see if it's already loaded.
     */
    uci_foreach_element( &ucihc->uci_ctx->root, e ){
        if ( e->type == UCI_TYPE_PACKAGE && !strcmp(e->name, p) ){
            (*package) = uci_to_package(e);
        }
    }
    
    if ( !(*package) ){
        if ( !(rc = uci_load(ucihc->uci_ctx, p, package)) == UCI_OK ){
            //UCIH_DEBUG("%s:uci_load(%s):  uci_rc=%d\n",
                //__func__, p, rc);
            return UCI_ERR_NOTFOUND;
        }
    }
    return UCI_OK;
}


int uci_fill_section(ucih_ctx_t ucihc, char **pso){
    char *first_sep, *second_sep = NULL;

    if ( !(*pso) )
        return UCI_ERR_INVAL;

    /* Things are a lot simpler if we just do the copy up here.  Then
     * our indexes are correct and we can mess with the string later if
     * necessary due to a missing section.
     */

    first_sep = index((*pso), '.');
    second_sep = rindex((*pso), '.');

    if ( !first_sep || (void*)first_sep == (void*)second_sep ){
        return UCI_ERR_INVAL;
    } else if ( (void*)second_sep != (void*)(first_sep+1) ){
        return UCI_OK;
    } else {
        /* Play some trickery.  We can insert a null string character in place
         * of the first seperator, then search and create the new pso string.
         * Afterwards we replace the original seperator.
         */
        char *section, *new_pso, *final_pso = NULL;
        size_t len = strlen((*pso)) + 1;
        char *p, *o;
        int rc;

        if ( !( new_pso = strdup((*pso)) ) ){
            return UCI_ERR_MEM;
        }

        p = new_pso;
        o = new_pso + (second_sep - (*pso)) + 1;

        p[(void*)first_sep - (void*)(*pso)] = '\0';
        if( !(rc = uci_find_section(ucihc, &section, p, o)) == UCI_OK ){
            free(new_pso);
            return rc;
        }

        len += strlen(section);
        if( !(final_pso = (char*)malloc(sizeof(char) * len)) ){
            free(section);
            free(new_pso);
            return UCI_ERR_MEM;
        }

        sprintf(final_pso, "%s.%s.%s", p, section, o);
        //UCIH_DEBUG("%s: %s -> %s\n", __func__, *pso, final_pso);
        free(section);
        free(new_pso);

        free( (*pso) );
        (*pso) = final_pso;
    }
    return UCI_OK;
}
