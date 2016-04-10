/*
   This is an example for the Si4713 FM Radio Transmitter with RDS

   Designed to work with the Si4713 in the adafruit shop

   These transmitters use I2C to communicate, plus reset pin.
   3 pins are required to interface

   Written by Richard Ciampa, Brandan Lockwood.
*/

#include <Wire.h>
#include <Adafruit_Si4713.h>
#include <SPI.h>
#include <WiFi101.h>

#define RESET_PIN 9               //Was pin 12 but WiFi101 shield uses 12
#define _BV(bit) (1 << (bit))
#define INIT_FMSTATION 10230      // 10230 == 102.30 MHz
#define PROP_TX_RDS_PI 0x269F     // RDS program ID KIOT
#define FM_FREQ_LOW 8750          // 87.50 MHZ
#define FM_FREQ_HIGH 10800        // 108.00 MHZ
#define FM_FREQ_STEP 10           // 10 MHZ step

int fm_chan;
Adafruit_Si4713 radio = Adafruit_Si4713(RESET_PIN);
char ssid[] = "IoTCSUMB";                //Network SSID (name)
char pass[] = "CST395SP";                //Network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;
char server[] = "hosting.otterlabs.org"; //FQDN for otterlabs.org (using DNS)
WiFiClient client;                       // Initialize client (port 80 default HTTP):
unsigned long lastConnectionTime = 0;    // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 6L * 60000L; // delay between updates, in milliseconds


void setup() {
  Serial.begin(9600);
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  fm_chan = INIT_FMSTATION;
  radioSetup();
  radioChannelScan();
}

void loop() {
  httpClientRead();
  radioStatus();
  // if ten seconds have passed since your last connection,
  // then connect again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    httpRequest();
  }
}

void radioSetup() {
  Serial.println("\n------------ FM TX Setup ------------\nCST395 IoT - Micro FM Transmitter");
  Serial.print("New station: ");
  Serial.println(fm_chan);

  radio = Adafruit_Si4713(RESET_PIN);
  if (! radio.begin()) {  // begin with address 0x63 (CS high default)
    Serial.println("Couldn't find radio?");
    while (1);
  }

  Serial.print("\nSet TX power");
  radio.setTXpower(115);  // dBuV, 88-115 max
  Serial.print("\nTuning into ");
  Serial.print(fm_chan / 100);
  Serial.print('.');
  Serial.println(fm_chan % 100);
  radio.tuneFM(fm_chan); // mhz

  // This will tell you the status in case you want to read it from the chip
  radio.readTuneStatus();
  Serial.print("\tCurr freq: ");
  Serial.println(radio.currFreq);
  Serial.print("\tCurr freq dBuV:");
  Serial.println(radio.currdBuV);
  Serial.print("\tCurr ANT cap:");
  Serial.println(radio.currAntCap);

  // begin the RDS/RDBS transmission
  radio.beginRDS(PROP_TX_RDS_PI);
  radio.setRDSstation("CST IoT!");
  radio.setRDSbuffer( "Welcome to IoT!");
  Serial.println("RDS on!\n-----------------------------------\n");
  radio.setGPIOctrl(_BV(1) | _BV(2));  // set GP1 and GP2 to output
}

void radioChannelScan() {
  Serial.println("\n--------------- FM Rx Scan ---------------");
  //Scan power of entire range from 87.5 to 108.0 MHz
  for (uint16_t f  = FM_FREQ_LOW; f < FM_FREQ_HIGH; f += FM_FREQ_STEP) {
    radio.readTuneMeasure(f);
    Serial.print("Measuring: "); Serial.print(f); Serial.print("...");
    radio.readTuneStatus();
    Serial.println(radio.currNoiseLevel);
  }
  Serial.println("------------------------------------------");
}

void radioStatus() {
  radio.readASQ();
  Serial.print("\tCurr ASQ: 0x");
  Serial.println(radio.currASQ, HEX);
  Serial.print("\tAudio Input Level: ");
  Serial.println(radio.currInLevel);
  // toggle GPO1 and GPO2
  radio.setGPIO(_BV(1));
  delay(500);
  radio.setGPIO(_BV(2));
  delay(500);
}

void httpRequest() {
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();

  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("\n---------------------------------\nConnecting...");
    // send the HTTP PUT request:
    Serial.println("connecting send the HTTP PUT request:...");
    client.println("GET /lockwoodbrandane/public_html/fm/micro-fm-webservice/mfm-service.php?method=getFreq&transID=l234u832u48932u4&q=1000&format=html HTTP/1.1");
    client.println("Host: hosting.otterlabs.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
    
    Serial.println("----------- Response ------------");
    Serial.print("Loading buffer: ");
    while (!client.available());
    Serial.print(client.available());
    Serial.println(" length");

    //time connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    Serial.println();
    Serial.println("disconnecting from server.");
    client.stop();
  }
}

void httpClientRead() {
  char c[5] = "", h;
  uint8_t i = 0;
  bool isFreq = false;
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  Serial.print("Char length: ");
  Serial.println(client.available());
  while (client.available()) {
    if (isFreq)
    {
      c[i] = client.read();
      Serial.print(c[i]);
      i++;
    } else {
      h = client.read();
      Serial.print(h);
    }
    if (h == '#') {
      isFreq = true;
    }
  }
  if (isFreq) {
    if (!(fm_chan == atoi(c))) {
      fm_chan = atoi(c);
      radioSetup();
    } else {
      Serial.println("\n--------------------------------------");
    }
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
