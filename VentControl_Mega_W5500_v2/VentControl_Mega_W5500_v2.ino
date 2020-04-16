// Libraries

//#include <DueFlashStorage.h>
//#include <efc.h>
//#include <flash_efc.h>


#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet2.h>
#include "DHT.h"

#define ethernet_h_

#define DHTPIN 45     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

float nCellarHumidity = 0;
float nCellarTemperature = 0;

#define DEBUG_MODE 1

#include <aREST.h>

// Defines the time for the watchdog in ms, max 15000
int watchdogTime = 15000;

// this function has to be present, otherwise watchdog won't work
void watchdogSetup(void) 
{
// do what you want here
}
  

enum VENT_SPEED
{
  VENT_OFF = 0,
  VENT_MIN,
  VENT_MAX
};

#define VENT_ONOFF_PIN 31
#define VENT_SPEED_PIN 33

#define W5500_RESET_PIN 30

#define VENT_TEMP_OFF 22
#define VENT_TEMP_MAX 25

#define VENT_CHANGE_INTERVAL 600000
#define TEMP_RECEIVE_TIMEOUT 300000

// Enter a MAC address for your controller below.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xFE, 0x41 };

// IP address in case DHCP fails
IPAddress ip(192,168,88,172);

// Ethernet server
EthernetServer server(80);

// Create aREST instance
aREST rest = aREST();

//DueFlashStorage dueFlashStorage;

// Variables to be exposed to the API
int temperature;
byte tempoff;
byte tempmax;

int vSpeed=VENT_OFF;
unsigned long int lastVentStateChange = millis();

unsigned long int lastTempOutput = millis();

unsigned long int lastTempReceived = millis();

unsigned long int lastCellarTempRead = millis();

void restartEthernet()
{
  digitalWrite(W5500_RESET_PIN, LOW);
  delay(500);
  digitalWrite(W5500_RESET_PIN, HIGH);
    // Start the Ethernet connection and the server
  Ethernet.begin(mac,ip);
  
  server.begin();
  Serial.print("Restart server at ");
  Serial.println(Ethernet.localIP());
  lastTempReceived = millis();
}

void setup(void)
{
  //Готовим пин для ресета W5500
  pinMode(W5500_RESET_PIN, OUTPUT);
  digitalWrite(W5500_RESET_PIN, HIGH);
  
  // Start Serial
  Serial.begin(115200);
  
  dht.begin();
  // Init variables and expose them to REST API
  temperature = 10;
  tempoff = VENT_TEMP_OFF;
  tempmax = VENT_TEMP_MAX;

//  byte t = dueFlashStorage.read(0);//Read temp OFF from flash
  byte t = EEPROM.read(0);//Read temp OFF from flash
  if(t>=5 && t<=30)
  {
    tempoff = t;
  }

//  t = dueFlashStorage.read(1);//Read temp MAX from flash
  t = EEPROM.read(1);//Read temp MAX from flash

  if(t>=10 && t<=40)
  {
    tempmax = t;
  }

  rest.variable("temperature",&temperature);
  rest.variable("vSpeed",&vSpeed);
  rest.variable("tempoff",&tempoff);
  rest.variable("tempmax",&tempmax);
  rest.variable("CellarTemp", &nCellarTemperature);
  rest.variable("CellarHumidity",&nCellarHumidity);

  // Function to be exposed
  rest.function("set_temp",SetTemperature);
  rest.function("set_off_temp",SetTempOFF);
  rest.function("set_max_temp",SetTempMAX);
  

  // Give name & ID to the device (ID should be 6 characters long)
  rest.set_id("008");
  rest.set_name("dapper_drake");

  // Start the Ethernet connection and the server
  Ethernet.begin(mac,ip);
  
  server.begin();
  Serial.print("Server is at ");
  Serial.println(Ethernet.localIP());

  pinMode(VENT_ONOFF_PIN, OUTPUT);
  pinMode(VENT_SPEED_PIN, OUTPUT);

  digitalWrite(VENT_ONOFF_PIN, HIGH);// Vent OFF
  digitalWrite(VENT_SPEED_PIN, HIGH);// SPEED MIN
  Serial.println("Vent OFF");
// watchdogEnable(watchdogTime);
}

void loop() {

  if((millis()-lastTempReceived) > TEMP_RECEIVE_TIMEOUT )
  {
    restartEthernet();
  }

  //Read the cellar temperature
  if (millis() - lastCellarTempRead > 60000)
  {  
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    nCellarHumidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    nCellarTemperature = dht.readTemperature();
      // Check if any reads failed and set failed value to -100.
    if (isnan(nCellarHumidity) ) {
      Serial.println(F("Failed to read Humidity from DHT sensor!"));
      nCellarHumidity = -100;
    }else if (isnan(nCellarTemperature) ) {
      Serial.println(F("Failed to read Temperature from DHT sensor!"));
      nCellarTemperature = -100;
    }
    Serial.print("CellarTemperature: ");
    Serial.print(nCellarTemperature);
    Serial.print(", CellarHumidity: ");
    Serial.println(nCellarHumidity);
    lastCellarTempRead = millis();
  }
  // listen for incoming clients
  EthernetClient client = server.available();
  
  rest.handle(client);

  if (millis() - lastTempOutput > 10000)
  {
    lastTempOutput = millis();
    Serial.print("temp:  ");
    Serial.println(temperature);
  }

// ******* Vent control ********

// Vent OFF if temp < VENT_TEMP_OFF degrees
// MIN if temp >= VENT_TEMP_OFF and temp < VENT_TEMP_MAX degrees
// MAX if temp >= VENT_TEMP_MAX degrees

  if((temperature < tempoff) && (millis() - lastVentStateChange > VENT_CHANGE_INTERVAL) && (vSpeed != VENT_OFF))
  {
    vSpeed=VENT_OFF;
    lastVentStateChange = millis();
    digitalWrite(VENT_ONOFF_PIN, HIGH);// Vent OFF
    digitalWrite(VENT_SPEED_PIN, HIGH);// SPEED MIN
    Serial.println("Vent OFF");
  }
  else if((temperature >= tempoff) && (temperature < tempmax) && (millis() - lastVentStateChange > VENT_CHANGE_INTERVAL) && (vSpeed != VENT_MIN))
  {
    vSpeed=VENT_MIN;
    lastVentStateChange = millis();
    digitalWrite(VENT_ONOFF_PIN, LOW);// Vent ON
    digitalWrite(VENT_SPEED_PIN, HIGH);// SPEED MIN    
    Serial.println("Vent MIN");
  }else if((temperature >= tempmax) && (millis() - lastVentStateChange > VENT_CHANGE_INTERVAL) && (vSpeed != VENT_MAX))
  {
    vSpeed=VENT_MAX;
    lastVentStateChange = millis();
    digitalWrite(VENT_ONOFF_PIN, LOW);// Vent ON
    digitalWrite(VENT_SPEED_PIN, LOW);// SPEED MAX    
    Serial.println("Vent MAX");
  }
}

// Custom function accessible by the API
int SetTemperature(String command) {

  // Get state from command
  int temp = command.toInt();

  temperature = temp;
  lastTempReceived = millis();
  return 1;
}

int SetTempOFF(String command) {

  // Get state from command
  byte temp = (byte)command.toInt();

  //Write temp OFF to flash
  if(temp>=5 && temp<=30)
  {
    tempoff = temp;
//    dueFlashStorage.write(0,temp); 
    EEPROM.write(0,temp); 
    Serial.print("Set temp OFF to: ");
    Serial.println(temp);
  }else
  {
    Serial.println("Wrong temp OFF");
  }
  return 1;
}

int SetTempMAX(String command) {

  // Get state from command
  byte temp = (byte)command.toInt();

  //Write temp MAX to flash
  if(temp>=10 && temp<=40)
  {
    tempmax = temp;
//    dueFlashStorage.write(1,temp);  
    EEPROM.write(1,temp);
    Serial.print("Set temp MAX to: ");
    Serial.println(temp);
  }else
  {
    Serial.println("Wrong temp MAX");
  }
  return 1;
}
