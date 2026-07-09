#ifndef CMCI_TYPES_H
#define CMCI_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Link type */
typedef enum {
	CMCI_LINK_UDP = 1,
	CMCI_LINK_UART = 2,
} cmci_link_type_t;

/* Frame types */
typedef enum {
	CMCI_FRAME_DATA        = 0,
	CMCI_FRAME_ACK         = 1,
	CMCI_FRAME_LINK_PROBE  = 2,
	CMCI_FRAME_LINK_PROBE_ACK = 3,
	CMCI_FRAME_HEARTBEAT   = 4,
	CMCI_FRAME_HEARTBEAT_ACK = 5,
} cmci_frame_type_t;

/* Receive metadata passed to user callback */
typedef struct {
	uint16_t message_id;
	uint16_t flags;
} cmci_recv_meta_t;

/* Receive callback prototype */
typedef void (*cmci_recv_cb)(
	int channel_id,
	const cmci_recv_meta_t *meta,
	const void *buf,
	int len,
	void *user_data
);

/* Link state callback prototype */
/* is_alive: 1 = link established, 0 = link lost */
typedef void (*cmci_link_state_cb)(int is_alive, void *user_data);

/* Options for cmci_init */
typedef struct {
	cmci_link_type_t type;
	int heartbeat_interval_ms;
	int heartbeat_miss_limit;
	int ack_timeout_ms;
	int max_retry;
	int reassembly_idle_timeout_ms;
	int max_message_size;
	int window_size;              /* sliding window: max in-flight fragments per message (0=default) */
	int batch_ack_deadline_ms;    /* max delay before forcing batch ACK (0=default 50ms) */
} cmci_options_t;

/* Context-level statistics (query via cmci_get_stats) */
typedef struct {
	/* Send */
	int64_t total_bytes_tx;
	int64_t total_fragments_tx;
	int64_t total_retransmits;
	int64_t total_acks_rx;
	/* Receive */
	int64_t total_bytes_rx;
	int64_t total_fragments_rx;
	int64_t total_dup_fragments_rx;
	int64_t total_reassembly_fail;
	/* Link */
	int64_t total_heartbeat_tx;
	int64_t total_heartbeat_rx;
	int64_t total_link_errors;
	/* Current state */
	int queue_depth;
	int in_flight_fragments;
	int link_rssi;                /* reserved */
} cmci_stats_t;

/* Channel-level statistics (query via cmci_channel_get_stats) */
typedef struct {
	int64_t total_bytes_tx;
	int64_t total_messages_tx;
	int64_t total_fragments_tx;
	int64_t total_retransmits;
	int64_t total_bytes_rx;
	int64_t total_messages_rx;
	int64_t total_fragments_rx;
	int64_t total_dup_fragments_rx;
	int send_queue_depth;
	int cur_window_usage;
	int max_window_usage;
} cmci_channel_stats_t;

/* UDP transport config */
typedef struct {
	const char *local_ip;
	int         local_port;
	const char *remote_ip;
	int         remote_port;
} cmci_udp_config_t;

/* UART transport config */
typedef struct {
	const char *device;
	int         baudrate;
	int         databits;
	int         stopbits;
	char        parity;
} cmci_uart_config_t;

/* Default message size limit (compile-time upper bound) */
#define CMCI_MAX_MESSAGE_SIZE  (64 * 1024)

/* Per-channel reassembly buffer size */
#define CMCI_CHANNEL_BUF_SIZE  (64 * 1024)

/* Default payload MTU per link type */
#define CMCI_UDP_PAYLOAD_MTU   1024
#define CMCI_UART_PAYLOAD_MTU  256

/* Sliding window defaults */
#define CMCI_WINDOW_SIZE_UDP   8
#define CMCI_WINDOW_SIZE_UART  4
#define CMCI_BATCH_ACK_DEADLINE_MS 50

/* Max in-flight fragments per message (soft limit, hardware-imposed) */
#define CMCI_MAX_FRAGMENTS 64

/* DUP_WINDOW_SIZE: number of recently completed message IDs to track per channel */
#define CMCI_DUP_WINDOW_SIZE 64

/* Magic and version for frame header */
#define CMCI_FRAME_MAGIC       0xCE
#define CMCI_FRAME_VERSION     0x01

/* Opaque handle types */
typedef struct cmci_context cmci_context_t;

#ifdef __cplusplus
}
#endif

#endif /* CMCI_TYPES_H */
