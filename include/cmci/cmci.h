#ifndef CMCI_H
#define CMCI_H

#include "cmci_types.h"
#include "cmci_errno.h"
#include "cmci_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cmci_init - Create and initialize a CMCI context.
 * @out_ctx:  Output pointer for the new context.
 * @opt:      Initialisation options (link type, timing, etc.).
 * Return: CMCI_OK on success, negative error code on failure.
 */
int cmci_init(cmci_context_t **out_ctx, const cmci_options_t *opt);

/**
 * cmci_deinit - Destroy a CMCI context and free all resources.
 *               Must be called after cmci_stop().
 */
int cmci_deinit(cmci_context_t *ctx);

/**
 * cmci_config_udp - Configure UDP transport parameters.
 *                   Must be called before cmci_start().
 */
int cmci_config_udp(cmci_context_t *ctx, const cmci_udp_config_t *cfg);

/**
 * cmci_config_uart - Configure UART transport parameters.
 *                    Must be called before cmci_start().
 */
int cmci_config_uart(cmci_context_t *ctx, const cmci_uart_config_t *cfg);

/**
 * cmci_start - Start the I/O thread and begin message processing.
 *              After this call the configuration is frozen.
 */
int cmci_start(cmci_context_t *ctx);

/**
 * cmci_stop - Gracefully stop the I/O thread and tear down transport.
 */
int cmci_stop(cmci_context_t *ctx);

/**
 * cmci_channel_register - Allocate a new channel on the given context.
 * @out_channel_id: Output parameter receiving the channel id.
 */
int cmci_channel_register(cmci_context_t *ctx, int *out_channel_id);

/**
 * cmci_channel_set_callback - Bind a receive callback to a channel.
 */
int cmci_channel_set_callback(cmci_context_t *ctx, int channel_id,
                              cmci_recv_cb cb, void *user_data);

/**
 * cmci_channel_send - Send a one-way message on a channel.
 *                     Blocks until the message is queued (not until ACK).
 */
int cmci_channel_send(cmci_context_t *ctx, int channel_id,
                      const void *buf, int len);

/**
 * cmci_set_link_state_callback - Register a callback for link state changes.
 * @cb: Callback invoked on link up (1) or link lost (0). Set to NULL to unregister.
 *      Must be called before cmci_start().
 */
int cmci_set_link_state_callback(cmci_context_t *ctx,
                                 cmci_link_state_cb cb, void *user_data);

/**
 * cmci_get_stats - Query context-level statistics (atomic snapshot).
 *                  Can be called at any time; does not reset counters.
 */
int cmci_get_stats(cmci_context_t *ctx, cmci_stats_t *out_stats);

/**
 * cmci_channel_get_stats - Query per-channel statistics.
 */
int cmci_channel_get_stats(cmci_context_t *ctx, int channel_id,
                           cmci_channel_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* CMCI_H */
