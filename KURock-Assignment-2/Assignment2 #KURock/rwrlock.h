#include <semaphore.h>

typedef struct _rwrlock_t {

	sem_t readlock;
	sem_t writelock;
	int readers;
	 
} rwrlock_t;

void rwrlock_init(rwrlock_t *rwr);
void rwrlock_acquire_writelock(rwrlock_t *rwr);
void rwrlock_release_writelock(rwrlock_t *rwr);
void rwrlock_acquire_readlock(rwrlock_t *rwr);
void rwrlock_release_readlock(rwrlock_t *rwr);


void rwrlock_init(rwrlock_t *rwr){
	rwr->readers = 0;
	sem_init(&rwr->readlock, 0, 1);
	sem_init(&rwr->writelock, 0, 1);
}

void rwrlock_acquire_writelock(rwrlock_t *rwr){
	sem_wait(&rwr->writelock);
}

void rwrlock_release_writelock(rwrlock_t *rwr){
	sem_post(&rwr->writelock);
}

void rwrlock_acquire_readlock(rwrlock_t *rwr){
	sem_wait(&rwr->readlock);
	rwr->readers++;
	if (rwr->readers == 1) {
		sem_wait(&rwr->writelock);
	}
	sem_post(&rwr->readlock);
}

void rwrlock_release_readlock(rwrlock_t *rwr){
	sem_wait(&rwr->readlock);
	rwr->readers--;
	if (rwr->readers == 0) {
		sem_post(&rwr->writelock);
	}
	sem_post(&rwr->readlock);
}