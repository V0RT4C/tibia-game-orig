#include "semaphore.h"
#include "logging/logging.h"
#include <cstring>

Semaphore::Semaphore(int Value){
	this->value = Value;

	// TODO(fusion): These should probably be non-recoverable errors.

	if(pthread_mutex_init(&this->mutex, NULL) != 0){
		error("Semaphore::Semaphore: Cannot initialize mutex.\n");
	}

	if(pthread_cond_init(&this->condition, NULL) != 0){
		error("Semaphore::Semaphore: Cannot initialize condition variable.\n");
	}
}

Semaphore::~Semaphore(void){
	// IMPORTANT(fusion): Due to how initialization is rolled out, `exit` may be
	// called after threads are spawned but before `ExitAll` is registered as an
	// exit handler. This means such threads may still be running or left global
	// semaphores in an inconsistent state if abruptly terminated. Either way,
	// they are still considered "in use".
	//  In this case, calling `destroy` on either mutex or condition variable is
	// undefined behaviour as per the manual but the actual implementation would
	// fail on `mutex_destroy` with `EBUSY` and hang on `cond_destroy`.
	//	The temporary solution is to check the result from `mutex_destroy` before
	// attempting to call `cond_destroy` to avoid hanging at exit.

	int ErrorCode;
	if((ErrorCode = pthread_mutex_destroy(&this->mutex)) != 0){
		error("Semaphore::~Semaphore: Cannot destroy mutex: (%d) %s.\n",
				ErrorCode, strerror(ErrorCode));
	}else if((ErrorCode = pthread_cond_destroy(&this->condition)) != 0){
		error("Semaphore::~Semaphore: Cannot destroy condition variable: (%d) %s.\n",
				ErrorCode, strerror(ErrorCode));
	}
}

void Semaphore::down(void){
	pthread_mutex_lock(&this->mutex);
	while(this->value <= 0){ // TODO(fusion): Make sure this is always a load operation?
		pthread_cond_wait(&this->condition, &this->mutex);
	}
	this->value -= 1;
	pthread_mutex_unlock(&this->mutex);
}

void Semaphore::up(void){
	pthread_mutex_lock(&this->mutex);
	this->value += 1;
	pthread_mutex_unlock(&this->mutex);
	pthread_cond_signal(&this->condition);
}
