# Guida colloquio

## Versione da 30 secondi

> È un progetto ITS di gruppo su un banco prova per anomaly detection di un motore brushed. La Portenta C33 acquisisce rumore e vibrazioni, usa NanoEdge AI per confrontare il segnale con il comportamento normale e pubblica i dati via MQTT. Un Raspberry Pi riceve i messaggi, li salva in SQLite, li espone su una dashboard Flask e può inviare una mail quando arriva un evento di anomalia. Per testare il sistema un servo applica una resistenza meccanica controllata al motore.

## Versione da 2 minuti

Parti dal problema, poi descrivi la catena completa. Spiega che il microfono e l'accelerometro producono frame, che NanoEdge lavora con una fase di learning e una di detection, e che la similarity è un indice di somiglianza al comportamento normale. Spiega MQTT con publisher/topic/subscriber, poi il lato Raspberry con Mosquitto, Python, SQLite e Flask. Chiudi con il punto più importante: il pulsante FAULT deve soltanto creare fisicamente il disturbo; l'allarme deve dipendere dai sensori e dall'AI.

## Domande che devi saper reggere

**Perché MQTT?** È leggero, semplice per sistemi IoT e disaccoppia il dispositivo embedded dal backend.

**Perché Raspberry Pi?** Permette di avere broker MQTT, Python, database e dashboard su un piccolo computer Linux separato dalla Portenta.

**Cos'è la similarity?** Un punteggio restituito dal modello NanoEdge che indica quanto il nuovo segnale assomiglia al comportamento appreso come normale.

**Perché il servo?** Per creare un fault meccanico ripetibile senza aspettare un guasto reale.

**Qual è stato un problema concreto?** Rete/IP, alimentazione e stabilità del servo, pin FAULT, upload Portenta e soprattutto la separazione tra simulare il fault e rilevarlo realmente.

**Cosa miglioreresti?** Recuperare/testare il backend originale, rendere univoca la revisione hardware, salvare dataset/versioni del modello, fare test ripetibili con metriche e aggiungere test automatici lato Python.

## Come parlare del tuo contributo

Usa solo ciò che ricordi davvero. Formula corretta: "Nel progetto di gruppo ho lavorato/ho seguito soprattutto ...". Se non ricordi chi ha scritto una parte, parla del sistema come lavoro di team e poi descrivi ciò che oggi sai spiegare e verificare nel repository.
