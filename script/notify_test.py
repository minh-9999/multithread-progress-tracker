#!/usr/bin/env python3
import json
import sys
import os

def main():
    if len(sys.argv) < 2:
        print("Usage: python notify.py <json_file>")
        return

    json_path = sys.argv[1]

    if not os.path.exists(json_path):
        print(f"File not found: {json_path}")
        return

    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    print("=== JOB SUMMARY ===")
    for key, value in data.items():
        print(f"{key}: {value}")

    # TODO: send notification via email, Telegram, etc.

if __name__ == "__main__":
    main()
