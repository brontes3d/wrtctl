#ifndef __WRTCTL_INT
#define __WRTCTL_INT
#include "wrtctl-net.h"

/* Allocate a d_data pointer, caller must free the data.
 *  Returns a net_errno
 */
int     create_dd( dd_t *dd, int fd );

/* Free a d_data pointer and any contained pointers.  Sets
 * dd to NULL
 */
void    free_dd( dd_t *dd );


#define free_packet( x ) \
    if ((x)){ \
        if ((x)->data) free((x)->data); \
        free((x)); \
    } \
    (x) = NULL;

/* Sends every packet in a d_data's sendq.  If any packet fails to send,
 * the operation is halted and the error returned.
 */
int flush_sendq( dd_t dd );

/* Wrapper around recv, waits until all data has been read, also attempts to
 * bring in a full struct packet at a time, including associated data.
 */
int recv_packet( dd_t dd );




/* Module Handling:
 *  Used by wrtctld to allow selection of which commands should be handled.
 */
#define MOD_MAGIC_LEN 4
#define MOD_ERRSTR_LEN 512

typedef struct module_list * mlh_t;

typedef struct mod_data *md_t;

struct mod_data {
    char *  mod_name;
    char *  mod_magic_str;
    int     mod_version;
    void *  mod_ctx;
    void *  dlp;
    int     (*mod_handler)(void*, net_cmd_t, packet_t*);
    char *  mod_errstr;
    STAILQ_ENTRY(mod_data) mod_data_list;
};



char *  load_module     (mlh_t ml, md_t *mdp, char *module_name);
void    unload_module   (mlh_t ml, md_t md);
char *  mod_strerror    (int err);


/* Built in Daemon Module internals */
#define DAEMON_MODVER 1
#define DAEMON_MOD_NAME "daemon-cmds"
#define DAE_CTX_CAST(x,y) ns_t x = (ns_t)y
char mod_errstr[MOD_ERRSTR_LEN];
#endif
