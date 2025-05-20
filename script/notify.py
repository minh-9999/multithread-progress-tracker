#!/usr/bin/env python3
import sys
import json
import requests
import os
import io

# Force UTF-8 output encoding (Windows safe)
if sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

def send_slack_webhook(data, webhook_url, fancy=True):
    total_jobs = data.get("total_jobs", 0)
    completed_jobs = data.get("completed_jobs", 0)
    average_latency = data.get("average_latency_ms", 0)
    total_time = data.get("total_execution_time_ms", 0)
    paused = data.get("paused", False)

    if fancy:
        paused_text = ":x: *Paused: Yes*" if paused else ":white_check_mark: *Paused: No*"
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

    try:
        res = requests.post(webhook_url, json=payload)
        print("Slack webhook:", res.status_code, res.text)
    except Exception as e:
        print("Slack webhook failed:", e)


def send_slack_file(token, channel, file_path):
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
                files={"file": f}
            )
        print("Slack file upload:", res.status_code, res.text)
    except Exception as e:
        print("Slack file upload failed:", e)


def send_script_simulate(file_path):
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            print("[Simulated sending]:")
            print(json.dumps(json.load(f), indent=2, ensure_ascii=False))
    except Exception as e:
        print("Simulated sending failed:", e)


def main_from_config():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(base_dir, "notify_config.json")
    summary_path = os.path.join(base_dir, "job_summary.json")

    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = json.load(f)
    except FileNotFoundError:
        print("Missing notify_config.json")
        return
    except json.JSONDecodeError:
        print("Malformed notify_config.json")
        return

    try:
        with open(summary_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        print("Missing job_summary.json")
        return
    except json.JSONDecodeError:
        print("Malformed job_summary.json")
        return

    method = config.get("method", "script")

    if method == "slack":
        send_slack_webhook(data, config.get("webhook_url", ""), fancy=True)
    elif method == "api":
        send_slack_file(config.get("token", ""), config.get("channel_id", ""), summary_path)
    else:
        send_script_simulate(summary_path)

    print(f"Loaded config: method = {method}")


def main():
    args = sys.argv[1:]

    if len(args) == 0:
        main_from_config()
    elif len(args) == 1 and args[0].endswith(".json"):
        print("Only JSON file provided, loading notify_config.json for method...")
        main_from_config()
    elif len(args) == 2:
        try:
            with open(args[1], 'r', encoding='utf-8') as f:
                data = json.load(f)
            send_slack_webhook(data, args[0], fancy=True)
        except Exception as e:
            print("Error sending webhook from args:", e)
    elif len(args) == 3:
        send_slack_file(args[0], args[1], args[2])
    else:
        print("Usage:")
        print("  python notify.py <webhook_url> <json_file>")
        print("  python notify.py <slack_token> <channel_id> <json_file>")


if __name__ == "__main__":
    main()
