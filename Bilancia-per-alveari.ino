#include "HX711.h"
#include "sms.h"
#include <SoftwareSerial.h>

//#define CALIBRAZIONE
#define DEBUG_MODE
#define GSM

#ifdef DEBUG_MODE                              // open Serial via USB-Serial
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINT2(x,y) Serial.print(x,y)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTLNDEC(x,DEC) Serial.println(x,DEC)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINT2(x,y)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTLNDEC(x,DEC)
#endif

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
char destinatario[20];

//const float LOADCELL_OFFSET = 226743; // the offset of the scale, is raw output without any weight, get this first and then do set.scale
const float LOADCELL_DIVIDER = -21.45;  // scala di calibrazione della cella di carico, this is the difference between the raw data of a known weight and an emprty scale

//int pon = 9;                            //dichiaro il Pin per relè
//pinMode(pon, OUTPUT);
//int dtr = 9;                            //dichiaro il Pin dtr per wake-up modem
//pinMode(dtr, OUTPUT);
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

    DEBUG_PRINTLN("calcolo della tara");
    DEBUG_PRINTLN("non mettere nulla sul piatto");
    //scale.set_offset(LOADCELL_OFFSET);
    scale.set_scale(LOADCELL_DIVIDER);
    scale.tare();                                 // reset the scale to 0
    DEBUG_PRINTLN("sistema pronto a pesare");

#ifdef GSM
    strcpy(destinatario,"+393473813504");         //Salva il mio numero nel char destinatario
    DEBUG_PRINTLN("ACCENSIONE MODULO GSM...");

    //    in questo momento il modem è alimentato direttamente da LM2596 e non viene usato un relè
    //rely_Off();
    //rely_On();

    DEBUG_PRINTLN("VERIFICA SE LA SCHEDA GSM E' CONNESSA.");

    //Inizializzo la connessione impostando il baurate
    if (gsm.begin(2400)) {
        DEBUG_PRINTLN("STATO Modulo GSM = CONNESSO");
        started=true;
    }
    else DEBUG_PRINTLN("STATO Modulo GSM = NON CONNESS0");
#endif
};

void loop() {
    DEBUG_PRINTLN("\r\nNuova lettura..");
    DEBUG_PRINT("\tData e Ora:\t\t\t\t\t\t");
    gsm_getclock();
    vcc = readVcc();
    DEBUG_PRINT("\tLettura VCC:\t\t\t\t\t\t");
    DEBUG_PRINT(vcc);
    DEBUG_PRINTLN(" mV");
    DEBUG_PRINT("\toffset:\t\t\t\t\t\t\t");
    DEBUG_PRINT(scale.get_offset());
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata media grezza con tara:\t\t\t\t");
    DEBUG_PRINT(scale.read_average(5));                    // Peso medio grezzo di 5 letture
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata singola grezza senza tara:\t\t\t");
    DEBUG_PRINT(scale.get_value());                        // Peso singolo grezzo, returns (read_average() - LOADCELL_OFFSET), that is the current value without the tare weight
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata media senza tara:\t\t\t\t");
 // Associo alla variabile units che sarà usata anche per SMS
    units = (scale.get_units(5));                          // Peso medio di 5 letture meno il peso della tara
    DEBUG_PRINT2(units, 0);
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata media su 5 letture senza tara diviso scala di calibrazione:");
    DEBUG_PRINT2(scale.get_units(5), 1);                   // la media di 5 letture meno il peso della tara, diviso per il valore della scala di calibrazione settata con set_scale, returns get_value() divided by LOADCELL_DIVIDER
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata media su 10 letture senza tara diviso scala di calibrazione:");
    DEBUG_PRINT2(scale.get_units(10), 1);                  // la media di 10 letture meno il peso della tara, diviso per il valore della scala di calibrazione settata con set_scale
    DEBUG_PRINTLN(" g");
    DEBUG_PRINT("\tPesata media su 20 letture senza tara diviso scala di calibrazione:");
    DEBUG_PRINT2(scale.get_units(20), 3);                  // la media di 20 letture meno il peso della tara, diviso per il valore della scala di calibrazione settata con set_scale
    DEBUG_PRINTLN(" g");
#ifdef GSM
    if(started) {                                     // Check if there is an active connection.
        digitalWrite(LED_BUILTIN, LOW);               // turn the LED off by making the voltage LOW

        char str_peso[5] = "";                                      //char temporanea dove inserire il valore di peso da inviare via SMS
        char messaggio[63] = "";                                    //char completa usata per inviare SMS
        char mittente[20] = "";                                     //reset char che contiene il numero di telefono di SMS ricevuti
        char smsbuffer[100] = "";
        char statSMS = -1;
        char position = sms.IsSMSPresent(SMS_READ);                 //Con questo comando controlla solo gli SMS già letti e li cancella
	strcpy(mittente,"+393473813504");                       //numero di telefono abilitato ad inviare comandi
        if (position) {
            DEBUG_PRINT("SMS Position: ");
            DEBUG_PRINTLNDEC(position,DEC);
            sms.GetSMS(position, mittente, smsbuffer, 100);     //Leggo il messaggio SMS e registro il numero del mittente
            DEBUG_PRINT("SMS ignorato ricevuto da:");
            DEBUG_PRINTLN(mittente);
            DEBUG_PRINT("\r\ncontenuto SMS:");
            DEBUG_PRINTLN(smsbuffer);
            DEBUG_PRINTLN(" => Trovato messaggio già letto.");
            statSMS = (sms.DeleteSMS(position));      //Cancello dalla SIM i messaggi ricevuti da destinatari sconosciuti o indesiderati
            if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non cancellato!");
            else DEBUG_PRINTLN("SMS cancellato!");
        }
        position = sms.IsSMSPresent(SMS_UNREAD);               //Con questo comando controlla solo gli SMS non letti
        if (position) {
            DEBUG_PRINT("SMS Position: ");
            DEBUG_PRINTLNDEC(position,DEC);
            sms.GetSMS(position, mittente, smsbuffer, 100);     //Leggo il messaggio SMS e registro il numero del mittente
            DEBUG_PRINT("Nuovo SMS ricevuto da:");
            DEBUG_PRINTLN(mittente);
            DEBUG_PRINT("\r\ncontenuto SMS:");
            DEBUG_PRINTLN(smsbuffer);
            if (strcmp(mittente,destinatario)==0)  {           // Verifico che sia del giusto mittente
                  if (strcmp(smsbuffer,"Reset")==0)  {         //Ricevuto sms "Reset" per resettare il programma
                      DEBUG_PRINTLN("...Riavvio...");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il comando appenna letto
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non cancellato!");
                      else DEBUG_PRINTLN("SMS cancellato!");
                      delay(500);
                      //digitalWrite(Reset, LOW);              //Eseguo il comando di Reset mettendo a massa il Pin 4 con Arduino Mega
                      Riavvia();                               //Comando di software reset con Arduino Uno
                  }
                  if (strcmp(smsbuffer,"Misure")==0)  {        //Ricevuto sms "Misure" per inviare sms con valori
                      dtostrf(units,0,0,str_peso);             // make the float into a char array
                      snprintf(messaggio,63,"VCC=%d mV | Peso medio=%s g",vcc, str_peso);
                      statSMS = (sms.SendSMS(destinatario,messaggio));
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non inviato");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il messaggio appenna letto
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non cancellato!");
                      else DEBUG_PRINTLN("SMS cancellato!");
                      delay(500);
                      DEBUG_PRINT("SMS inviato da:");
                      DEBUG_PRINTLN(mittente);
                      DEBUG_PRINT("\r\ncontenuto SMS:");
                      DEBUG_PRINTLN(messaggio);
                  }
                  else {
                      DEBUG_PRINTLN(" => Comando non riconosciuto!");
                      statSMS = (sms.SendSMS(destinatario,"Comando Errato"));  //risposta in caso di messaggio errato
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non inviato");
                      statSMS = (sms.DeleteSMS(position));     //Cancello dalla SIM il comando errato appenna ricevuto
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non cancellato!");
                      else DEBUG_PRINTLN("SMS cancellato!");
                      delay(500);
                  }
            }
            else {
                      DEBUG_PRINTLN(" => Mittente non riconosciuto!");
                      statSMS = (sms.DeleteSMS(position));      //Cancello dalla SIM i messaggi ricevuti da destinatari sconosciuti o indesiderati
                      if (statSMS!=1) DEBUG_PRINTLN("ERRORE: SMS non cancellato!");
                      else DEBUG_PRINTLN("SMS cancellato!");
            }
        }
    }
#endif
    //rely_Off();
    gsm_SoftSleepModeOn();
    //gsm_DeepSleepModeOn();
    scale.power_down();                              // put the ADC in sleep mode
    delay(5000);                                     // dormo 5 secondi
    //delay(60*5000);                                  // dormo 5 minuti
    //rely_On();
    gsm_SoftSleepModeOff();
    //gsm_DeepSleepModeOff();
    scale.power_up();
};

void gsm_getclock() {
#ifdef GSM
  String mystr="";
  int i = 0;
  gsm.SimpleWriteln("AT+CCLK?");
  delay(1000);
  gsm.WhileSimpleReadStr(mystr);
  int x = mystr.indexOf(String('"')) + 1;
  int y = mystr.lastIndexOf(String('"'));
  String datetime = mystr.substring(x, y);
  DEBUG_PRINTLN(datetime);
  delay(500);
#endif
}

void gsm_SoftSleepModeOn() {
#ifdef GSM
  gsm.SimpleWriteln("AT+CSCLK=2");
  delay(1000);
#endif
}

void gsm_SoftSleepModeOff() {
#ifdef GSM
  gsm.SimpleWriteln("AT+CSCLK=0");
  delay(1000);
#endif
}

//void gsm_DeepSleepModeOn() {
//#ifdef GSM
//  gsm.SimpleWriteln("AT+CFUN=0");
//  delay(1000);
//  gsm.SimpleWriteln("AT+CSCLK=1");
//  delay(1000);
//#endif
//}

//void gsm_DeepSleepModeOff() {
//#ifdef GSM
//  digitalWrite(dtr,LOW);
//  delay(5000);
//  gsm.SimpleWriteln("AT+CFUN=1");
//  delay(1000);
//#endif
//}

//void rely_On() {
//   digitalWrite(pon,HIGH);
//   delay(2000);
//}

//void rely_Off() {
//   digitalWrite(pon,LOW);
//   delay(5000);
//}

void calibrazione() {
  //1 Call set_scale() with no parameter.
  //2 Call tare() with no parameter.
  //3 Place a known weight on the scale and call get_units(10).
  //4 Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale().
  //5 Adjust the parameter in step 4 until you get an accurate reading.

  scale.set_scale();
  scale.tare();
  DEBUG_PRINTLN("Hai 5 secondi per posizionare 1kg di peso");
  delay(5000);
  long a = scale.get_units(10);          //stampa media dopo 10 letture
  long b = 1000;                         // 1kg
  double c = (double)(a/b);
  DEBUG_PRINT("media delle 10 letture: ");
  DEBUG_PRINTLN(a);
  DEBUG_PRINT("il peso noto: ");
  DEBUG_PRINTLN(b);
  DEBUG_PRINT("il valore da passare a set_scale è: ");
  DEBUG_PRINTLN(c);
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
