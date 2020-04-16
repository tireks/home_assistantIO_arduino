const int buttonPin = 52; // номер порта нашей кнопки
const int ledPin =    48; // номер порта светодиода
void setup() {
    // устанавливаем порт светодиода на выход
    pinMode(ledPin, OUTPUT);
    // устанавливаем порт кнопки на вход
    pinMode(buttonPin, INPUT);
}
void loop() {
    // читаем состояние порта кнопки и записываем в переменную
    int buttonState = digitalRead(buttonPin);
    // делаем простую проверку нашей переменной, если на входе в порт кнопки присутствует напряжение - включаем светодиод, иначе - выключаем
    if (buttonState == HIGH) {
        // подаем 5 вольт на порт наешго светодиода
        digitalWrite(ledPin, HIGH);
    
    } else {
        // выключаем светодиод
        digitalWrite(ledPin, LOW);
    
    }
}
