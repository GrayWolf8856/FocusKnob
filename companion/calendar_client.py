#!/usr/bin/env python3
"""
Calendar Client for FocusKnob

Fetches upcoming Outlook calendar events via Microsoft Graph API
using MSAL device code flow authentication.
Data is sent to the ESP32 device for display on the home screen
and in the dedicated Calendar app screen.
"""

import json
import logging
import os
from datetime import datetime, timedelta
from typing import Optional

import msal
import requests

logger = logging.getLogger(__name__)


class CalendarClient:
    """Handles Microsoft Graph calendar interactions."""

    GRAPH_URL = "https://graph.microsoft.com/v1.0"
    SCOPES = ["Calendars.Read"]

    def __init__(self, tenant_id: str, client_id: str, cache_dir: str):
        self.tenant_id = tenant_id
        self.client_id = client_id
        self.cache_dir = cache_dir
        self._cache_file = os.path.join(cache_dir, "msal_token_cache.json")
        self._token_cache = self._load_cache()
        self._app = msal.PublicClientApplication(
            client_id,
            authority=f"https://login.microsoftonline.com/{tenant_id}",
            token_cache=self._token_cache,
        )

    def _load_cache(self) -> msal.SerializableTokenCache:
        """Load MSAL token cache from disk."""
        cache = msal.SerializableTokenCache()
        if os.path.exists(self._cache_file):
            try:
                with open(self._cache_file, "r") as f:
                    cache.deserialize(f.read())
                logger.debug("Loaded MSAL token cache from disk")
            except Exception as e:
                logger.warning(f"Failed to load token cache: {e}")
        return cache

    def _save_cache(self):
        """Save MSAL token cache to disk."""
        if self._token_cache.has_state_changed:
            try:
                with open(self._cache_file, "w") as f:
                    f.write(self._token_cache.serialize())
                logger.debug("Saved MSAL token cache to disk")
            except Exception as e:
                logger.warning(f"Failed to save token cache: {e}")

    def _acquire_token(self) -> Optional[str]:
        """Acquire access token, using cache or device code flow."""
        # Try silent first (cached/refreshed token)
        accounts = self._app.get_accounts()
        if accounts:
            result = self._app.acquire_token_silent(
                self.SCOPES, account=accounts[0]
            )
            if result and "access_token" in result:
                self._save_cache()
                return result["access_token"]

        # No cached token — initiate device code flow
        logger.info("No cached token, initiating device code flow...")
        flow = self._app.initiate_device_flow(scopes=self.SCOPES)
        if "user_code" not in flow:
            logger.error(f"Device code flow failed: {flow.get('error_description', 'unknown error')}")
            return None

        # Log the auth URL and code for the user
        message = flow.get("message", "")
        logger.info(f"AUTH REQUIRED: {message}")
        print(f"\n{'='*60}")
        print(f"  FocusKnob Calendar Authentication Required")
        print(f"{'='*60}")
        print(f"  {message}")
        print(f"{'='*60}\n")

        # Open the browser for the user
        try:
            import subprocess
            url = flow.get("verification_uri", "https://microsoft.com/devicelogin")
            subprocess.Popen(["open", url])
        except Exception:
            pass

        # Block until user completes auth (MSAL polls automatically)
        result = self._app.acquire_token_by_device_flow(flow)
        if result and "access_token" in result:
            self._save_cache()
            logger.info("Device code auth completed successfully")
            return result["access_token"]

        error = result.get("error_description", result.get("error", "unknown"))
        logger.error(f"Device code auth failed: {error}")
        return None

    def test_connection(self) -> tuple:
        """Test credentials by fetching user profile."""
        try:
            token = self._acquire_token()
            if not token:
                return False, "Authentication failed — check Tenant/Client ID"

            resp = requests.get(
                f"{self.GRAPH_URL}/me",
                headers={"Authorization": f"Bearer {token}"},
                timeout=10,
            )
            if resp.status_code == 200:
                data = resp.json()
                name = data.get("displayName", "Unknown User")
                return True, f"Connected: {name}"
            else:
                return False, f"Graph API error: {resp.status_code}"
        except Exception as e:
            return False, str(e)

    def get_upcoming_events(self) -> Optional[dict]:
        """Fetch today's remaining calendar events.

        Returns dict ready to serialize and send to device:
        {
            "events": [{title, start_str, start_time, end_time,
                        duration_min, is_all_day, location}, ...],
            "next_meeting_min": 15  (-1 = in progress, -2 = none)
        }
        Returns None on failure.
        """
        try:
            token = self._acquire_token()
            if not token:
                logger.error("Failed to acquire token for calendar sync")
                return None

            now = datetime.now()
            # Get events from now until end of day
            end_of_day = now.replace(hour=23, minute=59, second=59)

            # Format as ISO 8601 for Graph API
            start_dt = now.strftime("%Y-%m-%dT%H:%M:%S")
            end_dt = end_of_day.strftime("%Y-%m-%dT%H:%M:%S")

            resp = requests.get(
                f"{self.GRAPH_URL}/me/calendarView",
                headers={
                    "Authorization": f"Bearer {token}",
                    "Prefer": 'outlook.timezone="Central Standard Time"',
                },
                params={
                    "startDateTime": start_dt,
                    "endDateTime": end_dt,
                    "$select": "subject,start,end,isAllDay,location",
                    "$orderby": "start/dateTime asc",
                    "$top": 10,
                },
                timeout=15,
            )

            if resp.status_code != 200:
                logger.error(f"Calendar API error: {resp.status_code}")
                return None

            data = resp.json()
            raw_events = data.get("value", [])
            events = []
            next_meeting_min = -2  # -2 = no meetings

            for ev in raw_events:
                subject = ev.get("subject", "No Title")
                is_all_day = ev.get("isAllDay", False)
                location = ev.get("location", {}).get("displayName", "")

                start_obj = ev.get("start", {})
                end_obj = ev.get("end", {})
                start_dt_str = start_obj.get("dateTime", "")
                end_dt_str = end_obj.get("dateTime", "")

                # Parse times
                try:
                    start_parsed = datetime.fromisoformat(start_dt_str.rstrip("Z"))
                    end_parsed = datetime.fromisoformat(end_dt_str.rstrip("Z"))
                except (ValueError, AttributeError):
                    continue

                # Compute duration
                duration = end_parsed - start_parsed
                duration_min = max(1, int(duration.total_seconds() / 60))

                # Pre-format start time for display (12h format)
                hour = start_parsed.hour
                minute = start_parsed.minute
                if hour == 0:
                    start_str = f"12:{minute:02d}a"
                elif hour < 12:
                    start_str = f"{hour}:{minute:02d}a"
                elif hour == 12:
                    start_str = f"12:{minute:02d}p"
                else:
                    start_str = f"{hour-12}:{minute:02d}p"

                # 24h format for potential device-side math
                start_time_24 = start_parsed.strftime("%H:%M")
                end_time_24 = end_parsed.strftime("%H:%M")

                # Truncate strings to fit device buffers
                if len(subject) > 31:
                    subject = subject[:28] + "..."
                if len(location) > 31:
                    location = location[:28] + "..."

                # Compute minutes until this meeting
                diff = (start_parsed - now).total_seconds() / 60
                if diff <= 0 and (end_parsed - now).total_seconds() > 0:
                    # Meeting is currently in progress
                    if next_meeting_min == -2:
                        next_meeting_min = -1
                elif diff > 0:
                    mins = int(diff)
                    if next_meeting_min == -2 or (next_meeting_min == -1) or (next_meeting_min >= 0 and mins < next_meeting_min):
                        if next_meeting_min != -1:
                            next_meeting_min = mins

                events.append({
                    "title": subject,
                    "start_str": start_str,
                    "start_time": start_time_24,
                    "end_time": end_time_24,
                    "duration_min": duration_min,
                    "is_all_day": is_all_day,
                    "location": location,
                })

            logger.info(f"Fetched {len(events)} calendar events, next in {next_meeting_min} min")
            return {
                "events": events,
                "next_meeting_min": next_meeting_min,
            }

        except Exception as e:
            logger.error(f"Failed to fetch calendar events: {e}")
            return None
