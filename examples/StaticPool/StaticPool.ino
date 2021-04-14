//
// StaticPool.ino
//
// Sample program for QDispatch library.
//
// 11 Sep 2018 <drifkind@acm.org>
//

// This example uses a ContextPool implementation that allocates a
// fixed block of memory at compile time, so you can use a context pool
// without the evil of dynamic allocation (if it is evil)

#include <QDispatch.h>

#define POOL_SIZE 5
StaticContextPool<POOL_SIZE> contextPool;
TaskDispatcher dispatcher(&contextPool);

// The rest of this is just the "QBlink" sample.

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
