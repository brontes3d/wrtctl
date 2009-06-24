#ifndef __WRTCTL_LOG_H
#define __WRTCTL_LOG_H
#include <stdbool.h>
extern bool wrtctl_verbose;
extern bool wrtctl_enable_log;

#define info(str...) \
    if ( wrtctl_verbose ) { \
        if ( wrtctl_enable_log )  syslog(LOG_INFO, str); \
        printf(str); \
    }
    
#define log(str...) \
    if ( wrtctl_enable_log )  syslog(LOG_NOTICE, str); \
    if ( wrtctl_verbose ) printf(str);
    
#define err(str...) \
    if ( wrtctl_enable_log ) syslog(LOG_ERR, str); \
    if ( wrtctl_verbose ) fprintf(stderr, __func__);fprintf(stderr,": " str);

#define err_rc(rc, str...) \
    if ( rc != 0 ) err(str);

#endif
