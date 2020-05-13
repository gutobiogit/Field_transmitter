// Bibliotecas ***************************************************************
#include <XBee.h>
#include <Wire.h>
#include <dht11.h>
#include <avr/sleep.h>

// Variaveis *****************************************************************

XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x4061371f);// 64-bit addressing: This is the SH + SL address of remote XBee
uint8_t payload[] = {'0','0','0',',','0','0',',','0','0',',','0','0'};
//umidade 000 , temperatura 00 , orvalho 00 , umidade 00



// Constates *****************************************************************

#define UMIDADEPIN A0  
#define DHT11PIN 8// pino leitor temp/humidade
dht11 DHT11;

int wakePin = 2;                 // pin used for waking up

int estado = 0;//estado DHT11 funcional
int temperatura = 0;//temperatura
int umidade = 0;//humidade
float orvalho = 0.0;//orvalho
int umidadesolo = 0;
int umidadesolores = 0;

String temperaturacon;
String umidadecon;
String orvalhocon;
String umidadesolocon;

XBee xbee = XBee();//variavel xbee
Tx64Request tx = Tx64Request(addr64, payload, sizeof(payload));//Xbee
TxStatusResponse txStatus = TxStatusResponse();//Xbee
Rx16Response rx16 = Rx16Response();
Rx64Response rx64 = Rx64Response();






// Funcoes ******************************************************************  

//***************************************************************************
int UmiSolo()

{
    umidadesolores = analogRead(UMIDADEPIN);
    umidadesolo = map(umidadesolores, 1023, 0, 0, 100);
    return(umidadesolo);

}


double dewPoint(double celsius, double humidity) // Calcula temp Condensacao
{
	// (1) Saturation Vapor Pressure = ESGG(T)
	double RATIO = 373.15 / (273.15 + celsius);
	double RHS = -7.90298 * (RATIO - 1);
	RHS += 5.02808 * log10(RATIO);
	RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
	RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
	RHS += log10(1013.246);

    // factor -3 is to adjust units - Vapor Pressure SVP * humidity
	double VP = pow(10, RHS - 3) * humidity;

    // (2) DEWPOINT = F(Vapor Pressure)
	double T = log(VP/0.61078);   // temp var
	return (241.88 * T) / (17.558 - T);
}
 //***************************************************************************
double temphum() // Dados temperatura e humidade e orvalho
{

      // Leitura temperatura
   int chk = DHT11.read(DHT11PIN);

   switch (chk)
   {
       case DHTLIB_OK: 
		estado=0; 
       case DHTLIB_ERROR_CHECKSUM: 
		estado=1;
       case DHTLIB_ERROR_TIMEOUT: 
		estado=2;
       default: 
		estado=3;
}

 
umidade=(DHT11.humidity);
temperatura=(DHT11.temperature);
orvalho=(dewPoint(DHT11.temperature, DHT11.humidity));
estado=estado;  
return(umidade,temperatura,orvalho,estado);  

}

//****************************************************************************************
  void transmitedados()//transmite dados via xbee
    {
    
  payload[0] = umidadecon[0];
  payload[1] = umidadecon[1];
  payload[2] = umidadecon[2];


  payload[4] = temperaturacon[0];
  payload[5] = temperaturacon[1];

  payload[7] = orvalhocon[0]; 
  payload[8] = orvalhocon[1];
  
 
  payload[10] = umidadesolocon[0];
  payload[11] = umidadesolocon[1];
  
  xbee.send(tx);
  if (xbee.readPacket(5000)) {
        // got a response!

        // should be a znet tx status            	
    	if (xbee.getResponse().getApiId() == TX_STATUS_RESPONSE) {
    	   xbee.getResponse().getZBTxStatusResponse(txStatus);
    		
    	   // get the delivery status, the fifth byte
           if (txStatus.getStatus() == SUCCESS) {
            	// success.  time to celebrate
           } else {
            	// the remote XBee did not receive our packet. is it powered on?
             	xbee.send(tx);
           }
        }      
    } else {
      // local XBee did not provide a timely TX Status Response -- should not happen
                xbee.send(tx);
    }
    
    
    } 

//**************************************************************************
void wakeUpNow()
{
 digitalWrite(12, HIGH);
 digitalWrite(10, HIGH); 
}



//**************************************************************************
void sleepNow()         // here we put the arduino to sleep
{
    /* Now is the time to set the sleep mode. In the Atmega8 datasheet
     * http://www.atmel.com/dyn/resources/prod_documents/doc2486.pdf on page 35
     * there is a list of sleep modes which explains which clocks and 
     * wake up sources are available in which sleep mode.
     *
     * In the avr/sleep.h file, the call names of these sleep modes are to be found:
     *
     * The 5 different modes are:
     *     SLEEP_MODE_IDLE         -the least power savings 
     *     SLEEP_MODE_ADC
     *     SLEEP_MODE_PWR_SAVE
     *     SLEEP_MODE_STANDBY
     *     SLEEP_MODE_PWR_DOWN     -the most power savings
     *
     * For now, we want as much power savings as possible, so we 
     * choose the according 
     * sleep mode: SLEEP_MODE_PWR_DOWN
     * 
     */  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here

    sleep_enable();          // enables the sleep bit in the mcucr register
                             // so sleep is possible. just a safety pin 

    /* Now it is time to enable an interrupt. We do it here so an 
     * accidentally pushed interrupt button doesn't interrupt 
     * our running program. if you want to be able to run 
     * interrupt code besides the sleep function, place it in 
     * setup() for example.
     * 
     * In the function call attachInterrupt(A, B, C)
     * A   can be either 0 or 1 for interrupts on pin 2 or 3.   
     * 
     * B   Name of a function you want to execute at interrupt for A.
     *
     * C   Trigger mode of the interrupt pin. can be:
     *             LOW        a low level triggers
     *             CHANGE     a change in level triggers
     *             RISING     a rising edge of a level triggers
     *             FALLING    a falling edge of a level triggers
     *
     * In all but the IDLE sleep modes only LOW can be used.
     */

    attachInterrupt(0,wakeUpNow, LOW); // use interrupt 0 (pin 2) and run function
                                       // wakeUpNow when pin 2 gets LOW 

    sleep_mode();            // here the device is actually put to sleep!!
                             // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP

    sleep_disable();         // first thing after waking from sleep:
                             // disable sleep...
                             
    detachInterrupt(0);      // disables interrupt 0 on pin 2 so the 
                             // wakeUpNow code will not be executed 
                             // during normal running time.


}




void setup () {
Serial.begin(9600);
xbee.setSerial(Serial);
pinMode(wakePin, INPUT);
Wire.begin();
attachInterrupt(0,wakeUpNow, LOW); // use interrupt 0 (pin 2) and run function
                                       // wakeUpNow when pin 2 gets LOW 
 
}



void loop () {
delay(300);  
sleepNow();
delay(300);
temphum();
UmiSolo();
  
umidadecon=String(umidade);  
temperaturacon=String(temperatura);
orvalhocon=String(int(orvalho));
umidadesolocon=String(umidadesolo);

transmitedados();
digitalWrite(12, LOW);
digitalWrite(10, LOW);
  
  
  





}
