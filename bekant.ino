#include "lin.h"

Lin lin;

void setup() {
  // put your setup code here, to run once:
  lin.begin(19200);

  pinMode(0, INPUT);
  digitalWrite(0, HIGH);

  pinMode(7, INPUT);
  digitalWrite(7, HIGH);
  pinMode(8, INPUT);
  digitalWrite(8, HIGH);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
}

unsigned long t = 0;

void delay_until(unsigned long ms) {
  long end = t + (1000 * ms);
  long d = end - micros();

  // crazy long delay; probably negative wrap-around
  // just return
  if ( d > 1000000 ) {
    t = micros();
    return;
  }
  
  while(d > 1000) {
    delay(1);
    d -= 1000;
  }
  d = end - micros();
  delayMicroseconds(d);
  t = end;
}

enum class Command {
  NONE,
  UP,
  DOWN,
};

enum class State {
  OFF,
  STARTING,
  UP,
  DOWN,
  STOPPING1,
  STOPPING2,
};

State state = State::OFF;

void loop() {
  // put your main code here, to run repeatedly:
  uint8_t empty[] = { 0, 0, 0 };
  uint8_t node_a[4] = { 0, 0, 0, 0 };
  uint8_t node_b[4] = { 0, 0, 0, 0 };
  uint8_t cmd[3] = { 0, 0, 0 };
  uint8_t res = 0;

  // Send ID 11
  lin.send(0x11, empty, 3, 2);
  delay_until(5);

  // Recv from ID 08
  res = lin.recv(0x08, node_a, 3, 2);
  delay_until(5);

  // Recv from ID 09
  res = lin.recv(0x09, node_b, 3, 2);
  delay_until(5);

  // Send ID 10, 6 times
  for(uint8_t i=0; i<6; i++) {
    lin.send(0x10, 0, 0, 2);
    delay_until(5);
  }

  // Send ID 1
  lin.send(0x01, 0, 0, 2);
  delay_until(5);

  uint16_t enc_a = node_a[0] | (node_a[1] << 8);
  uint16_t enc_b = node_b[0] | (node_b[1] << 8);
  uint16_t enc_target = enc_a;

  // Send ID 12
  switch(state) {
    case State::OFF:
      cmd[2] = 0xFC; // 0b11111100
      break;
    case State::STARTING:
      // may also command up ?
      // maybe 0xC4  // 0b10100100
      cmd[2] = 0x86; // 0b10000110
      break;
    case State::UP:
      enc_target = min(enc_a, enc_b);
      // maybe 0x86  // 0x10000110
      cmd[2] = 0x87; // 0b10000111
      break;
    case State::DOWN:
      enc_target = max(enc_a, enc_b);
      cmd[2] = 0x85; // 0b10000101
      break;
    case State::STOPPING1:
      cmd[2] = 0x87; // 0b10000111
      break;
    case State::STOPPING2:
      cmd[2] = 0x84; // 0b10000100
      break;
  }
  cmd[0] = enc_target&0xFF;
  cmd[1] = enc_target>>8;
  lin.send(0x12, cmd, 3, 2);

  // read buttons and compute next state
  Command user_cmd = Command::NONE;
  if(digitalRead(7) == 0) { // UP
    digitalWrite(13, HIGH);
    user_cmd = Command::UP;
  } else if(digitalRead(8) == 0) { // DOWN
    user_cmd = Command::DOWN;
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);    
  }

  switch(state) {
    case State::OFF:
      if(user_cmd != Command::NONE) {
        if( node_a[2] == 0x60 && node_b[2] == 0x60) {
          state = State::STARTING;
        }
      }
      break;
    case State::STARTING:
      if( node_a[2] == 0x02 && node_b[2] == 0x02) {
        switch(user_cmd) {
          case Command::NONE:
            state = State::OFF;
            break;
          case Command::UP:
            state = State::UP;
            break;
          case Command::DOWN:
            state = State::DOWN;
            break;
        }
      }
      break;
    case State::UP:
      if(user_cmd != Command::UP) {
        state = State::STOPPING1;
      }
      break;
    case State::DOWN:
      if(user_cmd != Command::DOWN) {
        state = State::STOPPING1;
      }
      break;
    case State::STOPPING1:
      state = State::STOPPING2;
      break;
    case State::STOPPING2:
      if( node_a[2] == 0x60 && node_b[2] == 0x60) {
        state = State::OFF;
      }
      break;
    default:
      state = State::OFF;
      break;
  }

  // Wait the remaining 150 ms in the cycle
  delay_until(150);
}