//
// Lambda.ino
//
// Sample program for QDispatch library.
//
// 9 Sep 2018 <drifkind@acm.org>
//

// The TripleBlink example using lambda expressions.
// This is advanced stuff. See:
// http://www.drdobbs.com/cpp/lambdas-in-c11/240168241

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
  dispatcher.callEvery(500, (void (*)())([]{ togglePin(led1Pin); }));
  dispatcher.callEvery(300, (void (*)())([]{ togglePin(led2Pin); }));
  dispatcher.callEvery(random(150, 600), (void (*)())([]{ togglePin(led3Pin); }));
}

void loop()
{
  dispatcher.run();
}

void togglePin(byte pin)
{
  digitalWrite(pin, !digitalRead(pin));
}