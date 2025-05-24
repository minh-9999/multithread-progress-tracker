#!/usr/bin/env python3
import sys
import json
import requests
import os
import io
import time

# Ensure UTF-8 output on Windows
if sys.stdout.encoding is None or sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')


def send_slack_webhook(data, webhook_url, fancy=True, retries=0):
    total_jobs = data.get("total_jobs", 0)
    completed_jobs = data.get("completed_jobs", 0)
    average_latency = data.get("average_latency_ms", 0)
    total_time = data.get("total_execution_time_ms", 0)
    paused = data.get("paused", False)

    paused_text = ":x: *Paused: Yes*" if paused else ":white_check_mark: *Paused: No*"

    if fancy:
        payload = {
            "text": "📊 Job Summary Report",
            "blocks": [
                {"type": "header", "text": {"type": "plain_text", "text": "📊 Job Summary Report"}},
                {"type": "divider"},
                {
                    "type": "section",
                    "fields": [
                        {"type": "mrkdwn", "text": f"*Total Jobs:*\n{total_jobs}"},
                        {"type": "mrkdwn", "text": f"*Completed:*\n{completed_jobs}"},
                        {"type": "mrkdwn", "text": f"*Avg Latency:*\n{average_latency} ms"},
                        {"type": "mrkdwn", "text": f"*Exec Time:*\n{total_time} ms"},
                        {"type": "mrkdwn", "text": paused_text}
                    ]
                }
            ]
        }
    else:
        paused_str = "Yes" if paused else "No"
        payload = {
            "text": f"""🔥 *Job Summary Report* 🔥
• ✅ Completed: {completed_jobs}/{total_jobs}
• ⏱️ Avg latency: {average_latency} ms
• ⌛ Total time: {total_time} ms
• ⏸️ Paused: {paused_str}"""
        }

    for attempt in range(1 + retries):
        try:
            res = requests.post(webhook_url, json=payload, timeout=5)
            print(f"[Slack Webhook] Attempt {attempt+1} - Status: {res.status_code}", flush=True)
            if res.status_code == 200:
                return True
        except Exception as e:
            print(f"[Slack Webhook] Attempt {attempt+1} failed: {e}", flush=True)
        time.sleep(1)
    return False


def send_slack_file(token, channel, file_path, retries=0):
    for attempt in range(1 + retries):
        try:
            with open(file_path, 'rb') as f:
                res = requests.post(
                    "https://slack.com/api/files.upload",
                    headers={"Authorization": f"Bearer {token}"},
                    data={
                        "channels": channel,
                        "title": "Job Summary",
                        "initial_comment": "📦 Job Summary JSON"
                    },
                    files={"file": f},
                    timeout=10
                )
            print(f"[Slack File] Attempt {attempt+1} - Status: {res.status_code}", flush=True)
            if res.status_code == 200:
                return True
        except Exception as e:
            print(f"[Slack File] Attempt {attempt+1} failed: {e}", flush=True)
        time.sleep(1)
    return False


def send_script_simulate(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            print("[Simulated Sending]:", flush=True)
            print(json.dumps(json.load(f), indent=2, ensure_ascii=False), flush=True)
    except Exception as e:
        print("Simulated sending failed:", e, flush=True)


def load_config():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(base_dir, "notify_config.json")

    config = {}
    if os.path.exists(config_path):
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
        except Exception as e:
            print("[Config] Failed to parse notify_config.json:", e, flush=True)

    placeholder_webhook = "__SLACK_WEBHOOK_URL__"
    placeholder_token = "__SLACK_TOKEN__"
    placeholder_channel = "__SLACK_CHANNEL_ID__"

    # Override webhook_url from env if placeholder or empty
    if config.get("webhook_url") in (None, "", placeholder_webhook):
        config["webhook_url"] = os.environ.get("SLACK_WEBHOOK_URL", "")
    # Override token
    if config.get("token") in (None, "", placeholder_token):
        config["token"] = os.environ.get("SLACK_TOKEN", "")
    # Override channel_id
    if config.get("channel_id") in (None, "", placeholder_channel):
        config["channel_id"] = os.environ.get("SLACK_CHANNEL_ID", "")

    # Default values
    config["method"] = config.get("method", "script")
    config["fancy"] = config.get("fancy", True)
    config["retry"] = int(config.get("retry", 0))
    config["attach_chart"] = config.get("attach_chart", False)

    # Basic checks:
    if config["method"] == "slack" and not config["webhook_url"]:
        print("[Error] SLACK_WEBHOOK_URL is required for 'slack' method. Please set env SLACK_WEBHOOK_URL.", flush=True)
    if config["method"] == "api" and (not config["token"] or not config["channel_id"]):
        print("[Error] SLACK_TOKEN and SLACK_CHANNEL_ID are required for 'api' method. Please set env variables.", flush=True)

    return config


def main_from_config():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    summary_path = os.path.join(base_dir, "job_summary.json")
    config = load_config()

    try:
        with open(summary_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception as e:
        print("[Error] Failed to load job_summary.json:", e, flush=True)
        return

    method = config["method"]

    if method == "slack":
        if config["webhook_url"]:
            send_slack_webhook(data, config["webhook_url"], fancy=config["fancy"], retries=config["retry"])
        else:
            print("[Abort] Missing webhook_url, cannot send slack message.", flush=True)
    elif method == "api":
        if config["token"] and config["channel_id"]:
            send_slack_file(config["token"], config["channel_id"], summary_path, retries=config["retry"])
        else:
            print("[Abort] Missing token or channel_id, cannot send slack file.", flush=True)
    else:
        send_script_simulate(summary_path)

    print(f"[Notify] Method: {method}", flush=True)


def main():
    args = sys.argv[1:]

    if len(args) == 0:
        main_from_config()
    elif len(args) == 1 and args[0].endswith(".json"):
        print("[Notify] Only JSON provided → loading notify_config.json", flush=True)
        main_from_config()
    elif len(args) == 2:
        try:
            with open(args[1], 'r', encoding='utf-8') as f:
                data = json.load(f)
            send_slack_webhook(data, args[0], fancy=True)
        except Exception as e:
            print("[Notify] Webhook failed:", e, flush=True)
    elif len(args) == 3:
        send_slack_file(args[0], args[1], args[2])
    else:
        print("Usage:", flush=True)
        print("  python notify.py <webhook_url> <json_file>", flush=True)
        print("  python notify.py <slack_token> <channel_id> <json_file>", flush=True)
        print("  python notify.py               # uses notify_config.json", flush=True)


if __name__ == "__main__":
    main()
