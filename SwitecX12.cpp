/*
 *  SwitecX12 Arduino Library
 *  Guy Carpenter, Clearwater Software - 2017
 *
 *  Licensed under the BSD2 license, see license.txt for details.
 *
 *  All text above must be included in any redistribution.
 */

#include <Arduino.h>
#include "SwitecX12.h"

// This table defines the acceleration curve.
// 1st value is the speed step, 2nd value is delay in microseconds
// 1st value in each row must be > 1st value in subsequent row
// 1st value in last row is used for maxVel

// original X12 table
static unsigned short olddefaultAccelTable[][2] = {
  {   20, 800},
  {   50, 400},
  {  100, 200},
  {  150, 150},
  {  300, 90}
};

// X25 table *4, /4
static unsigned short defaultAccelTable[][2] = {
  {   80, 750},
  {  200, 375},
  {  400, 250},
  {  600, 200},
  { 1200, 150}
};

static unsigned short newdefaultAccelTable[][2] = {
  {    80, 750},
  {   100, 350},
  {   150, 200},
  {   200, 150},
  {   300,  90}
};

const int stepPulseMicrosec = 1;
const int resetStepMicrosec = 400;
#define DEFAULT_ACCEL_TABLE_SIZE (sizeof(defaultAccelTable)/sizeof(*defaultAccelTable))

SwitecX12::SwitecX12(unsigned int steps, unsigned char pinStep, unsigned char pinDir, unsigned char pinReset)

{
  this->steps = steps;
  this->pinStep = pinStep;
  this->pinDir = pinDir;
  this->pinReset = pinReset;
  pinMode(pinStep, OUTPUT);
  pinMode(pinDir, OUTPUT);
  pinMode(pinReset, OUTPUT);
  digitalWrite(pinStep, LOW);
  digitalWrite(pinDir, LOW);
  digitalWrite(pinReset, LOW);
//  pinMode(13, OUTPUT);

  dir = 0;
  vel = 0;
  stopped = true;
  currentStep = 0;
  targetStep = 0;

  accelTable = defaultAccelTable;
  accelTable = newdefaultAccelTable;
//  maxVel = defaultAccelTable[DEFAULT_ACCEL_TABLE_SIZE-1][0]; // last value in table.
  maxVel = accelTable[DEFAULT_ACCEL_TABLE_SIZE-1][0]; // last value in table.
  digitalWrite(pinReset, HIGH);
}

void SwitecX12::step(int dir)
{
  digitalWrite(pinDir, dir > 0 ? HIGH : LOW);
//  digitalWrite(pinDir, dir > 0 ? LOW : HIGH);
//  digitalWrite(13, vel == maxVel ? HIGH : LOW);
  digitalWrite(pinStep, HIGH);
  delayMicroseconds(stepPulseMicrosec);
  digitalWrite(pinStep, LOW);
  currentStep += dir;
  if (currentStep > 65000) {
    currentStep = 0;
  }
}

void SwitecX12::stepTo(int position)
{
  int count;
  int dir;
  if (position > currentStep) {
    dir = 1;
    count = position - currentStep;
  } else {
    dir = -1;
    count = currentStep - position;
  }
  for (int i=0;i<count;i++) {
    step(dir);
    delayMicroseconds(resetStepMicrosec);
  }
}

void SwitecX12::zero()
{
  currentStep = steps - 1;
  stepTo(0);
  currentStep = targetStep = 0;
  vel = 0;
  dir = 0;
}

void SwitecX12::full()
{
  currentStep = 0;
  stepTo(steps-1);
  currentStep = targetStep = steps-1;
  vel = 0;
  dir = 0;
}

void SwitecX12::advance()
{
  // detect stopped state
  if (currentStep==targetStep && vel==0) {
    stopped = true;
    dir = 0;
    time0 = micros();
    return;
  }

  // if stopped, determine direction
  if (vel==0) {
    dir = currentStep<targetStep ? 1 : -1;
    // do not set to 0 or it could go negative in case 2 below
    vel = 1;
  }

  step(dir);

  // determine delta, number of steps in current direction to target.
  // may be negative if we are headed away from target
  int delta = dir>0 ? targetStep-currentStep : currentStep-targetStep;
 
if (debug) {
Serial.print("  target ");
Serial.print(targetStep);
Serial.print("  current ");
Serial.print(currentStep);
Serial.print("  vel ");
Serial.print(vel);
Serial.print("  delta ");
Serial.println(delta);
}

  if (delta>0) {
    // case 1 : moving towards target (maybe under accel or decel)
    if (delta < vel) {
      // time to declerate
      vel--;
    } else if (vel < maxVel) {
      // accelerating
      vel++;
    } else {
      // at full speed - stay there
    }
  } else {
    // case 2 : at or moving away from target (slow down!)
    vel--;
  }

  // vel now defines delay
  unsigned char i = 0;
  // this is why vel must not be greater than the last vel in the table.
  while (accelTable[i][0]<vel) {
    i++;
  }
  microDelay = accelTable[i][1];
  time0 = micros();
}

void SwitecX12::setPosition(unsigned int pos)
{
  // pos is unsigned so don't need to check for <0
  if (pos >= steps) pos = steps-1;
  targetStep = pos;
  if (stopped) {
    // reset the timer to avoid possible time overflow giving spurious deltas
    stopped = false;
    time0 = micros();
    microDelay = 0;
  }
}

void SwitecX12::update()
{
  if (!stopped) {
    unsigned long delta = micros() - time0;
    if (delta >= microDelay) {
      advance();
    }
  }
}
