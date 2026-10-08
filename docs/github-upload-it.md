# Pubblicazione su GitHub — passo per passo

## Prima di pubblicare

Controlla che non esistano password, token, email password o altri segreti. Questo pacchetto ha già rimosso la password Wi-Fi trovata nei materiali, ma fai comunque una ricerca finale.

## Metodo semplice con GitHub Desktop

1. Crea su GitHub un repository pubblico chiamato `motor-anomaly-detection-edge-ai` senza README automatico.
2. Scompatta questa cartella sul PC.
3. Apri GitHub Desktop -> **File -> Add local repository**. Se chiede di creare un repository Git nella cartella, accetta.
4. Controlla la lista dei file: `secrets.h`, `banco.env`, database e password non devono comparire.
5. Primo commit: `Initial public project archive`.
6. Collega il repository remoto GitHub e fai **Publish repository** / push.
7. Apri GitHub dal browser e verifica che immagini, diagramma Mermaid e link del README funzionino.

## Metodo terminale

```bash
cd motor-anomaly-detection-edge-ai
git init
git add .
git status
git commit -m "Initial public project archive"
git branch -M main
git remote add origin https://github.com/TUO-USERNAME/motor-anomaly-detection-edge-ai.git
git push -u origin main
```

## Dopo il primo push

Imposta una descrizione breve del repo, per esempio:

`ITS team project: Portenta C33 + NanoEdge AI + MQTT + Raspberry Pi + Flask for brushed-motor anomaly monitoring.`

Aggiungi topic GitHub come `embedded`, `iot`, `edge-ai`, `mqtt`, `raspberry-pi`, `flask`, `sqlite`, `arduino`, `condition-monitoring`.

Pinna questo repository sul tuo profilo GitHub.

## Commit successivi sensati

Non creare 30 commit finti. Fai commit reali quando capisci o migliori qualcosa, per esempio:

- `docs: clarify recovered vs reconstructed source`
- `docs: add architecture explanation`
- `raspberry: add safe environment configuration example`
- `firmware: document fault injection vs detection mismatch`
- `docs: add test procedure`
