#ifndef __WRTCTL_NET
#define __WRTCTL_NET
#include <config.h>

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#ifdef INTERNAL_QUEUE_H
#include "queue.h"
#else
#include <sys/queue.h>
#endif

#include <wrtctl-log.h>

#define WRTCTLD_DEFAULT_PORT    "2450"      /* Unencrypted traffic */
#define WRTCTL_SSL_PORT         "2451"      /* Local wrtctl stunnel connection */
#define WRTCTLD_SSL_PORT        "2452"      /* Server wrtctld stunnel connection */

#define MAX_PACKET_SIZE (uint32_t)1024*1024

char *net_strerror(int err);
enum net_errno{
    NET_OK = 0,
    NET_ERR_MEM,
    NET_ERR_FD,
    NET_ERR_INVAL,
    NET_ERR_CONNRESET,
    NET_ERR_PKTSZ,
    NET_ERR_NS,
    NET_ERR_TIMEOUT,
    NET_ERR_TPL,
    NET_ERR,
};


typedef struct d_data *dd_t;        /* Connection data */
typedef struct net_server *ns_t;    /* Daemon status, handles multiple connections */
typedef struct net_client *nc_t;    /* Client status, connects to a single daemon */
typedef struct net_cmd *net_cmd_t;  /* Simple command type, (uint16_t, char*, char*) */
typedef struct packet *packet_t;    /* Low level packet */


/* Module Handling:
 *  Used by wrtctld to allow selection of which commands should be handled
 *  Defined in mod.c, used in struct net_server * and modules.
 */
#define MOD_MAGIC_LEN 4
#define MOD_ERRSTR_LEN 512
struct mod_data;
enum mod_errno {
    MOD_OK = 0,
    MOD_ERR_MEM,
    MOD_ERR_FORK,
    MOD_ERR_SIG,
    MOD_ERR_PERM,
    MOD_ERR_INVAL,
    MOD_ERR_TPL,
    MOD_ERR_INT,
    MOD_ERR_LOAD,
    MOD_ERR_MAX
};



/* Packet types */
#define NET_CMD_MAGIC "NET"     /* struct net_cmd */
#define CMD_ID_LEN 4            /* Used to identify net_cmd packet type */

/* UCI Commands module, NET packet */
#define UCI_CMDS_MAGIC "UCI"
#define UCI_CMD_NONE    (uint16_t)0
#define UCI_CMD_SET     (uint16_t)1
#define UCI_CMD_GET     (uint16_t)2
#define UCI_CMD_COMMIT  (uint16_t)3
#define UCI_CMD_REVERT  (uint16_t)4
#define UCI_CMD_MAX     (uint16_t)5

/* System Commands module, NET packet */
#define SYS_CMDS_MAGIC "SYS"
#define SYS_CMD_NONE    (uint16_t)0
#define SYS_CMD_INITD   (uint16_t)1
#define SYS_CMD_FWVER   (uint16_t)2 /* TODO */
#define SYS_CMD_MAX     (uint16_t)3

/* Daemon Commands built-in, NET packet */
#define DAEMON_CMD_MAGIC "DAE"
#define DAEMON_CMD_NONE         (uint16_t)0
#define DAEMON_CMD_PING         (uint16_t)1
#define DAEMON_CMD_SHUTDOWN     (uint16_t)2


struct net_cmd {
    /* Simple packet structure */
    uint16_t    id;         /* Either command identifier or return code */
    char *      subsystem;  /* Module that should handle this command */
    char *      value;
};

int create_net_cmd_packet( packet_t *p, uint16_t id, char *subsystem, char *value );
int unpack_net_cmd_packet( net_cmd_t nc, packet_t p );
#define free_net_cmd_strs(x) \
    if ( x.subsystem ) \
        free(x.subsystem); \
    if ( x.value ) \
        free(x.value);




/* Client and Server structures */
struct net_server {
    int     port;
    int     listen_fd;
    bool    shutdown;
    void    *ctx;
    bool    enable_log;
    bool    verbose;

    char    *shutdown_path;

    int     (*server_loop)(ns_t);
    int     (*handler)(ns_t, dd_t);
    void    (*shutdown_dd)(ns_t, dd_t);
    STAILQ_HEAD(dd_list, d_data) dd_list;
    STAILQ_HEAD(module_list, mod_data) mod_list;
};

/* Creates a net_server structure on the given port.  Modules is a string, seperated by
 * commas, of the modules that need to be loaded.
 *  Returns a net_errno.
 */
int create_ns( ns_t *ns, char *port, char *modules, bool enable_log, bool verbose );

void free_ns( ns_t *ns );

/* Default server loop.  Runs through connections, accepts and hands any packets off
 * to the server handler.
 *  Returns a net_errno.
 */
int default_server_loop( ns_t ns );

/* Defaut packet handler.  Runs through modules to see if anyone can handle the recv'd
 * packet.  Potentially adding outgoing packets to the server's sendq.
 *  Returns a net_errno.
 *  Note:  Internal module failures should not be considered a handler failure.  If the
 *  handler is able to pass the packet off, it is a success.  Module errors should be
 *  handled/reported in the module and potentially sent back to the client.
 */
int default_handler( ns_t ns, dd_t dd );

/* Default mechanism for closing off a client connection */
void default_shutdown_dd( ns_t, dd_t dd );


struct net_client {
    dd_t    dd;
    bool    enable_log;
    bool    verbose;
};

/* Create a client, caller is responsible for freeing the allocated structure.
 *  Returns a net_error.
 */
int alloc_client(nc_t *nc, bool enable_log, bool verbose);

/* Create a client connection to the specified server.
 *  Returns a net_error.
 */
int create_conn(nc_t nc, char *to, char *port);

/* Close the connection associated with the client. */
void close_conn(nc_t nc);

#define nc_add_packet(nc, p) \
    STAILQ_INSERT_TAIL( &(nc->dd->sendq), p, packet_queue )

#define free_packet( x ) \
    if ((x)){ \
        if ((x)->data) free((x)->data); \
        free((x)); \
    } \
    (x) = NULL;

/* Wait timeout length for the server to respond.  A single complete
 * packet is considered a response.
 *  Returns a net_error
 */
int wait_on_response(nc_t nc, struct timeval *timeout, bool send_packets);


/* Connection data structure */
struct d_data {
    char    *host;
    int     fd;
    bool    shutdown;
    int     dd_errno;
    
    STAILQ_ENTRY(d_data)        dd_queue;
    STAILQ_HEAD(sendq, packet)  sendq;
    STAILQ_HEAD(recvq, packet)  recvq;
};


struct packet {
    uint32_t    len;        /* Length of the entire packet, len+cmd_id+data_len */
    char        cmd_id[CMD_ID_LEN];
    void        *data;
    STAILQ_ENTRY(packet) packet_queue;
};


/* Stunnel wrapper stuff */
#ifdef ENABLE_STUNNEL
typedef struct stunnel_ctx {
    char *  conf_file_path;
    char *  pid_file_path;
    FILE *  cf;
    pid_t   pid;
} * stunnel_ctx_t;

/*  The following two functions wrap the configuration and forking of a stunnel
 *  instance.
 *      - hostname:     Host to for the client to connect to.
 *      - key_path:     Path to the shared certificate to accept on both sides.
 *      - port:         Port that the unencrypted daemon listens on.  Default is WRTCTL_DEFAULT_PORT.
 *      - wrtctl_port   Port for encrypted client to connect to locally.  Default is WRTCTL_SSL_PORT.
 *      - wrtctld_port  Port the encrypted daemon listens on remotely.  Default is WRTCTLD_SSL_PORT.
 */
int start_stunnel_client(
    stunnel_ctx_t * ctx,
    char *          hostname,
    char *          key_path,
    char *          port,
    char *          wrtctl_port,
    char *          wrtctld_port );

int start_stunnel_server(
    stunnel_ctx_t * ctx,
    char *          key_path,
    char *          port,
    char *          wrtctl_port,
    char *          wrtctld_port );

/*
 * Stop the stunnel process and cleanup all memory.
 */
int kill_stunnel( stunnel_ctx_t *ctx );
#endif

#endif
