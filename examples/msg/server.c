/*
 * msg-server — Message echo server
 *
 * Receives messages from clients, prepends "echo: " prefix,
 * and sends the result back. No TLV, no extension dependencies.
 *
 * Supports UDP and UART transport.
 */

#include "cmci.h"
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
/* Receive callback — echo with prefix                                 */
/* ------------------------------------------------------------------ */
#define ECHO_PREFIX  "echo: "
#define ECHO_PREFIX_LEN  6

static void recv_cb(int channel_id,
                    const cmci_recv_meta_t *meta,
                    const void *buf, int len,
                    void *user_data)
{
	(void)channel_id;
	(void)meta;
	(void)user_data;

	/* Build response: "echo: " + original message */
	char echo_buf[ECHO_PREFIX_LEN + 1024];
	int echo_len;

	if (len > (int)(sizeof(echo_buf) - ECHO_PREFIX_LEN)) {
		CMCI_LOG(CMCI_LOG_WARN, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
		         "msg too long: %d bytes, truncating to %zu", len,
		         sizeof(echo_buf) - ECHO_PREFIX_LEN);
		len = (int)(sizeof(echo_buf) - ECHO_PREFIX_LEN);
	}

	memcpy(echo_buf, ECHO_PREFIX, ECHO_PREFIX_LEN);
	memcpy(echo_buf + ECHO_PREFIX_LEN, buf, (size_t)len);
	echo_len = ECHO_PREFIX_LEN + len;

	CMCI_LOG(CMCI_LOG_INFO, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
	         "server rx: len=%d msg=%.*s -> echo: %.*s",
	         len, len, (const char *)buf, echo_len, echo_buf);

	int ret = cmci_channel_send(g_ctx, g_channel, echo_buf, echo_len);
	if (ret != CMCI_OK) {
		CMCI_LOG(CMCI_LOG_ERROR, CMCI_LOG_CAT_EXTENSION, CMCI_LOG_SUBCAT_EXTENSION_CMD,
		         "cmci_channel_send failed: %s", cmci_strerror(ret));
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
		"\n"
		"UDP options:\n"
		"  --ip <addr>       Local bind address (default: 0.0.0.0)\n"
		"  --ip-port <port>  Local UDP port\n"
		"\n"
		"UART options:\n"
		"  --uart-port <dev> Serial device (e.g. /dev/ttyS0)\n"
		"  --baudrate <bps>  Serial baud rate\n",
		prog);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
	const char *media         = NULL;
	const char *ip            = "0.0.0.0";
	int         ip_port       = 0;
	const char *uart_port     = NULL;
	int         baudrate      = 0;
	cmci_log_level_t g_log_level = CMCI_LOG_NONE;

	/* Parse options */
	static struct option long_opts[] = {
		{"media",      required_argument, NULL, 'm'},
		{"ip",         required_argument, NULL, 'i'},
		{"ip-port",    required_argument, NULL, 'p'},
		{"uart-port",  required_argument, NULL, 'u'},
		{"baudrate",   required_argument, NULL, 'b'},
		{"log-level",  required_argument, NULL, 'l'},
		{"help",       no_argument,       NULL, 'h'},
		{0, 0, 0, 0}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "hm:i:p:u:b:l:", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'm': media = optarg; break;
		case 'i': ip = optarg; break;
		case 'p': ip_port = atoi(optarg); break;
		case 'u': uart_port = optarg; break;
		case 'b': baudrate = atoi(optarg); break;
		case 'l':
			if (parse_log_level(optarg, &g_log_level) != 0) {
				fprintf(stderr, "Error: invalid log level '%s' (use error|info|debug)\n", optarg);
				return 1;
			}
			break;
		case 'h':
		default:
			print_usage(argv[0]);
			return (opt == 'h') ? 0 : 1;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Error: unexpected argument '%s'\n", argv[optind]);
		print_usage(argv[0]);
		return 1;
	}

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

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGPIPE, SIG_IGN);

	if (g_log_level != CMCI_LOG_NONE) {
		cmci_log_set_all(g_log_level);
	}

	/* CMCI options */
	cmci_options_t opt_cmci;
	memset(&opt_cmci, 0, sizeof(opt_cmci));
	opt_cmci.type = is_udp ? CMCI_LINK_UDP : CMCI_LINK_UART;
	opt_cmci.heartbeat_interval_ms = 3000;
	opt_cmci.heartbeat_miss_limit = 3;
	opt_cmci.ack_timeout_ms = 600;
	opt_cmci.max_retry = 3;
	opt_cmci.window_size = 4;
	opt_cmci.reassembly_idle_timeout_ms = 2000;
	opt_cmci.max_message_size = 1024 * 8;

	int ret = cmci_init(&g_ctx, &opt_cmci);
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

	fprintf(stderr, "MSG server ready (%s), waiting for client...\n", media);

	while (g_running)
		sleep(1);

	fprintf(stderr, "\nShutting down...\n");
	cmci_stop(g_ctx);

out_deinit:
	cmci_deinit(g_ctx);
	fprintf(stderr, "Server stopped.\n");
	return 0;
}
