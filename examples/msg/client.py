#!/usr/bin/env python3
"""Message echo client — sends a message and prints the server's reply.

Usage::

    python client.py --media=udp --ip=1.2.3.4 --ip-port=9000 --msg "Hello"
    python client.py --media=uart --uart-port=/dev/ttyS0 --baudrate=115200 --msg "Hi"
"""

import argparse
import os
import signal
import sys
import time

_this_dir = os.path.dirname(os.path.abspath(__file__))
_parent = os.path.dirname(_this_dir)
_root = os.path.dirname(_parent)  # examples/
if _parent not in sys.path:
    sys.path.insert(0, _parent)

import cmci


def main():
    parser = argparse.ArgumentParser(
        description="CMCI message echo client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Sends a message to the msg-server, which prepends "echo: " and sends it back.
        """,
    )
    parser.add_argument("--media", required=True, choices=["udp", "uart"])
    parser.add_argument("--ip", default=None, help="Server IP address (UDP)")
    parser.add_argument("--ip-port", type=int, default=0, help="Server UDP port")
    parser.add_argument("--uart-port", default=None, help="Serial device (UART)")
    parser.add_argument("--baudrate", type=int, default=0, help="Serial baud rate")
    parser.add_argument("--log-level", default=None,
                        choices=["error", "warn", "info", "debug", "trace", "none"],
                        help="CMCI log level (default: none)")
    parser.add_argument("--msg", default="Hello CMCI", help="Message to send")
    args = parser.parse_args()

    is_udp = args.media == "udp"
    if is_udp and (not args.ip or not args.ip_port):
        parser.error("--ip and --ip-port are required for UDP")
    if not is_udp and (not args.uart_port or not args.baudrate):
        parser.error("--uart-port and --baudrate are required for UART")

    if args.log_level:
        cmci.set_log_level(args.log_level)

    # Signal handling
    g_running = True

    def on_signal(signum, frame):
        nonlocal g_running
        g_running = False

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    link_type = cmci.LINK_UDP if is_udp else cmci.LINK_UART
    with cmci.Context(
        link_type=link_type,
        heartbeat_interval=3000,
        ack_timeout=600,
        max_retry=3,
        max_message_size=8192,
    ) as ctx:
        if is_udp:
            ctx.config_udp("0.0.0.0", 0, args.ip, args.ip_port)
        else:
            ctx.config_uart(args.uart_port, args.baudrate)

        ch = ctx.channel_register()

        def on_reply(data):
            print(f"Reply: {data.decode('utf-8', errors='replace')}", flush=True)

        client = cmci.MsgClient(ctx, ch, on_reply=on_reply)

        ctx.start()
        time.sleep(0.3)

        msg = args.msg.encode("utf-8")
        print(f"Send: {args.msg}", flush=True)

        client.echo(msg, timeout_ms=10000)

    return 0


if __name__ == "__main__":
    sys.exit(main())
