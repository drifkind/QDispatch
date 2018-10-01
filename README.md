# QDispatch - Synchronous Task Library

`QDispatch` is a synchronous task library for Arduino (and compatible
devices). "Synchronous" means that, once a task starts, it has full control of the
processor until it finishes. This can also be called "cooperative
multitasking".

Multiple tasks are managed in two different ways. One is the _task dispatcher_,
used for tasks that are run regularly at a given time interval, or once after
a given time delay. The other is a _event barrier_, which runs tasks when a
particular event occurs.

The purpose of this is to make an application _look_ like it is asynchronous,
with multiple activities happening at the same time. The activities are cut up
into smaller pieces, each one running in a single short burst. When it reaches
a point where it needs to delay a while, or wait for some external event to
happen, it will schedule another task (or the same one again), then terminate.

## Installation and Use

`QDispatch` is an Arduino library. It can be installed from the IDE
Library Manager, or by copying the folder containing its source code into the
`libraries` directory.

To use, include the `QDispatch` header in your source code. When you use a
class defined in the header, the library will link automatically.

```c++
#include <QDispatch.h>
```

## Getting Started

There are three important classes defined in `QDispatch.h`:

- `TaskContext` represents a task.
- `TaskDispatcher` schedules and executes tasks based on time.
- `EventBarrier` executes tasks when specific events occur.

### TaskContext

```c++
void taskProcedure();
TaskContext theContext(taskProcedure);
```

This version of the `TaskContext` declaration (there are others) creates a
task that will execute the specified procedure. The task procedure takes no
parameters and returns no value.

### TaskDispatcher

```c++
TaskDispatcher dispatcher;
```

The dispatcher keeps a list of tasks that are scheduled to execute. Most
programs will have just one `TaskDispatcher`.

```c++
void setup()
{
    dispatcher.callAfter(theContext, 1000);
    dispatcher.callEvery(anotherContext, 250);
}
```

By default, times are specified in milliseconds, like the built-in functions
`delay()` and `millis()`. Note that `callEvery()` will schedule the first
run of the task immediately, then later ones after the specified time. If
you want different behavior, use `schedule()`, which lets you specify both
the initial delay and the interval:

```c++
dispatcher.schedule(typematicTask, 1000, 125);
```

These methods add the task to the dispatcher's list. It will not actually 
execute anything until you tell it to:

```c++
void loop()
{
    dispatcher.run();
}
```

`run()` looks at the first task on the list (ordered by start time). If it
is ready to go, the dispatcher executes it. If the task is set to run
repeatedly, it also schedules the next iteration; otherwise it is removed
from the list.

A scheduled task can be canceled using the `cancel()` methods of either
the context or the dispatcher. You can also cancel all scheduled tasks.

```c++
theContext.cancel();
dispatcher.cancel(theContext);
dispatcher.cancelAll();
```

### EventBarrier

```c++
EventBarrier event(dispatcher);
```

An event barrier must be associated with a task dispatcher. The barrier
also keeps a list of waiting tasks.

```c++
event.when(theContext);
anotherEvent.whenever(anotherContext);
```

The `when()` method adds a task to the list once. `whenever()` sets
the target to return to the list after it is dispatched.

```c++
event.signal();
anotherEvent.signalAll();
```

The `signal()` method dispatches the first task on the waiting list. More
specifially, it moves the task into the dispatcher's queue, with execution
time set to "right now". `signalAll()` does the same for all tasks, if more
than one is waiting.

Tasks waiting for an event can be canceled in much the same way as with
a dispatcher. In addition, calling `cancelAll()` on the dispatcher will
cancel tasks waiting for all of its associated events.

```c++
anotherContext.cancel();
event.cancel(theContext);
event.cancelAll();
dispatcher.cancelAll();
```

## More Features

### Object Method Tasks

A task procedure can be a method of an object. Executing the task calls
the method on that specific object instance, so you have to initialize
the context with both the method (function) name and a pointer to the
object.

```c++
class MyClass {
    void taskMethod();
};

MyClass myObject;
TaskContext objectContext(&MyClass::taskMethod, &myObject);
```

The (tortured) syntax above is the C++ way to specify a pointer to
a method. Fortunately, you only have to remember this one bit of
syntax, and do not have to understand it.

### Context Tags

A task context has one extra piece of data, a `void *` called the tag.
You can set it when you initialize the context, or read or write it later.

```c++
TaskContext taggedContext(taskProcedure, (void *)&buffer);
...
taggedContext.tag = NULL;
```

One use of the tag is with the `cancelAll()` methods. If you use this
variation, you can cancel _every_ context with the same tag.

When you initialize a context with an object method, the tag will
be set to the object pointer. This makes it easy to cancel all the
tasks related to a particular object.

```c++
dispatcher.cancelAll((void *)&myObject);
```

### Initializing a Context

You do not have to specify a task procedure when you create a
context. You can also name the procedure when you schedule the
task.

```c++
dispatcher.callAfter(theContext, 1000, taskProcedure);
dispatcher.callEvery(anotherContext, 250, &MyClass::taskMethod, &myObject);
event.when(eventContext, eventProcedure);
```

You can also cancel all tasks using this procedure:

```c++
dispatcher.cancelAll(taskProcedure);
dispatcher.cancelAll(&MyClass::taskMethod, &myObject);
```

### Allocating Contexts

At this point, declaring a task context is just red tape. It is
easier to let the library allocate and manage contexts so you can 
ignore them. This is the function of a context pool.

```c++
DynamicContextPool contextPool;
TaskDispatcher(&contextPool);
```

Now you can use another set of variations on the dispatcher and 
event barrier methods:

```c++
dispatcher.callAfter(1000, taskProcedure);
dispatcher.callEvery(250, &MyClass::taskMethod, &myObject);
event.when(eventProcedure);
```

When you schedule a task without specifying a context, the dispatcher
allocates one from the context pool, if you supplied one when you
created the dispatcher. An event barrier does the same, using the
context pool for its associated dispatcher.

`ContextPool` is an abstract method. `DynamicContextPool` is a specific
implementation of that method, that uses the C++ `new` keyword
(equivalent to the `malloc()` function) to allocate a new context.
When the task finishes or is canceled, the context goes back into
the pool and is available for reuse.

Many Arduino programmers try to avoid dynamic allocation. If you
prefer, you can stick to allocating your own contexts. Another option
is a `StaticContextPool`, which is part of the sample code.

**Note:** All functions that schedule a task without specifying
a context will cancel _any_ other tasks with the same task procedure
(same method and object in the case of an object method). If you
make multiple calls to schedule a certain task, you will end up with
only one instance of that task.

## Advanced Features

### The Timing Function

By default, the task dispatcher measures times in milliseconds. It is 
possible to use a different time base---for example, microseconds, as 
measured by the built-in `micros()` function. To do this, supply the 
dispatcher with a different timing function when declaring it.

```c++
TaskDispatcher dispatcher(micros);
```

The 32-bit `millis()` function wraps around about every 49 days, and
`micros()` wraps about every 70 minutes. The maximum delay you can
use with `QDispatch` is about half that amount, respectively. If you
keep within this limit and call `dispatcher.run()` reasonably often,
there will be no errors when the timer wraps around.

### Scheduling Policy

Scheduling policy is how the dispatcher decides the next time that
a recurring task will run. In a perfect world, a task set to run
every 100 milliseconds will run forever, perfectly in time, every
100 milliseconds. This may not be a perfect world.

For that to work, the dispatcher must be run frequently, at least
several times a millisecond. That is only possible if no task ever
takes more than a fraction of a millisecond. Otherwise a task may
not start on time, and timing errors occur and can build up.

There are four choices for scheduling policy, depening on how
critical exact timing is in your application:

- INTERVAL: The repeat interval for a task is measured from the
end of one run until the beginning of the next. Any delays cause
the whole system to slow down.
- CYCLE: The repeat interval is measured from the _beginning_ of
one run until the beginning of the next. This compensates for
delays caused by the current task, but not by others.
- TIMING: Repeat intervals are kept exactly on schedule. If one run 
is delayed, the next one will be moved up to compensate for it.

```c++
dispatcher.schedulingPolicy = TaskDispatcher::INTERVAL;
```

The default setting is INTERVAL.

### Foreground and Background

It is possible to build a program entirely out of tasks, so that
the only thing in the main loop is `dispatcher.run()`. Another
approach is to use tasks for interfacing with hardware, and other
things that require polling, and let your main program have a
more conventional structure.

The only requirement for this is that the foreground (main)
program calls `run()` occasionally to let background tasks run.
It should _not_ call `delay()`, or any other function that stops
execution without calling `run()`.

Two functions are provided to help:

```c++
TaskDispatcher::delay(long ticks);
```

Like `delay()`, but calls
`run()` repeatedly while waiting. The delay is measured in whatever
intervals the dispatcher's timing function measures.

```c++
EventBarrier::wait(long ticks)
```

Stops the foreground program until a background task calls 
`signal()` on the event. If this happens before time `ticks` elapses, 
`wait()` returns `true`. Otherwise it returns `false`. To wait
forever, omit the `ticks` parameter.

## Really Advanced Features

To experiment with other possible approaches, I wrote `QDispatch` so
that it will work correctly if called recursively---that is, if a
task calls `dispatcher.run()` or `dispatcher.delay()` or `event.wait()`.
This is similar to what might happen in an interrupt-driven system if
high-priority interrupts can interrupt lower-priority ones. There is no
priority, though---the result is more like round-robin scheduling.

The important points are:

1. The dispatcher will work correctly if called recursively.
2. The dispatcher will not reschedule a task until that task has
returned. So no task will be called recursively.

If you try this, I would be interested to hear about it.

## Last Words

The idea behind `QDispatch` is not new. Some notable relatives:

- [ArduinoThread](https://github.com/ivanseidel/ArduinoThread) by Ivan Seidel
- [TaskScheduler](https://github.com/arkhipenko/TaskScheduler) by Anatoli Arkhipenko

Any defects in `QDispatch` are, of course, entirely my own work.

David Rifkind <drifkind@acm.org>

September 6, 2018
