# Spiegazione completa del progetto — versione semplice

## L'idea in una frase

Abbiamo costruito un piccolo **banco prova intelligente** per capire se un motore sta lavorando normalmente oppure se il suo rumore e le sue vibrazioni diventano anomali.

## Il problema

Un motore che sta bene produce un certo rumore e certe vibrazioni. Se lo freni, si usura o lavora male, quei segnali cambiano. L'idea era quindi imparare il comportamento normale e confrontarlo con ciò che succede dopo.

## I pezzi principali

Il motore brushed è l'oggetto da controllare. Un microfono MAX9814 ascolta il rumore. Un accelerometro misura le vibrazioni. Una Portenta C33 legge i sensori. NanoEdge AI produce un punteggio di similarity. Un servo aziona un piccolo freno per creare volontariamente una condizione anomala. MQTT manda i dati via Wi-Fi. Il Raspberry Pi riceve i messaggi, li salva in SQLite, li mostra con Flask e può inviare una mail.

## Perché serve il servo

Non vogliamo rompere davvero il motore per verificare se il sistema rileva un guasto. Il servo applica una resistenza/freno controllata. In questo modo possiamo creare sempre lo stesso tipo di disturbo e vedere se sensori + AI se ne accorgono.

## Cosa significa similarity

NanoEdge AI osserva molti campioni di funzionamento normale. Dopo il learning, quando riceve un nuovo blocco di dati restituisce una similarity. Un valore alto significa "questo assomiglia a ciò che ho imparato come normale". Un valore basso significa "questo segnale è diverso".

La soglia è una decisione di progetto: sotto una certa percentuale il comportamento viene considerato anomalo. Nei materiali esistono revisioni con soglie diverse, per esempio 75 e 90; non bisogna fingere che ci fosse un unico valore definitivo.

## Come vengono acquisiti i dati

Per il microfono esiste uno sketch che campiona 256 valori ADC a 8 kHz. Quindi un frame dura circa 32 ms. Per il LIS2DW12 esiste uno sketch che campiona 256 misure X/Y/Z a 800 Hz. Questi frame servivano a NanoEdge AI Studio e la stessa logica di frame compare nel firmware integrato.

## Il flusso normale

Motore gira -> sensori misurano -> Portenta costruisce i frame -> NanoEdge valuta -> Portenta pubblica su MQTT -> Raspberry riceve -> SQLite salva -> Flask visualizza.

## Il flusso di test anomalia corretto

Premo FAULT -> il servo crea il disturbo meccanico -> i sensori misurano il nuovo comportamento -> NanoEdge calcola la similarity -> se è sotto soglia la Portenta pubblica `/anomalia` -> il Raspberry registra l'evento e può inviare la mail.

Questa distinzione è importantissima: **il pulsante crea il problema, non decide che esiste un problema**.

## MQTT spiegato senza teoria inutile

MQTT è come una bacheca con canali. La Portenta scrive su un canale, il Raspberry ascolta. `banco/motore_brushed/dati` contiene i valori live; `banco/motore_brushed/anomalia` rappresenta un evento di allarme; `banco/stato` contiene RUNNING, FAULT_TEST o STOPPED.

## Cosa fa il Raspberry

Mosquitto è il broker, cioè riceve e distribuisce i messaggi MQTT. Il monitor Python ascolta `banco/#`, interpreta i JSON e salva i dati in SQLite. Se il topic termina con `/anomalia`, prepara una mail. La dashboard Flask legge l'ultimo valore nel database e lo restituisce anche tramite `/api/live`, che il browser interroga periodicamente.

## Perché SQLite

È un database molto semplice contenuto in un file. Per un prototipo è sufficiente: non serve installare un server database separato e permette comunque di conservare lo storico.

## Perché Flask

Flask permette di creare rapidamente una piccola applicazione web in Python. La dashboard non deve fare elaborazioni pesanti: deve leggere gli ultimi dati e mostrarli.

## Cosa abbiamo imparato davvero

La parte interessante non è un singolo algoritmo. È l'integrazione: sensore, temporizzazione, microcontrollore, AI embedded, Wi-Fi, MQTT, Linux/Raspberry, database, web, alimentazione del servo e meccanica devono funzionare insieme.

## Cosa non devi raccontare come certo

Non dire che ogni file è stato scritto personalmente da te. Era un progetto di gruppo. Non dire che il Raspberry source qui presente è certamente l'originale: è una ricostruzione. Non dire che l'ADXL345 era sicuramente il sensore finale: i materiali mostrano revisioni diverse e il firmware recuperato usa LIS2DW12. Non dire che la logica FAULT del firmware recuperato era già perfetta: conserva una versione precedente che forza l'allarme.
