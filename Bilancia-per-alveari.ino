#include "HX711.h"
#include "sms.h"
#include <SoftwareSerial.h>

//#define DEBUG_MODE
#define GSM

#ifdef GSM
GSM GSM;
SMSGSM sms;
#endif

const int LOADCELL_DOUT_PIN = A1;
const int LOADCELL_SCK_PIN = A0;
HX711 scale;

char Misure[20];
boolean started=false;
char smsbuffer[10];
char Mittente[20];

const float LOADCELL_DIVIDER = -21.45;  // calibrazione della cella di carico
//const float LOADCELL_DIVIDER = 5895655;

float units;
float units_sms;
char peso[5];                           //char dove inserire il valore di peso da inviare via SMS

//int Reset = 4;                          //dichiaro il Pin per dare l'impulso di reset quando richiesto
void(* Riavvia)(void) = 0;              //Comando per Software Reset solo con Auduino Uno

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);             // initialize digital pin LED_BUILTIN as an output.
    digitalWrite(LED_BUILTIN, HIGH);          // turn the LED on (HIGH is the voltage level)

    //pinMode(Reset, OUTPUT);
    //digitalWrite(Reset, HIGH);              //Dichiaro il Pin 4 in che condizione deve stare quando non
    //delay(200);                             //viene interessato dal comando di Reset

    // initialize serial for debugging
    Serial.begin(19200);

    // init bilancia
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

#ifdef DEBUG_MODE
    Serial.println("Before setting up the scale:");
    Serial.print("read: \t\t");
    Serial.println(scale.read());                 // print a raw reading from the ADC

    Serial.print("read average: \t\t");
    Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

    Serial.print("get value: \t\t");
    Serial.println(scale.get_value(5));           // print the average of 5 readings from the ADC minus the tare weight (not set yet)

    Serial.print("get units: \t\t");
    Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight (not set) divided
                                                  // by the SCALE parameter (not set yet)
#endif

    scale.set_scale(LOADCELL_DIVIDER);
    //loadcell.set_offset(LOADCELL_OFFSET);
    scale.tare();                                 // reset the scale to 0

#ifdef DEBUG_MODE
    Serial.println("After setting up the scale:");

    Serial.print("read: \t\t");
    Serial.println(scale.read());                 // print a raw reading from the ADC

    Serial.print("read average: \t\t");
    Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

    Serial.print("get value: \t\t");
    Serial.println(scale.get_value(5));           // print the average of 5 readings from the ADC minus the tare weight, set with tare()

    Serial.print("get units: \t\t");
    Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided
                                                  // by the SCALE parameter set with set_scale
#endif
#ifdef GSM
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
#endif
    Serial.println("Inizio letture:");
};

void loop() {
    Serial.print("Nuova lettura:\t");
    Serial.print(scale.get_units(), 1);
    Serial.print("\t| media:\t");
    Serial.println(scale.get_units(10), 1);          // Faccio una media di 10 letture meno il peso della tara
 
    units = scale.get_units(10);                     // Faccio una media di 10 letture e le associo alla voce "units"

    Serial.print("Peso:");
    Serial.print(units, 0);
    Serial.println(" g ");
#ifdef GSM
    if(started) {                                    // Check if there is an active connection.
        digitalWrite(LED_BUILTIN, LOW);              // turn the LED off by making the voltage LOW
        char position;
        strcpy(Mittente,"3473813504");

        // Legge se ci sono messaggi disponibili sulla SIM Card
        // e li visualizza sul Serial1 Monitor.
        position = sms.IsSMSPresent(SMS_UNREAD);         // Con questo comando esegue solo gli Sms non letti
        if (position) {
            // Leggo il messaggio SMS e stabilisco chi sia il mittente
            sms.GetSMS(position, Mittente, smsbuffer, 10);
            Serial.println("Messaggio inviato da:");
            Serial.println(Mittente);
            Serial.println("\r\n SMS testo:");
            Serial.print(smsbuffer);

            if (strcmp(smsbuffer,"Reset")==0)  {         //Invio sms "Reset" per resettare il programma
                Serial.println("...Riavvio...");
                sms.DeleteSMS(position);                 //Cancello dalla SIM il comando appenna letto
                //digitalWrite(Reset, LOW);              //Eseguo il comando di Reset mettendo a massa il Pin 4
                delay(500);
                Riavvia();                               //Comando si software reset con Arduino Uno
            }
            if (strcmp(smsbuffer,"Misure")==0)  {        //Invio sms "Misure" per ricevere sms con valori
                units_sms = scale.get_units(5);
                dtostrf(units_sms,0,0,peso);
                snprintf(Misure,20,"Peso=%sg",peso);
                sms.SendSMS(Mittente,Misure);
                sms.DeleteSMS(position);                 //Cancello dalla SIM il comando appenna letto
                delay(500);
                Serial.print(Misure);
            }
            else {
                Serial.println(" => non riconosciuto!");
                sms.SendSMS(Mittente,"Comando Errato");  //risposta in caso di messaggio errato
                sms.DeleteSMS(position);                 //Cancello dalla SIM il comando appenna letto
                delay(500);
                //lcd.clear();
                //lcd.init();
            }
            sms.DeleteSMS(position);                     //Cancello dalla SIM i messaggi appena letti ed eseguiti
        }
    }
#endif
    scale.power_down();                              // put the ADC in sleep mode
    delay(5000);
    scale.power_up();
};
