#include <Ethernet.h>
#include <EthernetUdp.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

//////////////dht sett-s
#define DHTPIN 8 
#define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);
unsigned long last_dht_time = 0;
unsigned long dht_data_via_mqtt = 0;
char data_buf[6];
bool error_dht_temp_flag = false;
bool error_dht_hum_flag = false;
bool mqtt_send_flag = false;
//////////////end of dht sett-s

////////////// mqtt settings
// Debug output to serial console
#define mqtt_user "mqttusr"
#define mqtt_password "qwerty123"
IPAddress deviceIp(192, 168, 88, 34);
byte deviceMac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
char* deviceId  = "1st_floor_arduino_tst"; // Name of the sensor
char* switchTopic = "home-assistant/controller_STATES/wallpanel/battery"; 
char* state_temp_Topic = "home-assistant/sensors/indoor_global/indoor_temp";
char* state_hum_Topic = "home-assistant/sensors/indoor_global/indoor_humidity";
char* state_pir_Topic = "home-assistant/sensors/indoor_global/wallpanel_pir";
// MQTT server settings
byte mqttServer[] = {192, 168, 88, 17};
int mqttPort = 1883;
//end
EthernetClient ethClient;
void callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqtt_client(mqttServer, 1883, callback, ethClient);
int mqtt_connect_try = 0;
unsigned long last_mqtt_connect_time = 0;
//////////////end of mqtt sett-s

////////////// pir sett-s
bool last_state_pir_activ = false;
int state = 0;
unsigned long last_pir_time = 0;
//////////////end of pir set-s

#define DEBUG 1 

#define PIN_RELAY 3 //relay pin
int RusPIRPin = 5;

int digit_batt_lvl = 0;
bool subscribe_Signal = false;
bool charging_flag = true;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("in callback");
  payload[length] = '\0';
  Serial.print(topic);
  Serial.print("  ");
  String strTopic = String(topic);
  String strPayload = String((char*)payload);
  Serial.println(strPayload);
  if (strTopic == "home-assistant/controller_STATES/wallpanel/battery")
  {
    if (strPayload == "low_battery")
    {
      subscribe_Signal = true;
    }
    if (strPayload == "high_battery")
    {
      subscribe_Signal = false;
    }
  }
}

void reconnect() {
  while (!mqtt_client.connected() && mqtt_connect_try < 3) 
  {
    if (DEBUG)
    {
      Serial.print("Attempting MQTT connection...");
    }
    if (mqtt_client.connect(deviceId, mqtt_user, mqtt_password)) 
    {
      mqtt_connect_try = 0;
      if (DEBUG) 
      {
        Serial.println("connected");
        mqtt_client.subscribe(switchTopic);
        //add here startup publishs
      }
    } else 
    {
      if (DEBUG) 
      {
        Serial.print("failed, rc=");
        Serial.print(mqtt_client.state());
        Serial.println(" try again");
      }
      delay(500);
      mqtt_connect_try++;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Ethernet.begin(deviceMac, deviceIp);

  pinMode(PIN_RELAY, OUTPUT);
  pinMode(RusPIRPin, INPUT);
  
  dht.begin();
  
}

void loop() {
  //////////////mqtt charging controls
  mqtt_client.loop();
  if (((millis() - 10000) > last_mqtt_connect_time)) {
    if (!mqtt_client.connected()) {
      reconnect();
      mqtt_connect_try = 0;
      //mqtt_timer = 0;
    } 
    last_mqtt_connect_time = millis();
  }

  if (subscribe_Signal && !charging_flag)
  {
    digitalWrite(PIN_RELAY, HIGH);

    charging_flag = true;
  }
  if (!subscribe_Signal && charging_flag)
  {
    digitalWrite(PIN_RELAY, LOW);

    charging_flag = false;
  }
  //////////////end of mqtt charging controls
  
  //////////////dht controls
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float dht_hum = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float dht_temp = dht.readTemperature();
  if ((millis() - last_dht_time) > 60000)
  {
    if (isnan(dht_temp)) 
    {
      Serial.println("Error reading temperature!");
      error_dht_temp_flag = true;
    }
    else 
    {
      Serial.print("Temperature: ");
      Serial.print(dht_temp);
      Serial.println("C");
      error_dht_temp_flag = false;
    }
    if (isnan(dht_hum)) 
    {
      Serial.println("Error reading humidity!");
      error_dht_hum_flag = true;
    }
    else 
    {
      Serial.print("Humidity: ");
      Serial.print(dht_hum);
      Serial.println("%");
      error_dht_hum_flag = false;
    }
    last_dht_time = millis();
    
  }
  if (((millis() - dht_data_via_mqtt) > 60000) && !error_dht_hum_flag && !error_dht_temp_flag)
  {
    dtostrf(dht_temp, 5, 2, data_buf);
    delay(500);
    Serial.print("temp ");
    Serial.println(dht_temp);
    mqtt_client.publish(state_temp_Topic, data_buf);
    Serial.println(data_buf);
  
    Serial.print("hum ");
    Serial.println(dht_hum);
    dtostrf(dht_hum, 5, 2, data_buf);
    delay(500);
    mqtt_client.publish(state_hum_Topic, data_buf);
    Serial.println(data_buf);
    
    dht_data_via_mqtt = millis();
  }
  //////////////end of dht controls

  //int state = 0;
  
  if ((millis() - last_pir_time) > 90000)
  {
    state = digitalRead(RusPIRPin);
    /*if (DEBUG)
    {
      Serial.println("in pir stage");
    }*/
    if (state == HIGH && !last_state_pir_activ)
    {
      Serial.println("pir active!"); 
      strcpy(data_buf, "on");
      mqtt_client.publish(state_pir_Topic, data_buf);
      last_state_pir_activ = true;
      last_pir_time = millis();
    }
    if (state == LOW && last_state_pir_activ)
    {
      Serial.println("pir disactived!"); 
      strcpy(data_buf, "off");
      mqtt_client.publish(state_pir_Topic, data_buf);
      last_state_pir_activ = false;
    }
  }
  
  
  
}
