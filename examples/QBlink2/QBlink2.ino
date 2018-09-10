//
// QBlink2.ino
//
// Sample program for QDispatch library.
//
// 9 Sep 2018 <drifkind@acm.org>
//

// Another version of the QBlink example.

#include <QDispatch.h>

DynamicContextPool contextPool;
TaskDispatcher dispatcher(&contextPool);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  dispatcher.callEvery(1000, blinkTask);
}

void loop()
{
  dispatcher.run();
}

void blinkTask()
{
  digitalWrite(LED_BUILTIN, HIGH);
  dispatcher.callAfter(500, unblinkTask);
}

void unblinkTask()
{
  digitalWrite(LED_BUILTIN, LOW);
}