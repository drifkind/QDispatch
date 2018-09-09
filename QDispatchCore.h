/*
 *  QDispatchCore.h
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

#ifndef QDispatchCore_h
#define QDispatchCore_h

#include <Arduino.h>

namespace QDispatch {

class Task
{
public:
	Task() { u.f = NULL; object = &dummy; }
	Task(nullptr_t) : Task() {}
	Task(void (*f)()) { u.f = f; object = &dummy; }
	template<class C> Task(void (C::*cf)(), C *c) { u.cf = (void (Dummy::*)() const)cf; object = (const Dummy *)c; }
	explicit operator bool() const;
	void operator()() const;
	bool operator==(const Task &rhs) const;
	bool operator!=(const Task &rhs) const { return !(*this == rhs); }
	bool operator==(nullptr_t) const { return !(bool)*this; }
	bool operator!=(nullptr_t) const { return (bool)*this; }
private:
	class Dummy {};
	static Dummy const dummy;
	union {
		void (*f)();
		void (Dummy::*cf)() const;
	} u;
	const Dummy *object;
};

class TaskQueue;
class TaskDispatcher;
class EventBarrier;

class TaskContext
{
	friend class TaskQueue;
	friend class TaskDispatcher;
	friend class EventBarrier;
public:
	TaskContext() : TaskContext(Task(), (void *)NULL) {}
	TaskContext(Task target, const void *tag = NULL);
	template<class C> TaskContext(void (C::*cf)(), C *c) : TaskContext(Task(cf, c), (void *)c) {}
	TaskContext(const TaskContext &context) : TaskContext(context.target, context.tag) {}
	void cancel();
	bool isPending() const { return queue != NULL; }
	Task target;
	const void *tag;
private:
	TaskQueue *queue;
	TaskContext *nextContext;
	unsigned long dispatchTime;
	long repeatInterval;
	EventBarrier *signalEvent;
};

class ContextPool
{
public:
	ContextPool();
	TaskContext *fetch();
	TaskContext *(*emptyPoolHandler)();
protected:
	virtual TaskContext *fetchCore() = 0;
};

class DynamicContextPool : public ContextPool
{
public:
	DynamicContextPool(unsigned poolLimit);
	DynamicContextPool() : DynamicContextPool(0) {}
	const unsigned poolLimit;
protected:
	virtual TaskContext *fetchCore();
private:
	class DynamicContext : public TaskContext
	{
		friend class DynamicContextPool;
	private:
		DynamicContext *nextEntry;
	};
	DynamicContext *firstEntry;
	DynamicContext *currentEntry;
	unsigned poolCount;
};

class TaskQueue
{
public:
	void cancel(TaskContext &context);
	void cancelAll(Task target);
	template<class C> void cancelAll(void (C::*cf)(), C *c) { cancelAll(Task(cf, c)); }
	void cancelAll(const void *tag);
	void cancelAll();
	class iterator
	{
	public:
		explicit iterator(TaskContext *context);
		iterator& operator++();
		iterator operator++(int);
		bool operator==(iterator rhs) const;
		bool operator!=(iterator rhs) const { return !(*this == rhs); }
		TaskContext &operator*() const;
	private:
		TaskContext *context, *nextContext;
	};
	iterator begin() const { return iterator(firstContext); }
	iterator end() const { return endIterator; }
protected:
	TaskQueue();
	virtual void recycleContext(TaskContext *context) {}
	TaskContext **findContext(TaskContext *context);
	TaskContext *firstContext;
	TaskQueue *firstSubqueue;
	TaskQueue *nextSubqueue;
private:
	void cancel(TaskContext **ptr);
	static iterator endIterator;
};

class TaskDispatcher : public TaskQueue
{
	friend class EventBarrier;
public:
	enum SchedulingPolicy : uint8_t {
		INTERVAL,
		CYCLE,
		TIMING,
	};
	static unsigned long millis();
	explicit TaskDispatcher(ContextPool *contextPool) : TaskDispatcher(NULL, contextPool) {}
	TaskDispatcher(unsigned long (*timingFunction)() = NULL, ContextPool *contextPool = NULL);
	TaskContext *callAfter(long interval, Task target, const void *tag = NULL) { return schedule(interval, -1, target, tag); }
	TaskContext *callEvery(long interval, Task target, const void *tag = NULL) { return schedule(0, interval, target, tag); }
	TaskContext *schedule(long firstInterval, long nextInterval, Task target, const void *tag = NULL);
	template<class C> TaskContext *callAfter(long interval, void (C::*cf)(), C *c) { return callAfter(interval, Task(cf, c), (void *)c); }
	template<class C> TaskContext *callEvery(long interval, void (C::*cf)(), C *c) { return callEvery(interval, Task(cf, c), (void *)c); }
	template<class C> TaskContext *schedule(long firstInterval, long nextInterval, void (C::*cf)(), C *c) { return schedule(firstInterval, nextInterval, Task(cf, c), (void *)c); }
	void callAfter(TaskContext &context, long interval, Task target, const void *tag = NULL) { schedule(context, interval, -1, target, tag); }
	void callEvery(TaskContext &context, long interval, Task target, const void *tag = NULL) { schedule(context, 0, interval, target, tag); }
	void schedule(TaskContext &context, long firstInterval, long nextInterval, Task target, const void *tag = NULL);
	template<class C> void callAfter(TaskContext &context, long interval, void (C::*cf)(), C *c) { callAfter(context, interval, Task(cf, c), (void *)c); }
	template<class C> void callEvery(TaskContext &context, long interval, void (C::*cf)(), C *c) { callEvery(context, interval, Task(cf, c), (void *)c); }
	template<class C> void schedule(TaskContext &context, long firstInterval, long nextInterval, void (C::*cf)(), C *c) { schedule(context, firstInterval, nextInterval, Task(cf, c), (void *)c); }
	void callAfter(TaskContext &context, long interval) { schedule(context, interval, -1); }
	void callEvery(TaskContext &context, long interval) { schedule(context, 0, interval); }
	void schedule(TaskContext &context, long firstInterval, long nextInterval);
	bool run();
	void delay(long ticks);
	unsigned long (* const timingFunction)();
	ContextPool *contextPool;
	SchedulingPolicy schedulingPolicy;
protected:
	virtual void recycleContext(TaskContext *context);
private:
	void enqueueContext(TaskContext *context);
};

class EventBarrier : public TaskQueue
{
	friend class TaskDispatcher;
public:
	static const long FOREVER = -1;
	explicit EventBarrier(TaskDispatcher &dispatcher);
	TaskContext *when(Task target, const void *tag = NULL) { return onSignal(target, tag, false); }
	TaskContext *whenever(Task target, const void *tag = NULL) { return onSignal(target, tag, true); }
	template<class C> TaskContext *when(void (C::*cf)(), C *c) { return when(Task(cf, c), (void *)c); }
	template<class C> TaskContext *whenever(void (C::*cf)(), C *c) { return whenever(Task(cf, c), (void *)c); }
	void when(TaskContext &context, Task target, const void *tag = NULL) { onSignal(&context, target, tag, false); }
	void whenever(TaskContext &context, Task target, const void *tag = NULL) { onSignal(&context, target, tag, true); }
	template<class C> void when(TaskContext &context, void (C::*cf)(), C *c) { when(context, Task(cf, c), (void *)c); }
	template<class C> void whenever(TaskContext &context, void (C::*cf)(), C *c) { whenever(context, Task(cf, c), (void *)c); }
	void when(TaskContext &context) { onSignal(&context, false); }
	void whenever(TaskContext &context) { onSignal(&context, true); }
	bool wait(long ticks = FOREVER);
	bool signal();
	void signalAll();
	TaskDispatcher &dispatcher;
protected:
	virtual void recycleContext(TaskContext *context);
private:
	EventBarrier(const EventBarrier &);
	TaskContext *onSignal(Task target, const void *tag, bool repeat);
	void onSignal(TaskContext *context, Task target, const void *tag, bool repeat);
	void onSignal(TaskContext *context, bool repeat);
};

}	// namespace QDispatch

#endif
