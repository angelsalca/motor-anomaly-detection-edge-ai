#!/usr/bin/env python3
"""
PROGETTO BANCO PROVA - MONITOR MQTT / DATABASE / MAIL
Versione finale ricostruita dalla chat

Funzioni:
- Si collega al broker MQTT Mosquitto
- Ascolta banco/#
- Salva i dati in SQLite
- Invia mail quando arriva un topic /anomalia

Config mail tramite variabili ambiente:
SMTP_HOST=smtp.gmail.com
SMTP_PORT=587
SMTP_USER=tuamail@gmail.com
SMTP_PASSWORD=password_app
MAIL_FROM=tuamail@gmail.com
MAIL_TO=destinatario@example.com
"""

import json
import os
import sqlite3
import smtplib
import ssl
from datetime import datetime
from email.message import EmailMessage

import paho.mqtt.client as mqtt

MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "banco/#")

DB_PATH = os.getenv("DB_PATH", "/home/pi/banco/banco.db")

SMTP_HOST = os.getenv("SMTP_HOST", "smtp.gmail.com")
SMTP_PORT = int(os.getenv("SMTP_PORT", "587"))
SMTP_USER = os.getenv("SMTP_USER", "")
SMTP_PASSWORD = os.getenv("SMTP_PASSWORD", "")
MAIL_FROM = os.getenv("MAIL_FROM", SMTP_USER)
MAIL_TO = os.getenv("MAIL_TO", "")

# Evita invii ripetuti troppo ravvicinati per la stessa macchina
COOLDOWN_SECONDS = int(os.getenv("MAIL_COOLDOWN_SECONDS", "60"))
_last_alarm = {}


def init_db():
    os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS letture (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            topic TEXT NOT NULL,
            macchina TEXT,
            similarity INTEGER,
            microfono INTEGER,
            vibrazione INTEGER,
            mic_peak INTEGER,
            vib_peak INTEGER,
            rpm INTEGER,
            anomalia INTEGER,
            stato TEXT,
            payload TEXT NOT NULL
        )
        """
    )
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS anomalie (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            topic TEXT NOT NULL,
            macchina TEXT,
            similarity INTEGER,
            stato TEXT,
            payload TEXT NOT NULL,
            mail_inviata INTEGER DEFAULT 0
        )
        """
    )
    conn.commit()
    conn.close()


def now_iso():
    return datetime.now().isoformat(timespec="seconds")


def parse_payload(payload_bytes):
    text = payload_bytes.decode("utf-8", errors="replace")
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        data = {"raw": text}
    return text, data


def salva_lettura(topic, payload_text, data):
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute(
        """
        INSERT INTO letture (
            timestamp, topic, macchina, similarity, microfono, vibrazione,
            mic_peak, vib_peak, rpm, anomalia, stato, payload
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            now_iso(),
            topic,
            data.get("macchina"),
            data.get("similarity"),
            data.get("microfono"),
            data.get("vibrazione"),
            data.get("mic_peak"),
            data.get("vib_peak"),
            data.get("rpm"),
            1 if data.get("anomalia") else 0,
            data.get("stato"),
            payload_text,
        ),
    )
    conn.commit()
    conn.close()


def salva_anomalia(topic, payload_text, data, mail_inviata):
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute(
        """
        INSERT INTO anomalie (
            timestamp, topic, macchina, similarity, stato, payload, mail_inviata
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
        """,
        (
            now_iso(),
            topic,
            data.get("macchina"),
            data.get("similarity"),
            data.get("stato"),
            payload_text,
            1 if mail_inviata else 0,
        ),
    )
    conn.commit()
    conn.close()


def deve_inviare_mail(macchina):
    adesso = datetime.now().timestamp()
    ultimo = _last_alarm.get(macchina, 0)
    if adesso - ultimo >= COOLDOWN_SECONDS:
        _last_alarm[macchina] = adesso
        return True
    return False


def invia_mail_anomalia(topic, data, payload_text):
    if not SMTP_USER or not SMTP_PASSWORD or not MAIL_TO:
        print("[MAIL] Configurazione mail mancante: imposto mail_inviata=0")
        return False

    macchina = data.get("macchina", "sconosciuta")
    similarity = data.get("similarity", "N/D")
    stato = data.get("stato", "N/D")

    msg = EmailMessage()
    msg["From"] = MAIL_FROM
    msg["To"] = MAIL_TO
    msg["Subject"] = f"Anomalia banco prova - {macchina}"
    msg.set_content(
        f"""È stata rilevata un'anomalia sul banco prova.

Macchina: {macchina}
Similarity: {similarity}
Stato: {stato}
Topic MQTT: {topic}
Ora: {now_iso()}

Payload completo:
{payload_text}
"""
    )

    context = ssl.create_default_context()
    with smtplib.SMTP(SMTP_HOST, SMTP_PORT) as server:
        server.starttls(context=context)
        server.login(SMTP_USER, SMTP_PASSWORD)
        server.send_message(msg)

    print(f"[MAIL] Inviata mail anomalia per {macchina}")
    return True


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Connesso a {MQTT_HOST}:{MQTT_PORT}")
        client.subscribe(MQTT_TOPIC)
        print(f"[MQTT] Sottoscritto a {MQTT_TOPIC}")
    else:
        print(f"[MQTT] Errore connessione rc={rc}")


def on_message(client, userdata, msg):
    topic = msg.topic
    payload_text, data = parse_payload(msg.payload)

    print(f"[MQTT] {topic} {payload_text}")

    salva_lettura(topic, payload_text, data)

    if topic.endswith("/anomalia"):
        macchina = data.get("macchina", topic)
        mail_inviata = False
        if deve_inviare_mail(macchina):
            try:
                mail_inviata = invia_mail_anomalia(topic, data, payload_text)
            except Exception as exc:
                print(f"[MAIL] Errore invio: {exc}")
        else:
            print(f"[MAIL] Cooldown attivo per {macchina}, mail non inviata")

        salva_anomalia(topic, payload_text, data, mail_inviata)


def main():
    init_db()

    client = mqtt.Client(client_id="banco-monitor")
    client.on_connect = on_connect
    client.on_message = on_message

    print("[START] Monitor banco prova")
    print(f"[DB] {DB_PATH}")
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever()


if __name__ == "__main__":
    main()
