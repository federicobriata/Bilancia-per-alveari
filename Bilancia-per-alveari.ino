#include "HX711.h"
#include "sms.h"
#include <SoftwareSerial.h>

//#define CALIBRAZIONE
//#define DEBUG_MODE
#define GSM

#ifdef GSM
GSM GSM;
SMSGSM sms;
#endif

const int LOADCELL_DOUT_PIN = A1;
const int LOADCELL_SCK_PIN = A0;
HX711 scale;

int vcc;
boolean started=false;
float units;                                     //float dove inserire il valore di peso
char position;
char smsbuffer[10];
char mittente[20];
char str_peso[5];                        //char temporanea dove inserire il valore di peso da inviare via SMS
char messaggio[63];                           //char dove costruire il testo da inviare via SMS

//const float LOADCELL_OFFSET = 226743; // the offset of the scale, is raw output without any weight, get this first and then do set.scale
const float LOADCELL_DIVIDER = -21.45;  // scala di calibrazione della cella di carico, this is the difference between the raw data of a known weight and an emprty scale

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

#ifdef CALIBRAZIONE
    calibrazione();
#endif

#ifdef DEBUG_MODE
    Serial.println("Before setting up the scale:");
    Serial.print("read: \t\t");
    Serial.println(scale.read());                 // print a raw reading from the ADC

    Serial.print("read average: \t\t");
    Serial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

    Serial.print("get value: \t\t");
    Serial.println(scale.get_value(5));           // print the average of 5 readings from the ADC minus the tare weight (not set yet)

    Serial.print("get units: \t\t");
    Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight (not set) divided by the SCALE parameter (not set yet)
#endif

    //loadcell.set_offset(LOADCELL_OFFSET);
    scale.set_scale(LOADCELL_DIVIDER);
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
    Serial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided  by the SCALE parameter set with set_scale
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
    vcc = readVcc();
    Serial.print("Lettura VCC:\t\t");
    Serial.print(vcc);
    Serial.println(" mV");
    Serial.println("Pesata media grezza:\t");
    Serial.print(scale.read_average(5));	            // Peso medio grezzo di 5 letture
    Serial.println(" g");
    Serial.println("Pesata grezza singola:\t\t");
    Serial.print(scale.get_value());                  // Peso grezzo sungolo, returns (read_average() - LOADCELL_OFFSET), that is the current value without the tare weight
    Serial.println(" g");
    Serial.println("Pesata media senza tara:\t\t");
 // Associo alla variabile units che sarà usata anche per SMS
    units = (scale.get_units(5));                     // Peso medio di 5 letture meno il peso della tara"
    Serial.print(units, 0);
    Serial.println(" g");
    Serial.println("Pesata media senza tara diviso scala di calibrazione:\t");
    Serial.print(scale.get_units(5), 1);              // returns get_value() divided by LOADCELL_DIVIDER la media di 5 letture meno il peso della tara, diviso per il valore della scala di calibrazione settata con set_scale"
    Serial.println(" g");

#ifdef GSM
    if(started) {                                     // Check if there is an active connection.
        digitalWrite(LED_BUILTIN, LOW);               // turn the LED off by making the voltage LOW
	      str_peso[5] = "";                             //char temporanea dove inserire il valore di peso da inviare via SMS
        messaggio[63] = "";
        strcpy(mittente,"3473813504");

        // Legge se ci sono messaggi disponibili sulla SIM Card
        // e li visualizza sul Serial1 Monitor.
        position = sms.IsSMSPresent(SMS_UNREAD);         // Con questo comando esegue solo gli Sms non letti
        if (position) {
            // Leggo il messaggio SMS e stabilisco chi sia il mittente
            sms.GetSMS(position, mittente, smsbuffer, 10);
            Serial.println("Messaggio inviato da:");
            Serial.println(mittente);
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
                dtostrf(units,0,0,str_peso);             // make the float into a char array
                snprintf(messaggio,63,"VCC=%d mV | Peso=%s g",vcc, str_peso);
                sms.SendSMS(mittente,messaggio);
                sms.DeleteSMS(position);                 //Cancello dalla SIM il comando appenna letto
                delay(500);
                Serial.print(messaggio);
            }
            else {
                Serial.println(" => non riconosciuto!");
                sms.SendSMS(mittente,"Comando Errato");  //risposta in caso di messaggio errato
                sms.DeleteSMS(position);                 //Cancello dalla SIM il comando appenna letto
                delay(500);
            }
            sms.DeleteSMS(position);                     //Cancello dalla SIM i messaggi appena letti ed eseguiti
        }
    }
#endif
    scale.power_down();                              // put the ADC in sleep mode
    delay(5000);
    scale.power_up();
};

void calibrazione() {
  //1 Call set_scale() with no parameter.
  //2 Call tare() with no parameter.
  //3 Place a known weight on the scale and call get_units(10).
  //4 Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale().
  //5 Adjust the parameter in step 4 until you get an accurate reading.

  scale.set_scale();
  scale.tare();
  Serial.println("Hai 5 secondi per posizionare 2kg di peso");
  delay(5000);
  long x = scale.get_units(10);          //stampa media dopo 20 letture
  long y = 2000;                        // i 2kg
  double m = (double)(x/y);
  Serial.print("media delle 10 letture: ");
  Serial.println(x);
  Serial.print("il peso noto: ");
  Serial.println(y);
  Serial.print("il valore da passare a set_scale è:");
  Serial.println(m);
}

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
     ADMUX = _BV(MUX5) | _BV(MUX0) ;
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
