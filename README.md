# 🧵 Multithread Progress Tracker

Multi-threaded job progress tracking system (JobDispatcher), supporting:
- Thread pool automatic dispatch
- Job priority (priority queue)
- Progress tracking (%), ETA, latency
- Send notification when completed (via Slack or webhook)
- Script support Python & CMD
- Logging to file and console simultaneously


---

## 🚀 Build

git clone https://github.com/minh-9999/multithread-progress-tracker.git

```
cd multithread-progress-tracker
mkdir build && cd build
cmake ..
cmake --build .
```

## 🚀 Run

copy script folder to the target folder include execution file

⚠️ Before running, you need to set the SLACK_WEBHOOK_URL environment variable so that the script can send Slack notifications!

### Linux / macOS:
```
export SLACK_WEBHOOK_URL="https://hooks.slack.com/services/XXXX/XXXX/XXXX"
./Multithread-Progress-Tracker
```

### Windows PowerShell:
```
$env:SLACK_WEBHOOK_URL="https://hooks.slack.com/services/XXXX/XXXX/XXXX"
.\Multithread-Progress-Tracker.exe

```

### Windows CMD:
```
set SLACK_WEBHOOK_URL=https://hooks.slack.com/services/XXXX/XXXX/XXXX
Multithread-Progress-Tracker.exe

```

Run support script (Windows)
```
script\send_slack.cmd
```

Run Slack notification script 
```
python script/notify.py
```

![Screenshot](https://i.ibb.co/bjds9ws6/multithread-progress-tracker-result.png)


## 🧠 Features

| Features | Description |
| ------------------------------- | ------------------------------------ |
| ✅ Multi-threaded job execution | Optimized by thread pool |
| ⏳ Progress tracking + ETA | Calculation based on total number of jobs |
| 🔁 Retry, timeout, pause/resume | Customize each job |
| ⚙️ Detailed logging | Including time, thread ID, status |
| 📬 Send Slack/webhook | When completed or error |
| 🧪 Unit test + Integration test | Covers the entire Job Dispatcher |

## 🛠 Configure notify (notify_config.json)

``` json
{
  "method": "slack",
  "slack_webhook_url": "https://hooks.slack.com/services/XXXX/XXXX/XXXX",
  "notify_on": ["finish", "error"]
}
```

You can use "method": "webhook" if you don't use Slack

## 📜 License

MIT. 
Feel free to use, just don't forget to credit 😎