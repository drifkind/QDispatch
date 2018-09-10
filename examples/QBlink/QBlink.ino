//
// QBlink.ino
//
// Sample program for QDispatch library.
//
// 9 Sep 2018 <drifkind@acm.org>
//

// The QDispatch version of the standard Ardiuno Blink example.
//
// Equivalent code:
//
// void loop()
// {
//   digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
//   delay(500);
// }

#include <QDispatch.h>

DynamicContextPool contextPool;
TaskDispatcher dispatcher(&contextPool);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  dispatcher.callEvery(500, blinkTask);
}

void loop()
{
  dispatcher.run();
}

void blinkTask()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}