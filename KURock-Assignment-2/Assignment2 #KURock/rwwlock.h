#include <semaphore.h>

typedef struct _rwwlock_t {
    sem_t writelock;
    sem_t readlock;
    sem_t lockforreadlock;
    int readers;
    int write_request; 
} rwwlock_t;

void rwwlock_init(rwwlock_t* rww);
void rwwlock_acquire_writelock(rwwlock_t* rww);
void rwwlock_release_writelock(rwwlock_t* rww);
void rwwlock_acquire_readlock(rwwlock_t* rww);
void rwwlock_release_readlock(rwwlock_t* rww);

void rwwlock_init(rwwlock_t* rww) {
    rww->readers = 0;
    rww->write_request = 0;
    sem_init(&rww->writelock, 0, 1);
    sem_init(&rww->readlock, 0, 1);
    sem_init(&rww->lockforreadlock, 0, 1);
}

void rwwlock_acquire_writelock(rwwlock_t* rww) {
    sem_wait(&rww->lockforreadlock);
    rww->write_request = 1;
    sem_post(&rww->lockforreadlock);

    sem_wait(&rww->readlock);
}

void rwwlock_release_writelock(rwwlock_t* rww) {
    sem_wait(&rww->lockforreadlock);
    rww->write_request = 0;
    sem_post(&rww->lockforreadlock);

    sem_post(&rww->writelock);
    sem_post(&rww->readlock);
}

void rwwlock_acquire_readlock(rwwlock_t* rww) {
    while (1) {
        sem_wait(&rww->lockforreadlock);
        if (rww->write_request == 0) {
            rww->readers++;
            if (rww->readers == 1) {
                sem_wait(&rww->writelock);
            }
            sem_post(&rww->lockforreadlock);
            break;
        }
        sem_post(&rww->lockforreadlock);
    }
}

void rwwlock_release_readlock(rwwlock_t* rww) {
    sem_wait(&rww->lockforreadlock);
    rww->readers--;
    if (rww->readers == 0) {
        sem_post(&rww->writelock);
    }
    sem_post(&rww->lockforreadlock);
}


/*

typedef struct _rwwlock_t {
} rwwlock_t;
void rwwlock_init(rwwlock_t *rww);
void rwwlock_acquire_writelock(rwwlock_t *rww);
void rwwlock_release_writelock(rwwlock_t *rww);
void rwwlock_acquire_readlock(rwwlock_t *rww);
void rwwlock_release_readlock(rwwlock_t *rww);

void rwwlock_init(rwwlock_t *rww){
}
void rwwlock_acquire_writelock(rwwlock_t *rww){
}
void rwwlock_release_writelock(rwwlock_t *rww){
}
void rwwlock_acquire_readlock(rwwlock_t *rww){
}
void rwwlock_release_readlock(rwwlock_t *rww){
}


*/
