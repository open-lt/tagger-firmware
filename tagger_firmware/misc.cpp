/**
 * @file misc.cpp
 * @author gldhnchn (gldhnchn@posteo.de)
 * @brief small classes for tagger firmware
 * 
 */

#include "Arduino.h"
#include "misc.h"
#include <string>

Led::Led(int pin) {
  pinMode(pin, OUTPUT);
  _pin = pin;
}

void Led::light_on() {
  digitalWrite(_pin, 1);
}

void Led::light_off() {
  digitalWrite(_pin, 0);
}

void Led::blinks(int n, int duration_in_ms) {
  for (int i=1; i<=n; i++) {
    light_on();
    delay(duration_in_ms);
    light_off();
    delay(duration_in_ms);
  }
}

Button::Button(int pin) {
  pinMode(pin, INPUT_PULLUP);
  _pin = pin;
}

void Button::read_pin() {
  pressed = !digitalRead(_pin);
  return;
}

void Logger::error(std::string msg) {
  print("ERROR: ");
  print(msg.c_str());
  return;
}

void Logger::warning(std::string msg) {
  print("WARNING: ");
  print(msg.c_str());
  return;
}

void Logger::info(std::string msg) {
  print("INFO: ");
  print(msg.c_str());
  return;
}


void Logger::debug(std::string msg) {
  print("DEBUG: ");
  print(msg.c_str());
  return;
}

void Logger::errorln(std::string msg) {
  print("ERROR: ");
  println(msg.c_str());
  return;
}

void Logger::warningln(std::string msg) {
  print("WARNING: ");
  println(msg.c_str());
  return;
}

void Logger::infoln(std::string msg) {
  print("INFO: ");
  println(msg.c_str());
  return;
}


void Logger::debugln(std::string msg) {
  print("DEBUG: ");
  println(msg.c_str());
  return;
}