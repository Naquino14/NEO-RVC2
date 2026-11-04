#ifndef NRVC2_ERRNO_H
#define NRVC2_ERRNO_H

#include <errno.h>

/// General
#define EDEVNOTRDY 2000             /// Device Not Ready
#define ENOINIT 2001                /// System not initialized

/// Storage
#define ESTORAGEMOUNTED 2101        /// Storage device already mounted 
#define ESTORAGENOTMOUNTED 2102     /// Storage device not mounted

/// Security
#define EKEYREGEN 2200              /// Generic key regen error
#define ECRYPTO 2201                /// Generic crypto error
#define ESEQUENCE 2202              /// Sequence number invalid
#define EAUTH 2203                  /// Invalid tag

#endif // NRVC2_ERRNO_H