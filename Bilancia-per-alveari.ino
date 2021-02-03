#include "HX711.h"
#include "sms.h"
#include <SoftwareSerial.h>

GSM GSM;
SMSGSM sms;

#define DOUT A1
#define SCK A0
HX711 scale(DOUT, SCK);

boolean started=false;
float units;                                     //float dove inserire il valore di peso
char destinatario[20];

float valoredicalibrazione = -21.45;  // calibrazione della cella di carico

//int Reset = 4;                        //dichiaro il Pin per dare l'impulso di reset quando richiesto
void(* Riavvia)(void) = 0;          //Comando per Software Reset solo con Auduino Uno

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);             // initialize digital pin LED_BUILTIN as an output.
    digitalWrite(LED_BUILTIN, HIGH);          // turn the LED on (HIGH is the voltage level)

    //pinMode(Reset, OUTPUT);
    //digitalWrite(Reset, HIGH);              //Dichiaro il Pin 4 in che condizione deve stare quando non
    //delay(200);                             //viene interessato dal comando di Reset

    // initialize serial for debugging
    Serial.begin(19200);

    scale.set_scale(valoredicalibrazione);	//la scala si setta qui e non nel loop
    //scale.set_scale();			//senza parametro si fa solo per Calibrazione
    scale.tare();

    Serial.println("ACCENSIONE MODULO GSM...");

    //    in questo momento il modem è alimentato direttamente da LM2596
    //    digitalWrite(9,HIGH);
    //    delay(1000);
    //    digitalWrite(9,LOW);
    //    delay(5000);

    Serial.println("VERIFICA SE LA SCHEDA GSM E' CONNESSA.");

    //Inizializzo la connessione impostando il baurate
    if (gsm.begin(2400)) {
        Serial.println("STATO Modulo GSM = CONNESSO");
        started=true;
    }
    else Serial.println("STATO Modulo GSM = NON CONNESS0");
}

void loop() {
    //scale.set_scale(valoredicalibrazione);        //la scala NON si setta qui ma in setup una sola volta

    Serial.print("Peso");
 // Associo alla variabile units che sarà usata anche per SMS
    units = (scale.get_units(5));                     // Peso medio di 5 letture meno il peso della tara
    Serial.print(units, 0);
    Serial.println(" g");

    if(started) {                                     // Check if there is an active connection.
        digitalWrite(LED_BUILTIN, LOW);               // turn the LED off by making the voltage LOW

        char str_peso[5] = "";                                      //char temporanea dove inserire il valore di peso da inviare via SMS
        char messaggio[63] = "";                                    //char completa usata per inviare SMS
        char mittente[20] = "";                                     //reset char che contiene il numero di telefono di SMS ricevuti
        char smsbuffer[100] = "";
        char statSMS = -1;
        char position = sms.IsSMSPresent(SMS_READ);                 //Con questo comando controlla solo gli SMS già letti e li cancella

	strcpy(mittente,"+393473813504");			//numero di telefono abilitato ad inviare comandi

        // Legge se ci sono messaggi disponibili sulla SIM Card
        // e li visualizza sul Serial1 Monitor.
        if (position) {
            Serial.print("SMS Position: ");
            Serial.println(position,DEC);
            sms.GetSMS(position, mittente, smsbuffer, 100);     //Leggo il messaggio SMS e registro il numero del mittente
            Serial.print("SMS ignorato ricevuto da:");
            Serial.println(mittente);
            Serial.print("\r\ncontenuto SMS:");
            Serial.println(smsbuffer);
            Serial.println(" => Trovato messaggio già letto.");
            statSMS = (sms.DeleteSMS(position));      //Cancello dalla SIM i messaggi ricevuti da destinatari sconosciuti o indesiderati
            if (statSMS!=1) Serial.println("ERRORE: SMS non cancellato!");
            else Serial.println("SMS cancellato!");
        }
        position = sms.IsSMSPresent(SMS_UNREAD);               //Con questo comando controlla solo gli SMS non letti
        if (position) {
            Serial.print("SMS Position: ");
            Serial.println(position,DEC);
            sms.GetSMS(position, mittente, smsbuffer, 100);     //Leggo il messaggio SMS e registro il numero del mittente
            Serial.print("Nuovo SMS ricevuto da:");
            Serial.println(mittente);
            Serial.print("\r\ncontenuto SMS:");
            Serial.println(smsbuffer);
            if (strcmp(mittente,destinatario)==0)  {           // Verifico che sia del giusto mittente
                  if (strcmp(smsbuffer,"Reset")==0)  {         //Ricevuto sms "Reset" per resettare il programma
                      Serial.println("...Riavvio...");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il comando appenna letto
                      if (statSMS!=1) Serial.println("ERRORE: SMS non cancellato!");
                      else Serial.println("SMS cancellato!");
                      delay(500);
                      //digitalWrite(Reset, LOW);              //Eseguo il comando di Reset mettendo a massa il Pin 4 con Arduino Mega
                      Riavvia();                               //Comando di software reset con Arduino Uno
                  }
                  if (strcmp(smsbuffer,"Misure")==0)  {        //Ricevuto sms "Misure" per inviare sms con valori
                      dtostrf(units,0,0,str_peso);             // make the float into a char array
                      snprintf(messaggio,63,"Peso=%s g",str_peso);
                      statSMS = (sms.SendSMS(destinatario,messaggio));
                      if (statSMS!=1) Serial.println("ERRORE: SMS non inviato");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il messaggio appenna letto
                      if (statSMS!=1) Serial.println("ERRORE: SMS non cancellato!");
                      else Serial.println("SMS cancellato!");
                      delay(500);
                      Serial.print("SMS inviato da:");
                      Serial.println(mittente);
                      Serial.print("\r\ncontenuto SMS:");
                      Serial.println(messaggio);
                  }
                  else {
                      Serial.println(" => Comando non riconosciuto!");
                      statSMS = (sms.SendSMS(destinatario,"Comando Errato"));  //risposta in caso di messaggio errato
                      if (statSMS!=1) Serial.println("ERRORE: SMS non inviato");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il comando errato appenna ricevuto
                      if (statSMS!=1) Serial.println("ERRORE: SMS non cancellato!");
                      else Serial.println("SMS cancellato!");
                      delay(500);
                  }
            }
            else {
                      Serial.println(" => Mittente non riconosciuto!");
                      statSMS = (sms.DeleteSMS(position));      //Cancello dalla SIM i messaggi ricevuti da destinatari sconosciuti o indesiderati
                      if (statSMS!=1) Serial.println("ERRORE: SMS non cancellato!");
                      else Serial.println("SMS cancellato!");
            }
        }
    }
    scale.power_down();                              // put the ADC in sleep mode
    delay(5000);
    scale.power_up();
};
