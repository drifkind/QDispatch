//
// TripleBlink.ino
//
// Sample program for QDispatch library.
//
// 9 Sep 2018 <drifkind@acm.org>
//

// Blink three LEDs at different rates.

#include <QDispatch.h>

DynamicContextPool contextPool;
TaskDispatcher dispatcher(&contextPool);

const byte led1Pin = LED_BUILTIN;
const byte led2Pin = 3;
const byte led3Pin = 4;

void setup()
{
  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);
  pinMode(led3Pin, OUTPUT);
  dispatcher.callEvery(500, blink1Task);
  dispatcher.callEvery(300, blink2Task);
  dispatcher.callEvery(random(150, 600), blink3Task);
}

void loop()
{
  dispatcher.run();
}

void blink1Task()
{
  digitalWrite(led1Pin, !digitalRead(led1Pin));
}

void blink2Task()
{
  digitalWrite(led2Pin, !digitalRead(led2Pin));
}

void blink3Task()
{
  digitalWrite(led3Pin, !digitalRead(led3Pin));
}