#include <Arduino.h>

const int SENSOR1_PIN = 8;
const int SENSOR2_PIN = 9;
const int SENSOR3_PIN = 10;
const int SENSOR4_PIN = 11;


void setup() {
  // put your setup code here, to run once:
  pinMode(SENSOR1_PIN,INPUT);
  pinMode(SENSOR2_PIN,INPUT);
  pinMode(SENSOR3_PIN,INPUT);
  pinMode(SENSOR4_PIN,INPUT);

  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  int S1 = digitalRead(SENSOR1_PIN);
  int S2 = digitalRead(SENSOR2_PIN);
  int S3 = digitalRead(SENSOR3_PIN);
  int S4 = digitalRead(SENSOR4_PIN);

  Serial.print("S1: ");
  Serial.print(S1);
  Serial.print(" | S2: ");
  Serial.print(S2);
  Serial.print(" | S3: ");
  Serial.print(S3);
  Serial.print(" | S4: ");
  Serial.println(S4);

  delay(500);
}
