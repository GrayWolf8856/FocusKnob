#!/usr/bin/env python3
"""
FocusKnob Menu Bar App

macOS menu bar application that:
- Shows device connection status
- Provides settings UI for Jira/Notion credentials
- Handles user-input prompts from the daemon (work descriptions, durations)
- Controls the background sync service

Communicates with focusknob_sync.py daemon via Unix socket IPC.
"""

import json
import logging
import os
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Optional

import rumps

from focusknob_ipc import IPCClient

# Logging
LOG_DIR = Path.home() / "Library" / "Logs" / "FocusKnob"
LOG_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[
        logging.FileHandler(LOG_DIR / "menu.log"),
        logging.StreamHandler(),
    ],
)
logger = logging.getLogger(__name__)

# Config paths
CONFIG_DIR = Path.home() / "Library" / "Application Support" / "FocusKnob"
CONFIG_FILE = CONFIG_DIR / "config.json"
ICON_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon.png")


def load_config() -> dict:
    """Load config from file."""
    defaults = {
        "notion_token": "",
        "notion_database_id": "",
        "jira_base_url": "",
        "jira_email": "",
        "jira_api_token": "",
        "serial_port": "",
        "openweathermap_api_key": "",
        "weather_location": "",
        "calendar_tenant_id": "",
        "calendar_client_id": "",
    }
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE) as f:
                saved = json.load(f)
                defaults.update(saved)
        except Exception:
            pass
    return defaults


def save_config(config_data: dict):
    """Save config to file with restricted permissions."""
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    existing = load_config()
    existing.update(config_data)
    with open(CONFIG_FILE, "w") as f:
        json.dump(existing, f, indent=2)
    os.chmod(CONFIG_FILE, 0o600)


def test_jira(base_url: str, email: str, api_token: str) -> tuple:
    """Test Jira API connection. Returns (success, message)."""
    import requests

    if not base_url or not email or not api_token:
        return False, "Please enter all Jira fields"
    base_url = base_url.rstrip("/")
    try:
        resp = requests.get(
            f"{base_url}/rest/api/3/myself",
            headers={"Accept": "application/json"},
            auth=(email, api_token),
            timeout=10,
        )
        if resp.status_code == 200:
            name = resp.json().get("displayName", "Unknown")
            return True, f"Connected as {name}"
        elif resp.status_code == 401:
            return False, "Invalid credentials"
        else:
            return False, f"API error: {resp.status_code}"
    except Exception as e:
        return False, str(e)


def test_notion(token: str, database_id: str) -> tuple:
    """Test Notion API connection. Returns (success, message)."""
    import requests

    if not token or not database_id:
        return False, "Please enter both token and database ID"
    try:
        resp = requests.get(
            f"https://api.notion.com/v1/databases/{database_id}",
            headers={
                "Authorization": f"Bearer {token}",
                "Notion-Version": "2022-06-28",
            },
            timeout=10,
        )
        if resp.status_code == 200:
            data = resp.json()
            title = "Untitled"
            if data.get("title"):
                title = data["title"][0].get("plain_text", "Untitled")
            return True, f"Connected to: {title}"
        elif resp.status_code == 401:
            return False, "Invalid API token"
        elif resp.status_code == 404:
            return False, "Database not found"
        else:
            return False, f"API error: {resp.status_code}"
    except Exception as e:
        return False, str(e)


def test_weather(api_key: str, location: str) -> tuple:
    """Test OpenWeatherMap API connection. Returns (success, message)."""
    import requests

    if not api_key or not location:
        return False, "Please enter API key and location"
    try:
        resp = requests.get(
            "https://api.openweathermap.org/data/2.5/weather",
            params={"q": location, "appid": api_key, "units": "imperial"},
            timeout=10,
        )
        if resp.status_code == 200:
            data = resp.json()
            city = data.get("name", location)
            temp = data["main"]["temp"]
            return True, f"{city}: {temp:.0f}\u00b0F"
        elif resp.status_code == 401:
            return False, "Invalid API key"
        elif resp.status_code == 404:
            return False, "City not found"
        else:
            return False, f"API error: {resp.status_code}"
    except Exception as e:
        return False, str(e)


def test_calendar(tenant_id: str, client_id: str) -> tuple:
    """Test Microsoft Graph calendar connection. Returns (success, message)."""
    if not tenant_id or not client_id:
        return False, "Please enter Tenant ID and Client ID"
    try:
        from calendar_client import CalendarClient
        cache_dir = str(CONFIG_DIR)
        client = CalendarClient(tenant_id, client_id, cache_dir)
        return client.test_connection()
    except Exception as e:
        return False, str(e)


# ── Settings Window (PyObjC NSWindow) ────────────────────────────────

# Define the ObjC handler class once at module level to avoid re-registration errors
_SettingsHandler = None


def _get_settings_handler_class():
    """Get or create the ButtonHandler ObjC class (registered once)."""
    global _SettingsHandler
    if _SettingsHandler is not None:
        return _SettingsHandler

    import objc
    from AppKit import NSColor

    class _FKSettingsHandler(objc.lookUpClass("NSObject")):
        # Instance variables set after alloc/init
        _fields = None
        _window = None
        _app = None
        _jira_status = None
        _notion_status = None
        _weather_status = None
        _calendar_status = None

        def testJira_(self, sender):
            fields = self._fields
            url = fields["jira_base_url"].stringValue()
            email = fields["jira_email"].stringValue()
            token = fields["jira_api_token"].stringValue()
            status_label = self._jira_status

            def do_test():
                ok, msg = test_jira(url, email, token)
                def update():
                    status_label.setStringValue_(msg)
                    c = NSColor.systemGreenColor() if ok else NSColor.systemRedColor()
                    status_label.setTextColor_(c)
                rumps.Timer(lambda _: update(), 0).start()

            status_label.setStringValue_("Testing...")
            status_label.setTextColor_(NSColor.secondaryLabelColor())
            threading.Thread(target=do_test, daemon=True).start()

        def testNotion_(self, sender):
            fields = self._fields
            token = fields["notion_token"].stringValue()
            db_id = fields["notion_database_id"].stringValue()
            status_label = self._notion_status

            def do_test():
                ok, msg = test_notion(token, db_id)
                def update():
                    status_label.setStringValue_(msg)
                    c = NSColor.systemGreenColor() if ok else NSColor.systemRedColor()
                    status_label.setTextColor_(c)
                rumps.Timer(lambda _: update(), 0).start()

            status_label.setStringValue_("Testing...")
            status_label.setTextColor_(NSColor.secondaryLabelColor())
            threading.Thread(target=do_test, daemon=True).start()

        def testWeather_(self, sender):
            fields = self._fields
            api_key = fields["openweathermap_api_key"].stringValue()
            location = fields["weather_location"].stringValue()
            status_label = self._weather_status

            def do_test():
                ok, msg = test_weather(api_key, location)
                def update():
                    status_label.setStringValue_(msg)
                    c = NSColor.systemGreenColor() if ok else NSColor.systemRedColor()
                    status_label.setTextColor_(c)
                rumps.Timer(lambda _: update(), 0).start()

            status_label.setStringValue_("Testing...")
            status_label.setTextColor_(NSColor.secondaryLabelColor())
            threading.Thread(target=do_test, daemon=True).start()

        def testCalendar_(self, sender):
            fields = self._fields
            tenant_id = fields["calendar_tenant_id"].stringValue()
            client_id = fields["calendar_client_id"].stringValue()
            status_label = self._calendar_status

            def do_test():
                ok, msg = test_calendar(tenant_id, client_id)
                def update():
                    status_label.setStringValue_(msg)
                    c = NSColor.systemGreenColor() if ok else NSColor.systemRedColor()
                    status_label.setTextColor_(c)
                rumps.Timer(lambda _: update(), 0).start()

            status_label.setStringValue_("Testing...")
            status_label.setTextColor_(NSColor.secondaryLabelColor())
            threading.Thread(target=do_test, daemon=True).start()

        def save_(self, sender):
            new_config = {}
            for key, field in self._fields.items():
                new_config[key] = field.stringValue()
            save_config(new_config)
            logger.info("Settings saved")
            self._window.close()
            app = self._app
            if app.ipc and app.ipc.connected:
                try:
                    result = app.ipc.request("reload_config", timeout=5)
                    if result and result.get("success"):
                        logger.info(f"Daemon: {result.get('message')}")
                        rumps.notification("FocusKnob", "Settings Saved",
                                           result.get("message", ""))
                    else:
                        rumps.notification("FocusKnob", "Settings Saved",
                                           "Config saved but daemon reload failed")
                except Exception as e:
                    logger.error(f"Reload config IPC failed: {e}")
            else:
                rumps.notification("FocusKnob", "Settings Saved",
                                   "Restart service to apply changes")

        def cancel_(self, sender):
            self._window.close()

    _SettingsHandler = _FKSettingsHandler
    return _SettingsHandler


def show_settings_window(app: "FocusKnobApp"):
    """Show a native macOS settings window using PyObjC."""
    import objc
    from AppKit import (
        NSWindow, NSTextField, NSSecureTextField, NSButton,
        NSBezelStyleRounded, NSBackingStoreBuffered,
        NSWindowStyleMaskTitled, NSWindowStyleMaskClosable,
        NSApp, NSFont, NSColor,
        NSMakeRect,
    )

    config = load_config()

    # Window
    style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
    window = NSWindow.alloc().initWithContentRect_styleMask_backing_defer_(
        NSMakeRect(0, 0, 440, 830), style, NSBackingStoreBuffered, False
    )
    window.setTitle_("FocusKnob Settings")
    window.center()
    window.setLevel_(3)  # Floating
    content = window.contentView()

    fields = {}
    y = 780  # Start from top

    def add_label(text, ypos, bold=False):
        label = NSTextField.alloc().initWithFrame_(NSMakeRect(20, ypos, 400, 20))
        label.setStringValue_(text)
        label.setBezeled_(False)
        label.setEditable_(False)
        label.setDrawsBackground_(False)
        if bold:
            label.setFont_(NSFont.boldSystemFontOfSize_(13))
        else:
            label.setFont_(NSFont.systemFontOfSize_(11))
            label.setTextColor_(NSColor.secondaryLabelColor())
        content.addSubview_(label)
        return label

    def add_field(name, value, ypos, secure=False):
        cls = NSSecureTextField if secure else NSTextField
        field = cls.alloc().initWithFrame_(NSMakeRect(20, ypos, 400, 24))
        field.setStringValue_(value or "")
        field.setFont_(NSFont.systemFontOfSize_(13))
        content.addSubview_(field)
        fields[name] = field
        return field

    # Jira section
    add_label("Jira", y, bold=True); y -= 28
    add_label("Base URL", y); y -= 22
    add_field("jira_base_url", config.get("jira_base_url"), y); y -= 34
    add_label("Email", y); y -= 22
    add_field("jira_email", config.get("jira_email"), y); y -= 34
    add_label("API Token", y); y -= 22
    add_field("jira_api_token", config.get("jira_api_token"), y, secure=True); y -= 34

    # Test Jira button
    test_jira_btn = NSButton.alloc().initWithFrame_(NSMakeRect(20, y, 100, 28))
    test_jira_btn.setTitle_("Test Jira")
    test_jira_btn.setBezelStyle_(NSBezelStyleRounded)
    content.addSubview_(test_jira_btn)
    jira_status_label = add_label("", y + 2)
    jira_status_label.setFrame_(NSMakeRect(130, y + 4, 280, 20))
    y -= 30

    # Divider space
    y -= 10

    # Notion section
    add_label("Notion (optional)", y, bold=True); y -= 28
    add_label("API Token", y); y -= 22
    add_field("notion_token", config.get("notion_token"), y, secure=True); y -= 34
    add_label("Database ID", y); y -= 22
    add_field("notion_database_id", config.get("notion_database_id"), y); y -= 34

    # Test Notion button
    test_notion_btn = NSButton.alloc().initWithFrame_(NSMakeRect(20, y, 120, 28))
    test_notion_btn.setTitle_("Test Notion")
    test_notion_btn.setBezelStyle_(NSBezelStyleRounded)
    content.addSubview_(test_notion_btn)
    notion_status_label = add_label("", y + 2)
    notion_status_label.setFrame_(NSMakeRect(150, y + 4, 260, 20))
    y -= 30

    # Divider space
    y -= 10

    # Weather section
    add_label("Weather (optional)", y, bold=True); y -= 28
    add_label("OpenWeatherMap API Key", y); y -= 22
    add_field("openweathermap_api_key", config.get("openweathermap_api_key"), y); y -= 34
    add_label("City or Zip Code (e.g. San Francisco or 94102)", y); y -= 22
    add_field("weather_location", config.get("weather_location"), y); y -= 34

    # Test Weather button
    test_weather_btn = NSButton.alloc().initWithFrame_(NSMakeRect(20, y, 130, 28))
    test_weather_btn.setTitle_("Test Weather")
    test_weather_btn.setBezelStyle_(NSBezelStyleRounded)
    content.addSubview_(test_weather_btn)
    weather_status_label = add_label("", y + 2)
    weather_status_label.setFrame_(NSMakeRect(160, y + 4, 250, 20))
    y -= 30

    # Divider space
    y -= 10

    # Calendar section
    add_label("Outlook Calendar (optional)", y, bold=True); y -= 28
    add_label("Azure AD Tenant ID", y); y -= 22
    add_field("calendar_tenant_id", config.get("calendar_tenant_id"), y); y -= 34
    add_label("Client ID (Application ID)", y); y -= 22
    add_field("calendar_client_id", config.get("calendar_client_id"), y); y -= 34

    # Test Calendar button
    test_calendar_btn = NSButton.alloc().initWithFrame_(NSMakeRect(20, y, 140, 28))
    test_calendar_btn.setTitle_("Test Calendar")
    test_calendar_btn.setBezelStyle_(NSBezelStyleRounded)
    content.addSubview_(test_calendar_btn)
    calendar_status_label = add_label("", y + 2)
    calendar_status_label.setFrame_(NSMakeRect(170, y + 4, 250, 20))
    y -= 30

    # Divider space
    y -= 10

    # Serial port
    add_label("Serial Port Override (optional)", y, bold=True); y -= 28
    add_label("Leave blank for auto-detect", y); y -= 22
    add_field("serial_port", config.get("serial_port"), y); y -= 44

    # Save / Cancel buttons
    cancel_btn = NSButton.alloc().initWithFrame_(NSMakeRect(240, y, 90, 32))
    cancel_btn.setTitle_("Cancel")
    cancel_btn.setBezelStyle_(NSBezelStyleRounded)
    content.addSubview_(cancel_btn)

    save_btn = NSButton.alloc().initWithFrame_(NSMakeRect(340, y, 80, 32))
    save_btn.setTitle_("Save")
    save_btn.setBezelStyle_(NSBezelStyleRounded)
    save_btn.setKeyEquivalent_("\r")  # Enter key
    content.addSubview_(save_btn)

    # Create handler instance and wire up references
    HandlerClass = _get_settings_handler_class()
    handler = HandlerClass.alloc().init()
    handler._fields = fields
    handler._window = window
    handler._app = app
    handler._jira_status = jira_status_label
    handler._notion_status = notion_status_label
    handler._weather_status = weather_status_label
    handler._calendar_status = calendar_status_label

    test_jira_btn.setTarget_(handler)
    test_jira_btn.setAction_(objc.selector(handler.testJira_, signature=b"v@:@"))
    test_notion_btn.setTarget_(handler)
    test_notion_btn.setAction_(objc.selector(handler.testNotion_, signature=b"v@:@"))
    test_weather_btn.setTarget_(handler)
    test_weather_btn.setAction_(objc.selector(handler.testWeather_, signature=b"v@:@"))
    test_calendar_btn.setTarget_(handler)
    test_calendar_btn.setAction_(objc.selector(handler.testCalendar_, signature=b"v@:@"))
    save_btn.setTarget_(handler)
    save_btn.setAction_(objc.selector(handler.save_, signature=b"v@:@"))
    cancel_btn.setTarget_(handler)
    cancel_btn.setAction_(objc.selector(handler.cancel_, signature=b"v@:@"))

    window.makeKeyAndOrderFront_(None)
    NSApp.activateIgnoringOtherApps_(True)

    # Keep references so window and handler aren't garbage collected
    app._settings_window = window
    app._settings_handler = handler


# ── Main Menu Bar App ────────────────────────────────────────────────

class FocusKnobApp(rumps.App):
    def __init__(self):
        icon = ICON_PATH if os.path.exists(ICON_PATH) else None
        super().__init__(
            "FocusKnob",
            icon=icon,
            template=True,
            quit_button=None,
        )

        # Menu items
        self.title_item = rumps.MenuItem("FocusKnob", callback=None)
        self.title_item.set_callback(None)
        self.status_item = rumps.MenuItem("Status: Connecting...", callback=None)
        self.status_item.set_callback(None)

        self.menu = [
            self.title_item,
            self.status_item,
            None,  # separator
            rumps.MenuItem("Settings\u2026", callback=self.on_settings),
            rumps.MenuItem("View Logs", callback=self.on_view_logs),
            None,  # separator
            rumps.MenuItem("Restart Service", callback=self.on_restart),
            rumps.MenuItem("Quit", callback=self.on_quit),
        ]

        # IPC client
        self.ipc = IPCClient()
        self._ipc_thread = None
        self._reconnect_timer = None
        self._status_timer = None
        self._settings_window = None

        # Thread-safe queue for dispatching IPC events to the main thread
        self._event_queue = queue.Queue()

    def _start_ipc(self):
        """Connect to daemon and start listening."""
        if self.ipc.connect():
            self.ipc.set_event_callback(self._on_ipc_event)
            self._ipc_thread = threading.Thread(target=self._ipc_listen, daemon=True)
            self._ipc_thread.start()
            # Request initial status
            self._poll_status()
        else:
            self._schedule_reconnect()

    def _ipc_listen(self):
        """Background thread: read messages from daemon."""
        self.ipc.listen()
        # If we get here, connection dropped
        logger.info("IPC connection lost, will reconnect")
        self._update_status_text("Status: Daemon disconnected")
        self._schedule_reconnect()

    def _schedule_reconnect(self):
        """Schedule a reconnection attempt."""
        def reconnect(_):
            if not self.ipc.connected:
                self._start_ipc()
        # Use a one-shot timer to reconnect after 3 seconds
        t = rumps.Timer(reconnect, 3)
        t.start()
        # Stop after one fire
        def stop_after(_):
            t.stop()
        rumps.Timer(stop_after, 4).start()

    def _poll_status(self):
        """Request status from daemon."""
        if not self.ipc.connected:
            return
        try:
            result = self.ipc.request("status", timeout=5)
            if result:
                connected = result.get("connected", False)
                port = result.get("port", "")
                if connected and port:
                    text = f"Status: Connected ({port.split('/')[-1]})"
                elif connected:
                    text = "Status: Connected"
                else:
                    text = "Status: Device disconnected"
                self._update_status_text(text)
        except Exception as e:
            logger.debug(f"Status poll failed: {e}")

    def _drain_event_queue(self, _):
        """Called on main thread by a repeating rumps.Timer to process IPC events."""
        while True:
            try:
                event = self._event_queue.get_nowait()
            except queue.Empty:
                break

            event_type = event.get("type")

            if event_type == "status_update":
                connected = event.get("connected", False)
                port = event.get("port", "")
                if connected and port:
                    text = f"Status: Connected ({port.split('/')[-1]})"
                else:
                    text = "Status: Device disconnected"
                self.status_item.title = text

            elif event_type == "_set_status_text":
                self.status_item.title = event.get("text", "")

            elif event_type == "input_request":
                logger.info(f"Processing input_request on main thread: prompt={event.get('prompt')}")
                self._handle_input_request(event)

    def _update_status_text(self, text: str):
        """Update the status menu item (thread-safe via event queue)."""
        self._event_queue.put({"type": "_set_status_text", "text": text})

    def _on_ipc_event(self, event: dict):
        """Handle pushed events from daemon (called from IPC listener thread).

        We cannot interact with rumps or AppKit from this thread.
        Instead, enqueue events for the main-thread poller to pick up.
        """
        logger.info(f"IPC event received (bg thread): {event.get('type')}")
        self._event_queue.put(event)

    def _handle_input_request(self, event: dict):
        """Show a dialog for daemon input requests (runs on main thread)."""
        from AppKit import NSApp, NSApplication, NSApplicationActivationPolicyRegular, NSApplicationActivationPolicyAccessory

        # Temporarily become a regular app so the dialog takes focus
        # (menu bar apps are "accessory" apps and can't steal focus)
        NSApp.setActivationPolicy_(NSApplicationActivationPolicyRegular)
        NSApp.activateIgnoringOtherApps_(True)

        request_id = event.get("id")
        prompt = event.get("prompt", "")
        fields = event.get("fields", [])

        if not fields:
            return

        field = fields[0]
        label = field.get("label", "Input needed")
        default = field.get("default", "")
        field_name = field.get("name", "value")

        if prompt == "work_description":
            title = "FocusKnob \u2014 Log Work"
            ok_text = "Log to Jira"
            cancel_text = "Skip"
        elif prompt == "duration":
            title = "FocusKnob \u2014 Log Time"
            ok_text = "OK"
            cancel_text = "Cancel"
        elif prompt == "meeting_log":
            title = "FocusKnob \u2014 Log Meeting"
            ok_text = "Log to Jira"
            cancel_text = "Skip"
        else:
            title = "FocusKnob"
            ok_text = "OK"
            cancel_text = "Cancel"

        window = rumps.Window(
            message=label,
            title=title,
            default_text=default,
            ok=ok_text,
            cancel=cancel_text,
            dimensions=(320, 24),
        )

        response = window.run()

        # Switch back to accessory app (hide from dock)
        from AppKit import NSApplicationActivationPolicyAccessory
        NSApp.setActivationPolicy_(NSApplicationActivationPolicyAccessory)

        if response.clicked:
            # User pressed OK
            value = response.text.strip() or default
            self._send_input_response(request_id, {field_name: value})
        else:
            # User cancelled
            self._send_input_cancel(request_id)

    def _send_input_response(self, request_id: str, data: dict):
        """Send user input response to daemon."""
        if self.ipc.connected:
            try:
                self.ipc.send({
                    "type": "input_response",
                    "id": request_id,
                    "data": data,
                })
            except Exception as e:
                logger.error(f"Failed to send input response: {e}")

    def _send_input_cancel(self, request_id: str):
        """Send cancellation to daemon."""
        if self.ipc.connected:
            try:
                self.ipc.send({
                    "type": "input_response",
                    "id": request_id,
                    "cancelled": True,
                })
            except Exception as e:
                logger.error(f"Failed to send input cancel: {e}")

    # ── Menu callbacks ───────────────────────────────────────────────

    def on_settings(self, _):
        """Open settings window."""
        show_settings_window(self)

    def on_view_logs(self, _):
        """Open sync log in Console.app."""
        log_file = LOG_DIR / "sync.log"
        if log_file.exists():
            subprocess.Popen(["open", "-a", "Console", str(log_file)])
        else:
            rumps.notification("FocusKnob", "No Logs", "Log file not found")

    def on_restart(self, _):
        """Restart the sync daemon."""
        if self.ipc.connected:
            try:
                result = self.ipc.request("restart", timeout=5)
                if result and result.get("success"):
                    self._update_status_text("Status: Restarting...")
                    rumps.notification("FocusKnob", "Service Restarting", "")
            except Exception as e:
                logger.error(f"Restart IPC failed: {e}")
                rumps.notification("FocusKnob", "Restart Failed", str(e))
        else:
            # Try launchctl directly
            plist = Path.home() / "Library" / "LaunchAgents" / "com.focusknob.sync.plist"
            if plist.exists():
                subprocess.run(["launchctl", "unload", str(plist)], capture_output=True)
                subprocess.run(["launchctl", "load", str(plist)], capture_output=True)
                self._update_status_text("Status: Restarting...")
                rumps.notification("FocusKnob", "Service Restarting", "")

    def on_quit(self, _):
        """Quit the menu bar app (daemon keeps running)."""
        if self.ipc:
            self.ipc.close()
        rumps.quit_application()

    # ── Lifecycle ────────────────────────────────────────────────────

    @rumps.timer(30)
    def periodic_status(self, _):
        """Poll daemon status every 30 seconds."""
        if self.ipc.connected:
            self._poll_status()

    def run(self, **kwargs):
        """Start the app: connect IPC, then run the rumps event loop."""
        logger.info("FocusKnob Menu Bar app starting")
        # Start IPC connection in a short delay to let rumps init first
        t = rumps.Timer(lambda _: self._start_ipc(), 1)
        t.start()
        # Stop after one fire
        rumps.Timer(lambda _: t.stop(), 2).start()
        # Main-thread poller: drains the event queue every 0.5s
        self._queue_timer = rumps.Timer(self._drain_event_queue, 0.5)
        self._queue_timer.start()
        super().run(**kwargs)


def main():
    app = FocusKnobApp()
    app.run()


if __name__ == "__main__":
    main()
