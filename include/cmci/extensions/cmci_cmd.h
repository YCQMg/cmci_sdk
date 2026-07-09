#ifndef CMCI_CMD_H
#define CMCI_CMD_H

#include "cmci_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 * Public protocol API (platform-independent, always compiled)
 * =================================================================== */

/* TLV type constants */
#define CMCI_CMD_TYPE_COMMAND  'C'   /* Client->Server: async command */
#define CMCI_CMD_TYPE_SYNC     'S'   /* Client->Server: sync command (no output streaming) */
#define CMCI_CMD_TYPE_KILL     'K'   /* Client->Server: kill running command */
#define CMCI_CMD_TYPE_STDOUT   'O'   /* Server->Client: stdout output */
#define CMCI_CMD_TYPE_STDERR   'E'   /* Server->Client: stderr output */
#define CMCI_CMD_TYPE_EXIT     'X'   /* Server->Client: exit notification */

/* X-type TLV payload layout: cmd_id(1) + exit_code(1) + flags(1) */
#define CMCI_CMD_EXIT_PAYLOAD_SIZE       3
#define CMCI_CMD_EXIT_OFFSET_CMD_ID      0  /* uint8_t */
#define CMCI_CMD_EXIT_OFFSET_CODE        1  /* uint8_t */
#define CMCI_CMD_EXIT_OFFSET_FLAGS       2  /* uint8_t */

/* Exit flags */
#define CMCI_CMD_EXIT_FLAG_NORMAL  0x00  /* Normal exit */
#define CMCI_CMD_EXIT_FLAG_KILLED  0x01  /* Killed by client request */

/* Default constants */
#define CMCI_CMD_DEFAULT_CHUNK_SIZE    4096
#define CMCI_CMD_DEFAULT_MAX_WORKERS   4
#define CMCI_CMD_MAX_PENDING           64
#define CMCI_CMD_COMMAND_OFFSET_STR    1    /* cmd_id(1) + cmd_str(N) */

/**
 * cmci_cmd_tlv_encode - Encode a TLV packet into a flat buffer.
 * @type:      One of CMCI_CMD_TYPE_*.
 * @value:     Payload bytes.
 * @value_len: Number of payload bytes.
 * @out:       Destination buffer.
 * @out_cap:   Capacity of @out.
 * Return: Total encoded length on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_tlv_encode(uint8_t type, const void *value, uint16_t value_len,
                        void *out, int out_cap);

/**
 * cmci_cmd_tlv_decode - Decode a TLV packet from a flat buffer.
 * @buf:       Source buffer.
 * @len:       Number of bytes in @buf.
 * @type:      Output pointer for the decoded type byte.
 * @value:     Output pointer pointing into @buf for the value start.
 * @value_len: Output parameter for the value length.
 * Return: Consumed bytes on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_tlv_decode(const void *buf, int len,
                        uint8_t *type, const uint8_t **value, uint16_t *value_len);

/**
 * Callback type for cmci_cmd_chunk_send.
 * Called once per chunk with the full TLV-encoded buffer.
 * Return 0 to continue, non-zero to abort chunking.
 */
typedef int (*cmci_cmd_send_fn)(const void *tlv, int tlv_len, void *user_data);

/**
 * cmci_cmd_chunk_send - Split a payload into max_chunk-sized TLV chunks.
 *
 * @note v1 server does NOT reassemble multi-chunk payloads per cmd_id.
 *       Each TLV chunk is treated as an independent command by the server.
 *       Therefore commands SHOULD NOT exceed max_chunk in length.
 *       Typical safe limits:
 *         - UDP mode:  CMCI_UDP_PAYLOAD_MTU - 3 (1021 bytes)
 *         - UART mode: CMCI_UART_PAYLOAD_MTU - 3 (253 bytes)
 *       The chunking mechanism exists for protocol completeness and future
 *       reassembly support. For v1, ensure each command fits in one chunk.
 *
 * @type:      One of CMCI_CMD_TYPE_*.
 * @buf:       Payload to send.
 * @buf_len:   Total payload length.
 * @max_chunk: Maximum payload per chunk (before TLV framing).
 * @send_fn:   Callback invoked per chunk with the full TLV.
 * @user_data: Opaque pointer passed to @send_fn.
 * Return: CMCI_OK on success, first error code on failure.
 */
int cmci_cmd_chunk_send(uint8_t type, const void *buf, int buf_len,
                        int max_chunk, cmci_cmd_send_fn send_fn, void *user_data);

/* ===================================================================
 *  cmd_id 高层 helper
 *  基于通用 TLV encode/decode 封装, 自动处理 cmd_id 编解码
 * =================================================================== */

/**
 * cmci_cmd_encode_command - Encode a C-type TLV with cmd_id prefix.
 *
 * @note Internal buffer limits command to 256 bytes (cmd_id + command).
 *       This is sufficient for typical shell commands. For longer payloads,
 *       use cmci_cmd_send_request which handles chunking transparently.
 *
 * @cmd_id:    Client-assigned command id (uint8_t).
 * @command:   Command string.
 * @cmd_len:   Length of @command.
 * @out:       Destination buffer.
 * @out_cap:   Capacity of @out.
 * Return: Total encoded length on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_encode_command(uint8_t cmd_id, const void *command, uint16_t cmd_len,
                            void *out, int out_cap);

/**
 * cmci_cmd_encode_kill - Encode a K-type TLV with cmd_id.
 * @cmd_id:    Command id to kill.
 * @out:       Destination buffer.
 * @out_cap:   Capacity of @out.
 * Return: Total encoded length on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_encode_kill(uint8_t cmd_id, void *out, int out_cap);

/**
 * cmci_cmd_encode_exit - Encode an X-type TLV with cmd_id + exit info.
 * @cmd_id:    Command id.
 * @exit_code: Exit code (0-255).
 * @flags:     CMCI_CMD_EXIT_FLAG_*.
 * @out:       Destination buffer.
 * @out_cap:   Capacity of @out.
 * Return: Total encoded length on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_encode_exit(uint8_t cmd_id, uint8_t exit_code, uint8_t flags,
                         void *out, int out_cap);

/**
 * cmci_cmd_parse_cmd_id - Extract cmd_id from the start of a TLV payload.
 * @value:      TLV value pointer (from cmci_cmd_tlv_decode).
 * @value_len:  TLV value length.
 * @out_cmd_id: Output for the extracted cmd_id.
 * Return: Number of bytes consumed by cmd_id (1) on success,
 *         negative CMCI_ERR_PARAM if value_len < 1.
 */
int cmci_cmd_parse_cmd_id(const void *value, uint16_t value_len,
                          uint8_t *out_cmd_id);

/* ===================================================================
 *  Client helper API (platform-independent)
 * =================================================================== */

/**
 * cmci_cmd_send_request - Encode and send an async command request with cmd_id.
 * Builds a C-type TLV with payload [cmd_id(1B)][command(NB)], splits into
 * max_chunk-sized chunks, and sends each via cmci_channel_send.
 *
 * @note Completely automates chunking: for long commands, the payload is split
 *       into multiple C-type TLVs. Each TLV on the wire has the cmd_id prefix
 *       so the server can reassemble per-cmd_id context.
 *
 * @param ctx         CMCI context (must be started).
 * @param channel_id  Channel to send on.
 * @param cmd_id      Client-assigned command id (monotonically increasing).
 * @param command     Command string (null-terminated).
 * @param max_chunk   Max payload bytes per TLV chunk (before TLV framing).
 *                    Recommended: CMCI_UDP_PAYLOAD_MTU - 3 for UDP,
 *                    CMCI_UART_PAYLOAD_MTU - 3 for UART.
 * @return CMCI_OK on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_send_request(cmci_context_t *ctx, int channel_id,
                          uint8_t cmd_id,
                          const char *command, int max_chunk);

/**
 * cmci_cmd_sync_exec - Synchronous command execution.
 *
 * Send a command via S-type TLV, then block waiting for the X-type TLV result.
 * No stdout/stderr is streamed back — only the exit code is returned.
 *
 * Internal mechanism:
 *   - Allocates a sync slot (tagged with cmd_id) from a global pool.
 *   - Encodes S-type TLV and sends via cmci_channel_send.
 *   - Blocks on a condition variable, waiting for the matching X TLV.
 *   - The client's recv_cb must call cmci_cmd_sync_signal() on X TLV arrival
 *     to wake the waiting thread and propagate exit code.
 *   - On timeout, sends K-type TLV to clean up the server-side command.
 *
 * Cross-platform: uses cmci_platform_mutex_t / cmci_platform_cond_t,
 * so it works on both Linux and Windows without platform #ifdef.
 *
 * @param ctx          CMCI context.
 * @param channel_id   Channel to send on.
 * @param cmd_id       Client-assigned command id.
 * @param command      Command string.
 * @param cmd_len      Length of @command.
 * @param out_exit_code  Output for the exit code (0-255).
 * @param timeout_ms   Maximum wait time in milliseconds (0 = infinite).
 * @return CMCI_OK on success, CMCI_ERR_TIMEOUT if no X TLV within timeout,
 *         negative CMCI_ERR_* on other failures.
 */
int cmci_cmd_sync_exec(cmci_context_t *ctx, int channel_id,
                       uint8_t cmd_id,
                       const char *command, int cmd_len,
                       int *out_exit_code, int timeout_ms);

/**
 * cmci_cmd_sync_signal - Signal a waiting sync_exec thread that X TLV arrived.
 *
 * Called from the client's recv_cb when an X-type TLV is received.
 * Matches the cmd_id against a pending sync slot, copies exit info,
 * and wakes the blocked cmci_cmd_sync_exec caller.
 *
 * Must be safe to call from any thread (CMCI callback thread context).
 *
 * @param cmd_id    Command id from the X TLV payload.
 * @param exit_code Exit code from the X TLV payload.
 * @param flags     Flags from the X TLV payload (CMCI_CMD_EXIT_FLAG_*).
 */
void cmci_cmd_sync_signal(uint8_t cmd_id, uint8_t exit_code, uint8_t flags);

/**
 * cmci_cmd_send_kill - Send a kill request for a specific cmd_id.
 * Uses cmci_cmd_encode_kill internally.
 * @ctx:        CMCI context.
 * @channel_id: Channel to send on.
 * @cmd_id:     Command id to kill.
 * Return: CMCI_OK on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_send_kill(cmci_context_t *ctx, int channel_id,
                       uint8_t cmd_id);

/* ===================================================================
 * Linux server backend API
 * Only available when compiling for Linux.
 * =================================================================== */

#ifdef __linux__

/* Opaque cmd executor handle */
typedef struct cmci_cmd_executor cmci_cmd_executor_t;

/**
 * cmci_cmd_executor_config_t - Configuration for cmci_cmd_executor_create.
 * @ctx:           Initialised and running CMCI context.
 * @channel_id:    Channel used for output TLV sends.
 * @max_workers:   Maximum concurrent child processes (0 = default 4).
 * @workdir:       Default working directory (NULL = inherit server cwd).
 */
typedef struct {
	cmci_context_t *ctx;
	int             channel_id;
	int             max_workers;
	const char     *workdir;
} cmci_cmd_executor_config_t;

/**
 * cmci_cmd_executor_create - Create a cmd executor bound to a channel.
 * @out_exec: Output pointer for the new executor handle.
 * @cfg:      Configuration (ctx must be running).
 * Return: CMCI_OK on success, negative CMCI_ERR_* on failure.
 */
int cmci_cmd_executor_create(cmci_cmd_executor_t **out_exec,
                             const cmci_cmd_executor_config_t *cfg);

/**
 * cmci_cmd_executor_destroy - Destroy executor and wait for all running commands.
 * @exec: Executor handle (may be NULL).
 */
void cmci_cmd_executor_destroy(cmci_cmd_executor_t *exec);

/**
 * cmci_cmd_execute - Execute a command string with a client-assigned cmd_id.
 * Called from recv_cb or any context.
 * @exec:     Executor handle.
 * @cmd_id:   Client-assigned command identifier (opaque to server, echoed in X TLV).
 * @command:  Shell command string (passed to /bin/sh -c).
 * @workdir:  Per-command working directory override (NULL = use default).
 * @is_sync:  Non-zero = sync mode (no O/E output, waitpid in worker, fast return).
 *            Zero = async mode (capture stdout/stderr, stream O/E TLV).
 * Return: CMCI_OK on success (command queued), negative CMCI_ERR_* on failure.
 *         Returns CMCI_ERR_DUP if a command with the same cmd_id is already
 *         in the queue or running.
 */
int cmci_cmd_execute(cmci_cmd_executor_t *exec,
                     uint8_t cmd_id,
                     const char *command,
                     const char *workdir,
                     int is_sync);

/**
 * cmci_cmd_kill - Kill a specific command by cmd_id.
 * Safe to call from recv_cb or any thread context.
 * The target command may be running or still in queue;
 * in either case it is terminated/removed and an X TLV is sent.
 * @exec:   Executor handle.
 * @cmd_id: Command id to kill.
 * Return: CMCI_OK if the command was found and killed/cancelled,
 *         CMCI_ERR_NOT_FOUND if no command with that cmd_id exists.
 */
int cmci_cmd_kill(cmci_cmd_executor_t *exec, uint8_t cmd_id);

/**
 * cmci_cmd_cancel_all - Cancel all queued (not yet started) commands.
 *                       Running commands are not affected; use kill for those.
 * @exec: Executor handle.
 * Return: Number of commands cancelled.
 */
int cmci_cmd_cancel_all(cmci_cmd_executor_t *exec);

#endif /* __linux__ */

#ifdef __cplusplus
}
#endif

#endif /* CMCI_CMD_H */