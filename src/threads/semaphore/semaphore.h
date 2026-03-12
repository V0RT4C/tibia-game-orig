#ifndef TIBIA_THREADS_SEMAPHORE_H_
#define TIBIA_THREADS_SEMAPHORE_H_ 1

#include <pthread.h>

struct Semaphore {
	Semaphore(int Value);
	~Semaphore(void);
	void up(void);
	void down(void);

	// DATA
	// =================
	int value;
	pthread_mutex_t mutex;
	pthread_cond_t condition;
};

#endif // TIBIA_THREADS_SEMAPHORE_H_
