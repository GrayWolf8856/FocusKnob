#!/usr/bin/env python3
"""
Jira Client for FocusKnob

Handles:
- Fetching assigned Jira issues (open/in-progress)
- Logging work time to Jira issues
- Work description dialog (tkinter)
"""

import datetime
import json
import logging
import threading
from typing import Optional

import requests

logger = logging.getLogger(__name__)


class JiraClient:
    """Handles Jira REST API interactions."""

    def __init__(self, base_url: str, email: str, api_token: str):
        self.base_url = base_url.rstrip("/")
        self.email = email
        self.api_token = api_token
        self.headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
        }
        self.auth = (email, api_token)
        self._account_id = None  # cached from /myself

    def test_connection(self) -> tuple[bool, str]:
        """Test Jira API credentials."""
        try:
            response = requests.get(
                f"{self.base_url}/rest/api/3/myself",
                headers=self.headers,
                auth=self.auth,
                timeout=10,
            )
            if response.status_code == 200:
                user = response.json()
                name = user.get("displayName", "Unknown")
                return True, f"Connected as {name}"
            else:
                return False, f"HTTP {response.status_code}: {response.text[:100]}"
        except Exception as e:
            return False, str(e)

    def _get_account_id(self) -> Optional[str]:
        """Get current user's account ID (cached after first call)."""
        if self._account_id:
            return self._account_id
        try:
            response = requests.get(
                f"{self.base_url}/rest/api/3/myself",
                headers=self.headers,
                auth=self.auth,
                timeout=10,
            )
            if response.status_code == 200:
                self._account_id = response.json().get("accountId")
                return self._account_id
        except Exception as e:
            logger.error(f"Failed to get account ID: {e}")
        return None

    def get_today_worklogs(self) -> Optional[dict]:
        """Get total hours logged to Jira today.

        Returns {"logged_min": N, "target_min": 480/0} or None on failure.
        """
        try:
            account_id = self._get_account_id()
            if not account_id:
                logger.error("Cannot fetch worklogs: no account ID")
                return None

            today = datetime.date.today()
            today_str = today.isoformat()
            is_weekday = today.weekday() < 5

            jql = (
                f'worklogDate = "{today_str}" '
                f"AND worklogAuthor = currentUser()"
            )
            params = {
                "jql": jql,
                "maxResults": 50,
                "fields": "worklog",
            }
            response = requests.get(
                f"{self.base_url}/rest/api/3/search/jql",
                headers=self.headers,
                auth=self.auth,
                params=params,
                timeout=15,
            )
            if response.status_code != 200:
                logger.error(f"Jira worklog search error: {response.status_code}")
                return None

            issues = response.json().get("issues", [])
            total_seconds = 0

            for issue in issues:
                fields = issue.get("fields", {})
                worklog_data = fields.get("worklog", {})
                worklogs = worklog_data.get("worklogs", [])
                wl_total = worklog_data.get("total", 0)
                wl_max = worklog_data.get("maxResults", 20)

                # If there are more worklogs than returned inline, fetch all
                if wl_total > wl_max:
                    issue_key = issue["key"]
                    try:
                        wl_resp = requests.get(
                            f"{self.base_url}/rest/api/3/issue/{issue_key}/worklog",
                            headers=self.headers,
                            auth=self.auth,
                            timeout=10,
                        )
                        if wl_resp.status_code == 200:
                            worklogs = wl_resp.json().get("worklogs", [])
                    except Exception as e:
                        logger.warning(f"Failed to fetch full worklogs for {issue_key}: {e}")

                for wl in worklogs:
                    # Filter by author
                    author_id = wl.get("author", {}).get("accountId", "")
                    if author_id != account_id:
                        continue
                    # Filter by today's date
                    started = wl.get("started", "")
                    if started[:10] != today_str:
                        continue
                    total_seconds += wl.get("timeSpentSeconds", 0)

            logged_min = total_seconds // 60
            target_min = 480 if is_weekday else 0

            logger.info(f"Today's Jira worklogs: {logged_min} min logged, target {target_min} min")
            return {"logged_min": logged_min, "target_min": target_min}

        except Exception as e:
            logger.error(f"Failed to fetch today's worklogs: {e}")
            return None

    @staticmethod
    def _adf_to_plain_text(adf: dict) -> str:
        """Extract plain text from Atlassian Document Format (ADF) JSON."""
        if not adf or not isinstance(adf, dict):
            return ""
        parts = []

        def walk(node):
            if isinstance(node, dict):
                if node.get("type") == "text":
                    parts.append(node.get("text", ""))
                elif node.get("type") in ("hardBreak",):
                    parts.append("\n")
                for child in node.get("content", []):
                    walk(child)
                # Add newline after block-level elements
                if node.get("type") in ("paragraph", "heading", "bulletList",
                                         "orderedList", "listItem"):
                    parts.append("\n")
            elif isinstance(node, list):
                for child in node:
                    walk(child)

        walk(adf)
        # Collapse multiple newlines and strip
        text = "".join(parts).strip()
        while "\n\n\n" in text:
            text = text.replace("\n\n\n", "\n\n")
        return text

    def get_my_issues(self) -> list[dict]:
        """Fetch open/in-progress issues assigned to current user.

        Returns [{key, name, proj, status, desc}, ...] where:
          key    = issue key (e.g. "DEMOCAI-44")
          name   = issue summary, truncated to fit device buffer
          proj   = project name (e.g. "Democratized AI")
          status = status name (e.g. "In Progress")
          desc   = first ~127 chars of description plain text
        """
        try:
            jql = (
                "assignee = currentUser() "
                'AND statusCategory in ("To Do", "In Progress") '
                "ORDER BY updated DESC"
            )
            params = {
                "jql": jql,
                "maxResults": 20,
                "fields": "summary,project,status,description",
            }
            response = requests.get(
                f"{self.base_url}/rest/api/3/search/jql",
                headers=self.headers,
                auth=self.auth,
                params=params,
                timeout=15,
            )
            if response.status_code == 200:
                data = response.json()
                issues = data.get("issues", [])
                result = []
                for issue in issues:
                    key = issue["key"]
                    fields = issue["fields"]
                    summary = fields["summary"]
                    proj_name = fields.get("project", {}).get("name", "")
                    status_name = fields.get("status", {}).get("name", "")
                    desc_adf = fields.get("description")
                    desc_text = self._adf_to_plain_text(desc_adf)

                    # Truncate to fit device buffers (with null terminator)
                    if len(summary) > 47:
                        summary = summary[:44] + "..."
                    if len(proj_name) > 23:
                        proj_name = proj_name[:20] + "..."
                    if len(status_name) > 15:
                        status_name = status_name[:12] + "..."
                    if len(desc_text) > 127:
                        desc_text = desc_text[:124] + "..."

                    result.append({
                        "key": key,
                        "name": summary,
                        "proj": proj_name,
                        "status": status_name,
                        "desc": desc_text,
                    })
                logger.info(f"Fetched {len(result)} Jira issues for current user")
                return result
            else:
                logger.error(f"Jira search API error: {response.status_code}")
                return []
        except Exception as e:
            logger.error(f"Failed to fetch Jira issues: {e}")
            return []

    def log_work(
        self, issue_key: str, time_spent_seconds: int, description: str
    ) -> bool:
        """Log work time to a Jira issue."""
        try:
            payload = {
                "timeSpentSeconds": time_spent_seconds,
                "comment": {
                    "type": "doc",
                    "version": 1,
                    "content": [
                        {
                            "type": "paragraph",
                            "content": [
                                {"type": "text", "text": description}
                            ],
                        }
                    ],
                },
            }

            response = requests.post(
                f"{self.base_url}/rest/api/3/issue/{issue_key}/worklog",
                headers=self.headers,
                auth=self.auth,
                json=payload,
                timeout=10,
            )

            if response.status_code in (200, 201):
                logger.info(
                    f"Logged {time_spent_seconds}s to {issue_key}"
                )
                return True
            else:
                logger.error(
                    f"Jira worklog error: {response.status_code} - "
                    f"{response.text[:200]}"
                )
                return False
        except Exception as e:
            logger.error(f"Failed to log work to Jira: {e}")
            return False


class WorklogDialog:
    """Tkinter dialog for entering work description after a Pomodoro."""

    def __init__(self):
        self._result = None

    def show(self, project_key: str, duration_minutes: int) -> Optional[str]:
        """
        Show a dialog asking what the user worked on.
        Returns the description string, or None if cancelled.
        Blocks until user responds.
        """
        self._result = None

        # Run tkinter in a thread to avoid blocking issues
        thread = threading.Thread(
            target=self._run_dialog,
            args=(project_key, duration_minutes),
            daemon=True,
        )
        thread.start()
        thread.join(timeout=300)  # 5 minute max wait

        return self._result

    def _run_dialog(self, project_key: str, duration_minutes: int):
        try:
            import tkinter as tk
            from tkinter import ttk

            root = tk.Tk()
            root.title("FocusKnob — Log Work")
            root.geometry("420x280")
            root.resizable(False, False)

            # Center on screen
            root.update_idletasks()
            x = (root.winfo_screenwidth() - 420) // 2
            y = (root.winfo_screenheight() - 280) // 2
            root.geometry(f"+{x}+{y}")

            # Keep on top
            root.attributes("-topmost", True)
            root.lift()
            root.focus_force()

            # Header
            header = ttk.Label(
                root,
                text=f"Pomodoro Complete — {project_key}",
                font=("Helvetica", 16, "bold"),
            )
            header.pack(pady=(15, 5))

            duration_label = ttk.Label(
                root,
                text=f"{duration_minutes} minutes",
                font=("Helvetica", 12),
            )
            duration_label.pack(pady=(0, 10))

            # Prompt
            prompt = ttk.Label(root, text="What did you work on?")
            prompt.pack(pady=(0, 5))

            # Text entry
            text_var = tk.StringVar()
            entry = ttk.Entry(root, textvariable=text_var, width=50)
            entry.pack(padx=20, pady=5)
            entry.focus_set()

            # Buttons
            btn_frame = ttk.Frame(root)
            btn_frame.pack(pady=15)

            def on_submit():
                self._result = text_var.get().strip() or "Focus work"
                root.destroy()

            def on_cancel():
                self._result = None
                root.destroy()

            def on_enter(event):
                on_submit()

            submit_btn = ttk.Button(
                btn_frame, text="Log to Jira", command=on_submit
            )
            submit_btn.pack(side=tk.LEFT, padx=10)

            cancel_btn = ttk.Button(
                btn_frame, text="Skip", command=on_cancel
            )
            cancel_btn.pack(side=tk.LEFT, padx=10)

            entry.bind("<Return>", on_enter)
            root.protocol("WM_DELETE_WINDOW", on_cancel)

            root.mainloop()

        except ImportError:
            logger.warning("tkinter not available, using default description")
            self._result = "Focus work"
        except Exception as e:
            logger.error(f"Dialog error: {e}")
            self._result = None
