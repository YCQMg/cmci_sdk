#ifndef CMCI_PTY_H
#define CMCI_PTY_H

#include "cmci_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Public protocol API (platform-independent)
 * =================================================================== */

/* TLV type constants */
#define CMCI_PTY_TYPE_INPUT   'C'   /* Client keyboard input */
#define CMCI_PTY_TYPE_OUTPUT  'S'   /* Server stdout */
#define CMCI_PTY_TYPE_ERROR   'E'   /* Server stderr */
#define CMCI_PTY_TYPE_EXIT    'X'   /* Session exit notification */

/* Exit TLV payload indices */
#define CMCI_PTY_EXIT_REASON  0
#define CMCI_PTY_EXIT_STATUS  1
#define CMCI_PTY_EXIT_PAYLOAD_SIZE  2

/**
 * cmci_pty_tlv_encode - Encode a TLV packet into a flat buffer.
 * @type:      One of CMCI_PTY_TYPE_*.
 * @value:     Payload bytes.
 * @value_len: Number of payload bytes.
 * @out:       Destination buffer.
 * @out_cap:   Capacity of @out.
 * Return: Total encoded length on success, negative CMCI_ERR_* on failure.
 */
int cmci_pty_tlv_encode(uint8_t type, const void *value, uint16_t value_len,
                        void *out, int out_cap);

/**
 * cmci_pty_tlv_decode - Decode a TLV packet from a flat buffer.
 * @buf:       Source buffer.
 * @len:       Number of bytes in @buf.
 * @type:      Output pointer for the decoded type byte.
 * @value:     Output pointer pointing into @buf for the value start.
 * @value_len: Output parameter for the value length.
 * Return: Consumed bytes on success, negative CMCI_ERR_* on failure.
 */
int cmci_pty_tlv_decode(const void *buf, int len,
                        uint8_t *type, const uint8_t **value, uint16_t *value_len);

/**
 * Callback type for cmci_pty_chunk_send.
 * Called once per chunk with the full TLV-encoded buffer.
 * Return 0 to continue, non-zero to abort chunking.
 */
typedef int (*cmci_pty_send_fn)(const void *tlv, int tlv_len, void *user_data);

/**
 * cmci_pty_chunk_send - Split a payload into max_chunk-sized TLV chunks.
 * @type:      One of CMCI_PTY_TYPE_*.
 * @buf:       Payload to send.
 * @buf_len:   Total payload length.
 * @max_chunk: Maximum payload per chunk (before TLV framing).
 * @send_fn:   Callback invoked per chunk with the full TLV.
 * @user_data: Opaque pointer passed to @send_fn.
 * Return: CMCI_OK on success, first error code on failure.
 */
int cmci_pty_chunk_send(uint8_t type, const void *buf, int buf_len,
                        int max_chunk, cmci_pty_send_fn send_fn, void *user_data);

/* ===================================================================
 * Linux server backend API
 * Only available when compiling for Linux.
 * =================================================================== */

#ifdef __linux__

/* Opaque pty handle */
typedef struct cmci_pty cmci_pty_t;

/**
 * Callback invoked when pty shell session ends (shell exits).
 * Called from reader thread context — keep it lightweight.
 * @user_data: Opaque pointer from cmci_pty_config_t.
 */
typedef void (*cmci_pty_session_end_cb)(void *user_data);

/**
 * cmci_pty_config_t - Configuration for cmci_pty_open.
 * @ctx:               Initialised and running CMCI context.
 * @channel_id:        Channel used for output TLV sends.
 * @shell:             Command string, or NULL for interactive shell.
 * @rows:              Initial pty window rows (e.g. 24).
 * @cols:              Initial pty window cols (e.g. 80).
 * @on_session_end:    Optional callback when shell exits (reader thread ctx).
 * @session_end_data:  Opaque pointer passed to @on_session_end.
 */
typedef struct {
	cmci_context_t          *ctx;
	int                      channel_id;
	const char              *shell;
	int                      rows;
	int                      cols;
	cmci_pty_session_end_cb  on_session_end;
	void                    *session_end_data;
} cmci_pty_config_t;

/**
 * cmci_pty_open - Open a pty, fork a shell, and start output forwarding.
 * @out_pty: Output pointer for the new pty handle.
 * @cfg:     Configuration (ctx must be running).
 * Return: CMCI_OK on success, negative CMCI_ERR_* on failure.
 */
int cmci_pty_open(cmci_pty_t **out_pty, const cmci_pty_config_t *cfg);

/**
 * cmci_pty_write - Write data (client input) to the pty master.
 * @pty: Pty handle from cmci_pty_open.
 * @buf: Input payload (already TLV-decoded by caller).
 * @len: Number of bytes to write.
 * Return: Number of bytes written on success, negative CMCI_ERR_* on failure.
 */
int cmci_pty_write(cmci_pty_t *pty, const void *buf, int len);

/**
 * cmci_pty_resize - Send a window resize signal to the pty.
 * @pty:  Pty handle from cmci_pty_open.
 * @rows: New number of rows.
 * @cols: New number of columns.
 * Return: CMCI_OK on success, negative CMCI_ERR_* on failure.
 */
int cmci_pty_resize(cmci_pty_t *pty, int rows, int cols);

/**
 * cmci_pty_close - Close the pty, kill the shell, and free resources.
 * @pty: Pty handle from cmci_pty_open (may be NULL).
 *       Sends an exit TLV before tearing down.
 */
void cmci_pty_close(cmci_pty_t *pty);

#endif /* __linux__ */

#ifdef __cplusplus
}
#endif

#endif /* CMCI_PTY_H */
