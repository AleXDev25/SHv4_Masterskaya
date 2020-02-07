#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <OneWire.h>
#include <Bounce2.h>
#include <LiquidCrystal_I2C.h>

#define RF_CE 10
#define RF_CSN 9
#define DS_T1_PIN 6
#define DS_T2_PIN 7
#define KEY A3
#define KEY_UP 4
#define KEY_DOWN 3
#define PIN_RELAY A1
#define PIN_LED A2

RF24 radio(RF_CE, RF_CSN); // CE, CSN
RF24Network network(radio);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Bounce btn = Bounce();
Bounce btn_up = Bounce();
Bounce btn_down = Bounce();
OneWire ds1(DS_T1_PIN);
OneWire ds2(DS_T2_PIN);

const uint16_t this_node = 02;
const uint16_t base_node = 00;
const uint16_t disp_node = 01;

unsigned long currentMillis;
unsigned long SENS_prevMillis = 0;
const long SENS_interval = 4000;
unsigned long DISP_prevMillis = 0;
const long DISP_interval = 5000;
unsigned long CHANGE_prevMillis = 0;
const long CHANGE_interval = 200;

boolean relay_state = false;
boolean mode = true;

struct DATA_STRUCTURE
{
  float temp;
  float cur_floor_temp;
  float trgt_floor_temp;
} __attribute__((packed));
DATA_STRUCTURE container;

float GetTemp(OneWire &sensor)
{
  byte data[12];
  sensor.reset();
  sensor.write(0xCC);
  sensor.write(0x44, 1);
  delay(50);
  sensor.reset();
  sensor.write(0xCC);
  sensor.write(0xBE);

  for (byte i = 0; i < 10; i++)
  {
    data[i] = sensor.read();
  }
  int tmp = (data[1] << 8) | data[0];
  float temp = (float)tmp / 16.0;
  return temp;
}

void disp()
{
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(container.temp, 1);
  lcd.setCursor(0, 1);
  lcd.print("T: ");
  lcd.print(container.trgt_floor_temp, 1);
  lcd.setCursor(8, 1);
  lcd.print("C: ");
  lcd.print(container.cur_floor_temp, 1);
  if (mode)
  {
    lcd.setCursor(15, 0);
    lcd.print("A");
  }
  else
  {
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }
}

void setup()
{
  lcd.init();
  SPI.begin();
  radio.begin();
  network.begin(70, this_node);

  btn.attach(KEY);
  btn.interval(50);
  btn_up.attach(KEY_UP);
  btn_up.interval(50);
  btn_down.attach(KEY_DOWN);
  btn_down.interval(50);
  pinMode(KEY, INPUT_PULLUP);
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);

  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);

  lcd.backlight();

  container.trgt_floor_temp = 50.0;
}

void loop()
{
  currentMillis = millis();

  network.update();

  while (network.available())
  {
    RF24NetworkHeader receiver;
    network.peek(receiver);
    if (receiver.type == 'N')
    {
      network.read(receiver, &container, sizeof(container));
    }
  }

  btn.update();
  btn_up.update();
  btn_down.update();

  disp();

  if (btn.fell())
  {
    mode = !mode;
  }
  if (btn_up.read() == 0 && currentMillis - CHANGE_prevMillis >= CHANGE_interval)
  {
    CHANGE_prevMillis = currentMillis;
    container.trgt_floor_temp += 1.0;
  }
  if (btn_down.read() == 0 && currentMillis - CHANGE_prevMillis >= CHANGE_interval)
  {
    CHANGE_prevMillis = currentMillis;
    container.trgt_floor_temp -= 1.0;
  }

  if (currentMillis - SENS_prevMillis >= SENS_interval)
  {
    SENS_prevMillis = currentMillis;
    container.temp = GetTemp(ds1);
    delay(10);
    container.cur_floor_temp = GetTemp(ds2);
    lcd.clear();
  }

  if (mode)
  {
    if (container.cur_floor_temp < (container.trgt_floor_temp - 5))
      relay_state = true;
    if (container.cur_floor_temp >= container.trgt_floor_temp)
      relay_state = false;
  }
  if (!mode)
    relay_state = true;

  if (currentMillis - DISP_prevMillis >= DISP_interval)
  {
    DISP_prevMillis = currentMillis;
    RF24NetworkHeader transmitter(base_node, 'N');
    network.write(transmitter, &container, sizeof(container));
  }

  if (relay_state)
  {
    digitalWrite(PIN_RELAY, HIGH);
    digitalWrite(PIN_LED, HIGH);
  }
  if (!relay_state)
  {
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_LED, LOW);
  }
  delay(50);
}