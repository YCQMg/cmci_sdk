#ifndef CMCI_ERRNO_H
#define CMCI_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

/* All error codes are negative; zero means success */
#define CMCI_OK                   0
#define CMCI_ERR_PARAM           (-1)
#define CMCI_ERR_NOMEM           (-2)
#define CMCI_ERR_IO              (-3)
#define CMCI_ERR_TIMEOUT         (-4)
#define CMCI_ERR_CRC             (-5)
#define CMCI_ERR_CLOSED          (-6)
#define CMCI_ERR_RETRY_EXCEEDED  (-7)
#define CMCI_ERR_STATE           (-8)
#define CMCI_ERR_BUSY            (-9)
#define CMCI_ERR_REENTRY         (-10)
#define CMCI_ERR_NOT_FOUND       (-11)
#define CMCI_ERR_MSG_TOO_LARGE   (-12)
#define CMCI_ERR_DUP             (-13)

/* Reassembly-specific error codes (detailed sub-types of CMCI_ERR_PARAM) */
#define CMCI_ERR_REASSY_STATE    (-14)  /* !ra->active || msg_id mismatch on non-first frag */
#define CMCI_ERR_REASSY_FRAG_IDX (-15)  /* fragment index >= total count */
#define CMCI_ERR_REASSY_SLOT_SIZE (-16) /* payload_len exceeds slot_size */

/* Return a short string description of an error code */
const char *cmci_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* CMCI_ERRNO_H */
