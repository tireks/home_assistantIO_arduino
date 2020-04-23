#include <PubSubClient.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
/*#include <EthernetClient.h>
#include <EthernetServer.h>*/
#include <EthernetUdp.h>

const int buttonPin = 40; // номер порта нашей кнопки
const int ledPin =    48; // номер порта светодиода
int button_output_flag = 1; //flag for serial output of button state
int button_activ_flag[2] = {-1,0};
#define DEBUG 1 // Debug output to serial console
#define mqtt_user "mqttusr"
#define mqtt_password "qwerty123"
// Device settings
IPAddress deviceIp(192, 168, 88, 34);
byte deviceMac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
char* deviceId  = "sensor_diod"; // Name of the sensor
char* stateTopic = "home-assistant/controller_states/sensor_diod"; // MQTT topic where values are published
char* switchTopic = "home-assistant/controller_switches/sensor_diod";
int sensorPin = 48; // Pin to which the sensor is connected to
char buf[4]; // Buffer to store the sensor value
int updateInterval = 1000; // Interval in milliseconds
int subSignal = -1;

// MQTT server settings
byte mqttServer[] = {192, 168, 88, 17};
int mqttPort = 1883;

EthernetClient ethClient;
void callback(char* topic, byte* payload, unsigned int length);
PubSubClient client(mqttServer, 1883, callback, ethClient);

void reconnect() {
  while (!client.connected()) {
#if DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    if (client.connect(deviceId, mqtt_user, mqtt_password)) {
#if DEBUG
      Serial.println("connected");
      client.subscribe(switchTopic);
#endif
    } else {
#if DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  payload[length] = '\0';
  Serial.print(topic);
  Serial.print("  ");
  String strTopic = String(topic);
  String strPayload = String((char*)payload);
  Serial.println(strPayload);
  if (strTopic == "home-assistant/controller_switches/sensor_diod")
  {
    if (strPayload == "on_")
    {
      subSignal = 1;
    }
    if (strPayload == "off")
    {
      subSignal = -1;
    }
  }
}

void setup() {
    // устанавливаем порт светодиода на выход
    pinMode(ledPin, OUTPUT);
    // устанавливаем порт кнопки на вход
    pinMode(buttonPin, INPUT_PULLUP);
    Serial.begin(9600);
    //client.setServer(mqttServer, mqttPort);
    Ethernet.begin(deviceMac, deviceIp);
}
void loop() {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    // читаем состояние порта кнопки и записываем в переменную
    int buttonState = digitalRead(buttonPin);
    // делаем простую проверку нашей переменной, если на входе в порт кнопки присутствует напряжение - включаем светодиод, иначе - выключаем
    if (buttonState == LOW || subSignal != 0) 
    {
      if ((button_activ_flag[1] == 0 && button_activ_flag[0] == -1) || subSignal == 1)
      {
        button_activ_flag[0] = 1;
        button_activ_flag[1] = 1;
      }
      if ((button_activ_flag[1] == 0 && button_activ_flag[0] == 1) || subSignal == -1)
      {
        button_activ_flag[0] = -1;
        button_activ_flag[1] = -1;
      }
    }
    /*if (subSignal != 0) 
    {
      if (subSignal == 1)
      {
        button_activ_flag[0] = 1;
        button_activ_flag[1] = 1;
      }
      if (subSignal == -1)
      {
        button_activ_flag[0] = -1;
        button_activ_flag[1] = -1;
      }
    }*/
    if ((buttonState == HIGH && button_activ_flag[1] != 0) || subSignal != 0)
    {
      button_activ_flag[1] = 0;
      subSignal = 0;
    }
    if (button_activ_flag[0] == 1 && button_activ_flag[1] == 0)
    {
      digitalWrite(ledPin, HIGH);
      if (button_output_flag == 0) 
      {
        Serial.println("button_on");
        strcpy(buf, "on_");
        client.publish(stateTopic, buf);
        button_output_flag = 1;
      }
    }
    
     if (button_activ_flag[0] == -1 && button_activ_flag[1] == 0) {
        // выключаем светодиод
        digitalWrite(ledPin, LOW);
        if (button_output_flag == 1)
        {
          Serial.println("button_off");
          strcpy(buf, "off");
          client.publish(stateTopic, buf);
          button_output_flag = 0;
        }
        
    }
}


