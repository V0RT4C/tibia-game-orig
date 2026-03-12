#include "threads.h"
#include "logging/logging.h"

struct TThreadStarter {
	ThreadFunction *Function;
	void *Argument;
	bool Detach;
};

static void *ThreadStarter(void *Pointer){
	TThreadStarter *Starter = (TThreadStarter*)Pointer;
	ThreadFunction *Function = Starter->Function;
	void *Argument = Starter->Argument;
	bool Detach = Starter->Detach;
	delete Starter;

	int Result = Function(Argument);

	// TODO(fusion): Just store the integer as a pointer and avoid allocation?
	int *ResultPointer = NULL;
	if(!Detach){
		ResultPointer = new int;
		*ResultPointer = Result;
	}
	pthread_exit(ResultPointer);

	return NULL; // Unreachable.
}

ThreadHandle StartThread(ThreadFunction *Function, void *Argument, bool Detach){
	TThreadStarter *Starter = new TThreadStarter;
	Starter->Function = Function;
	Starter->Argument = Argument;
	Starter->Detach = Detach;

	pthread_t Handle;
	int err = pthread_create(&Handle, NULL, ThreadStarter, Starter);
	if(err != 0){
		error("StartThread: Cannot create thread; error code %d.\n", err);
		return INVALID_THREAD_HANDLE;
	}

	if(Detach){
		pthread_detach(Handle);
	}

	return (ThreadHandle)Handle;
}

ThreadHandle StartThread(ThreadFunction *Function, void *Argument, size_t StackSize, bool Detach){
	TThreadStarter *Starter = new TThreadStarter;
	Starter->Function = Function;
	Starter->Argument = Argument;
	Starter->Detach = Detach;

	pthread_t Handle;
	pthread_attr_t Attr;
	pthread_attr_init(&Attr);
	pthread_attr_setstacksize(&Attr, StackSize);
	int err = pthread_create(&Handle, &Attr, ThreadStarter, Starter);
	pthread_attr_destroy(&Attr);
	if(err != 0){
		error("StartThread: Cannot create thread; error code %d.\n", err);
		return INVALID_THREAD_HANDLE;
	}

	if(Detach){
		pthread_detach(Handle);
	}

	return (ThreadHandle)Handle;
}

ThreadHandle StartThread(ThreadFunction *Function, void *Argument, void *Stack, size_t StackSize, bool Detach){
	TThreadStarter *Starter = new TThreadStarter;
	Starter->Function = Function;
	Starter->Argument = Argument;
	Starter->Detach = Detach;

	pthread_t Handle;
	pthread_attr_t Attr;
	pthread_attr_init(&Attr);
	pthread_attr_setstack(&Attr, Stack, StackSize);
	int err = pthread_create(&Handle, &Attr, ThreadStarter, Starter);
	pthread_attr_destroy(&Attr);
	if(err != 0){
		error("StartThread: Cannot create thread; error code %d.\n", err);
		return INVALID_THREAD_HANDLE;
	}

	if(Detach){
		pthread_detach(Handle);
	}

	return (ThreadHandle)Handle;
}

int JoinThread(ThreadHandle Handle){
	int Result = 0;
	int *ResultPointer;

	pthread_join((pthread_t)Handle, (void**)&ResultPointer);
	if(ResultPointer != NULL){
		Result = *ResultPointer;
		delete ResultPointer;
	}

	return Result;
}

void DelayThread(int Seconds, int MicroSeconds){
	if(Seconds == 0 && MicroSeconds == 0){
		sched_yield();
	}else if(MicroSeconds == 0){
		sleep(Seconds);
	}else{
		usleep(MicroSeconds + Seconds * 1000000);
	}
}
