#!/usr/bin/env python3
"""
PROGETTO BANCO PROVA - DASHBOARD FLASK
Versione finale ricostruita dalla chat

Funzioni:
- Legge gli ultimi dati da SQLite
- Mostra dashboard web
- Espone /api/live per aggiornamento live senza ricaricare pagina
"""

import os
import sqlite3
from flask import Flask, jsonify, render_template_string

DB_PATH = os.getenv("DB_PATH", "/home/pi/banco/banco.db")

app = Flask(__name__)

HTML = """
<!doctype html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Banco prova - Dashboard</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f3f5f7;
      margin: 0;
      padding: 24px;
      color: #222;
    }
    h1 { margin-top: 0; }
    .status {
      background: white;
      padding: 16px;
      border-radius: 12px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      margin-bottom: 20px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 16px;
    }
    .card {
      background: white;
      border-radius: 12px;
      padding: 18px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      border-left: 8px solid #999;
    }
    .card.ok { border-left-color: #2eaa57; }
    .card.alarm { border-left-color: #d93025; }
    .name {
      font-size: 22px;
      font-weight: bold;
      margin-bottom: 10px;
    }
    .row {
      display: flex;
      justify-content: space-between;
      border-bottom: 1px solid #eee;
      padding: 7px 0;
      gap: 12px;
    }
    .label { color: #666; }
    .value { font-weight: bold; }
    .badge {
      display: inline-block;
      padding: 4px 10px;
      border-radius: 999px;
      color: white;
      font-weight: bold;
      font-size: 13px;
    }
    .badge.ok { background: #2eaa57; }
    .badge.alarm { background: #d93025; }
    .small { color: #777; font-size: 13px; }
  </style>
</head>
<body>
  <h1>Dashboard banco prova</h1>

  <div class="status">
    <div><b>Stato banco:</b> <span id="stato-banco">---</span></div>
    <div class="small">Ultimo aggiornamento: <span id="ultimo-aggiornamento">---</span></div>
  </div>

  <div class="grid" id="cards"></div>

<script>
const macchine = ["pompa", "motoriduttore", "motore_brushed"];

function valore(v) {
  if (v === null || v === undefined) return "---";
  return v;
}

function nomeBello(nome) {
  if (nome === "motore_brushed") return "Motore brushed";
  if (nome === "motoriduttore") return "Motoriduttore";
  if (nome === "pompa") return "Pompa";
  return nome;
}

function renderCard(m) {
  const alarm = m.anomalia === true || m.anomalia === 1;
  return `
    <div class="card ${alarm ? 'alarm' : 'ok'}">
      <div class="name">${nomeBello(m.macchina)}</div>
      <div class="row"><span class="label">Anomalia</span><span class="badge ${alarm ? 'alarm' : 'ok'}">${alarm ? 'SÌ' : 'NO'}</span></div>
      <div class="row"><span class="label">Similarity</span><span class="value">${valore(m.similarity)}</span></div>
      <div class="row"><span class="label">Microfono</span><span class="value">${valore(m.microfono)}</span></div>
      <div class="row"><span class="label">Mic peak</span><span class="value">${valore(m.mic_peak)}</span></div>
      <div class="row"><span class="label">Vibrazione</span><span class="value">${valore(m.vibrazione)}</span></div>
      <div class="row"><span class="label">Vib peak</span><span class="value">${valore(m.vib_peak)}</span></div>
      <div class="row"><span class="label">RPM</span><span class="value">${valore(m.rpm)}</span></div>
      <div class="row"><span class="label">Stato</span><span class="value">${valore(m.stato)}</span></div>
      <div class="small">${valore(m.timestamp)}</div>
    </div>
  `;
}

async function aggiornaLive() {
  try {
    const res = await fetch('/api/live');
    const data = await res.json();

    document.getElementById('stato-banco').textContent = valore(data.stato_banco);
    document.getElementById('ultimo-aggiornamento').textContent = new Date().toLocaleTimeString();

    let html = '';
    for (const nome of macchine) {
      const m = data.macchine[nome] || { macchina: nome };
      html += renderCard(m);
    }
    document.getElementById('cards').innerHTML = html;
  } catch (err) {
    console.error(err);
    document.getElementById('stato-banco').textContent = 'ERRORE CONNESSIONE';
  }
}

aggiornaLive();
setInterval(aggiornaLive, 500);
</script>
</body>
</html>
"""


def query_db(query, params=()):
    if not os.path.exists(DB_PATH):
        return []
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute(query, params)
    rows = cur.fetchall()
    conn.close()
    return [dict(row) for row in rows]


def ultimo_per_macchina(macchina):
    rows = query_db(
        """
        SELECT timestamp, macchina, similarity, microfono, vibrazione, mic_peak,
               vib_peak, rpm, anomalia, stato
        FROM letture
        WHERE macchina = ?
        ORDER BY id DESC
        LIMIT 1
        """,
        (macchina,),
    )
    if rows:
        row = rows[0]
        row["anomalia"] = bool(row.get("anomalia"))
        return row
    return {"macchina": macchina}


def ultimo_stato_banco():
    rows = query_db(
        """
        SELECT payload
        FROM letture
        WHERE topic = 'banco/stato'
        ORDER BY id DESC
        LIMIT 1
        """
    )
    if not rows:
        return "---"

    import json
    try:
        return json.loads(rows[0]["payload"]).get("stato", "---")
    except Exception:
        return rows[0]["payload"]


@app.route("/")
def index():
    return render_template_string(HTML)


@app.route("/api/live")
def api_live():
    macchine = ["pompa", "motoriduttore", "motore_brushed"]
    data = {
        "stato_banco": ultimo_stato_banco(),
        "macchine": {m: ultimo_per_macchina(m) for m in macchine},
    }
    return jsonify(data)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)
