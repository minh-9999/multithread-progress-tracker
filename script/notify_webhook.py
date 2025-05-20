import sys
import requests

def notify_webhook(url, message):
    payload = {"text": message}
    headers = {'Content-Type': 'application/json'}
    response = requests.post(url, json=payload, headers=headers)
    print("Webhook response:", response.status_code, response.text)

if __name__ == '__main__':
    webhook_url = "https://hooks.slack.com/services/..."  # replace by real URL 
    msg = sys.argv[1] if len(sys.argv) > 1 else "Job queue completed"
    notify_webhook(webhook_url, msg)
