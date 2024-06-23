#include <semaphore.h>

typedef struct _seqlock_t {
    int cnt;
    sem_t writelock;
} seqlock_t;

void seqlock_init(seqlock_t* seq);
void seqlock_write_lock(seqlock_t* seq);
void seqlock_write_unlock(seqlock_t* seq);
unsigned seqlock_read_begin(seqlock_t* seq);
unsigned seqlock_read_retry(seqlock_t* seq, unsigned cnt);


void seqlock_init(seqlock_t* seq) {
    seq->cnt = 0;
    sem_init(&seq->writelock, 0, 1);
}

void seqlock_write_lock(seqlock_t* seq) {
    sem_wait(&seq->writelock); 
    seq->cnt++; 
}

void seqlock_write_unlock(seqlock_t* seq) {
    seq->cnt++; 
    sem_post(&seq->writelock); 
}

unsigned seqlock_read_begin(seqlock_t* seq) {
    unsigned cnt;
    sem_wait(&seq->writelock); 
    cnt = seq->cnt;
    sem_post(&seq->writelock); 
    while (cnt & 1) { 
        sem_wait(&seq->writelock); 
        cnt = seq->cnt;
        sem_post(&seq->writelock); 
    }
    return cnt;
}

unsigned seqlock_read_retry(seqlock_t* seq, unsigned cnt) {
    sem_wait(&seq->writelock); 
    int result = (seq->cnt != cnt || (seq->cnt & 1));
    sem_post(&seq->writelock); 
    return result;
}







