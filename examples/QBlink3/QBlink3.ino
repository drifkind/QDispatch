//
// QBlink3.ino
//
// Sample program for QDispatch library.
//
// 13 Sep 2018 <drifkind@acm.org>
//

// A third version of the QBlink example. We are going for baroque now.
// One task is the timer. It sends an event every 500 milliseconds. Another
// task watches for that event and toggles the LED whenever it occurs.
// There is no reason to do it this way--it just shows how to use an
// EventBarrier.

#include <QDispatch.h>

DynamicContextPool contextPool;
TaskDispatcher dispatcher(&contextPool);
EventBarrier timerEvent(dispatcher);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  dispatcher.callEvery(1000, timerTask);
  timerEvent.whenever(blinkTask);
}

void loop()
{
  dispatcher.run();
}

void timerTask()
{
  timerEvent.signal();
}

void blinkTask()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}
