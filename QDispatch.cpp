/*
 *  QDispatch.cpp
 *
 *  13 Oct 2017 David Rifkind <drifkind@acm.org>
 *
 *  This software is licensed under the MIT license.
 *  
 *  Copyright (c) 2018 David Rifkind
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE. 
 */

#include "QDispatchCore.h"

namespace QDispatch {

// Task
//

// An object pointer of &dummy means this is a non-method function
// pointer. We don't use NULL as a marker because it's possible to
// specify a method with a null 'this' pointer. Not a good idea,
// but possible.
const Task::Dummy Task::dummy;

Task::operator bool() const
{
	return (object == &dummy ? u.f != NULL : u.cf != NULL && object != NULL);
}

void Task::operator()() const
{
	object == &dummy ? (u.f)() : (object->*(u.cf))();
}

bool Task::operator==(const Task &rhs) const
{
	if (object != rhs.object)
		return false;
	return (object == &dummy ? u.f == rhs.u.f : u.cf == rhs.u.cf);
}


// TaskContext
//
TaskContext::TaskContext(Task target, const void *tag)
: target(target), tag(tag)
{
	queue = NULL;
	nextContext = NULL;
	signalEvent = NULL;
}

void TaskContext::cancel()
{
	if (queue != NULL)
		queue->cancel(*this);
}


// ContextPool
//
ContextPool::ContextPool()
{
	emptyPoolHandler = NULL;
}

TaskContext *ContextPool::fetch()
{
	TaskContext *context = fetchCore();

	if (context == NULL && emptyPoolHandler != NULL)
		context = emptyPoolHandler();

	return context;
}


// DynamicContextPool
//
DynamicContextPool::DynamicContextPool(unsigned poolLimit)
: poolLimit(poolLimit)
{
	firstEntry = NULL;
	currentEntry = NULL;
	poolCount = 0;
}

TaskContext *DynamicContextPool::fetchCore()
{
	if (firstEntry != NULL) {

		if (currentEntry == NULL)
			currentEntry = firstEntry;

		DynamicContext *startEntry = currentEntry;

		do {
			DynamicContext *context = currentEntry;
			currentEntry = currentEntry->nextEntry ?: firstEntry;
			if (!context->isPending())
				return context;
		} while (currentEntry != startEntry);
	}

	DynamicContext *newContext = new DynamicContext;
	newContext->nextEntry = firstEntry;
	firstEntry = newContext;

	return newContext;
}


// TaskQueue
//
TaskQueue::TaskQueue()
{
	firstContext = NULL;
	firstSubqueue = NULL;
	nextSubqueue = NULL;
}

void TaskQueue::cancel(TaskContext &context)
{
	for (TaskContext **ptr = &firstContext; *ptr != NULL; ptr = &(*ptr)->nextContext) {
		if (*ptr == &context) {
			cancel(ptr);
			break;
		}
	}
}

void TaskQueue::cancelAll(const void *tag)
{
	TaskContext **ptr = &firstContext;

	while (*ptr != NULL) {
		if ((*ptr)->tag == tag)
			cancel(ptr);
		else
			ptr = &(*ptr)->nextContext;
	}

	for (TaskQueue *subqueue = firstSubqueue; subqueue != NULL; subqueue = subqueue->nextSubqueue)
		subqueue->cancelAll(tag);
}

void TaskQueue::cancelAll(Task target)
{
	TaskContext **ptr = &firstContext;

	while (*ptr != NULL) {
		if ((*ptr)->target == target)
			cancel(ptr);
		else
			ptr = &(*ptr)->nextContext;
	}

	for (TaskQueue *subqueue = firstSubqueue; subqueue != NULL; subqueue = subqueue->nextSubqueue)
		subqueue->cancelAll(target);
}

void TaskQueue::cancelAll()
{
	while (firstContext != NULL)
		cancel(&firstContext);

	for (TaskQueue *subqueue = firstSubqueue; subqueue != NULL; subqueue = subqueue->nextSubqueue)
		subqueue->cancelAll();
}

void TaskQueue::cancel(TaskContext **ptr)
{
	TaskContext *context = *ptr;
	*ptr = context->nextContext;
	context->queue = NULL;
	context->nextContext = NULL;
}

TaskContext **TaskQueue::findContext(TaskContext *context)
{
	for (TaskContext **ptr = &firstContext; *ptr != NULL; ptr = &(*ptr)->nextContext) {
		if (*ptr == context)
			return ptr;
	}

	return NULL;
}


// TaskQueue::iterator
//

TaskQueue::iterator TaskQueue::endIterator = TaskQueue::iterator(NULL);

TaskQueue::iterator::iterator(TaskContext *context)
{
	this->context = context;
	this->nextContext = context ? context->nextContext : NULL;
}

TaskQueue::iterator& TaskQueue::iterator::operator++()
{
	context = nextContext;
	if (context != NULL)
		nextContext = context->nextContext;
	return *this;
}

TaskQueue::iterator TaskQueue::iterator::operator++(int)
{
	iterator result = *this;
	++(*this);
	return result;
}

bool TaskQueue::iterator::operator==(iterator rhs) const
{
	return (context == rhs.context);
}

TaskContext &TaskQueue::iterator::operator*() const
{
	return *context;
}


// TaskDispatcher
//
unsigned long TaskDispatcher::millis()
{
	return ::millis();
}

TaskDispatcher::TaskDispatcher(unsigned long (*timingFunction)(), ContextPool *contextPool)
: timingFunction(timingFunction ?: TaskDispatcher::millis), contextPool(contextPool)
{
	schedulingPolicy = INTERVAL;
}

TaskContext *TaskDispatcher::schedule(long firstInterval, long nextInterval, Task target, const void *tag)
{
	TaskContext *context = NULL;

	if (contextPool != NULL)
		context = contextPool->fetch();

	if (context != NULL) {
		cancelAll(target);
		schedule(*context, firstInterval, nextInterval, target, tag);
	}

	return context;
}

void TaskDispatcher::schedule(TaskContext &context, long firstInterval, long nextInterval, Task target, const void *tag)
{
	if (firstInterval >= 0) {
		context.target = target;
		context.tag = tag;
		schedule(context, firstInterval, nextInterval);
	}
}

void TaskDispatcher::schedule(TaskContext &context, long firstInterval, long nextInterval)
{
	if (firstInterval >= 0) {
		context.cancel();
		context.dispatchTime = timingFunction() + firstInterval;
		context.repeatInterval = nextInterval;
		context.signalEvent = NULL;
		enqueueContext(&context);
	}
}

bool TaskDispatcher::run()
{
	if (firstContext == NULL)
		return false;

	TaskContext *context = firstContext;
	unsigned long now = timingFunction();
	unsigned long dispatchTime = context->dispatchTime;

	if ((long)(now - dispatchTime) < 0)
		return false;

	firstContext = context->nextContext;
	context->nextContext = NULL;

	if (context->repeatInterval < 0 && context->signalEvent == NULL)
		context->queue = NULL;
		
	// If context->queue is still set the context is still marked as busy
	// and won't be recycled while the task is running.

	if (context->target)
		context->target();
		
	// We delayed rescheduling until now so that the task would not be
	// called recursively if things get funny. Now we have to make sure
	// that it was not canceled or reused in the meantime.

	if (context->queue == this &&  context->nextContext == NULL && findContext(context) == NULL) {

		long repeatInterval = context->repeatInterval;

		if (repeatInterval >= 0) {

			unsigned long then = now;
			now = timingFunction();
			long overrun;
			
			switch (schedulingPolicy) {
			case INTERVAL:
			default:
				dispatchTime = now + repeatInterval;
				break;
			case CYCLE:
				dispatchTime = then + repeatInterval;
				if (dispatchTime < now)
					dispatchTime = now;
				break;
			case TIMING:
				dispatchTime = dispatchTime + repeatInterval;
				// If we've missed a cycle entirely, pick up again
				// at the next next.
				overrun = now - dispatchTime;
				if (overrun > 0)
					dispatchTime = now + (overrun % repeatInterval);
				break;
			}

			context->dispatchTime = dispatchTime;
			enqueueContext(context);

		} else if (context->signalEvent != NULL) {
			context->signalEvent->recycleContext(context);
		} else {
			context->queue = NULL;
		}
	}

	return true;
}

void TaskDispatcher::delay(long ticks)
{
	unsigned long endTime = timingFunction() + ticks;

	while ((long)(timingFunction() - endTime) < 0)
		run();
}

void TaskDispatcher::recycleContext(TaskContext *context)
{
	context->dispatchTime = timingFunction();
	context->repeatInterval = -1;
	enqueueContext(context);
}

void TaskDispatcher::enqueueContext(TaskContext *context)
{
	TaskContext **ptr = &firstContext;

	while (true) {
		TaskContext *thisContext = *ptr;
		if (thisContext == NULL || (long)(context->dispatchTime - thisContext->dispatchTime) < 0) {
			context->queue = this;
			context->nextContext = thisContext;
			*ptr = context;
			break;
		}
		ptr = &thisContext->nextContext;
	}
}


// EventBarrier
//
EventBarrier::EventBarrier(TaskDispatcher &dispatcher)
: dispatcher(dispatcher)
{
	nextSubqueue = dispatcher.firstSubqueue;
	dispatcher.firstSubqueue = this;
}

bool EventBarrier::wait(long ticks)
{
	unsigned long (*timingFunction)() = dispatcher.timingFunction;
	unsigned long endTime = timingFunction() + ticks;
	TaskContext context;
	when(context);

	do {
		dispatcher.run();
		if (!context.isPending())
			return true;
	} while (ticks == FOREVER || (long)(timingFunction() - endTime) < 0);

	context.cancel();

	return false;
}

bool EventBarrier::signal()
{
	if (firstContext != NULL) {
		TaskContext *context = firstContext;
		firstContext = context->nextContext;
		dispatcher.recycleContext(context);
		return true;
	} 

	return false;
}

void EventBarrier::signalAll()
{
	while (signal())
		;
}

TaskContext *EventBarrier::onSignal(Task target, const void *tag, bool repeat)
{
	TaskContext *context = NULL;

	if (dispatcher.contextPool != NULL)
		context = dispatcher.contextPool->fetch();

	if (context != NULL) {
		dispatcher.cancelAll(target);
		onSignal(context, target, tag, repeat);
	}

	return context;
}

void EventBarrier::onSignal(TaskContext *context, Task target, const void *tag, bool repeat)
{
	context->target = target;
	context->tag = tag;
	onSignal(context, repeat);
}

void EventBarrier::onSignal(TaskContext *context, bool repeat)
{
	context->cancel();
	context->signalEvent = repeat ? this : NULL;
	recycleContext(context);
}

void EventBarrier::recycleContext(TaskContext *context)
{
	TaskContext **ptr = &firstContext;

	while (*ptr != NULL)
		ptr = &(*ptr)->nextContext;

	context->queue = this;
	context->nextContext = *ptr;
	*ptr = context;
}

}   // namespace QDispatch
