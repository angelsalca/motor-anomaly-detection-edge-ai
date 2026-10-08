# Raspberry Pi software

The files in this directory are a **reconstruction from the project documentation/chat history**, not a verified byte-for-byte recovery of the Raspberry Pi filesystem used during the original bench demonstration.

They implement the architecture documented by the project:

- Mosquitto as MQTT broker
- `monitor_mqtt_mail.py` subscribes to `banco/#`
- SQLite stores readings and anomaly events
- email is sent when an `/anomalia` topic arrives
- `dashboard.py` reads SQLite and exposes `/api/live`
- the browser refreshes live values every 500 ms without reloading the whole page

## Install

```bash
sudo apt install mosquitto mosquitto-clients python3-pip
mkdir -p /home/pi/banco
cd /home/pi/banco
python3 -m pip install -r requirements.txt
```

Copy `banco.env.example` to `banco.env`, fill in local values and **never commit that file**.

Run manually:

```bash
python3 monitor_mqtt_mail.py
python3 dashboard.py
```

Dashboard: `http://<raspberry-ip>:5000`

Install monitor service:

```bash
sudo cp banco-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now banco-monitor.service
sudo systemctl status banco-monitor.service
```

Quick MQTT test:

```bash
mosquitto_sub -h localhost -t 'banco/#' -v
```
