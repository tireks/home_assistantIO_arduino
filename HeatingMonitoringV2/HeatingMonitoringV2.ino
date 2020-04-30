#include <TimeLib.h>
#include <UTFT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <PubSubClient.h>



// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS A0
#define ONE_WIRE_BUS1 21
#define TEMPERATURE_PRECISION 12


//********************************************************************************************
byte mac[] = { 0x50, 0x46, 0x5D, 0x56, 0x85, 0x48 }; //MAC-адрес Arduino
#define postingInterval 600000  // интервал между отправками данных в миллисекундах (10 минут)
//********************************************************************************************

IPAddress server(94,19,113,221); // IP сервера народного мониторинга
char macbuf[13];
 
EthernetClient client;
unsigned long lastConnectionTime = 0;           // время последней передачи данных
boolean lastConnected = false;                  // состояние подключения
String strReplyBuffer;                         // буфер для отправки в виде строки
int lastConnectionError = 1;                    // код ошибки последнего соединения
int nConnectionErrors = 0;                      // количество ошибок соединения

time_t lastSyncTime = 0;                            //время и дата последней синхронизации

// NTP для синхронизации времени
EthernetUDP Udp;
unsigned long lastNTPSyncTime = 0;           // время последней попытки синхронизации времени
#define NTPSyncInterval 3600000  // интервал между попытками синхронизации времени в миллисекундах (60 минут)
IPAddress timeServer(192, 168, 88, 1);   // IP-адрес NTP сервера 185.17.8.100-doesnt work properly, trying microtik
//IPAddress timeServer(192, 168, 153, 1);   // Используем маршрутизатор в качестве NTP сервера
const int timeZone = 7;                   // TimeZone
//****************************************************************************************
unsigned int localPort = 53;             // local port to listen for UDP packets

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);//Сеть для датчиков отопления
OneWire oneWireInOut(ONE_WIRE_BUS1);//Сеть для датчиков внешней и внутренней температуры


// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);//Датчики отопления
DallasTemperature sensorsInOut(&oneWireInOut);//Датчики внешней и внутренней температуры

unsigned long lastTempTime = 0;           // время последнего запроса температуры

#define SENSOR_NUM 9
// arrays to hold device addresses
DeviceAddress Thermometers[9]= {  { 0x28, 0xFF, 0x78, 0x5B, 0x63, 0x14, 0x02, 0x7D },
                                  { 0x28, 0x0D, 0x74, 0x3D, 0x06, 0x00, 0x00, 0x22 },
                                  { 0x28, 0xB2, 0xA8, 0x3F, 0x06, 0x00, 0x00, 0x79 },
                                  { 0x28, 0xFF, 0x25, 0x13, 0x63, 0x14, 0x02, 0xF8 },
                                  { 0x28, 0x2B, 0x35, 0x3E, 0x06, 0x00, 0x00, 0x8A },
                                  { 0x28, 0xFF, 0x67, 0x0D, 0x63, 0x14, 0x03, 0xE1 },
                                  { 0x28, 0xF2, 0x8E, 0x3D, 0x06, 0x00, 0x00, 0x6F },
                                  { 0x28, 0x06, 0x9E, 0x3F, 0x06, 0x00, 0x00, 0xBA },
                                  { 0x28, 0xFF, 0x91, 0x42, 0x63, 0x14, 0x03, 0xA1 }
                                };
                                
DeviceAddress ThermometerIn  =   { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };                          
DeviceAddress ThermometerOut =   { 0x28, 0x1B, 0x82, 0x3D, 0x06, 0x00, 0x00, 0xF1 };                          

// массивы температур целой части и дробной части
int currentTempWhole[SENSOR_NUM];
int currentTempFract[SENSOR_NUM];

int currentInTempWhole, currentInTempFract;
int currentOutTempWhole, currentOutTempFract;


bool Debug = true; //режим отладки

UTFT    myGLCD(ITDB32S,38,39,40,41);
extern uint8_t BigFont[];
extern uint8_t SmallFont[];

int dispx, dispy, text_y_center;

int line_height = 16;
int line_number = 0;
bool isBigFont = true;

///////////////////////////////////////////
//// Livolo control 
#define PIRPin 46
#define LightSensorPin 48
int LightSensorLevel = 0;

int lSwitch1Pin = 48;
int lState1Pin = 50;
int lSwitch2Pin = 49;
int lState2Pin = 51;
boolean bL1On = false;
boolean bL2On = false;
boolean bAlreadyOn = false;
boolean bPIROn = false;


unsigned long LivoloTimeConst_ON = 900000; //temp
unsigned long lastPIRTime = 0;
bool bLastPIROn = false;
unsigned long lastLivoloEvent = 0;
unsigned long lastLivilo_ON_event = 0;
unsigned long lastLivilo_OFF_event = 0;
unsigned long dontCheckState = 0;
//unsigned long LivoloTime = 15*6000; //temp
unsigned long PIRTime = 120000; //temp
//unsigned long LivoloTimeConst_ON = 15 * 6000; //temp
unsigned long LivoloTimeConst_OFF = 90000; //temp
bool manual_turned_on = true;
bool manual_turned_off = true;
bool start_livolocntrl_flag = true;  
void LivoloOn(int n);
void LivoloOff(int n);

////////////////////////////////////////////
// OpenHAB over MQTT

EthernetClient ethClient;
byte MQTTserver[] = { 192, 168, 88, 17 };

void callback(char* topic, byte* payload, unsigned int length);
PubSubClient MQTTclient(MQTTserver, 1883, callback, ethClient);

char* deviceId  = "livolo_lamp_boiler"; // Name of the sensor
char* stateTopic_1s = "home-assistant/controller_STATES/lights/boiler_livolo_1"; 
char* switchTopic_1s = "home-assistant/controller_SWITCHES/lights/boiler_livolo_1"; 
char* stateTopic_2s = "home-assistant/controller_STATES/lights/boiler_livolo_2"; 
char* switchTopic_2s = "home-assistant/controller_SWITCHES/lights/boiler_livolo_2"; 
#define mqtt_user "mqttusr"
#define mqtt_password "qwerty123"
bool DEBUG1 = true;
bool debug2 = true;
char buf_mqtt[4]; // Buffer to store the sensor value
int subSignal_1L = -1;
int subSignal_2L = -1;
int mqtt_connect_try = 0;

unsigned long lastMqtt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  Serial.print(topic);
  Serial.print("  ");
  String strTopic = String(topic);
  String strPayload = String((char*)payload);
  Serial.println(strPayload);

  if (strTopic == "home-assistant/controller_SWITCHES/lights/boiler_livolo_1")
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
  if (strTopic == "home-assistant/controller_SWITCHES/lights/boiler_livolo_2")
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

///////////////////////////////////////////
//// Livolo control end

time_t getNtpTime();
void httpRequest() ;

void SwitchLivolo(int n)
{
  if(n == 1)
  {
    digitalWrite(lSwitch1Pin, HIGH);
    delay(100);
    digitalWrite(lSwitch1Pin, LOW);
    delay(50);
  }
  if(n == 2)
  {
    digitalWrite(lSwitch2Pin, HIGH);
    delay(100);
    digitalWrite(lSwitch2Pin, LOW);
    delay(50);
  }
  dontCheckState = millis();
}

void LivoloOn(int n) //while turning on and turning off switcher, calling "SwitchLivolo",
{                    // because modifying state of switcher is process of applying voltage to a specific pin
  if ((n == 1) && (!bL1On))
  {
    SwitchLivolo(1);
    strcpy(buf_mqtt, "on");
    MQTTclient.publish(stateTopic_1s, buf_mqtt);
   // mqtt_timer = millis();
  }
  if ((n == 2) && (!bL2On))
  {
    SwitchLivolo(2);
    strcpy(buf_mqtt, "on");
    MQTTclient.publish(stateTopic_2s, buf_mqtt);
    //mqtt_timer = millis();
  }
}

void LivoloOff(int n)//while turning on and turning off switcher, calling "SwitchLivolo",
{                    //because modifying state of switcher is process of applying voltage to a specific pin
  if ((n == 1) && (bL1On))
  {
    SwitchLivolo(1);
    strcpy(buf_mqtt, "off");
    MQTTclient.publish(stateTopic_1s, buf_mqtt);
    //mqtt_timer = millis();
  }
  if ((n == 2) && (bL2On))
  {
    SwitchLivolo(2);
    strcpy(buf_mqtt, "off");
    MQTTclient.publish(stateTopic_2s, buf_mqtt);
    //mqtt_timer = millis();
  }
}

void reconnect() {
  while (!MQTTclient.connected() && mqtt_connect_try < 3) 
  {
    if (DEBUG1)
    {
      Serial.print("Attempting MQTT connection...");
    }
    if (MQTTclient.connect(deviceId, mqtt_user, mqtt_password)) 
    {
      mqtt_connect_try = 0;
      if (DEBUG1) 
      {
        Serial.println("connected");
        MQTTclient.subscribe(switchTopic_1s);
        strcpy(buf_mqtt, "off");
        MQTTclient.publish(stateTopic_1s, buf_mqtt);
        MQTTclient.subscribe(switchTopic_2s);
        strcpy(buf_mqtt, "off");
        MQTTclient.publish(stateTopic_2s, buf_mqtt);
      }
    } else 
    {
      if (DEBUG1) 
      {
        Serial.print("failed, rc=");
        Serial.print(MQTTclient.state());
        Serial.println(" try again in 5 seconds");
      }
      delay(5000);
      mqtt_connect_try++;
    }
  }
}


void(* resetFunc) (void) = 0; // Reset MC function

void setup()
{
  
  if (Debug)
  {
    Serial.begin(9600);
  }

  pinMode(PIRPin, INPUT);
  pinMode(lSwitch1Pin, OUTPUT);
  pinMode(lState1Pin, INPUT_PULLUP);
  pinMode(lSwitch2Pin, OUTPUT);
  pinMode(lState2Pin, INPUT_PULLUP);
  
  lastPIRTime = millis();
  lastLivoloEvent = millis();
  dontCheckState = millis();
  lastLivilo_OFF_event = millis();
  lastLivilo_ON_event = millis();
  
  // Start up the library DallasTemperature
  sensors.begin();
  sensorsInOut.begin();

  sensors.setWaitForConversion(false); //Переводим получение температуры в асинхронный режим
  sensorsInOut.setWaitForConversion(false); //Переводим получение температуры в асинхронный режим
  
  sensorsInOut.setResolution(ThermometerIn, TEMPERATURE_PRECISION);
  sensorsInOut.setResolution(ThermometerOut, TEMPERATURE_PRECISION);

  // set the resolution 
  for(int i = 0; i<SENSOR_NUM; i++)
  {
    sensors.setResolution(Thermometers[i], TEMPERATURE_PRECISION);
  }

  //Инициализируем массив температур
  for(int i = 0; i<SENSOR_NUM; i++)
  {
    currentTempWhole[i] = DEVICE_DISCONNECTED_C;
    currentTempFract[i] = 0;
  }
 
  currentInTempWhole = DEVICE_DISCONNECTED_C;
  currentInTempFract = 0;
   
  currentOutTempWhole = DEVICE_DISCONNECTED_C;
  currentOutTempFract = 0;
    
  myGLCD.InitLCD();
  myGLCD.clrScr();
  if(isBigFont)
  {
    myGLCD.setFont(BigFont);
    line_height = 16;
  }else
  {
    myGLCD.setFont(SmallFont);
    line_height = 12;
  }

  dispx=myGLCD.getDisplayXSize();
  dispy=myGLCD.getDisplayYSize();
  text_y_center=(dispy/2)-6;

  myGLCD.setColor(255, 0, 0);
  myGLCD.fillRect(0, 0, dispx-1, line_height+2);
  myGLCD.setColor(255, 255, 255);
  myGLCD.drawLine(0, line_height+3, dispx-1, line_height+3);
  
  if (Debug)
  {
    Serial.println("Start Ethernet");
  }

  // Пробуем подключиться по Ethernet
  do 
  {
    if (Debug)
    {
      Serial.println("Try to configure Ethernet using DHCP");
    }
    delay(1234);
  } while (Ethernet.begin(mac) == 0);
    
  if (Debug)
  {
    Serial.println("Ethernet started");
  }
  
  Udp.begin(localPort);

  //Запрашиваем время с NTP сервера
  setSyncProvider(getNtpTime);
  if (timeStatus() != timeNotSet)
  {
    if (Debug)
    {
      Serial.println("Time set");
    }
  }
  lastNTPSyncTime = millis(); //первая попытка синхронизации
  
  lastConnectionTime = millis()-postingInterval+60000; //первое соединение c сервером народного мониторинга через 60 секунд после запуска
  Serial.println(LivoloTimeConst_ON);
  /*if (MQTTclient.connect("myhome-boiler-room")) {
    Serial.println("MQTT connected");
    MQTTclient.publish("/myhome/out/Boiler_Room_Switch1", "OFF");
    MQTTclient.subscribe("/myhome/in/#");
  }*/
  reconnect();
}

void formatTimeDigits(char strOut[3], int num)
{
  strOut[0] = '0' + (num / 10);
  strOut[1] = '0' + (num % 10);
  strOut[2] = '\0';
}

void loop()
{
//////////////////////////////////////////////////
// MQTT
 if (lastMqtt > millis()) lastMqtt = 0; /// ?????
   
 MQTTclient.loop();

  if (millis() > (lastMqtt + 60000)) {
    if (!MQTTclient.connected()) {
      reconnect();
      mqtt_connect_try = 0;
      //mqtt_timer = 0;
    } 
    lastMqtt = millis();
  }
  
/////////////////////////////////////////////////
//////////////////////////////  Livolo start


//Light sensor
int LightLevel = digitalRead(LightSensorPin);



//End light sensor
  int state = 0;
// lcd.setCursor(10, 0); 
//  lcd.print("      "); 
  
  //if((millis()-lastLivoloEvent) > LivoloTime)
  if ((((millis() - lastLivilo_ON_event) > LivoloTimeConst_ON && manual_turned_on) || (((millis() - lastLivilo_OFF_event) > LivoloTimeConst_OFF) && manual_turned_off)) || start_livolocntrl_flag)
  {
    state = digitalRead(PIRPin);
    
    if( /*(hour()>17 || hour()<10) && */(state == HIGH))//проверяем датчик движения только с 17:00 до 10:00 ///////////temp disactive time control
    {
      if(!bAlreadyOn)
      {
        LivoloOn(1);
        LivoloOn(2);
        start_livolocntrl_flag = false;
        debug2 = true; //unlock "Debug" to write text in serial output
        if (debug2)
        {
          Serial.println("Livolo On by PIR!");
        }
        bAlreadyOn = true;
        bPIROn = true;
      }
      
      lastPIRTime = millis();
      
//      lcd.setCursor(10, 0); 
//      lcd.print("PIR ON"); 

      
    }
    else 
    {
      if((millis()-lastPIRTime) > PIRTime)
      {
        LivoloOff(1);
        LivoloOff(2); 
        if (debug2)
        {
          Serial.println("Livolo Off by PIR!");
          debug2 = false; //locking "Debug1" to prevent spamming "Livolo Off by PIR!"
          manual_turned_off = true;
          manual_turned_on = true;
        }   
        //bL1On = false;
        //bL2On = false;
        bAlreadyOn = false;
        bPIROn = false; //QUESTION MODIFICATION
      }
      
    }
  }
  else
  {
//      lcd.setCursor(10, 0); 
//      lcd.print("EVENT"); 
  }

  /////////mqtt controller
  if (subSignal_1L != 0)
  {
    Serial.println("activated by 1 mqtt");
    Serial.println(subSignal_1L);
    state = digitalRead(lState1Pin);
    if ((state == HIGH && subSignal_1L == 1) || (state == LOW && subSignal_1L == -1))
    {
      digitalWrite(lSwitch1Pin, HIGH);
      delay(100);
      digitalWrite(lSwitch1Pin, LOW);
      delay(50);
    }
    subSignal_1L =0;
    delay(300);
  }
  if (subSignal_2L != 0)
  {
    Serial.println("activated by 2 mqtt");
    Serial.println(subSignal_2L);
    state = digitalRead(lState2Pin);
    if ((state == HIGH && subSignal_2L == 1) || (state == LOW && subSignal_2L == -1))
    {
      digitalWrite(lSwitch2Pin, HIGH);
      delay(100);
      digitalWrite(lSwitch2Pin, LOW);
      delay(50);
    }
    subSignal_2L =0;
    delay(300);
  }
    
  
  /////////
  
  state = digitalRead(lState1Pin);
  if((millis()-dontCheckState) > 1000)
  {
    if((state == HIGH && bL1On)||(state == LOW && !bL1On))
    {
      //Состояние изменилось, инвертируем состояние и выводим изменение на экран
      debug2 = true;//unlock "Debug1" to write text in serial output
      if (debug2)
      {
        Serial.println("Livolo 1 manual Event");
      }
      //lastLivoloEvent = millis();
      if (!bL1On)
      {
        strcpy(buf_mqtt, "on");
        MQTTclient.publish(stateTopic_1s, buf_mqtt);
        //mqtt_timer = millis();
        lastLivilo_ON_event = millis();
        manual_turned_on = true;
        manual_turned_off = false;
      }
      else
      {
        strcpy(buf_mqtt, "off");
        MQTTclient.publish(stateTopic_1s, buf_mqtt);
        //mqtt_timer = millis();
        lastLivilo_OFF_event = millis();
        manual_turned_on = false;
        manual_turned_off = true;
      }
    }
  }
  if(state == HIGH)
  {
    bL1On = false;
  }else
  {
    bL1On = true;
  }

  state = digitalRead(lState2Pin);
  if((millis()-dontCheckState) > 1000)
  {
    if((state == HIGH && bL2On)||(state == LOW && !bL2On))
    {
      //Состояние изменилось, инвертируем состояние и выводим изменение на экран
      //bL2On = !bL2On;
      debug2 = true;//unlock "Debug1" to write text in serial output
      if (debug2)
      {
        Serial.println("Livolo 2 manual Event!");

      }
      if (!bL2On)
      {
        strcpy(buf_mqtt, "on");
        MQTTclient.publish(stateTopic_2s, buf_mqtt);
        //mqtt_timer = millis();
        lastLivilo_ON_event = millis();
        manual_turned_on = true;
        manual_turned_off = false;
      }
      else
      {
        strcpy(buf_mqtt, "off");
        MQTTclient.publish(stateTopic_2s, buf_mqtt);
        //mqtt_timer = millis();
        lastLivilo_OFF_event = millis();
        manual_turned_on = false;
        manual_turned_off = true;
      }
      //bRusL2On = !bRusL2On;
      //lastLivoloEvent = millis();
    }  
  }
  if(state == HIGH)
  {
    bL2On = false;
  }else
  {
    bL2On = true;
  }


////////////////////////////////////////////////
//////////////////////////////  Livolo end
  
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus

  int i = 0;

  if(millis() - lastTempTime > 5000)
  {
    if (Debug)
    {
      Serial.print("Requesting temperatures...");
    }
    sensors.requestTemperatures();
    sensorsInOut.requestTemperatures();
    if (Debug)
    {
      Serial.println("DONE");
    }
    lastTempTime = millis();
  }

  if(millis() - lastTempTime > 800)
  {
   // Get current temperatures
    float temp = -127;
    for(i = 0; i<SENSOR_NUM; i++)
    {
      temp = sensors.getTempC(Thermometers[i]);
      if(temp > 0 && temp < 100)
      {
        currentTempWhole[i] = (int)temp;//Берем целую часть
        currentTempFract[i] = (int)((temp-currentTempWhole[i])*100);//Берем два знака после запятой
      }
    }
    
    temp = sensorsInOut.getTempC(ThermometerIn);
    if(temp > -70 && temp < 150)
    {
      currentInTempWhole = (int)temp;//Берем целую часть
      currentInTempFract = (int)((temp-currentInTempWhole)*100);//Берем два знака после запятой
    }
    
    temp = sensorsInOut.getTempC(ThermometerOut);
    if(temp > -70 && temp < 150)
    {
      currentOutTempWhole = (int)temp;//Берем целую часть
      currentOutTempFract = (int)((temp-currentOutTempWhole)*100);//Берем два знака после запятой
    }
    
    
    if(currentOutTempFract < 0)
    {
      currentOutTempFract = -1*currentOutTempFract;
    }

  }

  line_number=0;
  myGLCD.setColor(255, 255, 255);
  myGLCD.setBackColor(255, 0, 0);
  //Стираем мусор от времени
  String stringOutput = String("           ");
  myGLCD.print(stringOutput, CENTER, line_height*line_number*3/2+1);
  //Выводим текущее время
  char strOut[3];  
  formatTimeDigits(strOut, minute());
  stringOutput = String(hour(), DEC)+String(':')+String(strOut)+String(':');
  formatTimeDigits(strOut, second());
  stringOutput = stringOutput + String(strOut);
  myGLCD.print(stringOutput, CENTER, line_height*line_number*3/2+1);
  myGLCD.setBackColor(0, 0, 0);

  line_number=1;
  stringOutput = String("Temp:  In    Out   Mix");
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);

// Print basement heating circuit temperatures
  line_number=2;
  stringOutput = String("Base:  ");
  for(i=0;i<3;i++)
  {
    //Берем целую часть
    formatTimeDigits(strOut, currentTempWhole[i]);
    stringOutput = stringOutput+String(strOut)+String('.');
    //Берем два знака после запятой
    formatTimeDigits(strOut, currentTempFract[i]);
    stringOutput = stringOutput+String(strOut)+String(' ');
  }
  
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);

// Print first floor heating circuit temperatures
  line_number=3;
  stringOutput = String("1 fl:  ");
  for(i=3;i<6;i++)
  {
    //Берем целую часть
    formatTimeDigits(strOut, currentTempWhole[i]);
    stringOutput = stringOutput+String(strOut)+String('.');
    //Берем два знака после запятой
    formatTimeDigits(strOut, currentTempFract[i]);
    stringOutput = stringOutput+String(strOut)+String(' ');
  }
  
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);

// Print second floor heating circuit temperatures
  line_number=4;
  stringOutput = String("2 fl:  ");

  for(i=6;i<9;i++)
  {
    //Берем целую часть
    formatTimeDigits(strOut, currentTempWhole[i]);
    stringOutput = stringOutput+String(strOut)+String('.');
    //Берем два знака после запятой
    formatTimeDigits(strOut, currentTempFract[i]);
    stringOutput = stringOutput+String(strOut)+String(' ');
  }

  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);

//Print indoor temperature  
  //myGLCD.setColor(0, 255, 0);
  line_number=5;
  stringOutput = String("In temp: ");
  //Берем целую часть
  formatTimeDigits(strOut, /*currentInTempWhole*/99);
  stringOutput = stringOutput+String(strOut)+String('.');
  //Берем два знака после запятой
  formatTimeDigits(strOut, /*currentInTempFract*/99);
  stringOutput = stringOutput+String(strOut)+String("   ");
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);
 
//Print outdoor temperature  
  //myGLCD.setColor(255, 0, 0);
  line_number=6;
  stringOutput = String("Out temp: ");
  //Берем целую часть
  char tempbuf[8] = "       ";
  itoa(currentOutTempWhole,tempbuf,10);
  String str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String('.');
  //Берем два знака после запятой
  formatTimeDigits(strOut, currentOutTempFract);
  stringOutput = stringOutput+String(strOut)+String("   ");
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);  
  
//Print last sync time 
  myGLCD.setColor(0, 0, 255);
  myGLCD.setFont(SmallFont);
  line_number=8;
  stringOutput = String(" Last sync: ");
  //Берем день
  itoa(day(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String('.');
  //Берем месяц
  itoa(month(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String('.');
  //Берем год
  itoa(year(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String(' ');  
  //Берем часы
  itoa(hour(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String(':');
  //Берем минуты
  itoa(minute(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String(':');
  //Берем секунды
  itoa(second(lastSyncTime),tempbuf,10);
  str = String(tempbuf);
  str.trim();
  stringOutput = stringOutput+str+String(", Code:")+String(lastConnectionError,DEC)+ String(", Errors:")+String(nConnectionErrors,DEC)+String("        ");
  
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);

//Print debug info
  line_number=9;
  String strIsPIROn = String("false");
  if(bPIROn)
  {
    strIsPIROn = String("true");
  }
  stringOutput = String("PIR:")+strIsPIROn + String(",Livolo Event:") + String(lastLivoloEvent,DEC)+String("                       "); 
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);
  
/*  String strIsConnected = String("false");
  if(client.connected())
  {
    strIsConnected = String("true");
  }
  stringOutput = String(" LT:")+String(lastConnectionTime,DEC)+ String(",CT:")+String(millis(),DEC)+String(",IsCon:")+ strIsConnected+String("        "); 
  myGLCD.print(stringOutput, 1, line_height*line_number*3/2+1);
*/
  
  myGLCD.setFont(BigFont);
// Отправка данных на сервер    

//Если количество ошибок соединения превысило 100, то пробуем переполучить IP по DHCP
  if(nConnectionErrors > 100)
  {
    Ethernet.maintain();
  }
  //Если количество ошибок превысило 150, то перегружаем Ардуину
   if(nConnectionErrors > 150)
  {
    resetFunc();
  }
  
  //Если вдруг нам случайно приходят откуда-то какие-то данные,
  //то просто читаем их и игнорируем, чтобы очистить буфер
  if (client.available()) 
  {
    client.read();
  }

  if (!client.connected() && lastConnected) 
  {
    if (Debug)
    {
      Serial.println();
      Serial.println("disconnecting.");
    }
    client.stop();
  }
  
  //Запрашиваем время с NTP сервера
  if((millis() - lastNTPSyncTime) > NTPSyncInterval)
  {
    setSyncProvider(getNtpTime);
    //Если время получили успешно, то записываем данные в RTC
    if (timeStatus() != timeNotSet)
    {
      if (Debug)
      {
        Serial.println("Time set");
      }
    }
  }

  
  //если прошло определённое время, то делаем замер,
  //переподключаемся и отправляем данные
  if ((millis() - lastConnectionTime) > postingInterval) 
  {
    if(client.connected())
    {
        if (Debug)
        {
          Serial.println();
          Serial.println("disconnecting.");
        }
        client.stop();
    }
 
    //формирование HTTP-запроса
    strReplyBuffer = String("ID=");
 
    memset(macbuf, 0, sizeof(macbuf));
    //Конвертируем MAC-адрес
    for (int k=0; k<6; k++)
    {
      int b1=mac[k]/16;
      int b2=mac[k]%16;
      char c1[2],c2[2];
 
      if (b1>9) c1[0]=(char)(b1-10)+'A';
      else c1[0] = (char)(b1) + '0';
      if (b2>9) c2[0]=(char)(b2-10)+'A';
      else c2[0] = (char)(b2) + '0';
 
      c1[1]='\0';
      c2[1]='\0';
 
      strcat(macbuf,c1);
      strcat(macbuf,c2);
    }
    strReplyBuffer += String(macbuf);
 
    for (int j=0; j<SENSOR_NUM; j++)
    {
      if(currentTempWhole[j] != DEVICE_DISCONNECTED_C)//Посылаем только реальные данные
      {
        strReplyBuffer += String('&');
   
        //конвертируем адрес термодатчика
        for (int k=7; k>=0; k--)
        {
  //Старый некомпиляющийся код преобразования адреса в HEX
  /*        
          int b1=Thermometers[j][k]/16;
          int b2=Thermometers[j][k]%16;
          char c1[2],c2[2];
   
          if (b1>9) c1[0]=(char)(b1-10)+'A';
          else c1[0] = (char)(b1) + '0';
          if (b2>9) c2[0]=(char)(b2-10)+'A';
          else c2[0] = (char)(b2) + '0';
   
          c1[1]='\0';
          c2[1]='\0';
   
          strReplyBuffer += String(c1);
          strReplyBuffer += String(c2);*/
          if(Thermometers[j][k]<= 0x0F)
          {
            strReplyBuffer += String("0");
          }
          strReplyBuffer += String(Thermometers[j][k],HEX);
        }
        strReplyBuffer += String('=');
  
        itoa(currentTempWhole[j],tempbuf,10);
        str = String(tempbuf);
        str.trim();

        strReplyBuffer += str;
        strReplyBuffer += String('.');
  
        if (currentTempFract[j]<10)
        {
          strReplyBuffer += String('0');
        }
        itoa(currentTempFract[j],tempbuf,10);
        strReplyBuffer += String(tempbuf);
      }
    }
    
    //strReplyBuffer += String('\0');

    //Добавляем внутреннюю температуру
    if(currentInTempWhole != DEVICE_DISCONNECTED_C)//Посылаем только реальные данные
    {
      strReplyBuffer += String('&');
 
      //конвертируем адрес термодатчика
      for (int k=7; k>=0; k--)
      {
//Старый некомпиляющийся код преобразования адреса в HEX
  /*         
        int b1=ThermometerIn[k]/16;
        int b2=ThermometerIn[k]%16;
        char c1[2],c2[2];
 
        if (b1>9) c1[0]=(char)(b1-10)+'A';
        else c1[0] = (char)(b1) + '0';
        if (b2>9) c2[0]=(char)(b2-10)+'A';
        else c2[0] = (char)(b2) + '0';
 
        c1[1]='\0';
        c2[1]='\0';
 
        strReplyBuffer += String(c1);
        strReplyBuffer += String(c2);*/
        if(ThermometerIn[k]<= 0x0F)
        {
          strReplyBuffer += String("0");
        }
        strReplyBuffer += String(ThermometerIn[k],HEX);
      }
      strReplyBuffer += String('=');

      itoa(currentInTempWhole,tempbuf,10);
      str = String(tempbuf);
      str.trim();

      strReplyBuffer += str;
      strReplyBuffer += String('.');

      if (currentInTempFract<10)
      {
        strReplyBuffer += String('0');
      }
      itoa(currentInTempFract,tempbuf,10);
      strReplyBuffer += String(tempbuf);
    }
    //Добавляем внешнюю температуру   
    if(currentOutTempWhole != DEVICE_DISCONNECTED_C)//Посылаем только реальные данные
    {
      strReplyBuffer += String('&');
 
      //конвертируем адрес термодатчика
      for (int k=7; k>=0; k--)
      {
  //Старый некомпиляющийся код преобразования адреса в HEX
  /*
        int b1=ThermometerOut[k]/16;
        int b2=ThermometerOut[k]%16;
        char c1[2],c2[2];
 
        if (b1>9) c1[0]=(char)(b1-10)+'A';
        else c1[0] = (char)(b1) + '0';
        if (b2>9) c2[0]=(char)(b2-10)+'A';
        else c2[0] = (char)(b2) + '0';
 
        c1[1]='\0';
        c2[1]='\0';
 
        strReplyBuffer += String(c1);
        strReplyBuffer += String(c2);*/
        if(ThermometerOut[k]<= 0x0F)
        {
          strReplyBuffer += String("0");
        }
        strReplyBuffer += String(ThermometerOut[k],HEX);
      }
      strReplyBuffer += String('=');

      itoa(currentOutTempWhole,tempbuf,10);
      str = String(tempbuf);
      str.trim();

      strReplyBuffer += str;
      strReplyBuffer += String('.');

      if (currentOutTempFract<10)
      {
        strReplyBuffer += String('0');
      }
      itoa(currentOutTempFract,tempbuf,10);
      strReplyBuffer += String(tempbuf);
    }


    if (Debug)
    {
      Serial.println(strReplyBuffer);
      Serial.print("Content-Length: ");
      Serial.println(strReplyBuffer.length());
    }
   
      //отправляем запрос
      httpRequest();

      
  }
  //храним последнее состояние подключения
  lastConnected = client.connected();

}

void httpRequest() 
{
  lastConnectionError = client.connect("narodmon.ru", 80);
  if (lastConnectionError == 1)
  {
    if (Debug)
    {
      Serial.println("connecting...");
    }
    // отправляем HTTP POST запрос:
    client.println("POST http://narodmon.ru/post.php HTTP/1.0");
    client.println("Host: narodmon.ru");
    //client.println("User-Agent: arduino-ethernet");
    //client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(strReplyBuffer.length());
    client.println();
    client.println(strReplyBuffer);
    //client.println();
 
    lastConnectionTime = millis();
    lastSyncTime = now();
    nConnectionErrors = 0;
  } 
  else
  {
    if (Debug)
    {
      Serial.println("connection failed");
      Serial.println("disconnecting.");
    }
    client.stop();
    nConnectionErrors++;
  }
}

int len(char *buf)
{
  int i=0; 
  do
  {
    i++;
  } while (buf[i]!='\0');
  return i;
}
 
void reverse(char s[])
{
  int i, j;
  char c;
 
  for (i = 0, j = strlen(s)-1; i<j; i++, j--) 
  {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}
 
void itoa(int n, char s[])
{
  int i, sign;
 
  if ((sign = n) < 0)       /* записываем знак */
    n = -n;                 /* делаем n положительным числом */
  i = 0;
  do {                      /* генерируем цифры в обратном порядке */
    s[i++] = n % 10 + '0';  /* берем следующую цифру */
  } while ((n /= 10) > 0);  /* удаляем */
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  reverse(s);
}

/*-------- NTP code ----------*/
 
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
  
time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets 
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) 
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) 
    {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }  
  return 0; // return 0 if unable to get the time
}
 

/*-------- NTP code END ----------*/
