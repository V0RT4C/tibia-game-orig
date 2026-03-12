#ifndef TIBIA_THREADS_THREADS_H_
#define TIBIA_THREADS_THREADS_H_ 1

#include "common/types/types.h"
#include <pthread.h>

typedef pthread_t ThreadHandle;
typedef int (ThreadFunction)(void *);

// TODO(fusion): Probably have another way to tell whether the thread was
// successfully spawned or not?
constexpr ThreadHandle INVALID_THREAD_HANDLE = 0;

ThreadHandle StartThread(ThreadFunction *Function, void *Argument, bool Detach);
ThreadHandle StartThread(ThreadFunction *Function, void *Argument, size_t StackSize, bool Detach);
ThreadHandle StartThread(ThreadFunction *Function, void *Argument, void *Stack, size_t StackSize, bool Detach);
int JoinThread(ThreadHandle Handle);
void DelayThread(int Seconds, int MicroSeconds);

#endif // TIBIA_THREADS_THREADS_H_
