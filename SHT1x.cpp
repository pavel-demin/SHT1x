/**
 * SHT1x Library
 *
 * Recent elaborations by Louis Thiery (me@louisthiery.com) <hoping to see a GNU or CC license>
 *
 * Copyright 2009 Jonathan Oxer <jon@oxer.com.au> / <www.practicalarduino.com>
 * Based on previous work by:
 *    Maurice Ribble: <www.glacialwanderer.com/hobbyrobotics/?p=5>
 *    Wayne ?: <ragingreality.blogspot.com/2008/01/ardunio-and-sht15.html>
 *
 * Manages communication with SHT1x series (SHT10, SHT11, SHT15)
 * temperature / humidity sensors from Sensirion (www.sensirion.com).
 */
#if (ARDUINO >= 100)
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include "SHT1x.h"

#define DEFAULT_VOLTAGE 5

SHT1x::SHT1x(int dataPin, int clockPin)
{
  _dataPin = dataPin;
  _clockPin = clockPin;
  _dataInputMode = INPUT;
  _setConversionCoeffs(DEFAULT_VOLTAGE);
}

SHT1x::SHT1x(int dataPin, int clockPin, float voltage, bool intPullup)
{
  _dataPin = dataPin;
  _clockPin = clockPin;
  _dataInputMode = (intPullup ? INPUT_PULLUP : INPUT);
  _setConversionCoeffs(voltage);
}

#define VALS_IN_TABLE 5

// dynamically determines conversion coefficients based on voltage
// tables generated by datasheet
void SHT1x::_setConversionCoeffs(float voltage){
  _D2C =   0.01; // for 14 Bit DEGC
  _D2F =   0.018; // for 14 Bit DEGC
  
  const float coeffsC[] = {40.1,39.8,39.7,39.6,39.4};
  const float coeffsF[] = {40.2,39.6,39.5,39.3,38.9};
  const float vals[]={5,4,3.5,3,2.5};
  
  for (int i=1;i<VALS_IN_TABLE;i++){
	if(voltage>vals[i]){
		_D1C=-_linearInterpolation(coeffsC[i-1],coeffsC[i],vals[i-1],voltage);
		_D1F=-_linearInterpolation(coeffsF[i-1],coeffsF[i],vals[i-1],voltage);
		break;
	}
  }
}

float SHT1x::_linearInterpolation(float coeffA, float coeffB, float valA, float input){
        Serial.println((coeffA-coeffB)/valA*input+coeffB);
	return (coeffA-coeffB)/valA*input+coeffB;
}


/* ================  Public methods ================ */

/**
 * Requests, reads, and parses temperatures in C
 */
float SHT1x::readTemperatureC()
{
  //just putting together functions
  requestTemperature();
  return parseTemperatureC(readInTemperature());
}

float SHT1x::parseTemperatureC(int raw){
  return (raw * _D2C) + _D1C;
}


/**
 * Requests, reads, and parses temperatures in F
 */
float SHT1x::readTemperatureF()
{
  requestTemperature();
  return parseTemperatureF(readInTemperature());
}

float SHT1x::parseTemperatureF(int raw){
  // Convert raw value to degrees Fahrenheit
  return (raw * _D2F) + _D1F;
}

/**
 * Temperature Request
 */
void SHT1x::requestTemperature()
{
  int _val;
  // Command to send to the SHT1x to request Temperature
  int _gTempCmd  = 0b00000011;
  sendCommandSHT(_gTempCmd, _dataPin, _clockPin);
}

/**
 * Read-in Temperature
 */
int SHT1x::readInTemperature()
{
  waitForResultSHT(_dataPin);

  _temperatureRaw= getData16SHT(_dataPin, _clockPin); //store in variable for humidity 
  skipCrcSHT(_dataPin, _clockPin);
  return _temperatureRaw;
}

/**
 * Reads current temperature-corrected relative humidity
 */
float SHT1x::readHumidity()
{
  requestHumidity();
  return parseHumidity(readInHumidity());
}

void SHT1x::requestHumidity(){

  // Command to send to the SHT1x to request humidity
  int _gHumidCmd = 0b00000101;

  // Fetch the value from the sensor
  sendCommandSHT(_gHumidCmd, _dataPin, _clockPin);
}

float SHT1x::readInHumidity(){
  int val;                    // Raw humidity value returned from sensor
  waitForResultSHT(_dataPin);
  val = getData16SHT(_dataPin, _clockPin);
  skipCrcSHT(_dataPin, _clockPin);
  return val;
}

float SHT1x::parseHumidity(int raw){
  float linearHumidity;       // Humidity with linear correction applied
  float correctedHumidity;    // Temperature-corrected humidity
  float temperature;          // Raw temperature value

  // Conversion coefficients from SHT15 datasheet
  const float C1 = -4.0;       // for 12 Bit
  const float C2 =  0.0405;    // for 12 Bit
  const float C3 = -0.0000028; // for 12 Bit
  const float T1 =  0.01;      // for 14 Bit
  const float T2 =  0.00008;   // for 14 Bit

  // Apply linear conversion to raw value
  linearHumidity = C1 + C2 * raw + C3 * raw * raw;

  //temperature request should be accomplished right before this!
  temperature = parseTemperatureC(_temperatureRaw);

  // Correct humidity value for current temperature
  correctedHumidity = (temperature - 25.0 ) * (T1 + T2 * raw) + linearHumidity;

  return correctedHumidity;
}

/**
 */
uint8_t SHT1x::shiftIn(int _dataPin, int _clockPin, int _numBits)
{
  int ret = 0;
  int i;
  for (i=0; i<_numBits; ++i)
  {
     digitalWrite(_clockPin, HIGH);
     ret = ret*2 + digitalRead(_dataPin);
     digitalWrite(_clockPin, LOW);
  }
  return(ret);
}

/**
 */
void SHT1x::sendCommandSHT(int _command, int _dataPin, int _clockPin)
{
  int ack;

  // Transmission Start
  pinMode(_dataPin, OUTPUT);
  pinMode(_clockPin, OUTPUT);
  digitalWrite(_dataPin, HIGH);
  digitalWrite(_clockPin, HIGH);
  digitalWrite(_dataPin, LOW);
  digitalWrite(_clockPin, LOW);
  digitalWrite(_clockPin, HIGH);
  digitalWrite(_dataPin, HIGH);
  digitalWrite(_clockPin, LOW);

  // The command (3 msb are address and must be 000, and last 5 bits are command)
  shiftOut(_dataPin, _clockPin, MSBFIRST, _command);

  // Verify we get the correct ack
  digitalWrite(_clockPin, HIGH);
  pinMode(_dataPin, _dataInputMode);
  ack = digitalRead(_dataPin);

  digitalWrite(_clockPin, LOW);
  ack = digitalRead(_dataPin);
}

/**
 */
void SHT1x::waitForResultSHT(int _dataPin)
{
  pinMode(_dataPin, _dataInputMode);
 
  unsigned long int start = millis();
  
  //wait for SHT to show that its ready or move along it timeout is reached
  while( digitalRead(_dataPin) && (millis()-start)<TIMEOUT_MILLIS );
}

/**
 */
int SHT1x::getData16SHT(int _dataPin, int _clockPin)
{
  int val;

  // Get the most significant bits
  pinMode(_dataPin, _dataInputMode);
  pinMode(_clockPin, OUTPUT);
  val = shiftIn(_dataPin, _clockPin, 8);
  val *= 256;

  // Send the required ack
  pinMode(_dataPin, OUTPUT);
  digitalWrite(_dataPin, HIGH);
  digitalWrite(_dataPin, LOW);
  digitalWrite(_clockPin, HIGH);
  digitalWrite(_clockPin, LOW);

  // Get the least significant bits
  pinMode(_dataPin, _dataInputMode);
  val |= shiftIn(_dataPin, _clockPin, 8);

  return val;
}

/**
 */
void SHT1x::skipCrcSHT(int _dataPin, int _clockPin)
{
  // Skip acknowledge to end trans (no CRC)
  pinMode(_dataPin, OUTPUT);
  pinMode(_clockPin, OUTPUT);

  digitalWrite(_dataPin, HIGH);
  digitalWrite(_clockPin, HIGH);
  digitalWrite(_clockPin, LOW);
}
