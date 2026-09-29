#!/usr/bin/env python3
"""
Bridge between the BMV080 board and the Andromeda engine.

The board emits two kinds of line:

    {"topic":"bmv080","data":{"ID":..,"R":..,"PM10":..,"PM25":..,"PM1":..,"obst":"no",..}}
    PFF,uptime,channel,frequency,snr

Where they go depends on NET_MODE_ACCESS_POINT in the firmware's include/net_main.h,
because net_send_bytes() is compiled differently for the two modes:

    1 (access point) - both kinds land in the board's ring buffer and are served over
                       HTTP at /pff. There is no UDP in this mode.  ->  --http
    0 (client)       - they are sent as UDP datagrams to port 4210. ->  default mode

Andromeda connects as a TCP *client* to 127.0.0.1:8080 and reads one JSON object per
line, in the shape of its SensorData struct:

    {"sensor_name":..,"timestamp":..,"source":"direct_tcp",
     "data":{"pm1_0":..,"pm2_5":..,"pm10_0":..,"is_obstructed":false}}

This script receives the former and serves the latter. PFF lines are counted and
dropped - the engine's telemetry struct has no place for single detections yet.

    python andromeda_bridge.py --http       # board is an access point (192.168.4.1)
    python andromeda_bridge.py              # board is a client, UDP 4210
    python andromeda_bridge.py --fake       # no sensor needed, generates values
    python andromeda_bridge.py --verbose    # log every forwarded line

Start this BEFORE Andromeda. The engine connects once during startup and does not
retry, so a bridge started later is never picked up.
"""

import argparse
import json
import logging
import math
import socket
import threading
import time
import urllib.error
import urllib.request

UDP_PORT = 4210
TCP_HOST = "127.0.0.1"
TCP_PORT = 8080


def translate(payload):
    """Maps one firmware message onto Andromeda's SensorData schema.

    Returns None for anything that is not a particulate matter message, so other
    topics are dropped here instead of reaching the engine as a malformed line.
    """
    if payload.get("topic") != "bmv080":
        return None

    data = payload.get("data", {})
    return {
        "sensor_name": str(data.get("ID", "BMV080")),
        "timestamp": int(time.time() * 1000),
        "source": "direct_tcp",
        "data": {
            # BMV080Telemetry declares these as int, the firmware prints them with %.0f.
            "pm1_0": int(float(data.get("PM1", 0))),
            "pm2_5": int(float(data.get("PM25", 0))),
            "pm10_0": int(float(data.get("PM10", 0))),
            # The firmware sends the string "yes"/"no", the engine wants a bool.
            "is_obstructed": str(data.get("obst", "no")).lower() == "yes",
        },
    }


class Clients:
    """The connected engines.

    A client that went away is dropped and the bridge keeps running, so Andromeda
    can be restarted without restarting this script.
    """

    def __init__(self):
        self._lock = threading.Lock()
        self._sockets = []

    def add(self, sock):
        with self._lock:
            self._sockets.append(sock)

    def broadcast(self, line):
        payload = (line + "\n").encode("utf-8")
        with self._lock:
            alive = []
            for sock in self._sockets:
                try:
                    sock.sendall(payload)
                    alive.append(sock)
                except OSError:
                    sock.close()
                    logging.info("engine disconnected")
            self._sockets = alive
            return len(alive)


def forward(clients, payload, verbose):
    message = translate(payload)
    if message is None:
        return
    line = json.dumps(message, separators=(",", ":"))
    count = clients.broadcast(line)
    if verbose:
        logging.info("-> %d client(s): %s", count, line)


def serve_tcp(clients, stop, verbose):
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((TCP_HOST, TCP_PORT))
    server.listen(4)
    server.settimeout(0.5)
    logging.info("waiting for Andromeda on %s:%d", TCP_HOST, TCP_PORT)

    while not stop.is_set():
        try:
            sock, address = server.accept()
        except socket.timeout:
            continue
        # The engine greets with "hello_sensor\n". We do not need it, but draining it
        # keeps the receive buffer from filling up over a long session.
        sock.settimeout(0.2)
        try:
            greeting = sock.recv(64)
            if verbose and greeting:
                logging.info("greeting: %s", greeting.decode("utf-8", "replace").strip())
        except OSError:
            pass
        sock.settimeout(None)
        logging.info("engine connected from %s:%d", *address)
        clients.add(sock)

    server.close()


def receive_udp(clients, stop, verbose):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", UDP_PORT))
    sock.settimeout(0.5)
    logging.info("listening for the board on UDP port %d", UDP_PORT)

    pff_lines = 0
    while not stop.is_set():
        try:
            datagram, _ = sock.recvfrom(4096)
        except socket.timeout:
            continue

        # One datagram usually carries several lines.
        for line in datagram.decode("utf-8", "replace").splitlines():
            line = line.strip()
            if not line:
                continue
            if line.startswith("PFF,"):
                pff_lines += 1
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                logging.warning("not JSON, ignored: %s", line[:80])
                continue
            forward(clients, payload, verbose)

    sock.close()
    if pff_lines:
        logging.info("dropped %d PFF lines", pff_lines)


def poll_http(clients, stop, verbose, host, interval):
    """Polls /pff, which in access point mode also carries the particulate matter lines.

    The reply starts with "<next_sequence> <lost>", then one line per entry. Handing the
    sequence number back as ?since= asks only for what is new. The ring buffer holds 512
    lines and the PFF stream fills it within seconds, so "lost" is normal here and only
    logged in verbose mode - the PM lines arrive about once a second and are not missed
    as long as the poll interval stays well below the buffer's turnaround.
    """
    url = f"http://{host}/pff"
    since = None

    # Bypass the system proxy. urllib picks it up from the Windows settings by default,
    # and a corporate proxy cannot reach the board's own network - the request then runs
    # into a timeout that looks exactly like an unreachable board.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    logging.info("polling %s every %.1f s", url, interval)

    while not stop.is_set():
        target = url if since is None else f"{url}?since={since}"
        try:
            with opener.open(target, timeout=4) as response:
                body = response.read().decode("utf-8", "replace")
        except (urllib.error.URLError, OSError) as error:
            logging.warning("board not reachable: %s", error)
            stop.wait(interval)
            continue

        lines = body.splitlines()
        if not lines:
            stop.wait(interval)
            continue

        header = lines[0].split()
        try:
            since = int(header[0])
        except (IndexError, ValueError):
            logging.warning("unexpected reply header: %s", lines[0][:60])
            stop.wait(interval)
            continue
        if verbose and len(header) > 1 and header[1] == "1":
            logging.info("ring buffer dropped lines before sequence %d", since)

        for line in lines[1:]:
            line = line.strip()
            if not line or line.startswith("PFF,"):
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                continue
            forward(clients, payload, verbose)

        stop.wait(interval)


def generate_fake(clients, stop, verbose):
    """Plausible values without a sensor, on a slow wave so the engine visibly reacts."""
    logging.info("fake mode, no sensor needed")
    start = time.monotonic()

    while not stop.is_set():
        elapsed = time.monotonic() - start
        wave = (math.sin(elapsed * 0.3) + 1.0) * 0.5   # 0 .. 1
        pm2_5 = 5.0 + wave * 45.0                      # 5 .. 50 ug/m3

        forward(clients, {
            "topic": "bmv080",
            "data": {
                "ID": "FAKE",
                "PM1": round(pm2_5 * 0.6),
                "PM25": round(pm2_5),
                "PM10": round(pm2_5 * 1.6),
                "obst": "no",
            },
        }, verbose)
        stop.wait(1.0)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--http", nargs="?", const="192.168.4.1", metavar="HOST",
                        help="poll the board's /pff endpoint instead of listening for UDP; "
                             "use this when the board is its own access point")
    parser.add_argument("--interval", type=float, default=0.5,
                        help="seconds between HTTP polls (default 0.5)")
    parser.add_argument("--fake", action="store_true",
                        help="generate values instead of listening for the board")
    parser.add_argument("--verbose", action="store_true",
                        help="log every forwarded line")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s",
                        datefmt="%H:%M:%S")

    clients = Clients()
    stop = threading.Event()

    if args.fake:
        source = lambda: generate_fake(clients, stop, args.verbose)
    elif args.http:
        source = lambda: poll_http(clients, stop, args.verbose, args.http, args.interval)
    else:
        source = lambda: receive_udp(clients, stop, args.verbose)

    threads = [
        threading.Thread(target=serve_tcp, args=(clients, stop, args.verbose), daemon=True),
        threading.Thread(target=source, daemon=True),
    ]
    for thread in threads:
        thread.start()

    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        logging.info("stopping")
        stop.set()
        for thread in threads:
            thread.join(timeout=2.0)


if __name__ == "__main__":
    main()
