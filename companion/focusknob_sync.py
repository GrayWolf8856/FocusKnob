#!/usr/bin/env python3
"""
FocusKnob Sync Service

Background service that:
- Auto-detects FocusKnob USB connection
- Syncs time on connect
- Forwards time logs and notes to Notion API

Usage:
    python3 focusknob_sync.py

Environment variables:
    NOTION_TOKEN - Notion integration token
    NOTION_DATABASE_ID - Notion database ID for logs
"""

import os
import sys
import json
import signal
import time
import uuid
import logging
import datetime
import subprocess
import threading
from pathlib import Path
from typing import Optional, List

import serial
import serial.tools.list_ports
import requests

from jira_client import JiraClient
from weather_client import WeatherClient
from calendar_client import CalendarClient
from focusknob_ipc import IPCServer

# Configuration
BAUD_RATE = 115200
DEVICE_NAME = "FocusKnob"
POLL_INTERVAL = 2.0  # seconds between checking for device
PING_INTERVAL = 5.0  # seconds between pings when connected
COMMAND_TIMEOUT = 3.0  # seconds to wait for response

# Logging setup
LOG_DIR = Path.home() / "Library" / "Logs" / "FocusKnob"
LOG_DIR.mkdir(parents=True, exist_ok=True)
LOG_FILE = LOG_DIR / "sync.log"

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)


class NotionClient:
    """Handles Notion API interactions."""

    def __init__(self, token: str, database_id: str):
        self.token = token
        self.database_id = database_id
        self.base_url = "https://api.notion.com/v1"
        self.headers = {
            "Authorization": f"Bearer {token}",
            "Notion-Version": "2022-06-28",
            "Content-Type": "application/json"
        }

    def add_log_entry(self, log_data: dict) -> bool:
        """Add a time log entry to Notion database."""
        try:
            properties = {
                "Date": {
                    "date": {
                        "start": log_data.get("date", datetime.date.today().isoformat())
                    }
                },
                "Work Minutes": {
                    "number": log_data.get("total_work_minutes", 0)
                },
                "Break Minutes": {
                    "number": log_data.get("total_break_minutes", 0)
                },
                "Pomodoros": {
                    "number": log_data.get("pomodoros", 0)
                }
            }

            payload = {
                "parent": {"database_id": self.database_id},
                "properties": properties
            }

            response = requests.post(
                f"{self.base_url}/pages",
                headers=self.headers,
                json=payload,
                timeout=10
            )

            if response.status_code == 200:
                logger.info(f"Added log entry to Notion: {log_data.get('date')}")
                return True
            else:
                logger.error(f"Notion API error: {response.status_code} - {response.text}")
                return False

        except Exception as e:
            logger.error(f"Failed to add Notion entry: {e}")
            return False

    def add_note(self, note_data: dict) -> bool:
        """Add a note entry to Notion database."""
        try:
            properties = {
                "Title": {
                    "title": [
                        {
                            "text": {
                                "content": note_data.get("text", "")[:100]
                            }
                        }
                    ]
                },
                "Content": {
                    "rich_text": [
                        {
                            "text": {
                                "content": note_data.get("text", "")
                            }
                        }
                    ]
                },
                "Timestamp": {
                    "date": {
                        "start": note_data.get("timestamp", datetime.datetime.now().isoformat())
                    }
                }
            }

            payload = {
                "parent": {"database_id": self.database_id},
                "properties": properties
            }

            response = requests.post(
                f"{self.base_url}/pages",
                headers=self.headers,
                json=payload,
                timeout=10
            )

            if response.status_code == 200:
                logger.info("Added note to Notion")
                return True
            else:
                logger.error(f"Notion API error: {response.status_code} - {response.text}")
                return False

        except Exception as e:
            logger.error(f"Failed to add note to Notion: {e}")
            return False


class SerialMonitor:
    """Monitors USB serial ports for FocusKnob device."""

    def __init__(self):
        self.serial_port = None
        self.connected = False
        self.port_name = None

    def find_device(self) -> Optional[str]:
        """Find FocusKnob device port."""
        ports = serial.tools.list_ports.comports()
        for port in ports:
            # ESP32-S3 typically shows as USB Serial or with CP210x/CH340
            if any(x in port.description.lower() for x in ['usb', 'serial', 'esp32', 'cp210', 'ch340']):
                # Try to connect and verify it's FocusKnob
                if self._verify_device(port.device):
                    return port.device
        return None

    def _verify_device(self, port: str) -> bool:
        """Verify the device is FocusKnob by sending PING."""
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=COMMAND_TIMEOUT)
            time.sleep(0.5)  # Wait for device to be ready

            # Clear any pending data
            ser.reset_input_buffer()

            # Send PING
            ser.write(b"PING\n")
            ser.flush()

            # Wait for PONG response
            start = time.time()
            while time.time() - start < COMMAND_TIMEOUT:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line == "PONG":
                        ser.close()
                        return True

            ser.close()
            return False

        except Exception as e:
            logger.debug(f"Device verification failed for {port}: {e}")
            return False

    def connect(self, port: str) -> bool:
        """Connect to device."""
        try:
            self.serial_port = serial.Serial(port, BAUD_RATE, timeout=COMMAND_TIMEOUT)
            self.port_name = port
            self.connected = True
            time.sleep(0.5)
            logger.info(f"Connected to FocusKnob on {port}")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to {port}: {e}")
            return False

    def disconnect(self):
        """Disconnect from device."""
        if self.serial_port:
            try:
                self.serial_port.close()
            except:
                pass
        self.serial_port = None
        self.connected = False
        self.port_name = None
        logger.info("Disconnected from FocusKnob")

    def send_command(self, command: str) -> List[str]:
        """Send command and return response lines.

        Writes in small chunks (48 bytes) with short delays to avoid
        ESP32-S3 USB CDC byte drops on large payloads.
        """
        if not self.connected or not self.serial_port:
            return []

        try:
            data = f"{command}\n".encode()
            CHUNK_SIZE = 48
            for i in range(0, len(data), CHUNK_SIZE):
                self.serial_port.write(data[i:i + CHUNK_SIZE])
                self.serial_port.flush()
                if i + CHUNK_SIZE < len(data):
                    time.sleep(0.005)  # 5ms between chunks

            responses = []
            start = time.time()
            while time.time() - start < COMMAND_TIMEOUT:
                if self.serial_port.in_waiting:
                    line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        responses.append(line)
                        # Reset timeout on each response
                        start = time.time()
                else:
                    time.sleep(0.01)

            return responses

        except Exception as e:
            logger.error(f"Command failed: {e}")
            self.connected = False
            return []

    def read_line(self, timeout: float = 0.1) -> Optional[str]:
        """Read a single line with timeout."""
        if not self.connected or not self.serial_port:
            return None

        try:
            self.serial_port.timeout = timeout
            line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
            return line if line else None
        except Exception as e:
            logger.debug(f"Read error: {e}")
            return None

    def send_ok(self):
        """Send OK acknowledgment."""
        if self.connected and self.serial_port:
            try:
                self.serial_port.write(b"OK\n")
                self.serial_port.flush()
            except:
                pass


class TimeSync:
    """Handles time synchronization."""

    @staticmethod
    def get_current_time_iso() -> str:
        """Get current time in ISO8601 format."""
        return datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")

    @staticmethod
    def sync_time(monitor: SerialMonitor) -> bool:
        """Sync time to device."""
        time_str = TimeSync.get_current_time_iso()
        responses = monitor.send_command(f"TIME:{time_str}")

        for response in responses:
            if response == "TIME_OK":
                logger.info(f"Time synced to {time_str}")
                return True
            elif response.startswith("ERROR:"):
                logger.error(f"Time sync failed: {response}")
                return False

        logger.warning("Time sync: no response")
        return False


class FocusKnobSync:
    """Main sync service."""

    CONFIG_FILE = Path.home() / "Library" / "Application Support" / "FocusKnob" / "config.json"

    def __init__(self):
        self.monitor = SerialMonitor()
        self.notion = None
        self.jira = None
        self.running = False
        self._start_time = time.time()
        self._config_lock = threading.Lock()
        self._pending_inputs = {}  # uuid -> {"event": Event, "data": None}
        self._pending_lock = threading.Lock()

        # Load Notion credentials from config file or environment
        notion_token, notion_db = self._load_credentials()
        if notion_token and notion_db:
            self.notion = NotionClient(notion_token, notion_db)
            logger.info("Notion integration enabled")
        else:
            logger.warning("Notion credentials not set - sync to Notion disabled")

        # Load Jira credentials
        jira_url, jira_email, jira_token = self._load_jira_credentials()
        if jira_url and jira_email and jira_token:
            self.jira = JiraClient(jira_url, jira_email, jira_token)
            logger.info("Jira integration enabled")
        else:
            logger.info("Jira credentials not set - Jira sync disabled")

        # Load weather credentials
        weather_key, weather_loc = self._load_weather_credentials()
        if weather_key and weather_loc:
            self.weather = WeatherClient(weather_key, weather_loc)
            logger.info("Weather integration enabled")
        else:
            self.weather = None
            logger.info("Weather credentials not set - weather disabled")
        self._last_weather_sync = 0

        # Load calendar credentials
        calendar_tenant, calendar_client = self._load_calendar_credentials()
        if calendar_tenant and calendar_client:
            cache_dir = str(self.CONFIG_FILE.parent)
            self.calendar = CalendarClient(calendar_tenant, calendar_client, cache_dir)
            logger.info("Calendar integration enabled")
        else:
            self.calendar = None
            logger.info("Calendar credentials not set - calendar disabled")
        self._last_calendar_sync = 0

        # Jira hours sync tracking
        self._last_jira_hours_sync = 0

        # Meeting end tracking
        self._prompted_meetings = set()  # (date_str, meeting_hash)
        self._last_calendar_events = []  # cached from last sync
        self._today_str = ""  # for daily reset

        # Start IPC server for menu bar app communication
        self._ipc = IPCServer(handler=self._handle_ipc_message)
        self._ipc.start()

    def _load_credentials(self) -> tuple:
        """Load credentials from config file or environment variables."""
        # First try config file
        if self.CONFIG_FILE.exists():
            try:
                with open(self.CONFIG_FILE) as f:
                    config = json.load(f)
                    token = config.get("notion_token", "")
                    db_id = config.get("notion_database_id", "")
                    if token and db_id and not token.startswith("YOUR_"):
                        logger.info("Loaded credentials from config file")
                        return token, db_id
            except Exception as e:
                logger.warning(f"Failed to load config file: {e}")

        # Fall back to environment variables
        token = os.environ.get("NOTION_TOKEN", "")
        db_id = os.environ.get("NOTION_DATABASE_ID", "")
        if token and db_id and not token.startswith("YOUR_"):
            logger.info("Loaded credentials from environment")
            return token, db_id

        return None, None

    def _load_jira_credentials(self) -> tuple:
        """Load Jira credentials from config file or environment variables."""
        if self.CONFIG_FILE.exists():
            try:
                with open(self.CONFIG_FILE) as f:
                    config = json.load(f)
                    url = config.get("jira_base_url", "")
                    email = config.get("jira_email", "")
                    token = config.get("jira_api_token", "")
                    if url and email and token and not token.startswith("YOUR_"):
                        logger.info("Loaded Jira credentials from config file")
                        return url, email, token
            except Exception as e:
                logger.warning(f"Failed to load Jira config: {e}")

        url = os.environ.get("JIRA_BASE_URL", "")
        email = os.environ.get("JIRA_EMAIL", "")
        token = os.environ.get("JIRA_API_TOKEN", "")
        if url and email and token and not token.startswith("YOUR_"):
            logger.info("Loaded Jira credentials from environment")
            return url, email, token

        return None, None, None

    def _load_weather_credentials(self) -> tuple:
        """Load weather credentials from config file."""
        if self.CONFIG_FILE.exists():
            try:
                with open(self.CONFIG_FILE) as f:
                    config = json.load(f)
                    api_key = config.get("openweathermap_api_key", "")
                    location = config.get("weather_location", "")
                    if api_key and location and not api_key.startswith("YOUR_"):
                        logger.info("Loaded weather credentials from config file")
                        return api_key, location
            except Exception as e:
                logger.warning(f"Failed to load weather config: {e}")
        return None, None

    def _load_calendar_credentials(self) -> tuple:
        """Load calendar credentials from config file."""
        if self.CONFIG_FILE.exists():
            try:
                with open(self.CONFIG_FILE) as f:
                    config = json.load(f)
                    tenant_id = config.get("calendar_tenant_id", "")
                    client_id = config.get("calendar_client_id", "")
                    if tenant_id and client_id and not tenant_id.startswith("YOUR_"):
                        logger.info("Loaded calendar credentials from config file")
                        return tenant_id, client_id
            except Exception as e:
                logger.warning(f"Failed to load calendar config: {e}")
        return None, None

    def sync_calendar(self):
        """Fetch calendar events and send to device."""
        if not self.calendar:
            return

        data = self.calendar.get_upcoming_events()
        if data is None:
            logger.warning("Failed to fetch calendar data")
            return

        # Cache events for meeting-end tracking
        self._last_calendar_events = data.get("events", [])

        cal_json = json.dumps(data, separators=(",", ":"))
        command = f"CALENDAR:{cal_json}"
        responses = self.monitor.send_command(command)

        for response in responses:
            if response == "CALENDAR_OK":
                logger.info("Synced calendar to device")
                break
        else:
            logger.warning("No acknowledgment for calendar sync")

        # Check for ended meetings
        self._check_ended_meetings()

    def sync_weather(self):
        """Fetch weather data and send to device."""
        if not self.weather:
            return

        data = self.weather.get_current_and_forecast()
        if data is None:
            logger.warning("Failed to fetch weather data")
            return

        weather_json = json.dumps(data, separators=(",", ":"))
        command = f"WEATHER:{weather_json}"
        responses = self.monitor.send_command(command)

        for response in responses:
            if response == "WEATHER_OK":
                logger.info("Synced weather to device")
                return
        logger.warning("No acknowledgment for weather sync")

    def sync_jira_issues(self):
        """Fetch assigned Jira issues and send to device."""
        if not self.jira:
            return

        issues = self.jira.get_my_issues()
        if not issues:
            logger.warning("No Jira issues fetched")
            return

        issues_json = json.dumps(issues, separators=(",", ":"))
        command = f"JIRA_PROJECTS:{issues_json}"
        responses = self.monitor.send_command(command)

        for response in responses:
            if response == "JIRA_PROJECTS_OK":
                logger.info(f"Synced {len(issues)} Jira issues to device")
                return
        logger.warning("No acknowledgment for Jira issues sync")

    def sync_jira_hours(self):
        """Fetch today's Jira hours and send to device."""
        if not self.jira:
            return

        hours_data = self.jira.get_today_worklogs()
        if hours_data is None:
            logger.warning("Failed to fetch Jira hours data")
            return

        hours_json = json.dumps(hours_data, separators=(",", ":"))
        command = f"JIRA_HOURS:{hours_json}"
        responses = self.monitor.send_command(command)

        for response in responses:
            if response == "JIRA_HOURS_OK":
                logger.info(f"Synced Jira hours: {hours_data.get('logged_min', 0)} min")
                return
        logger.warning("No acknowledgment for Jira hours sync")

    def _check_ended_meetings(self):
        """Check if any meetings have ended and prompt to log them."""
        if not self.jira or not self.calendar:
            return

        now = datetime.datetime.now()
        today_str = now.strftime("%Y-%m-%d")

        # Daily reset
        if today_str != self._today_str:
            self._prompted_meetings.clear()
            self._today_str = today_str

        # Weekend skip
        if now.weekday() >= 5:
            return

        for event in self._last_calendar_events:
            # Skip all-day events
            if event.get("is_all_day"):
                continue

            # Skip very short meetings (< 10 min)
            duration_min = event.get("duration_min", 0)
            if duration_min < 10:
                continue

            # Parse end time
            end_time_str = event.get("end_time", "")
            if not end_time_str:
                continue

            try:
                end_hour, end_minute = map(int, end_time_str.split(":"))
                end_dt = now.replace(hour=end_hour, minute=end_minute, second=0, microsecond=0)
            except (ValueError, AttributeError):
                continue

            # Check: meeting has ended within 0-10 minutes
            title = event.get("title", "Unknown")
            meeting_key = (today_str, hash((title, end_time_str)))

            if meeting_key in self._prompted_meetings:
                continue

            time_since_end = (now - end_dt).total_seconds() / 60
            if time_since_end < 0 or time_since_end > 10:
                continue

            # Mark as prompted
            self._prompted_meetings.add(meeting_key)

            # Prompt user
            self._prompt_meeting_log(title, duration_min)

    def _prompt_meeting_log(self, meeting_title: str, duration_min: int) -> bool:
        """Show popup asking user to log a meeting to Jira. Returns True on success."""
        # Play notification sound
        subprocess.Popen(
            ["afplay", "/System/Library/Sounds/Glass.aiff"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        # Build suggestion list
        issue_list = ""
        if self.jira:
            issues = self.jira.get_my_issues()
            if issues:
                suggestions = [f"{iss['key']} - {iss['name']}" for iss in issues[:5]]
                issue_list = "\n".join(suggestions)

        prompt_text = (
            f"Meeting ended: {meeting_title}\n"
            f"Duration: {duration_min} min\n\n"
            f"Enter Jira issue key to log to:"
        )
        if issue_list:
            prompt_text += f"\n\n{issue_list}"

        response = self._request_user_input(
            prompt="meeting_log",
            meeting_title=meeting_title,
            duration=duration_min,
            fields=[{
                "name": "issue_key",
                "type": "text",
                "label": prompt_text,
                "default": "",
            }],
            timeout=120,
        )

        if response is None or response.get("cancelled"):
            logger.info(f"User skipped meeting log for: {meeting_title}")
            return False

        issue_key = response.get("data", {}).get("issue_key", "").strip().upper()
        if not issue_key:
            return False

        # Log to Jira
        time_spent_seconds = duration_min * 60
        description = f"Meeting: {meeting_title}"
        success = self.jira.log_work(issue_key, time_spent_seconds, description)

        if success:
            logger.info(f"Logged {duration_min}min meeting '{meeting_title}' to {issue_key}")
            # Refresh hours display
            self.sync_jira_hours()
            return True
        else:
            logger.error(f"Failed to log meeting to {issue_key}")
            return False

    def handle_jira_log_meeting(self, payload: str):
        """Handle JIRA_LOG_MEETING from device — user wants to log a calendar meeting.

        Payload format: MEETING_TITLE|DURATION_MIN
        """
        try:
            payload = payload.strip().replace("\x00", "")
            logger.info(f"JIRA_LOG_MEETING raw: {payload!r}")

            parts = payload.split("|", 1)
            if len(parts) < 2:
                logger.error(f"Invalid JIRA_LOG_MEETING format: {payload!r}")
                self.monitor.send_command("JIRA_LOG_ERROR:Invalid data")
                return

            meeting_title = parts[0].strip()
            try:
                duration_minutes = int(parts[1].strip())
            except ValueError:
                duration_minutes = 30

            if not self.jira:
                self.monitor.send_command("JIRA_LOG_ERROR:Jira not configured")
                return

            success = self._prompt_meeting_log(meeting_title, duration_minutes)
            if success:
                self.monitor.send_command("JIRA_LOG_OK")
            else:
                self.monitor.send_command("JIRA_LOG_ERROR:Cancelled or failed")

        except Exception as e:
            logger.error(f"JIRA_LOG_MEETING handler error: {e}")
            self.monitor.send_command("JIRA_LOG_ERROR:Error processing")

    def handle_jira_timer_done(self, payload: str):
        """Handle JIRA_TIMER_DONE from device — use mic for description, log to Jira.

        Payload format: ISSUE_KEY|MINUTES  (e.g. "DEMOCAI-59|1")
        Simple pipe-delimited to avoid USB CDC byte-drop on long JSON.
        """
        try:
            payload = payload.strip().replace("\x00", "")
            logger.info(f"JIRA_TIMER_DONE raw: {payload!r}")

            parts = payload.split("|")
            if len(parts) < 2:
                logger.error(f"Invalid JIRA_TIMER_DONE format: {payload!r}")
                self.monitor.send_command("JIRA_LOG_ERROR:Invalid data")
                return

            issue_key = parts[0].strip()
            try:
                duration_minutes = int(parts[1].strip())
            except ValueError:
                duration_minutes = 1

            logger.info(f"Jira timer done: {issue_key} - {duration_minutes}min")

            if not self.jira:
                logger.error("Jira not configured, cannot log work")
                self.monitor.send_command("JIRA_LOG_ERROR:Jira not configured on Mac")
                return

            # Use speech-to-text to get work description
            description = self._get_work_description(issue_key, duration_minutes)

            if description is None:
                logger.info("User cancelled/skipped work description")
                self.monitor.send_command("JIRA_LOG_ERROR:Cancelled by user")
                return

            # Log work to Jira
            time_spent_seconds = duration_minutes * 60
            success = self.jira.log_work(issue_key, time_spent_seconds, description)

            if success:
                self.monitor.send_command("JIRA_LOG_OK")
                logger.info(f"Logged {duration_minutes}min to {issue_key}: {description}")
            else:
                self.monitor.send_command("JIRA_LOG_ERROR:Jira API error")
                logger.error(f"Failed to log work to {issue_key}")

        except Exception as e:
            logger.error(f"JIRA_TIMER_DONE handler error: {e} | raw={payload!r}")
            self.monitor.send_command("JIRA_LOG_ERROR:Error processing")

    def _request_user_input(self, prompt: str, timeout: float = 300, **kwargs) -> Optional[dict]:
        """Send input_request to menu bar app via IPC and block until response.

        Returns the response message dict, or None on timeout / no clients.
        """
        if not self._ipc.has_clients():
            logger.warning("No menu bar app connected, skipping user input")
            return None

        request_id = str(uuid.uuid4())
        event = threading.Event()

        with self._pending_lock:
            self._pending_inputs[request_id] = {"event": event, "data": None}

        self._ipc.push_event({
            "type": "input_request",
            "id": request_id,
            "prompt": prompt,
            **kwargs,
        })

        got_response = event.wait(timeout=timeout)

        with self._pending_lock:
            entry = self._pending_inputs.pop(request_id, {})

        if not got_response:
            logger.warning(f"Input request '{prompt}' timed out after {timeout}s")
            return None

        return entry.get("data")

    def _resolve_input(self, message: dict):
        """Called by IPC thread when an input_response arrives."""
        request_id = message.get("id")
        with self._pending_lock:
            entry = self._pending_inputs.get(request_id)
            if entry:
                entry["data"] = message
                entry["event"].set()

    def _get_work_description(self, issue_key: str, duration_minutes: int) -> Optional[str]:
        """Get work description via IPC to menu bar app (or default fallback)."""
        # Play notification sound
        subprocess.Popen(
            ["afplay", "/System/Library/Sounds/Glass.aiff"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        response = self._request_user_input(
            prompt="work_description",
            issue_key=issue_key,
            duration=duration_minutes,
            fields=[{
                "name": "description",
                "type": "text",
                "label": f"What did you work on?\n\n{issue_key} \u2014 {duration_minutes} min",
                "default": "Focus work",
            }],
        )

        if response is None:
            logger.warning("No response from menu bar app, using default")
            return "Focus work"
        if response.get("cancelled"):
            return None
        return response.get("data", {}).get("description", "Focus work")

    def handle_jira_log_time(self, payload: str):
        """Handle JIRA_LOG_TIME from device — manual time logging without timer.

        Payload format: ISSUE_KEY  (e.g. "DEMOCAI-59")
        Mac prompts for duration + description, then logs to Jira.
        """
        try:
            issue_key = payload.strip().replace("\x00", "")
            logger.info(f"JIRA_LOG_TIME: {issue_key}")

            if not self.jira:
                logger.error("Jira not configured, cannot log work")
                self.monitor.send_command("JIRA_LOG_ERROR:Jira not configured on Mac")
                return

            # Prompt for duration via AppleScript
            duration_minutes = self._get_duration(issue_key)
            if duration_minutes is None or duration_minutes <= 0:
                logger.info("User cancelled/skipped duration input")
                self.monitor.send_command("JIRA_LOG_ERROR:Cancelled by user")
                return

            # Prompt for description (reuse same flow as timer done)
            description = self._get_work_description(issue_key, duration_minutes)

            if description is None:
                logger.info("User cancelled/skipped work description")
                self.monitor.send_command("JIRA_LOG_ERROR:Cancelled by user")
                return

            # Log work to Jira
            time_spent_seconds = duration_minutes * 60
            success = self.jira.log_work(issue_key, time_spent_seconds, description)

            if success:
                self.monitor.send_command("JIRA_LOG_OK")
                logger.info(f"Manual log: {duration_minutes}min to {issue_key}: {description}")
            else:
                self.monitor.send_command("JIRA_LOG_ERROR:Jira API error")
                logger.error(f"Failed to log work to {issue_key}")

        except Exception as e:
            logger.error(f"JIRA_LOG_TIME handler error: {e} | raw={payload!r}")
            self.monitor.send_command("JIRA_LOG_ERROR:Error processing")

    def handle_jira_open(self, payload: str):
        """Handle JIRA_OPEN from device — open issue in browser."""
        try:
            import subprocess
            issue_key = payload.strip().replace("\x00", "")
            logger.info(f"Opening Jira issue in browser: {issue_key}")

            # Build URL from config
            jira_url, _, _ = self._load_jira_credentials()
            if jira_url:
                url = f"{jira_url.rstrip('/')}/browse/{issue_key}"
                subprocess.Popen(["open", url])
                logger.info(f"Opened: {url}")
            else:
                logger.error("No Jira URL configured")
        except Exception as e:
            logger.error(f"JIRA_OPEN error: {e}")

    @staticmethod
    def _parse_duration(text: str) -> Optional[int]:
        """Parse a duration string into minutes.

        Accepts:
            Plain number:  "30" → 30 min
            Hours:         "3h" → 180 min, "1.5h" → 90 min
            Minutes:       "45m" → 45 min
            Combined:      "1h30m" → 90 min, "2h 15m" → 135 min
            Decimal hours: "1.5" → 90 min (only if contains '.')
        """
        import re
        text = text.strip().lower().replace(" ", "")

        # Combined: 1h30m, 2h15m
        match = re.match(r'^(\d+)h(\d+)m?$', text)
        if match:
            return int(match.group(1)) * 60 + int(match.group(2))

        # Hours only: 3h, 1.5h
        match = re.match(r'^(\d+\.?\d*)h$', text)
        if match:
            return int(float(match.group(1)) * 60)

        # Minutes only: 45m
        match = re.match(r'^(\d+)m$', text)
        if match:
            return int(match.group(1))

        # Decimal (treat as hours): 1.5
        match = re.match(r'^(\d+\.\d+)$', text)
        if match:
            return int(float(match.group(1)) * 60)

        # Plain integer (minutes)
        match = re.match(r'^(\d+)$', text)
        if match:
            return int(match.group(1))

        return None

    def _get_duration(self, issue_key: str) -> Optional[int]:
        """Prompt user for duration via IPC to menu bar app."""
        # Play notification tone
        subprocess.Popen(
            ["afplay", "/System/Library/Sounds/Glass.aiff"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        response = self._request_user_input(
            prompt="duration",
            issue_key=issue_key,
            fields=[{
                "name": "duration",
                "type": "text",
                "label": f"How long to log?\n\n{issue_key}\n(e.g. 30, 1h, 1h30m, 1.5h)",
                "default": "30",
            }],
            timeout=120,
        )

        if response is None or response.get("cancelled"):
            return None

        duration_text = response.get("data", {}).get("duration", "")
        minutes = self._parse_duration(duration_text)
        if minutes is None or minutes <= 0:
            logger.error(f"Could not parse duration: {duration_text!r}")
            return None
        logger.info(f"Parsed duration: {duration_text!r} \u2192 {minutes} min")
        return minutes

    def handle_log(self, log_json: str):
        """Handle LOG response from device."""
        try:
            log_data = json.loads(log_json)
            logger.info(f"Received log: {log_data.get('date')} - {log_data.get('pomodoros')} pomodoros")

            if self.notion:
                if self.notion.add_log_entry(log_data):
                    self.monitor.send_ok()
                else:
                    logger.error("Failed to sync log to Notion")
            else:
                # No Notion, just acknowledge
                self.monitor.send_ok()

        except json.JSONDecodeError as e:
            logger.error(f"Invalid log JSON: {e}")

    def handle_note(self, note_json: str):
        """Handle NOTE response from device."""
        try:
            note_data = json.loads(note_json)
            logger.info(f"Received note: {note_data.get('text', '')[:50]}...")

            if self.notion:
                if self.notion.add_note(note_data):
                    self.monitor.send_ok()
                else:
                    logger.error("Failed to sync note to Notion")
            else:
                # No Notion, just acknowledge
                self.monitor.send_ok()

        except json.JSONDecodeError as e:
            logger.error(f"Invalid note JSON: {e}")

    def process_responses(self, responses: List[str]):
        """Process responses from device."""
        for response in responses:
            if response.startswith("LOG:"):
                self.handle_log(response[4:])
            elif response.startswith("NOTE:"):
                self.handle_note(response[5:])
            elif response.startswith("JIRA_TIMER_DONE:"):
                self.handle_jira_timer_done(response[16:])
            elif response.startswith("JIRA_LOG_TIME:"):
                self.handle_jira_log_time(response[14:])
            elif response.startswith("JIRA_OPEN:"):
                self.handle_jira_open(response[10:])
            elif response == "JIRA_PROJECTS_OK":
                logger.debug("Device acknowledged Jira projects")
            elif response == "WEATHER_OK":
                logger.debug("Device acknowledged weather data")
            elif response == "CALENDAR_OK":
                logger.debug("Device acknowledged calendar data")
            elif response == "JIRA_HOURS_OK":
                logger.debug("Device acknowledged Jira hours data")
            elif response.startswith("JIRA_LOG_MEETING:"):
                self.handle_jira_log_meeting(response[17:])
            elif response.startswith("ERROR:"):
                logger.error(f"Device error: {response[6:]}")
            elif response in ["PONG", "TIME_OK", "OK", "READY:FocusKnob"]:
                pass  # Expected responses
            elif response.startswith("USBSync:") or response.startswith("TimeLog:"):
                logger.debug(f"Device debug: {response}")
            else:
                logger.debug(f"Unknown response: {response}")

    def run(self):
        """Main service loop."""
        logger.info("FocusKnob Sync Service started")
        self.running = True
        last_ping = 0

        while self.running:
            try:
                if not self.monitor.connected:
                    # Look for device
                    port = self.monitor.find_device()
                    if port:
                        if self.monitor.connect(port):
                            # Notify menu bar app
                            self._ipc.push_event({
                                "type": "status_update",
                                "connected": True,
                                "port": self.monitor.port_name,
                            })
                            # Initial time sync
                            TimeSync.sync_time(self.monitor)
                            # Request logs
                            responses = self.monitor.send_command("GET_LOGS")
                            self.process_responses(responses)
                            # Sync Jira projects to device
                            self.sync_jira_issues()
                            # Sync weather to device
                            self.sync_weather()
                            self._last_weather_sync = time.time()
                            # Sync calendar to device
                            self.sync_calendar()
                            self._last_calendar_sync = time.time()
                            # Sync Jira hours to device
                            self.sync_jira_hours()
                            self._last_jira_hours_sync = time.time()
                            last_ping = time.time()
                    else:
                        time.sleep(POLL_INTERVAL)

                else:
                    # Connected - periodic ping and process responses
                    if time.time() - last_ping >= PING_INTERVAL:
                        responses = self.monitor.send_command("PING")
                        if not responses or "PONG" not in responses:
                            logger.warning("Lost connection to device")
                            self.monitor.disconnect()
                            self._ipc.push_event({
                                "type": "status_update",
                                "connected": False,
                                "port": None,
                            })
                        else:
                            self.process_responses(responses)
                            last_ping = time.time()

                    # Periodic weather refresh (every 15 minutes)
                    if self.weather and time.time() - self._last_weather_sync >= 900:
                        self.sync_weather()
                        self._last_weather_sync = time.time()

                    # Periodic calendar refresh (every 5 minutes)
                    if self.calendar and time.time() - self._last_calendar_sync >= 300:
                        self.sync_calendar()
                        self._last_calendar_sync = time.time()

                    # Periodic Jira hours refresh (every 5 minutes)
                    if self.jira and time.time() - self._last_jira_hours_sync >= 300:
                        self.sync_jira_hours()
                        self._last_jira_hours_sync = time.time()

                    # Check for unsolicited messages — drain all available lines
                    lines = []
                    while True:
                        line = self.monitor.read_line(0.1)
                        if line:
                            lines.append(line)
                        else:
                            break
                    if lines:
                        self.process_responses(lines)

                    time.sleep(0.05)

            except KeyboardInterrupt:
                logger.info("Shutting down...")
                self.running = False
            except Exception as e:
                logger.error(f"Error in main loop: {e}")
                if self.monitor.connected:
                    self.monitor.disconnect()
                time.sleep(1)

        self.monitor.disconnect()
        self._ipc.stop()
        logger.info("FocusKnob Sync Service stopped")

    # ── IPC handlers ─────────────────────────────────────────────────

    def _handle_ipc_message(self, message: dict) -> Optional[dict]:
        """Dispatch incoming IPC messages from the menu bar app."""
        msg_type = message.get("type")
        msg_id = message.get("id")

        if msg_type == "status":
            return {
                "type": "status_result", "id": msg_id,
                "connected": self.monitor.connected,
                "port": self.monitor.port_name,
                "jira_enabled": self.jira is not None,
                "notion_enabled": self.notion is not None,
                "weather_enabled": self.weather is not None,
                "calendar_enabled": self.calendar is not None,
                "uptime_seconds": int(time.time() - self._start_time),
            }

        elif msg_type == "reload_config":
            success, msg = self._reload_config()
            return {"type": "reload_config_result", "id": msg_id,
                    "success": success, "message": msg}

        elif msg_type == "restart":
            threading.Thread(target=self._do_restart, daemon=True).start()
            return {"type": "restart_result", "id": msg_id, "success": True}

        elif msg_type == "logs":
            lines = message.get("lines", 50)
            content = self._read_log_tail(lines)
            return {"type": "logs_result", "id": msg_id, "content": content}

        elif msg_type == "input_response":
            self._resolve_input(message)
            return None

        else:
            return {"type": "error", "id": msg_id,
                    "message": f"Unknown command: {msg_type}"}

    def _reload_config(self) -> tuple:
        """Re-read config.json and rebuild API clients without restarting."""
        try:
            with self._config_lock:
                notion_token, notion_db = self._load_credentials()
                if notion_token and notion_db:
                    self.notion = NotionClient(notion_token, notion_db)
                else:
                    self.notion = None

                jira_url, jira_email, jira_token = self._load_jira_credentials()
                if jira_url and jira_email and jira_token:
                    self.jira = JiraClient(jira_url, jira_email, jira_token)
                else:
                    self.jira = None

                weather_key, weather_loc = self._load_weather_credentials()
                if weather_key and weather_loc:
                    self.weather = WeatherClient(weather_key, weather_loc)
                else:
                    self.weather = None

                calendar_tenant, calendar_client = self._load_calendar_credentials()
                if calendar_tenant and calendar_client:
                    cache_dir = str(self.CONFIG_FILE.parent)
                    self.calendar = CalendarClient(calendar_tenant, calendar_client, cache_dir)
                else:
                    self.calendar = None

            parts = []
            if self.notion:
                parts.append("Notion enabled")
            if self.jira:
                parts.append("Jira enabled")
            if self.weather:
                parts.append("Weather enabled")
            if self.calendar:
                parts.append("Calendar enabled")
            msg = "Config reloaded: " + (", ".join(parts) or "No integrations configured")
            logger.info(msg)
            return True, msg

        except Exception as e:
            logger.error(f"Config reload failed: {e}")
            return False, str(e)

    def _read_log_tail(self, lines: int = 50) -> str:
        """Read the last N lines from the sync log file."""
        try:
            log_path = LOG_DIR / "sync.log"
            if not log_path.exists():
                return ""
            with open(log_path, "r") as f:
                all_lines = f.readlines()
                return "".join(all_lines[-lines:])
        except Exception as e:
            return f"Error reading logs: {e}"

    def _do_restart(self):
        """Exit so launchd KeepAlive restarts us."""
        logger.info("Restart requested via IPC, exiting...")
        time.sleep(0.5)
        self.running = False
        sys.exit(1)

    def stop(self):
        """Stop the service."""
        self.running = False
        if hasattr(self, '_ipc'):
            self._ipc.stop()


def main():
    """Entry point."""
    logger.info("=" * 50)
    logger.info("FocusKnob Sync Service")
    logger.info("=" * 50)

    sync = FocusKnobSync()

    try:
        sync.run()
    except KeyboardInterrupt:
        sync.stop()

    return 0


if __name__ == "__main__":
    sys.exit(main())
