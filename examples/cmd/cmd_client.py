#!/usr/bin/env python3
"""Remote command execution client (CMD extension).

Usage::

    # Non-interactive mode — execute single command, stream output, return exit code
    python cmd_client.py --media=udp --ip=1.2.3.4 --ip-port=9000 "ls -la"

    # Interactive mode — stdin loop, async/sync/kill/exit
    python cmd_client.py --media=uart --uart-port=/dev/ttyUSB0 --baudrate=115200
"""

import argparse
import os
import signal
import sys
import time

# Auto-discover cmci module when not installed via pip / PYTHONPATH
try:
    import cmci
except ImportError:
    _this_dir = os.path.dirname(os.path.abspath(__file__))
    for _rel in ['../..']:
        _p = os.path.abspath(os.path.join(_this_dir, _rel))
        if os.path.isdir(os.path.join(_p, 'cmci', 'clibs')):
            sys.path.insert(0, _p)
            break
    import cmci


def main():
    parser = argparse.ArgumentParser(
        description="CMCI remote command execution client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Modes:
  With <command>:    Execute command in non-interactive mode. Output is
                     streamed to stdout/stderr; exit code is returned.
  Without <command>: Interactive shell. Commands:
    <command>         Run asynchronously (streaming output)
    !<command>        Run synchronously (block, exit code only)
    kill:<cmd_id>     Kill a running/queued command by id
    exit|quit         Exit the client
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
    parser.add_argument(
        "command", nargs="*", default=None, help="Command to execute (non-interactive)"
    )
    args = parser.parse_args()

    # ---- Validate ----
    is_udp = args.media == "udp"
    if is_udp and (not args.ip or not args.ip_port):
        parser.error("--ip and --ip-port are required for UDP")
    if not is_udp and (not args.uart_port or not args.baudrate):
        parser.error("--uart-port and --baudrate are required for UART")

    batch_cmd = " ".join(args.command) if args.command else None
    max_chunk = (
        cmci.UDP_PAYLOAD_MTU - 3 if is_udp else cmci.UART_PAYLOAD_MTU - 3
    )

    # ---- log level ----
    if args.log_level:
        cmci.set_log_level(args.log_level)

    # ---- Signal handling ----
    g_running = True

    def on_signal(signum, frame):
        nonlocal g_running
        g_running = False

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    # ---- cmci context ----
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

        if batch_cmd is not None:
            # ======== Non-interactive mode (use CmdClient) ========
            client = cmci.CmdClient(ctx, ch, max_chunk=max_chunk)
            ctx.start()
            time.sleep(0.3)

            exit_code = client.sync_exec(batch_cmd, timeout_ms=60000)

            out = client.stdout
            if out:
                sys.stdout.buffer.write(out)
            err = client.stderr
            if err:
                sys.stderr.buffer.write(err)

            return exit_code

        # ======== Interactive mode (use CmdClient with output_cb) ========
        def output_cb(ttype, payload):
            """Live-print stdout/stderr as they arrive."""
            if ttype == cmci.CMD_TYPE_STDOUT:
                sys.stdout.buffer.write(payload)
                sys.stdout.buffer.flush()
            elif ttype == cmci.CMD_TYPE_STDERR:
                sys.stderr.buffer.write(payload)
                sys.stderr.buffer.flush()

        client = cmci.CmdClient(ctx, ch, max_chunk=max_chunk, output_cb=output_cb)
        ctx.start()
        time.sleep(0.3)

        print(
            "Interactive CMD client.  "
            "Commands: <cmd> (async)  !<cmd> (sync)  "
            "kill:<id>  exit/quit",
            flush=True,
        )

        while g_running:
            try:
                line = input("cmd> ")
            except (EOFError, KeyboardInterrupt):
                break

            line = line.strip()
            if not line:
                continue
            if line in ("exit", "quit"):
                break

            if line.startswith("kill:"):
                kid = int(line[5:])
                client.send_kill(kid)
                print(f"[kill sent for cmd_id={kid}]")
                continue

            is_sync = line.startswith("!")
            cmd_str = line[1:].strip() if is_sync else line
            if not cmd_str:
                continue

            if is_sync:
                ec = client.sync_exec(cmd_str, timeout_ms=30000)
                print(f"[sync exit_code={ec}]")
            else:
                cmd_id = client.send_request(cmd_str)
                print(f"[async cmd_id={cmd_id}]")

    return 0


if __name__ == "__main__":
    sys.exit(main())
