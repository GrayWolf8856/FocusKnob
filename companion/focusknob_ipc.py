#!/usr/bin/env python3
"""
FocusKnob IPC — Unix socket communication between daemon and menu bar app.

Protocol: JSON objects delimited by newlines over a Unix domain socket.
Daemon runs the IPCServer, menu bar app connects via IPCClient.
"""

import json
import logging
import os
import select
import socket
import threading
import uuid
from typing import Callable, Optional

logger = logging.getLogger(__name__)

SOCKET_PATH = "/tmp/focusknob.sock"


class IPCServer:
    """Unix socket server for the daemon side.

    Accepts connections from the menu bar app, dispatches incoming
    messages to a handler callback, and pushes events to connected clients.
    """

    def __init__(self, handler: Callable[[dict], Optional[dict]]):
        """
        Args:
            handler: Called with each incoming message dict.
                     Return a response dict, or None for no response.
        """
        self.handler = handler
        self.sock = None
        self.clients = []  # list of connected client sockets
        self._clients_lock = threading.Lock()
        self._write_lock = threading.Lock()
        self.running = False

    def start(self):
        """Bind the socket and start the listener thread."""
        # Remove stale socket file
        if os.path.exists(SOCKET_PATH):
            try:
                os.unlink(SOCKET_PATH)
            except OSError:
                pass

        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.bind(SOCKET_PATH)
        os.chmod(SOCKET_PATH, 0o600)
        self.sock.listen(2)
        self.sock.setblocking(False)
        self.running = True

        thread = threading.Thread(target=self._accept_loop, daemon=True)
        thread.start()
        logger.info(f"IPC server listening on {SOCKET_PATH}")

    def _accept_loop(self):
        """Accept connections and spawn reader threads."""
        while self.running:
            try:
                readable, _, _ = select.select([self.sock], [], [], 1.0)
                if readable:
                    client, _ = self.sock.accept()
                    client.setblocking(True)
                    with self._clients_lock:
                        self.clients.append(client)
                    logger.info("IPC client connected")
                    threading.Thread(
                        target=self._client_reader,
                        args=(client,),
                        daemon=True,
                    ).start()
            except Exception as e:
                if self.running:
                    logger.debug(f"IPC accept error: {e}")

    def _client_reader(self, client: socket.socket):
        """Read newline-delimited JSON from a client."""
        buf = ""
        try:
            while self.running:
                data = client.recv(8192)
                if not data:
                    break
                buf += data.decode("utf-8", errors="ignore")
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        message = json.loads(line)
                        response = self.handler(message)
                        if response is not None:
                            self._send_to(client, response)
                    except json.JSONDecodeError:
                        logger.warning(f"IPC: invalid JSON: {line[:100]}")
                    except Exception as e:
                        logger.error(f"IPC handler error: {e}")
        except Exception as e:
            logger.debug(f"IPC client disconnected: {e}")
        finally:
            with self._clients_lock:
                if client in self.clients:
                    self.clients.remove(client)
            try:
                client.close()
            except Exception:
                pass
            logger.info("IPC client disconnected")

    def _send_to(self, client: socket.socket, message: dict):
        """Send a JSON message to a specific client."""
        try:
            data = json.dumps(message, separators=(",", ":")) + "\n"
            with self._write_lock:
                client.sendall(data.encode("utf-8"))
        except Exception as e:
            logger.debug(f"IPC send error: {e}")

    def push_event(self, event: dict):
        """Send an event to all connected clients."""
        with self._clients_lock:
            clients = list(self.clients)
        for client in clients:
            self._send_to(client, event)

    def has_clients(self) -> bool:
        """Check if any clients are connected."""
        with self._clients_lock:
            return len(self.clients) > 0

    def stop(self):
        """Clean shutdown."""
        self.running = False
        with self._clients_lock:
            for client in self.clients:
                try:
                    client.close()
                except Exception:
                    pass
            self.clients.clear()
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        if os.path.exists(SOCKET_PATH):
            try:
                os.unlink(SOCKET_PATH)
            except Exception:
                pass
        logger.info("IPC server stopped")


class IPCClient:
    """Unix socket client for the menu bar app side.

    Connects to the daemon's IPC socket, sends requests,
    and listens for pushed events.
    """

    def __init__(self, socket_path: str = SOCKET_PATH):
        self.socket_path = socket_path
        self.sock = None
        self.connected = False
        self._write_lock = threading.Lock()
        self._buf = ""
        self._pending = {}  # id -> threading.Event, result holder
        self._pending_lock = threading.Lock()
        self._event_callback = None  # callable for pushed events

    def connect(self) -> bool:
        """Connect to the daemon socket."""
        try:
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.sock.connect(self.socket_path)
            self.sock.setblocking(True)
            self.connected = True
            logger.info("IPC connected to daemon")
            return True
        except Exception as e:
            self.sock = None
            self.connected = False
            logger.debug(f"IPC connect failed: {e}")
            return False

    def set_event_callback(self, callback: Callable[[dict], None]):
        """Set callback for server-pushed events (input_request, status_update)."""
        self._event_callback = callback

    def send(self, message: dict):
        """Send a JSON message to the daemon."""
        if not self.connected or not self.sock:
            raise ConnectionError("Not connected")
        try:
            data = json.dumps(message, separators=(",", ":")) + "\n"
            with self._write_lock:
                self.sock.sendall(data.encode("utf-8"))
        except Exception as e:
            self.connected = False
            raise ConnectionError(f"Send failed: {e}")

    def request(self, msg_type: str, timeout: float = 10.0, **kwargs) -> Optional[dict]:
        """Send a request and wait for the corresponding response.

        Returns the response dict, or None on timeout.
        """
        req_id = str(uuid.uuid4())
        event = threading.Event()
        holder = {"result": None}

        with self._pending_lock:
            self._pending[req_id] = (event, holder)

        try:
            self.send({"type": msg_type, "id": req_id, **kwargs})
        except ConnectionError:
            with self._pending_lock:
                self._pending.pop(req_id, None)
            return None

        event.wait(timeout=timeout)

        with self._pending_lock:
            self._pending.pop(req_id, None)

        return holder["result"]

    def listen(self):
        """Blocking loop that reads messages from the socket.

        Routes responses to pending requests and pushes events to the callback.
        Call this in a background thread.
        """
        while self.connected:
            try:
                data = self.sock.recv(8192)
                if not data:
                    self.connected = False
                    break
                self._buf += data.decode("utf-8", errors="ignore")
                while "\n" in self._buf:
                    line, self._buf = self._buf.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        message = json.loads(line)
                        self._dispatch(message)
                    except json.JSONDecodeError:
                        logger.warning(f"IPC client: invalid JSON: {line[:100]}")
            except Exception as e:
                if self.connected:
                    logger.debug(f"IPC listen error: {e}")
                self.connected = False
                break

        logger.info("IPC listener stopped")

    def _dispatch(self, message: dict):
        """Route an incoming message to the right handler."""
        msg_id = message.get("id")
        msg_type = message.get("type", "")

        # Check if this is a response to a pending request
        if msg_id and msg_type.endswith("_result"):
            with self._pending_lock:
                entry = self._pending.get(msg_id)
            if entry:
                event, holder = entry
                holder["result"] = message
                event.set()
                return

        # Otherwise it's a pushed event
        if self._event_callback:
            try:
                self._event_callback(message)
            except Exception as e:
                logger.error(f"IPC event callback error: {e}")

    def close(self):
        """Close the connection."""
        self.connected = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None
