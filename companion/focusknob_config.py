#!/usr/bin/env python3
"""
FocusKnob Configuration App  [DEPRECATED]

Superseded by focusknob_menu.py (macOS menu bar app with native settings UI).
This file is kept for reference only.

Original purpose: A web-based GUI for configuring Notion integration credentials.
Opens in your default browser.
"""

import os
import sys
import json
import subprocess
import webbrowser
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs, urlparse
from pathlib import Path

import requests

# Configuration
PORT = 8765
CONFIG_DIR = Path.home() / "Library" / "Application Support" / "FocusKnob"
CONFIG_FILE = CONFIG_DIR / "config.json"
PLIST_PATH = Path.home() / "Library" / "LaunchAgents" / "com.focusknob.sync.plist"


def load_config():
    """Load config from file."""
    defaults = {
        "notion_token": "",
        "notion_database_id": "",
        "jira_base_url": "",
        "jira_email": "",
        "jira_api_token": "",
    }
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE) as f:
                saved = json.load(f)
                defaults.update(saved)
        except:
            pass
    return defaults


def save_config(config_data: dict):
    """Save config to file, merging with existing."""
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    existing = load_config()
    existing.update(config_data)
    with open(CONFIG_FILE, "w") as f:
        json.dump(existing, f, indent=2)


def update_launchagent(token, database_id):
    """Update LaunchAgent plist with new credentials."""
    if not PLIST_PATH.exists():
        return False, "LaunchAgent not installed"

    try:
        result = subprocess.run(
            ["plutil", "-convert", "json", "-o", "-", str(PLIST_PATH)],
            capture_output=True, text=True
        )
        plist_data = json.loads(result.stdout)

        if "EnvironmentVariables" not in plist_data:
            plist_data["EnvironmentVariables"] = {}

        plist_data["EnvironmentVariables"]["NOTION_TOKEN"] = token
        plist_data["EnvironmentVariables"]["NOTION_DATABASE_ID"] = database_id

        temp_json = CONFIG_DIR / "temp_plist.json"
        with open(temp_json, "w") as f:
            json.dump(plist_data, f)

        subprocess.run(
            ["plutil", "-convert", "xml1", str(temp_json), "-o", str(PLIST_PATH)],
            check=True
        )
        temp_json.unlink()
        return True, "LaunchAgent updated"
    except Exception as e:
        return False, str(e)


def restart_service():
    """Restart the sync service."""
    try:
        subprocess.run(["launchctl", "unload", str(PLIST_PATH)], capture_output=True)
        subprocess.run(["launchctl", "load", str(PLIST_PATH)], capture_output=True, check=True)
        return True, "Service restarted"
    except Exception as e:
        return False, str(e)


def get_service_status():
    """Get sync service status."""
    try:
        result = subprocess.run(
            ["launchctl", "list", "com.focusknob.sync"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            parts = result.stdout.strip().split("\t")
            if len(parts) >= 1 and parts[0] != "-":
                return f"Running (PID {parts[0]})"
            return "Not running"
        return "Not installed"
    except:
        return "Unknown"


def test_notion(token, database_id):
    """Test Notion connection."""
    if not token or not database_id:
        return False, "Please enter both token and database ID"

    headers = {
        "Authorization": f"Bearer {token}",
        "Notion-Version": "2022-06-28"
    }

    try:
        response = requests.get(
            f"https://api.notion.com/v1/databases/{database_id}",
            headers=headers,
            timeout=10
        )

        if response.status_code == 200:
            data = response.json()
            title = "Untitled"
            if data.get("title"):
                title = data["title"][0].get("plain_text", "Untitled")
            return True, f"Connected to: {title}"
        elif response.status_code == 401:
            return False, "Invalid API token"
        elif response.status_code == 404:
            return False, "Database not found"
        else:
            return False, f"API error: {response.status_code}"
    except Exception as e:
        return False, str(e)


def test_jira(base_url, email, api_token):
    """Test Jira connection."""
    if not base_url or not email or not api_token:
        return False, "Please enter all Jira fields"

    base_url = base_url.rstrip("/")
    try:
        response = requests.get(
            f"{base_url}/rest/api/3/myself",
            headers={"Accept": "application/json"},
            auth=(email, api_token),
            timeout=10,
        )
        if response.status_code == 200:
            user = response.json()
            name = user.get("displayName", "Unknown")
            return True, f"Connected as {name}"
        elif response.status_code == 401:
            return False, "Invalid credentials"
        elif response.status_code == 403:
            return False, "Access forbidden — check API token permissions"
        else:
            return False, f"API error: {response.status_code}"
    except requests.exceptions.ConnectionError:
        return False, "Could not connect — check the base URL"
    except Exception as e:
        return False, str(e)


HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FocusKnob Setup</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            padding: 40px;
            width: 100%;
            max-width: 480px;
        }
        h1 {
            font-size: 28px;
            color: #1a1a2e;
            margin-bottom: 8px;
        }
        .subtitle {
            color: #666;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            font-weight: 600;
            color: #333;
            margin-bottom: 8px;
            font-size: 14px;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px 16px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 14px;
            transition: border-color 0.2s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        .hint {
            font-size: 12px;
            color: #888;
            margin-top: 6px;
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-top: 8px;
        }
        .checkbox-group input { width: auto; }
        .checkbox-group label { margin: 0; font-weight: normal; }
        .buttons {
            display: flex;
            gap: 12px;
            margin-top: 30px;
        }
        button {
            flex: 1;
            padding: 14px 20px;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.1s, box-shadow 0.2s;
        }
        button:active { transform: scale(0.98); }
        .btn-secondary {
            background: #f0f0f0;
            color: #333;
        }
        .btn-secondary:hover { background: #e0e0e0; }
        .btn-primary {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .btn-primary:hover { box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4); }
        .status-box {
            margin-top: 20px;
            padding: 12px 16px;
            border-radius: 8px;
            font-size: 14px;
            display: none;
        }
        .status-success {
            background: #d4edda;
            color: #155724;
            display: block;
        }
        .status-error {
            background: #f8d7da;
            color: #721c24;
            display: block;
        }
        .status-info {
            background: #e7f1ff;
            color: #004085;
            display: block;
        }
        .divider {
            border-top: 1px solid #eee;
            margin: 30px 0 20px;
        }
        .service-status {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 14px;
            color: #666;
        }
        .service-status .status {
            font-weight: 600;
            color: #28a745;
        }
        .service-status .status.offline { color: #dc3545; }
    </style>
</head>
<body>
    <div class="container">
        <h1>FocusKnob Setup</h1>
        <p class="subtitle">Configure your FocusKnob integrations</p>

        <form id="configForm">
            <h2 style="font-size:18px; color:#1a1a2e; margin-bottom:15px;">Notion</h2>

            <div class="form-group">
                <label for="token">Notion API Token</label>
                <input type="password" id="token" name="token" value="{token}" placeholder="secret_...">
                <div class="checkbox-group">
                    <input type="checkbox" id="showToken" onchange="toggleField('token', 'showToken')">
                    <label for="showToken">Show token</label>
                </div>
            </div>

            <div class="form-group">
                <label for="database_id">Notion Database ID</label>
                <input type="text" id="database_id" name="database_id" value="{database_id}" placeholder="abc123...">
                <p class="hint">Find this in your database URL: notion.so/<strong>[database-id]</strong>?v=...</p>
            </div>

            <div id="notionStatus" class="status-box"></div>

            <div class="buttons">
                <button type="button" class="btn-secondary" onclick="testNotion()">Test Notion</button>
            </div>

            <div class="divider"></div>

            <h2 style="font-size:18px; color:#1a1a2e; margin-bottom:15px;">Jira</h2>

            <div class="form-group">
                <label for="jira_url">Jira Base URL</label>
                <input type="text" id="jira_url" name="jira_url" value="{jira_base_url}" placeholder="https://yourcompany.atlassian.net">
                <p class="hint">Your Atlassian Cloud URL (no trailing slash)</p>
            </div>

            <div class="form-group">
                <label for="jira_email">Jira Email</label>
                <input type="text" id="jira_email" name="jira_email" value="{jira_email}" placeholder="you@company.com">
            </div>

            <div class="form-group">
                <label for="jira_token">Jira API Token</label>
                <input type="password" id="jira_token" name="jira_token" value="{jira_api_token}" placeholder="ATATT3x...">
                <div class="checkbox-group">
                    <input type="checkbox" id="showJiraToken" onchange="toggleField('jira_token', 'showJiraToken')">
                    <label for="showJiraToken">Show token</label>
                </div>
                <p class="hint">Create at <a href="https://id.atlassian.com/manage-profile/security/api-tokens" target="_blank" style="color:#667eea;">id.atlassian.com/manage-profile/security/api-tokens</a></p>
            </div>

            <div id="jiraStatus" class="status-box"></div>

            <div class="buttons">
                <button type="button" class="btn-secondary" onclick="testJira()">Test Jira</button>
            </div>

            <div class="divider"></div>

            <div id="statusBox" class="status-box"></div>

            <div class="buttons">
                <button type="submit" class="btn-primary" style="flex:1;">Save All & Restart</button>
            </div>
        </form>

        <div class="divider"></div>

        <div class="service-status">
            <span>Sync Service:</span>
            <span class="status {status_class}" id="serviceStatus">{service_status}</span>
        </div>
    </div>

    <script>
        function toggleField(fieldId, checkboxId) {
            const input = document.getElementById(fieldId);
            const checkbox = document.getElementById(checkboxId);
            input.type = checkbox.checked ? 'text' : 'password';
        }

        function showStatus(boxId, message, type) {
            const box = document.getElementById(boxId);
            box.textContent = message;
            box.className = 'status-box status-' + type;
        }

        async function testNotion() {
            const token = document.getElementById('token').value;
            const dbId = document.getElementById('database_id').value;

            showStatus('notionStatus', 'Testing Notion connection...', 'info');

            try {
                const response = await fetch('/test_notion', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `token=${encodeURIComponent(token)}&database_id=${encodeURIComponent(dbId)}`
                });
                const data = await response.json();
                showStatus('notionStatus', data.message, data.success ? 'success' : 'error');
            } catch (e) {
                showStatus('notionStatus', 'Connection failed: ' + e.message, 'error');
            }
        }

        async function testJira() {
            const url = document.getElementById('jira_url').value;
            const email = document.getElementById('jira_email').value;
            const token = document.getElementById('jira_token').value;

            showStatus('jiraStatus', 'Testing Jira connection...', 'info');

            try {
                const response = await fetch('/test_jira', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: `jira_url=${encodeURIComponent(url)}&jira_email=${encodeURIComponent(email)}&jira_token=${encodeURIComponent(token)}`
                });
                const data = await response.json();
                showStatus('jiraStatus', data.message, data.success ? 'success' : 'error');
            } catch (e) {
                showStatus('jiraStatus', 'Connection failed: ' + e.message, 'error');
            }
        }

        document.getElementById('configForm').addEventListener('submit', async (e) => {
            e.preventDefault();

            const body = new URLSearchParams({
                token: document.getElementById('token').value,
                database_id: document.getElementById('database_id').value,
                jira_url: document.getElementById('jira_url').value,
                jira_email: document.getElementById('jira_email').value,
                jira_token: document.getElementById('jira_token').value,
            });

            showStatus('statusBox', 'Saving configuration...', 'info');

            try {
                const response = await fetch('/save', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: body.toString()
                });
                const data = await response.json();
                showStatus('statusBox', data.message, data.success ? 'success' : 'error');
                if (data.success) {
                    document.getElementById('serviceStatus').textContent = 'Running';
                    document.getElementById('serviceStatus').classList.remove('offline');
                }
            } catch (e) {
                showStatus('statusBox', 'Save failed: ' + e.message, 'error');
            }
        });

        setInterval(async () => {
            try {
                const response = await fetch('/status');
                const data = await response.json();
                const el = document.getElementById('serviceStatus');
                el.textContent = data.status;
                if (data.status.includes('Running')) {
                    el.classList.remove('offline');
                } else {
                    el.classList.add('offline');
                }
            } catch (e) {}
        }, 5000);
    </script>
</body>
</html>
"""


class ConfigHandler(BaseHTTPRequestHandler):
    """HTTP request handler for config app."""

    def log_message(self, format, *args):
        pass  # Suppress logging

    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            config = load_config()
            token = config.get("notion_token", "")
            db_id = config.get("notion_database_id", "")
            jira_url = config.get("jira_base_url", "")
            jira_email = config.get("jira_email", "")
            jira_token = config.get("jira_api_token", "")

            # Mask tokens for display
            display_token = token if not token or token.startswith("YOUR_") else token[:10] + "..." + token[-4:]
            display_jira_token = jira_token if not jira_token or jira_token.startswith("YOUR_") else jira_token[:8] + "..." + jira_token[-4:]

            service_status = get_service_status()
            status_class = "" if "Running" in service_status else "offline"

            html = HTML_TEMPLATE.format(
                token=display_token,
                database_id=db_id,
                jira_base_url=jira_url,
                jira_email=jira_email,
                jira_api_token=display_jira_token,
                service_status=service_status,
                status_class=status_class
            )

            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(html.encode())

        elif self.path == "/status":
            self.send_json({"status": get_service_status()})

        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length).decode()
        params = parse_qs(post_data)

        if self.path == "/test_notion" or self.path == "/test":
            token = params.get("token", [""])[0]
            db_id = params.get("database_id", [""])[0]
            success, message = test_notion(token, db_id)
            self.send_json({"success": success, "message": message})

        elif self.path == "/test_jira":
            jira_url = params.get("jira_url", [""])[0]
            jira_email = params.get("jira_email", [""])[0]
            jira_token = params.get("jira_token", [""])[0]
            success, message = test_jira(jira_url, jira_email, jira_token)
            self.send_json({"success": success, "message": message})

        elif self.path == "/save":
            token = params.get("token", [""])[0]
            db_id = params.get("database_id", [""])[0]
            jira_url = params.get("jira_url", [""])[0]
            jira_email = params.get("jira_email", [""])[0]
            jira_token = params.get("jira_token", [""])[0]

            # Save all config
            save_config({
                "notion_token": token,
                "notion_database_id": db_id,
                "jira_base_url": jira_url,
                "jira_email": jira_email,
                "jira_api_token": jira_token,
            })

            # Update LaunchAgent with Notion env vars
            if token and db_id:
                success, message = update_launchagent(token, db_id)
                if not success:
                    self.send_json({"success": False, "message": f"Config saved but LaunchAgent update failed: {message}"})
                    return

            # Restart service
            success, message = restart_service()
            if success:
                self.send_json({"success": True, "message": "Configuration saved and service restarted!"})
            else:
                self.send_json({"success": False, "message": f"Config saved but restart failed: {message}"})

        else:
            self.send_response(404)
            self.end_headers()


def main():
    """Run the configuration server."""
    server = HTTPServer(("127.0.0.1", PORT), ConfigHandler)
    print(f"FocusKnob Configuration")
    print(f"Opening browser to http://127.0.0.1:{PORT}")
    print("Press Ctrl+C to quit")

    # Open browser
    webbrowser.open(f"http://127.0.0.1:{PORT}")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
