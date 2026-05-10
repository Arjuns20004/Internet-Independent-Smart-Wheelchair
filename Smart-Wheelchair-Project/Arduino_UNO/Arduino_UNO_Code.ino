#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

/* ================= RFID ================= */
#define SS_PIN 8
#define RST_PIN 7
MFRC522 rfid(SS_PIN, RST_PIN);

/* ================= ACCEL ================= */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

/* ================= SERIAL ================= */
SoftwareSerial voiceSerial(4,5);   // Changed from 12,13
SoftwareSerial gsm(3,2);

/* ================= ULTRASONIC ================= */
#define TRIG A2
#define ECHO A1

/* ================= EYE BLINK ================= */
#define BLINK_PIN A0

/* ================= RELAY ================= */
#define RELAY_LEFT 9
#define RELAY_RIGHT 10

/* ================= STATES ================= */
bool unlocked = false;
bool fallSent = false;

enum Move {STOP, FORWARD, BACKWARD, LEFT, RIGHT};
Move requestedMove = STOP;
Move lastMove = STOP;

/* ================= VOICE ================= */
byte voiceFrame[4];
byte voiceIndex = 0;

/* ================= MODE ================= */
unsigned long lastSwitch = 0;
bool voiceMode = true;

/* ================= BLINK ================= */
bool lastBlinkState = HIGH;
int blinkCount = 0;
unsigned long lastBlinkTime = 0;
unsigned long blinkGap = 900;

/* ================= SETUP ================= */
void setup(){

  Serial.begin(9600);

  voiceSerial.begin(9600);
  gsm.begin(9600);

  SPI.begin();
  rfid.PCD_Init();

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(BLINK_PIN, INPUT);

  pinMode(RELAY_LEFT, OUTPUT);
  pinMode(RELAY_RIGHT, OUTPUT);

  stopMotor();

  if(!accel.begin()){

    Serial.println("ADXL345 FAIL");

    while(1);
  }

  accel.setRange(ADXL345_RANGE_16_G);

  Serial.println("UNO READY");
  Serial.println("SYSTEM LOCKED - SCAN RFID");
}

/* ================= LOOP ================= */
void loop(){

  checkRFID();

  if(!unlocked){

    requestedMove = STOP;

    stopMotor();

    Serial.println("WAITING FOR RFID...");

    delay(500);

    return;
  }

  switchController();

  if(voiceMode)
    readVoice();
  else
    readESP();

  readEyeBlink();

  checkObstacle();
  checkFall();

  applyMotor();
}

/* ================= RFID ================= */
void checkRFID(){

  if (!rfid.PICC_IsNewCardPresent())
    return;

  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println("RFID CARD DETECTED");

  String tagID = "";

  for(byte i=0; i<rfid.uid.size; i++){

    if(rfid.uid.uidByte[i] < 0x10)
      tagID += "0";

    tagID += String(rfid.uid.uidByte[i], HEX);
  }

  tagID.toUpperCase();

  Serial.print("TAG UID: ");
  Serial.println(tagID);

  if(tagID == "82C36506"){

    unlocked = true;

    Serial.println("ACCESS GRANTED - SYSTEM UNLOCKED");
  }
  else{

    unlocked = false;

    requestedMove = STOP;

    Serial.println("ACCESS DENIED");
  }

  rfid.PICC_HaltA();
}

/* ================= MODE SWITCH ================= */
void switchController(){

  if(millis() - lastSwitch >= 2000){

    voiceMode = !voiceMode;

    lastSwitch = millis();

    if(voiceMode){

      voiceSerial.listen();

      Serial.println("MODE -> VOICE");
    }
    else{

      Serial.println("MODE -> ESP");
    }
  }
}

/* ================= ESP COMMAND ================= */
void readESP(){

  while(Serial.available()){

    char cmd = Serial.read();

    if(cmd=='F'){

      requestedMove = FORWARD;

      Serial.println("ESP -> FORWARD");
    }

    else if(cmd=='B'){

      requestedMove = BACKWARD;

      Serial.println("ESP -> BACKWARD");
    }

    else if(cmd=='L'){

      requestedMove = LEFT;

      Serial.println("ESP -> LEFT");
    }

    else if(cmd=='R'){

      requestedMove = RIGHT;

      Serial.println("ESP -> RIGHT");
    }

    else if(cmd=='S'){

      requestedMove = STOP;

      Serial.println("ESP -> STOP");
    }
  }
}

/* ================= VOICE ================= */
void readVoice(){

  voiceSerial.listen();

  while(voiceSerial.available()){

    voiceFrame[voiceIndex++] = voiceSerial.read();

    if(voiceIndex == 4){

      if(voiceFrame[0]==0xA1 && voiceFrame[1]==0xB2 && voiceFrame[2]==0x90){

        requestedMove = FORWARD;

        Serial.println("VOICE -> FORWARD");
      }

      else if(voiceFrame[0]==0xA1 && voiceFrame[1]==0xB2){

        requestedMove = BACKWARD;

        Serial.println("VOICE -> BACKWARD");
      }

      else if(voiceFrame[0]==0xC3 && voiceFrame[1]==0xD4 && voiceFrame[2]==0x45){

        requestedMove = LEFT;

        Serial.println("VOICE -> LEFT");
      }

      else if(voiceFrame[0]==0xC3 && voiceFrame[1]==0xD4){

        requestedMove = RIGHT;

        Serial.println("VOICE -> RIGHT");
      }

      else if(voiceFrame[0]==0xE5 && voiceFrame[1]==0xF6){

        requestedMove = STOP;

        Serial.println("VOICE -> STOP");
      }

      voiceIndex = 0;
    }
  }
}

/* ================= EYE BLINK ================= */
void readEyeBlink(){

  bool state = digitalRead(BLINK_PIN);

  if(lastBlinkState == HIGH && state == LOW){

    blinkCount++;

    lastBlinkTime = millis();

    Serial.print("BLINK COUNT: ");
    Serial.println(blinkCount);
  }

  lastBlinkState = state;

  if(blinkCount > 0 && millis() - lastBlinkTime > blinkGap){

    if(blinkCount == 2){

      requestedMove = STOP;

      Serial.println("BLINK -> STOP");
    }

    else if(blinkCount == 3){

      requestedMove = RIGHT;

      Serial.println("BLINK -> RIGHT");
    }

    else if(blinkCount == 4){

      requestedMove = LEFT;

      Serial.println("BLINK -> LEFT");
    }

    else if(blinkCount == 5){

      requestedMove = FORWARD;

      Serial.println("BLINK -> FORWARD");
    }

    blinkCount = 0;
  }
}

/* ================= ULTRASONIC ================= */
void checkObstacle(){

  static unsigned long lastCheck = 0;

  if(millis() - lastCheck < 100)
    return;

  lastCheck = millis();

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 15000);

  int distance = duration * 0.034 / 2;

  if(distance > 0 && distance < 30){

    requestedMove = STOP;

    Serial.println("OBSTACLE DETECTED - MOTOR STOP");
  }
}

/* ================= FALL ================= */
void checkFall(){

  sensors_event_t event;

  accel.getEvent(&event);

  float pitch =
  atan2(event.acceleration.x,
  sqrt(event.acceleration.y * event.acceleration.y +
       event.acceleration.z * event.acceleration.z))
  *180/PI;

  if(abs(pitch) > 60){

    requestedMove = STOP;

    Serial.println("FALL DETECTED");

    if(!fallSent){

      sendSMS("FALL DETECTED");

      Serial.println("SMS SENT");

      fallSent = true;
    }
  }
  else{

    fallSent = false;
  }
}

/* ================= MOTOR ================= */
void applyMotor(){

  if(requestedMove == lastMove)
    return;

  switch(requestedMove){

    case FORWARD:
      forward();
      break;

    case BACKWARD:
      backward();
      break;

    case LEFT:
      left();
      break;

    case RIGHT:
      right();
      break;

    default:
      stopMotor();
      break;
  }

  lastMove = requestedMove;
}

/* ================= RELAY CONTROL ================= */
void forward(){

  digitalWrite(RELAY_LEFT, LOW);
  digitalWrite(RELAY_RIGHT, LOW);

  Serial.println("MOTOR -> FORWARD");
}

void backward(){

  digitalWrite(RELAY_LEFT, LOW);
  digitalWrite(RELAY_RIGHT, LOW);

  Serial.println("MOTOR -> BACKWARD");
}

void left(){

  digitalWrite(RELAY_LEFT, HIGH);
  digitalWrite(RELAY_RIGHT, LOW);

  Serial.println("MOTOR -> LEFT");
}

void right(){

  digitalWrite(RELAY_LEFT, LOW);
  digitalWrite(RELAY_RIGHT, HIGH);

  Serial.println("MOTOR -> RIGHT");
}

void stopMotor(){

  digitalWrite(RELAY_LEFT, HIGH);
  digitalWrite(RELAY_RIGHT, HIGH);

  Serial.println("MOTOR -> STOP");
}

/* ================= GSM ================= */
void sendSMS(String msg){

  gsm.listen();

  gsm.println("AT+CMGF=1");
  delay(1000);

  gsm.println("AT+CMGS=\"+919345636893\"");
  delay(1000);

  gsm.print(msg);
  delay(100);

  gsm.write(26);
}
