int RusPIRPin = 5;

int RusSwitch1Pin = 33;
int RusState1Pin = A1;
int RusSwitch2Pin = 29;
int RusState2Pin = A3;
bool bRusL1On = false;
/**/
bool bRusL2On = false;
bool bRusAlreadyOn = false;
bool bRusPIROn = false;

unsigned long lastRusPIRTime = 0;
bool bLastRusPIROn = false;
unsigned long lastRusLivoloEvent = 0;
unsigned long dontCheckStateRus = 0;
unsigned long RusLivoloTimeConst = 15 * 1000; //time, when system is ignoring PIR after manual
                        //turn on/off the light --MAYBE,NOT SURE
unsigned long RusPIRTimeConst = 3 * 1000;//time, when system is ignoring PIR after PIR-controlled
                     //turn on/off the light --MAYBE,NOT SURE

bool Debug = true;

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
  }
  if ((n == 2) && (!bRusL2On))
  {
    SwitchLivolo(2);
  }
}

void LivoloOff(int n)//while turning on and turning off switcher, calling "SwitchLivolo",
{                    //because modifying state of switcher is process of applying voltage to a specific pin
  if ((n == 1) && (bRusL1On))
  {
    SwitchLivolo(1);
  }
  if ((n == 2) && (bRusL2On))
  {
    SwitchLivolo(2);
  }
}


void setup()
{
  if (Debug)
  {
    Serial.begin(9600);
  }

  pinMode(RusPIRPin, INPUT);
  pinMode(RusSwitch1Pin, OUTPUT);
  pinMode(RusState1Pin, INPUT_PULLUP);
  pinMode(RusSwitch2Pin, OUTPUT);
  pinMode(RusState2Pin, INPUT_PULLUP);

  lastRusPIRTime = millis();//means at what time pir was last activated
  lastRusLivoloEvent = millis();//means at what time 1-st/2-nd/both lamp(s) was manually activated/disactivated
  dontCheckStateRus = millis();//delay due to electronic "noise" of PIR
}

void loop() {
  /////////////////////////////////////////////////
  //////////////////////////////  Livolo start

  int state = 0;


  if ((millis() - lastRusLivoloEvent) > RusLivoloTimeConst)
  {
    state = digitalRead(RusPIRPin);

    if (/*(hour()>17 || hour()<10) &&*/ (state == HIGH))//ïðîâåðÿåì äàò÷èê äâèæåíèÿ òîëüêî ñ 17:00 äî 10:00(todo, but now disactivated)
    {
      if (!bRusAlreadyOn)
      {
        LivoloOn(1);
        LivoloOn(2);
        Debug = true; //unlock "Debug" to write text in serial output
        if (Debug)
        {
          Serial.println("Ruslan Livolo On by PIR!");

        }
        bRusAlreadyOn = true;
        bRusPIROn = true;
      }
      lastRusPIRTime = millis();
      //bRusPIROn = true;


    }
    else
    {
      if ((millis() - lastRusPIRTime) > RusPIRTimeConst)
      {
        LivoloOff(1);
        LivoloOff(2);
        if (Debug)
        {
          Serial.println("Ruslan Livolo Off by PIR!");
          Debug = false; //locking "Debug" to prevent spamming "Ruslan Livolo Off by PIR!"
        }
        bRusAlreadyOn = false;
        bRusPIROn = false;
      }
      //bRusPIROn = false;
    }
  }


  state = digitalRead(RusState1Pin);
  if ((millis() - dontCheckStateRus) > 1000)
  {
    if ((state == HIGH && bRusL1On) || (state == LOW && !bRusL1On))
    {
      //state of dat light has changed
      Debug = true;//unlock "Debug" to write text in serial output
      if (Debug)
      {
        Serial.println("Ruslan Livolo 1 manual Event");

      }
      //bL1On = !bL1On;
      lastRusLivoloEvent = millis();
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
      Debug = true;//unlock "Debug" to write text in serial output
      if (Debug)
      {
        Serial.println("Ruslan Livolo 2 manual Event!");

      }
      //bRusL2On = !bRusL2On;
      lastRusLivoloEvent = millis();
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
}
