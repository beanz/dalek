//Workaround arduino pre-processor bug/s
#if 1
__asm volatile ("nop");
#endif

// Uncomment next line if you're Benjie
//#define L298N_MODE 1
// Uncomment next line if you're Bracken
//#define SN754410_MODE 1
// Uncomment next line if you're using the Roboclaw motor controller
#define ROBOCLAW_MODE 1

#ifdef ROBOCLAW_MODE

/*
  ROBOCLAW_MODE is based on
    examples/PacketSerialSimplePWM/PacketSerialSimplePWM.ino

  from:

    http://downloads.ionmc.com/code/arduino.zip

   It requires RoboClaw.cpp and RoboClaw.h from the above zip to be
   placed in directory $HOME/Arduino/libraries/RoboClaw

   It assumes M1 is left motor and M2 is right motor and arduino pins
   9 (RX) and 10 (TX) are connected to RoboClaw S2 and S1 respectively.
*/

// ROBOCLAW_MODE doesn't change the PWM speeds
#define TIME_MULTIPLIER 1

#else

// Because we're adjusting the PWM speeds (see TCCR0B and TCCR1B)
// we're also adjusting the millis() and similar clocks so we must
// remember to multiply durations by this!
#define TIME_MULTIPLIER 64

#endif

// Maximum command length, including initial command code and terminating \n
// (2 bytes more than the maximum payload)
#define BUFFER_SIZE 64
// Watchdog timer duration in milliseconds
#define WATCHDOG_TIMER_DURATION 100

#ifdef ROBOCLAW_MODE

#include <SoftwareSerial.h>
#include "RoboClaw.h"
//See limitations of Arduino SoftwareSerial
SoftwareSerial serial(9, 10);
RoboClaw roboclaw(&serial,10000);

#define address 0x80

#elif defined L298N_MODE
int leftForwardPin = 12;
int leftBackwardPin = 11;
int leftEnablePin = 10;
int rightForwardPin = 8;
int rightBackwardPin = 7;
int rightEnablePin = 6;
#else
int leftForwardPin = 5;
int leftBackwardPin = 6;
int rightForwardPin = 9;
int rightBackwardPin = 10;
#endif
int ledPin = 13;

int bufferLength = 0;
char buffer[BUFFER_SIZE+1];

long last_command_time = 0;

#define SUCCESS 0
#define E_INCORRECT_PAYLOAD_LENGTH 1
#define E_UNKNOWN_COMMAND 2
#define E_INVALID_COMMAND 3
#define E_INVALID_PAYLOAD 4

// the setup routine runs once when you press reset:
void setup()  {

  memset(buffer, 0, BUFFER_SIZE+1);
  pinMode(ledPin, OUTPUT);

#ifdef ROBOCLAW_MODE
  roboclaw.begin(38400);
  roboclaw.ForwardM1(address, 0); //start Motor1 forward at half speed
  roboclaw.BackwardM2(address, 0); //start Motor2 backward at half speed

#else
  //increase PWM freq beyond audible range
  TCCR0B = TCCR0B & 0b11111000 | 1; //set PWM ports 5 & 6 to 62.5KHz AFFECTS MILIS AND DELAY
  TCCR1B = TCCR1B & 0b11111000 | 1; //set PWM ports 9 & 10 to ~31KHz

  pinMode(leftForwardPin, OUTPUT);
  pinMode(leftBackwardPin, OUTPUT);
  pinMode(rightForwardPin, OUTPUT);
  pinMode(rightBackwardPin, OUTPUT);
#ifdef L298N_MODE
  pinMode(leftEnablePin, OUTPUT);
  pinMode(rightEnablePin, OUTPUT);
  analogWrite(leftForwardPin, 0);
  analogWrite(leftBackwardPin, 0);
  analogWrite(leftEnablePin, 0);
  analogWrite(rightForwardPin, 0);
  analogWrite(rightBackwardPin, 0);
  analogWrite(leftEnablePin, 0);
#elif defined SN754410_MODE
  analogWrite(leftForwardPin, 0);
  analogWrite(leftBackwardPin, 0);
  analogWrite(rightForwardPin, 0);
  analogWrite(rightBackwardPin, 0);
#else
  analogWrite(leftForwardPin, 255);
  analogWrite(leftBackwardPin, 255);
  analogWrite(rightForwardPin, 255);
  analogWrite(rightBackwardPin, 255);
#endif
#endif
  Serial.begin(115200);
  last_command_time = millis();
}

// Command 'L': 0 - off, 1 - on
int cmdLED(char *str, int len) {
  if (len != 1) return E_INCORRECT_PAYLOAD_LENGTH;
  if (str[0] == '0') {
    digitalWrite(ledPin, LOW);
  } else if (str[0] == '1') {
    digitalWrite(ledPin, HIGH);
  } else {
    return E_INVALID_PAYLOAD;
  }
  return SUCCESS;
}

// Command 'M', format: "+a0-ff": 161/256 left forwards, 256/256 right backwards
int cmdMotor(char *str, int len) {
  if (len != 5) return E_INCORRECT_PAYLOAD_LENGTH;
  int left = 0;
  int right = 0;
  int sign = 0;
  int vars = sscanf(str, "%2x%2x%x", &left, &right, &sign);
  if (vars < 2) { // < 2 includes EOF which is -1
    return E_INVALID_PAYLOAD;
  }
  if (sign & 1) {
    left = -left;
  }
  if (sign & 2) {
    right = -right;
  }
  go(left, right);
  return SUCCESS;
}

void go(int left, int right) {
#ifdef ROBOCLAW_MODE
  if (left >= 0) {
    roboclaw.ForwardM1(address, left/2);
  } else {
    roboclaw.BackwardM1(address, abs(left/2));
  }

  if (right >= 0) {
    roboclaw.ForwardM2(address, right/2);
  } else {
    roboclaw.BackwardM2(address, abs(right/2));
  }

#elif defined L298N_MODE
  if (left >= 0) {
    digitalWrite(leftForwardPin, HIGH);
    digitalWrite(leftBackwardPin, LOW);
  } else {
    digitalWrite(leftForwardPin, LOW);
    digitalWrite(leftBackwardPin, HIGH);
  }
  
  if (right >= 0) {
    digitalWrite(rightForwardPin, HIGH);
    digitalWrite(rightBackwardPin, LOW);
  } else {
    digitalWrite(rightForwardPin, LOW);
    digitalWrite(rightBackwardPin, HIGH);
  }
  analogWrite(leftEnablePin, abs(left));
  analogWrite(rightEnablePin, abs(right));
#elif defined SN754410_MODE
  if (left >= 0) {
    analogWrite(leftBackwardPin, 0);
    analogWrite(leftForwardPin, left);
  } else {
    analogWrite(leftForwardPin, 0);
    analogWrite(leftBackwardPin, abs(left));
  }

  if (right >= 0) {
    analogWrite(rightBackwardPin, 0);
    analogWrite(rightForwardPin, right);
  } else {
    analogWrite(rightForwardPin, 0);
    analogWrite(rightBackwardPin, abs(right));
  }
#else
  if (left >= 0) {
    analogWrite(leftForwardPin, 255 - left);
    analogWrite(leftBackwardPin, 255);
  } else {
    analogWrite(leftForwardPin, 255);
    analogWrite(leftBackwardPin, left);
  }

  if (right >= 0) {
    analogWrite(rightForwardPin, 255 - right);
    analogWrite(rightBackwardPin, 255);
  } else {
    analogWrite(rightForwardPin, 255);
    analogWrite(rightBackwardPin, right);
  }
#endif
}

// Shrinks buffer by shifting the characters count bytes left.
void shiftBuffer(int count) {
  memmove(buffer, buffer+count, BUFFER_SIZE - count);
  memset(buffer + BUFFER_SIZE - count, 0, count + 1);
  bufferLength -= count;
}

void loop()  {
  int availableBytes = Serial.available();
  if (availableBytes <= 0) {
    if (millis() - last_command_time > WATCHDOG_TIMER_DURATION * TIME_MULTIPLIER) {
      go(0, 0);
    }
    return;
  }
  int maxLen = BUFFER_SIZE - bufferLength;
  if (availableBytes > maxLen) { // Ensure we don't overflow buffer
    availableBytes = maxLen;
  }
  Serial.readBytes(buffer + bufferLength, availableBytes);
  bufferLength += availableBytes;
  char *newlinePos;
  while (newlinePos = (char *)memchr(buffer, '\n', bufferLength)) {
    newlinePos[0] = '\0'; // Change \n to \0 temporarily
    int result = E_UNKNOWN_COMMAND;
    if (buffer[0] == 'M') {
      result = cmdMotor(buffer + 1, (newlinePos - buffer) - 1);
    } else if (buffer[0] == 'L') {
      result = cmdLED(buffer + 1, (newlinePos - buffer) - 1);
    }
    Serial.println(result, HEX);
    shiftBuffer((newlinePos - buffer) + 1); // +1 because we want to lose the \0 too
  }
  if (bufferLength == BUFFER_SIZE) {
    shiftBuffer(BUFFER_SIZE); // No newlines, buffer full: start over
    Serial.println(E_INVALID_COMMAND, HEX);
  }
  last_command_time = millis();
}
