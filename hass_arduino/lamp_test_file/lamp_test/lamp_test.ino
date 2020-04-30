#include <PubSubClient.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
/*#include <EthernetClient.h>
#include <EthernetServer.h>*/
#include <EthernetUdp.h>

int RusPIRPin = 5;
int RusSwitch1Pin = 33;
int RusState1Pin = A1;
int RusSwitch2Pin = 29;
int RusState2Pin = A3;
bool bRusL1On = false;
bool bRusL2On = false;
bool bRusAlreadyOn = false;
bool bRusPIROn = false;
bool Debug1 = true;
bool debug2 = false;
unsigned long lastRusPIRTime = 0;  //time of last pir active
bool bLastRusPIROn = false;
unsigned long lastRusLivoloEvent = 0;
unsigned long lastRUSLivilo_ON_event = 0;
unsigned long lastRUSLivilo_OFF_event = 0;
unsigned long dontCheckStateRus = 0; //locking signal from livolo states to prevent voltage noise
unsigned long RusLivoloTimeConst = 15 * 1000; //time, when system is ignoring PIR after manual
                                              //turn on/off the light --MAYBE,NOT SURE
unsigned long RusLivoloTimeConst_ON = 10 * 1000;
unsigned long RusLivoloTimeConst_OFF = 5 * 1000;   
int manual_turned_on = 1;
int manual_turned_off = 1;  
bool start_livolocntrl_flag = true;                                   
unsigned long RusPIRTimeConst = 3 * 1000;//time, when system is ignoring PIR after PIR-controlled
                                        //turn on/off the light --MAYBE,NOT SURE
/////////////////MQTT//////////////////
IPAddress deviceIp(192, 168, 88, 34);
byte deviceMac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
unsigned long mqtt_timer = 0;
#define DEBUG 1
char* deviceId  = "livolo_lamp_test"; // Name of the sensor
char* stateTopic_1s = "home-assistant/controller_STATES/lights/test_livolo_1"; 
char* switchTopic_1s = "home-assistant/controller_SWITCHES/lights/test_livolo_1"; 
char* stateTopic_2s = "home-assistant/controller_STATES/lights/test_livol0_2"; 
char* switchTopic_2s = "home-assistant/controller_SWITCHES/lights/test_livolo_2"; 
#define mqtt_user "mqttusr"
#define mqtt_password "qwerty123"
char buf[4]; // Buffer to store the sensor value
int updateInterval = 1000; // Interval in milliseconds
byte mqttServer[] = {192, 168, 88, 17};
int mqttPort = 1883;
#define publ_update_period 10000
EthernetClient ethClient;
void callback(char* topic, byte* payload, unsigned int length);
PubSubClient client(mqttServer, 1883, callback, ethClient);
int subSignal_1L = -1;
int subSignal_2L = -1;
int fuckinflag = 0;

void reconnect() {
  while (!client.connected()) {
#if DEBUG
    Serial.print("Attempting MQTT connection...");
#endif
    if (client.connect(deviceId, mqtt_user, mqtt_password)) {
#if DEBUG
      Serial.println("connected");
      client.subscribe(switchTopic_1s);
      strcpy(buf, "off");
      client.publish(stateTopic_1s, buf);
      client.subscribe(switchTopic_2s);
      strcpy(buf, "off");
      client.publish(stateTopic_2s, buf);
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
  if (strTopic == "home-assistant/controller_SWITCHES/lights/test_livolo_1")
  {
    if (strPayload == "on")
    {
      subSignal_1L = 1;
    }
    if (strPayload == "off")
    {
      subSignal_1L = -1;
    }
  }
  if (strTopic == "home-assistant/controller_SWITCHES/lights/test_livolo_2")
  {
    if (strPayload == "on")
    {
      subSignal_2L = 1;
    }
    if (strPayload == "off")
    {
      subSignal_2L = -1;
    }
  }
}


void SwitchLivolo(int n)//process of applying voltage to a specific pin
{
  if (n == 1)
  {
    digitalWrite(RusSwitch1Pin, HIGH);
    delay(100);
    digitalWrite(RusSwitch1Pin, LOW);
    delay(50);
  }
  if (n == 2)
  {
    digitalWrite(RusSwitch2Pin, HIGH);
    delay(100);
    digitalWrite(RusSwitch2Pin, LOW);
    delay(50);
  }
  dontCheckStateRus = millis(); // used for some kind of a delay, cos livolo is makin' some 
                  //voltage noise after "process of applying voltage to a specific pin"
}

void LivoloOn(int n) //while turning on and turning off switcher, calling "SwitchLivolo",
{                    // because modifying state of switcher is process of applying voltage to a specific pin
  if ((n == 1) && (!bRusL1On))
  {
    SwitchLivolo(1);
    strcpy(buf, "on");
    client.publish(stateTopic_1s, buf);
    mqtt_timer = millis();
  }
  if ((n == 2) && (!bRusL2On))
  {
    SwitchLivolo(2);
    strcpy(buf, "on");
    client.publish(stateTopic_2s, buf);
    mqtt_timer = millis();
  }
}

void LivoloOff(int n)//while turning on and turning off switcher, calling "SwitchLivolo",
{                    //because modifying state of switcher is process of applying voltage to a specific pin
  if ((n == 1) && (bRusL1On))
  {
    SwitchLivolo(1);
    strcpy(buf, "off");
    client.publish(stateTopic_1s, buf);
    mqtt_timer = millis();
  }
  if ((n == 2) && (bRusL2On))
  {
    SwitchLivolo(2);
    strcpy(buf, "off");
    client.publish(stateTopic_2s, buf);
    mqtt_timer = millis();
  }
}


void setup()
{
  if (Debug1)
  {
    Serial.begin(9600);
  }

  pinMode(RusPIRPin, INPUT);
  pinMode(RusSwitch1Pin, OUTPUT);
  pinMode(RusState1Pin, INPUT_PULLUP);
  pinMode(RusSwitch2Pin, OUTPUT);
  pinMode(RusState2Pin, INPUT_PULLUP);

  Ethernet.begin(deviceMac, deviceIp);

  lastRusPIRTime = millis();//means at what time pir was last activated
  lastRusLivoloEvent = millis();//means at what time 1-st/2-nd/both lamp(s) was manually activated/disactivated
  lastRUSLivilo_OFF_event = millis();
  lastRUSLivilo_ON_event = millis();
  dontCheckStateRus = millis();//delay due to electronic "noise" of PIR
  mqtt_timer = millis();
}

void loop() {
  if (!client.connected()) {
      reconnect();
      mqtt_timer = 0;
    }
    client.loop();
  /////////////////////////////////////////////////
  //////////////////////////////  Livolo start
  int state = 0;


  //if ((millis() - lastRusLivoloEvent) > RusLivoloTimeConst) --ACHTUNG!!! VERY IMPORTANT
  if ((((millis() - lastRUSLivilo_ON_event) > RusLivoloTimeConst_ON && manual_turned_on == 1) || (((millis() - lastRUSLivilo_OFF_event) > RusLivoloTimeConst_OFF) && manual_turned_off == 1)) || start_livolocntrl_flag)
  {
    if (debug2)
    {
      Serial.println("stage 1");
    }
    
    state = digitalRead(RusPIRPin);

    if (fuckinflag == 0 && (state == HIGH))//ïðîâåðÿåì äàò÷èê äâèæåíèÿ òîëüêî ñ 17:00 äî 10:00(todo, but now disactivated)
    {
      if (debug2)
      {
      Serial.println("stage 2");
      }
      if (!bRusAlreadyOn)
      {
        LivoloOn(1);
        LivoloOn(2);
        start_livolocntrl_flag = false;
        Debug1 = true; //unlock "Debug" to write text in serial output
        if (Debug1)
        {
          Serial.println("Ruslan Livolo On by PIR!");

        }
        bRusAlreadyOn = true;
        bRusPIROn = true;
      }
      lastRusPIRTime = millis();
      debug2 = false;
      //bRusPIROn = true;


    }
    else
    {
      if ((millis() - lastRusPIRTime) > RusPIRTimeConst)
      {
        LivoloOff(1);
        LivoloOff(2);
        if (Debug1)
        {
          Serial.println("Ruslan Livolo Off by PIR!");
          Debug1 = false; //locking "Debug1" to prevent spamming "Ruslan Livolo Off by PIR!"
          manual_turned_on = 1;
          manual_turned_off = 1;
        }
        bRusAlreadyOn = false;
        bRusPIROn = false;
      }
      //bRusPIROn = false;
    }
    debug2 = false;
  }

  /////////mqtt controller
  if (subSignal_1L != 0)
  {
    Serial.println("activated by 1 mqtt");
    Serial.println(subSignal_1L);
    state = digitalRead(RusState1Pin);
    if ((state == HIGH && subSignal_1L == 1) || (state == LOW && subSignal_1L == -1))
    {
      digitalWrite(RusSwitch1Pin, HIGH);
      delay(100);
      digitalWrite(RusSwitch1Pin, LOW);
      delay(50);
    }
    subSignal_1L =0;
    delay(300);
  }
  if (subSignal_2L != 0)
  {
    Serial.println("activated by 2 mqtt");
    Serial.println(subSignal_2L);
    state = digitalRead(RusState2Pin);
    if ((state == HIGH && subSignal_2L == 1) || (state == LOW && subSignal_2L == -1))
    {
      digitalWrite(RusSwitch2Pin, HIGH);
      delay(100);
      digitalWrite(RusSwitch2Pin, LOW);
      delay(50);
    }
    subSignal_2L =0;
    delay(300);
  }
    
  
  /////////


  state = digitalRead(RusState1Pin);
  if ((millis() - dontCheckStateRus) > 1000)
  {
    if ((state == HIGH && bRusL1On) || (state == LOW && !bRusL1On))
    {
      //state of dat light has changed
      Debug1 = true;//unlock "Debug1" to write text in serial output
      if (Debug1)
      {
        Serial.println("Ruslan Livolo 1 manual Event");

      }
      if (!bRusL1On)
      {
        strcpy(buf, "on");
        client.publish(stateTopic_1s, buf);
        mqtt_timer = millis();
        lastRUSLivilo_ON_event = millis();
        manual_turned_on = 1;
        manual_turned_off = 0;
      }
      else
      {
        strcpy(buf, "off");
        client.publish(stateTopic_1s, buf);
        mqtt_timer = millis();
        lastRUSLivilo_OFF_event = millis();
        manual_turned_on = 0;
        manual_turned_off = 1;
      }
      
      //bL1On = !bL1On;
      //lastRusLivoloEvent = millis(); --ACHTUNG!!! VERY IMPORTANT
    }
  }
  if (state == HIGH)
  {
    bRusL1On = false;
  }
  else
  {
    bRusL1On = true;
  }

  state = digitalRead(RusState2Pin);
  if ((millis() - dontCheckStateRus) > 1000)
  {
    if ((state == HIGH && bRusL2On) || (state == LOW && !bRusL2On))
    {
      //state of dat light has changed
      Debug1 = true;//unlock "Debug1" to write text in serial output
      if (Debug1)
      {
        Serial.println("Ruslan Livolo 2 manual Event!");
        debug2 = true;
      }
      if (!bRusL2On)
      {
        strcpy(buf, "on");
        client.publish(stateTopic_2s, buf);
        mqtt_timer = millis();
        lastRUSLivilo_ON_event = millis();
        manual_turned_on = 1;
        manual_turned_off = 0;
      }
      else
      {
        strcpy(buf, "off");
        client.publish(stateTopic_2s, buf);
        mqtt_timer = millis();
        lastRUSLivilo_OFF_event = millis();
        manual_turned_on = 0;
        manual_turned_off = 1;
      }
      //bRusL2On = !bRusL2On;
      //lastRusLivoloEvent = millis(); --ACHTUNG!!! VERY IMPORTANT
    }
  }
  if (state == HIGH)
  {
    bRusL2On = false;
  }
  else
  {
    bRusL2On = true;
  }
  if (millis() - mqtt_timer > publ_update_period)
  {
    Serial.println("upd mqtt");
    if (bRusL1On)
    {
      strcpy(buf, "on");
      client.publish(stateTopic_1s, buf);
    } 
    else
    {
      strcpy(buf, "off");
      client.publish(stateTopic_1s, buf);
    }
    if (bRusL2On)
    {
      strcpy(buf, "on");
      client.publish(stateTopic_2s, buf);
    } 
    else
    {
      strcpy(buf, "off");
      client.publish(stateTopic_2s, buf);
    }
    mqtt_timer = millis();
  }
}




