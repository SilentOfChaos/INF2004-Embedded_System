/*
 *  app_mbf.h
 *  Message buffer sizes in messages, not bytes.
 *
 *  micro T-Kernel stores each message as a 4-byte header plus the message
 *  rounded up to a multiple of 4 bytes (kernel/tkernel/messagebuf.h,
 *  HEADERSZ and ROUNDSZ).  APP_MBF_BYTES gives the buffer size that holds
 *  exactly count messages of size bytes, so packing a message smaller
 *  frees memory without changing how many messages fit (minimum-bit
 *  change, docs/decisions.md section I).
 *
 *  Pure C: no kernel headers, so the host unit tests can build it.
 */

#ifndef APP_MBF_H
#define APP_MBF_H

#include <stdint.h>

#define APP_MBF_HEADER      4u      /* HEADERSZ: sizeof(INT) */
#define APP_MBF_ROUND       4u      /* ROUNDSZ: messages rounded up to 4 */

/* Kernel storage of one message of size bytes. */
#define APP_MBF_MSG_BYTES(size)                                           \
    (APP_MBF_HEADER +                                                     \
     ((((uint32_t) (size)) + (APP_MBF_ROUND - 1u)) &                      \
      ~(APP_MBF_ROUND - 1u)))

/* Buffer bytes for exactly count messages of size bytes. */
#define APP_MBF_BYTES(count, size)                                        \
    (((uint32_t) (count)) * APP_MBF_MSG_BYTES(size))

#endif /* APP_MBF_H */

/*** end of file ***/
