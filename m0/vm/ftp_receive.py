#!/usr/bin/env python3
"""Minimal FTP receive server for ReactOS guest -> host log transfer.

Guest downloads ftpput.exe over HTTP (10.0.2.2:8000), runs it, and this
server accepts the uploaded glyph-probe-result.txt into m0/vm/uploads/.

Anonymous write-only: no need to ship credentials into the guest.
Passive ports are fixed so the guest (behind QEMU SLIRP NAT) can connect
back to the host data port.
"""
import os
import sys

from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

HOST = "0.0.0.0"
PORT = 2121
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "uploads"))
PASSIVE_PORTS = range(60000, 60100)


def main() -> int:
    os.makedirs(ROOT, exist_ok=True)
    authorizer = DummyAuthorizer()
    # "w" = write only. Guest cannot list or read back; it only drops files.
    authorizer.add_anonymous(ROOT, perm="w")

    # pyftpdlib 2.x: pass the handler CLASS (not an instance); settings are
    # applied as class attributes, matching the library's own __main__.py.
    handler = FTPHandler
    handler.authorizer = authorizer
    handler.passive_ports = list(PASSIVE_PORTS)
    handler.banner = "ros-pinyin guest log receiver ready"

    server = FTPServer((HOST, PORT), handler)
    print(f"FTP receiver listening on {HOST}:{PORT} -> {ROOT}", flush=True)
    print(f"passive ports: {PASSIVE_PORTS.start}-{PASSIVE_PORTS.stop - 1}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("FTP receiver stopped", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
