#!/usr/bin/env python3
"""
Weather Client for FocusKnob

Fetches current weather and forecast from OpenWeatherMap API.
Data is sent to the ESP32 device for display on the home screen
and in the dedicated Weather app screen.
"""

import logging
from datetime import datetime
from typing import Optional

import requests

logger = logging.getLogger(__name__)


class WeatherClient:
    """Handles OpenWeatherMap API interactions."""

    BASE_URL = "https://api.openweathermap.org/data/2.5"

    def __init__(self, api_key: str, location: str):
        self.api_key = api_key
        self.location = location

    def test_connection(self) -> tuple:
        """Test API key and location by fetching current weather."""
        try:
            resp = requests.get(
                f"{self.BASE_URL}/weather",
                params={
                    "q": self.location,
                    "appid": self.api_key,
                    "units": "imperial",
                },
                timeout=10,
            )
            if resp.status_code == 200:
                data = resp.json()
                city = data.get("name", self.location)
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

    def get_current_and_forecast(self) -> Optional[dict]:
        """Fetch current weather + 24-hour forecast.

        Returns combined dict ready to serialize and send to device:
        {
            "current": {temp, temp_min, temp_max, humidity, wind_speed,
                        condition_id, condition, description},
            "forecast": [{temp, condition_id, hour_str, description}, ...]
        }
        Returns None on failure.
        """
        current = self._fetch_current()
        if current is None:
            return None

        forecast = self._fetch_forecast()

        result = {"current": current}
        if forecast:
            result["forecast"] = forecast

        return result

    def _fetch_current(self) -> Optional[dict]:
        """Fetch current weather conditions."""
        try:
            resp = requests.get(
                f"{self.BASE_URL}/weather",
                params={
                    "q": self.location,
                    "appid": self.api_key,
                    "units": "imperial",
                },
                timeout=10,
            )
            if resp.status_code != 200:
                logger.error(f"Weather API error: {resp.status_code}")
                return None

            data = resp.json()
            main = data["main"]
            wind = data.get("wind", {})
            weather = data.get("weather", [{}])[0]

            condition = weather.get("main", "Unknown")
            description = weather.get("description", "")

            # Truncate to fit device buffers
            if len(condition) > 31:
                condition = condition[:28] + "..."
            if len(description) > 47:
                description = description[:44] + "..."

            return {
                "temp": round(main["temp"]),
                "temp_min": round(main.get("temp_min", main["temp"])),
                "temp_max": round(main.get("temp_max", main["temp"])),
                "humidity": main.get("humidity", 0),
                "wind_speed": round(wind.get("speed", 0)),
                "condition_id": weather.get("id", 800),
                "condition": condition,
                "description": description,
            }
        except Exception as e:
            logger.error(f"Failed to fetch current weather: {e}")
            return None

    def _fetch_forecast(self) -> Optional[list]:
        """Fetch next 24 hours of forecast (8 x 3-hour intervals)."""
        try:
            resp = requests.get(
                f"{self.BASE_URL}/forecast",
                params={
                    "q": self.location,
                    "appid": self.api_key,
                    "units": "imperial",
                    "cnt": 8,
                },
                timeout=10,
            )
            if resp.status_code != 200:
                logger.warning(f"Forecast API error: {resp.status_code}")
                return None

            data = resp.json()
            entries = data.get("list", [])
            result = []

            for entry in entries:
                dt = entry.get("dt", 0)
                main = entry.get("main", {})
                weather = entry.get("weather", [{}])[0]

                # Pre-compute local hour string to avoid timezone on device
                dt_local = datetime.fromtimestamp(dt)
                hour = dt_local.hour
                if hour == 0:
                    hour_str = "12am"
                elif hour < 12:
                    hour_str = f"{hour}am"
                elif hour == 12:
                    hour_str = "12pm"
                else:
                    hour_str = f"{hour - 12}pm"

                description = weather.get("description", "")
                if len(description) > 47:
                    description = description[:44] + "..."

                result.append({
                    "temp": round(main.get("temp", 0)),
                    "condition_id": weather.get("id", 800),
                    "hour_str": hour_str,
                    "description": description,
                })

            logger.info(f"Fetched {len(result)} forecast entries")
            return result

        except Exception as e:
            logger.error(f"Failed to fetch forecast: {e}")
            return None
