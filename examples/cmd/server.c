/*
 * cmd-server — Remote command execution server
 *
 * Uses cmci_cmd_executor to execute shell commands forwarded by cmd clients.
 * Supports async (C-type) and sync (S-type) command execution, plus kill (K-type).
 * Linux only (v1).
 */

#include "cmci.h"
#include "cmci/extensions/cmci_cmd.h"
#include "cmci_errno.h"
#include "cmci_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

/* ------------------------------------------------------------------ */
/* Global state                                                        */
/* ------------------------------------------------------------------ */
static volatile int g_running = 1;
static cmci_context_t *g_ctx = NULL;
static cmci_cmd_executor_t *g_exec = NULL;
static int g_channel = -1;

/* ------------------------------------------------------------------ */
/* Signal handling                                                     */
/* ------------------------------------------------------------------ */
static void on_signal(int sig)
{
	(void)sig;
	g_running = 0;
}

/* ------------------------------------------------------------------ */
/* Link state callback                                                 */
/* ------------------------------------------------------------------ */
static void on_link_state(int is_alive, void *user_data)
{
	(void)user_data;
	if (!is_alive) {
		fprintf(stderr, "\nLink lost\n");
		g_running = 0;
	}
}

/* ------------------------------------------------------------------ */
/* Receive callback — decode TLV, dispatch to executor                 */
/* ------------------------------------------------------------------ */
static void recv_cb(int channel_id,
                    const cmci_recv_meta_t *meta,
                    const void *buf, int len,
                    void *user_data)
{
	(void)channel_id;
	(void)meta;
	(void)user_data;

	uint8_t type;
	const uint8_t *value;
	uint16_t value_len;

	int ret = cmci_cmd_tlv_decode(buf, len, &type, &value, &value_len);
	if (ret < 0) {
		fprintf(stderr, "server: tlv decode failed: %s\n", cmci_strerror(ret));
		return;
	}

	CMCI_LOG(CMCI_LOG_DEBUG, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
	         "server rx: ch=%d msg=%u type=%c len=%d value_len=%u",
	         channel_id,
	         meta ? meta->message_id : 0,
	         (char)type,
	         len,
	         value_len);

	switch (type) {
	case CMCI_CMD_TYPE_COMMAND:  /* async command */
	case CMCI_CMD_TYPE_SYNC:     /* sync command */
	{
		uint8_t cmd_id;
		ret = cmci_cmd_parse_cmd_id(value, value_len, &cmd_id);
		if (ret < 0) {
			fprintf(stderr, "server: parse_cmd_id failed: %s\n", cmci_strerror(ret));
			return;
		}

		/* Extract command string (skip cmd_id byte) */
		int cmd_len = value_len - 1;
		const char *cmd_str = (cmd_len > 0) ? (const char *)value + 1 : "";

		int is_sync = (type == CMCI_CMD_TYPE_SYNC) ? 1 : 0;

		ret = cmci_cmd_execute(g_exec, cmd_id, cmd_str, NULL, is_sync);
		if (ret != CMCI_OK) {
			fprintf(stderr, "server: execute cmd_id=%u failed: %s\n",
			        cmd_id, cmci_strerror(ret));
		}
		break;
	}

	case CMCI_CMD_TYPE_KILL:
	{
		uint8_t cmd_id;
		ret = cmci_cmd_parse_cmd_id(value, value_len, &cmd_id);
		if (ret < 0) {
			fprintf(stderr, "server: parse_cmd_id failed: %s\n", cmci_strerror(ret));
			return;
		}

		ret = cmci_cmd_kill(g_exec, cmd_id);
		if (ret == CMCI_ERR_NOT_FOUND) {
			CMCI_LOG(CMCI_LOG_WARN, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
			         "kill: cmd_id=%u not found", cmd_id);
		}
		break;
	}

	default:
		/* Unknown TLV type — log and discard */
		CMCI_LOG(CMCI_LOG_WARN, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
		         "unknown tlv type: 0x%02x ('%c')", type, (char)type);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Parse log level string                                              */
/* ------------------------------------------------------------------ */
static int parse_log_level(const char *s, cmci_log_level_t *out)
{
	if (strcmp(s, "error") == 0) { *out = CMCI_LOG_ERROR; return 0; }
	if (strcmp(s, "info")  == 0) { *out = CMCI_LOG_INFO;  return 0; }
	if (strcmp(s, "debug") == 0) { *out = CMCI_LOG_DEBUG; return 0; }
	return -1;
}

/* ------------------------------------------------------------------ */
/* Usage                                                               */
/* ------------------------------------------------------------------ */
static void print_usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s --media=<udp|uart> [options]\n"
		"\n"
		"General options:\n"
		"  --help, -h        Show this help and exit\n"
		"  --log-level <level>  CMCI log level: error|info|debug (default: error)\n"
		"  --max-workers <n>  Max concurrent worker threads (default: 4)\n"
		"\n"
		"UDP options:\n"
		"  --ip <addr>       Local bind address (default: 0.0.0.0)\n"
		"  --ip-port <port>  Local UDP port\n"
		"\n"
		"UART options:\n"
		"  --uart-port <dev> Serial device (e.g. /dev/ttyUSB0)\n"
		"  --baudrate <bps>  Serial baud rate\n",
		prog);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
	const char *media = NULL;
	const char *ip = "0.0.0.0";
	int ip_port = 0;
	const char *uart_port = NULL;
	int baudrate = 0;
	int max_workers = 0;
	cmci_log_level_t g_log_level = CMCI_LOG_NONE;

	static struct option long_opts[] = {
		{"help",        no_argument,       NULL, 'h'},
		{"media",       required_argument, NULL, 'm'},
		{"ip",          required_argument, NULL, 'i'},
		{"ip-port",     required_argument, NULL, 'p'},
		{"uart-port",   required_argument, NULL, 'u'},
		{"baudrate",    required_argument, NULL, 'b'},
		{"log-level",   required_argument, NULL, 'l'},
		{"max-workers", required_argument, NULL, 'w'},
		{NULL, 0, NULL, 0}
	};

	int c;
	while ((c = getopt_long(argc, argv, "hm:i:p:u:b:l:w:", long_opts, NULL)) != -1) {
		switch (c) {
		case 'm': media = optarg; break;
		case 'i': ip = optarg; break;
		case 'p': ip_port = atoi(optarg); break;
		case 'u': uart_port = optarg; break;
		case 'b': baudrate = atoi(optarg); break;
		case 'w': max_workers = atoi(optarg); break;
		case 'h': print_usage(argv[0]); return 0;
		case 'l':
			if (parse_log_level(optarg, &g_log_level) != 0) {
				fprintf(stderr, "Error: invalid log level '%s' (use error|info|debug)\n", optarg);
				return 1;
			}
			break;
		default:
			print_usage(argv[0]);
			return 1;
		}
	}

	/* No positional args expected */
	if (optind < argc) {
		fprintf(stderr, "Error: unexpected argument '%s'\n", argv[optind]);
		print_usage(argv[0]);
		return 1;
	}

	/* Validate media */
	if (!media) {
		fprintf(stderr, "Error: --media is required (udp|uart)\n");
		print_usage(argv[0]);
		return 1;
	}

	int is_udp = 0, is_uart = 0;
	if (strcmp(media, "udp") == 0) {
		is_udp = 1;
	} else if (strcmp(media, "uart") == 0) {
		is_uart = 1;
	} else {
		fprintf(stderr, "Error: invalid --media '%s' (use udp|uart)\n", media);
		return 1;
	}

	if (is_udp && ip_port == 0) {
		fprintf(stderr, "Error: --ip-port is required for UDP\n");
		return 1;
	}
	if (is_uart && (!uart_port || baudrate == 0)) {
		fprintf(stderr, "Error: --uart-port and --baudrate are required for UART\n");
		return 1;
	}

	/* Signal handling */
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGPIPE, SIG_IGN);

	if (g_log_level != CMCI_LOG_NONE) {
		cmci_log_set_all(g_log_level);
	}

	/* CMCI options */
	cmci_options_t opt;
	memset(&opt, 0, sizeof(opt));
	opt.type = is_udp ? CMCI_LINK_UDP : CMCI_LINK_UART;
	opt.heartbeat_interval_ms = 3000;
	opt.heartbeat_miss_limit = 3;
	opt.ack_timeout_ms = 600;
	opt.max_retry = 3;
	opt.window_size = 4;
	opt.reassembly_idle_timeout_ms = 2000;
	opt.max_message_size = 1024 * 8;

	int ret = cmci_init(&g_ctx, &opt);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_init failed: %s\n", cmci_strerror(ret));
		return 1;
	}

	if (is_udp) {
		cmci_udp_config_t cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.local_ip   = ip;
		cfg.local_port = ip_port;
		ret = cmci_config_udp(g_ctx, &cfg);
		if (ret != CMCI_OK) {
			fprintf(stderr, "cmci_config_udp failed: %s\n", cmci_strerror(ret));
			goto out_deinit;
		}
	} else {
		cmci_uart_config_t cfg;
		memset(&cfg, 0, sizeof(cfg));
		cfg.device   = uart_port;
		cfg.baudrate = baudrate;
		cfg.databits = 8;
		cfg.stopbits = 1;
		cfg.parity   = 'N';
		ret = cmci_config_uart(g_ctx, &cfg);
		if (ret != CMCI_OK) {
			fprintf(stderr, "cmci_config_uart failed: %s\n", cmci_strerror(ret));
			goto out_deinit;
		}
	}

	ret = cmci_channel_register(g_ctx, &g_channel);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_channel_register failed: %s\n", cmci_strerror(ret));
		goto out_deinit;
	}

	ret = cmci_channel_set_callback(g_ctx, g_channel, recv_cb, NULL);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_channel_set_callback failed: %s\n", cmci_strerror(ret));
		goto out_deinit;
	}

	ret = cmci_set_link_state_callback(g_ctx, on_link_state, NULL);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_set_link_state_callback failed: %s\n", cmci_strerror(ret));
		goto out_deinit;
	}

	ret = cmci_start(g_ctx);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_start failed: %s\n", cmci_strerror(ret));
		goto out_deinit;
	}

	/* Create executor */
	cmci_cmd_executor_config_t exec_cfg;
	memset(&exec_cfg, 0, sizeof(exec_cfg));
	exec_cfg.ctx         = g_ctx;
	exec_cfg.channel_id  = g_channel;
	exec_cfg.max_workers = max_workers;
	exec_cfg.workdir     = NULL;

	ret = cmci_cmd_executor_create(&g_exec, &exec_cfg);
	if (ret != CMCI_OK) {
		fprintf(stderr, "cmci_cmd_executor_create failed: %s\n", cmci_strerror(ret));
		cmci_stop(g_ctx);
		goto out_deinit;
	}

	fprintf(stderr, "CMD server ready (%s), waiting for client...\n", media);

	/* Main loop — wait for shutdown */
	while (g_running)
		sleep(1);

	fprintf(stderr, "\nShutting down...\n");

	/* Cleanup */
	if (g_exec) {
		cmci_cmd_executor_destroy(g_exec);
		g_exec = NULL;
	}

	cmci_stop(g_ctx);

out_deinit:
	cmci_deinit(g_ctx);
	g_ctx = NULL;

	fprintf(stderr, "Server stopped.\n");
	return (ret == CMCI_OK) ? 0 : 1;
}