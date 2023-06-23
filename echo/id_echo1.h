#include "inet_sockets1.h" /* Declares our socket functions */
#include "tlpi_hdr1.h"

#define SERVICE "echo" /* Name of UDP service */

#define BUF_SIZE                                                               \
    500 /* Maximum size of datagrams that can                                  \
           be read by client and server */
