#ifndef NRVC2_ERRNO_H
#define NRVC2_ERRNO_H

#include <errno.h>

#define EDEVNOTRDY 2000             /// Device Not Ready
#define ESTORAGEMOUNTED 2001        /// Storage device already mounted 
#define ESTORAGENOTMOUNTED 2002     /// Storage device not mounted

#define EKEYREGEN 2100              /// Generic key regen error
#define ECRYPTO 2101                /// Generic crypto error
#define ESEQUENCE 2102              /// Sequence number invalid
#define EAUTH 2103                  /// Invalid tag

#endif // NRVC2_ERRNO_H