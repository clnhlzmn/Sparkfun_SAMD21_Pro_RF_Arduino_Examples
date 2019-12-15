#include <TinyGPS++.h>

/*
  Both the TX and RX ProRF boards will need a wire antenna. We recommend a 3" piece of wire.
  This example is a modified version of the example provided by the Radio Head
  Library which can be found here:
  www.github.com/PaulStoffregen/RadioHeadd
*/

#include <SPI.h>

//Radio Head Library:
#include <RH_RF95.h> 

//nmea parser
TinyGPSPlus gps;

// We need to provide the RFM95 module's chip select and interrupt pins to the
// rf95 instance below.On the SparkFun ProRF those pins are 12 and 6 respectively.
RH_RF95 rf95(12, 6);

int LED = 13; //Status LED is on pin 13
int GPSEN = 2; //gps en pin 2

int packetCounter = 0; //Counts the number of packets sent
long timeSinceLastPacket = 0; //Tracks the time stamp of last packet received

// The broadcast frequency is set to 921.2, but the SADM21 ProRf operates
// anywhere in the range of 902-928MHz in the Americas.
// Europe operates in the frequencies 863-870, center frequency at 868MHz.
// This works but it is unknown how well the radio configures to this frequency:
//float frequency = 864.1; 
float frequency = 921.2; //Broadcast frequency

void setup()
{
  pinMode(LED, OUTPUT);
  pinMode(GPSEN, OUTPUT);
  digitalWrite(GPSEN, HIGH); //disable

  SerialUSB.begin(9600);
  // It may be difficult to read serial messages on startup. The following line
  // will wait for serial to be ready before continuing. Comment out if not needed.
  while(!SerialUSB); 
  SerialUSB.println("RFM Client!"); 

  //Initialize the Radio.
  if (rf95.init() == false){
    SerialUSB.println("Radio Init Failed - Freezing");
    while (1);
  }
  else{
    //An LED inidicator to let us know radio initialization has completed. 
    SerialUSB.println("Transmitter up!"); 
    digitalWrite(LED, HIGH);
    delay(500);
    digitalWrite(LED, LOW);
    delay(500);
  }

  // Set frequency
  rf95.setFrequency(frequency);

   // The default transmitter power is 13dBm, using PA_BOOST.
   // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
   // you can set transmitter powers from 5 to 23 dBm:
   // Transmitter power can range from 14-20dbm.
   rf95.setTxPower(14, false);

  //gps
  Serial1.begin(9600);
}

enum State {
  SLEEP,
  WAIT,
  FIX,
};

State state = SLEEP;

unsigned long time_last = millis();

void loop()
{

  unsigned long time_now = millis();
  bool new_message = false;

  String message;
  
  switch (state) {
  case SLEEP:
    if (time_now - time_last >= 5000) {
      time_last = time_now;
      state = WAIT;
      digitalWrite(GPSEN, LOW); //enable gps
      SerialUSB.println("gps enabled");
    }
    break;
  case WAIT:
    if (time_now - time_last >= 30000) {
      time_last = time_now;
      state = SLEEP;
      digitalWrite(GPSEN, HIGH);
      SerialUSB.println("gps disabled, couldn't get fix");
      message = "couldn't get fix";
      new_message = true;
      break;
    }
    while (Serial1.available() > 0) {
      char data = Serial1.read();
      if (gps.encode(data)) {
        if (gps.location.isValid()) {
          time_last = time_now;
          state = FIX;
          SerialUSB.println("got fix");
          break;
        }
      }
    }
    break;
  case FIX:
    while (Serial1.available() > 0) {
      char data = Serial1.read();
      if (gps.encode(data)) {
        time_last = time_now;
        state = SLEEP;
        digitalWrite(GPSEN, HIGH);
        SerialUSB.println("got data, gps disabled");
        message = String(gps.date.value()) + "," + 
                  String(gps.time.value()) + "," + 
                  String(gps.location.lat(), 6) + "," + 
                  String(gps.location.lng(), 6);
        new_message = true;
      }
    }
    break;
  }

  if (new_message) {  
    //Send a message to the other radio
    rf95.send((uint8_t*)message.c_str(), message.length());
    rf95.waitPacketSent();
  }
}
