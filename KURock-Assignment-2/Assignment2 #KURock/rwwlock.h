#include <semaphore.h>

typedef struct _rwwlock_t {
    sem_t writelock;
    sem_t readlock;
    sem_t lockforreadlock;
    int readers;
    int write_request; // writer가 lock을 요청한 상황을 나타내는 플래그
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
    // Writer가 lock을 요청했음을 표시
    sem_wait(&rww->lockforreadlock);
    rww->write_request = 1;
    sem_post(&rww->lockforreadlock);

    // readlock을 잠금으로 설정하여 모든 reader가 종료되기를 대기
    sem_wait(&rww->readlock);
    // writelock을 잠금으로 설정
}

void rwwlock_release_writelock(rwwlock_t* rww) {
    // write_request 플래그를 리셋
    sem_wait(&rww->lockforreadlock);
    rww->write_request = 0;
    sem_post(&rww->lockforreadlock);

    sem_post(&rww->writelock);
    sem_post(&rww->readlock);
}

void rwwlock_acquire_readlock(rwwlock_t* rww) {
    while (1) {
        // reader가 새로운 읽기를 시도할 때 write_request를 확인
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