#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
 
#include "seqlock.h"
#include "rwwlock.h"
#include "rwrlock.h"
 

 
#define STRING_LEN  64

// 1. lock_t lock.type 정의
typedef enum locks{ // 사용할 Lock의 타입을 정의합니다.
    rwr,
    rww,
    seq,
}lock_t; 

unsigned long glob_counter = 0;

long long w_counter = 0;
long long r_counter = 0;
long long glob_string[STRING_LEN] = {0,};

int is_done = 0;

void* ku_lock;

struct thread_args{ // 스레드에 전달할 인자를 담고 있습니다.
    int locktype;
    int whz;
    int rhz;
    int duration;
    FILE* fp;
};

struct reader_view { // 읽기 스레드가 읽은 데이터를 저장하는 구조체.
    int counter;
    char string[STRING_LEN+1];
};

struct linked_list{ // 읽기 스레드가 읽은 데이터를 연결 리스트로 관리하기 위한 구조체.
    struct reader_view value;
    struct linked_list* next;
};


// 3
struct results{ // 스레드 실행 결과를 저장하는 구조체.
    unsigned long rcnt;
    unsigned long wcnt;
}; 
 
struct linked_list* insert(struct linked_list* node, struct reader_view value){   // insert: 연결 리스트에 새로운 노드를 삽입.
    struct linked_list* new = (struct linked_list*)malloc(sizeof(struct linked_list));
    node->next = new;
    new->value = value;
    new->next = NULL;
    return new;
}

void traverse(FILE* fp, struct linked_list* node){ // traverse: 연결 리스트를 순회하며 파일에 데이터를 기록.

    struct linked_list* current = node->next;
    struct linked_list* tmp;

    while(current != NULL){
        fprintf(fp,"%d %s\n",current->value.counter, current->value.string);
        tmp = current;
        current = current->next;
        free(tmp);
    }
}
    

static inline int
timespec_cmp (struct timespec a, struct timespec b){ // timespec_cmp: 두 timespec 구조체를 비교.
  return (a.tv_sec < b.tv_sec ? -1
	  : a.tv_sec > b.tv_sec ? 1
	  : a.tv_nsec - b.tv_nsec);
}

// 실행 5
void writer_ops(){ // writer_ops: 쓰기 작업을 수행.
    char alph = glob_counter % 26 + 'a';
    glob_counter++;
    for(int i = 0; i < STRING_LEN; i++)
        glob_string[i] =  alph;

}  

// w_rww_lock_routine, w_rwr_lock_routine, w_seq_lock_routine: 각각 rww, rwr, seq 락을 사용하여 쓰기 작업을 수행.

void w_rww_lock_routine(){ // 실행 4 w - rww
    rwwlock_acquire_writelock((rwwlock_t*)ku_lock);
    writer_ops();  // 실행 5
    rwwlock_release_writelock((rwwlock_t*)ku_lock);
}
void w_rwr_lock_routine(){ // 실행 4 w - rwr
    rwrlock_acquire_writelock((rwrlock_t*)ku_lock);
    writer_ops();  // 실행 5
    rwrlock_release_writelock((rwrlock_t*)ku_lock);
}
void w_seq_lock_routine(){ // 실행 4 w - seq
    seqlock_write_lock((seqlock_t*)ku_lock);
    writer_ops();  // 실행 5
    seqlock_write_unlock((seqlock_t*)ku_lock);
}










// 실행 3-w  writer 스레드가 실행

void* writer(void* args){ // writer: 쓰기 스레드 함수.

    struct thread_args *t_args = args; // thread_args: 스레드에 전달할 인자를 담고 있습니다.

    struct timespec current;
    struct timespec deadline;

    useconds_t period = 1000000/t_args->whz;
    unsigned long cnt = 0;

    clock_gettime(CLOCK_MONOTONIC, &current);

    deadline.tv_sec = t_args->duration + current.tv_sec;
    deadline.tv_nsec = current.tv_nsec;

    do
    {
        /* critical section in */
        if(t_args->locktype == rwr)
            w_rwr_lock_routine();  // 실행 4 w - rwr  w_rww_lock_routine, w_rwr_lock_routine, w_seq_lock_routine: 각각 rww, rwr, seq 락을 사용하여 쓰기 작업을 수행.
        else if(t_args->locktype == rww)
            w_rww_lock_routine(); // 실행 4 w - rww 
        else if(t_args->locktype == seq)
            w_seq_lock_routine();  // 실행 4 w - seq

        clock_gettime(CLOCK_MONOTONIC, &current);
        cnt++;
        usleep(period);
    } while (timespec_cmp(deadline, current) > 0); // deadline과 current 구조체를 비교
    
    /*
    * timespec_cmp (struct timespec a, struct timespec b){ // timespec_cmp: 두 timespec 구조체를 비교.
        return (a.tv_sec < b.tv_sec ? -1
	        : a.tv_sec > b.tv_sec ? 1
	        : a.tv_nsec - b.tv_nsec);
}
    */
    
    return (void*) cnt;
}







 
 
  



struct reader_view reader_ops(){ // 

    struct reader_view view;
    view.counter = glob_counter;
    for(int i = 0; i < STRING_LEN; i++)
        view.string[i] = glob_string[i];
    view.string[STRING_LEN] = '\0';

    return view;
}

// r_seq_lock_routine, r_rww_lock_routine, r_rwr_lock_routine: 각각 seq, rww, rwr 락을 사용하여 읽기 작업을 수행.
struct linked_list* r_seq_lock_routine(struct linked_list* list){ 

    struct reader_view view;
    struct linked_list* current;
    unsigned cnt;
    do{
        cnt = seqlock_read_begin((seqlock_t *)ku_lock);
        view = reader_ops();
    }while(seqlock_read_retry((seqlock_t*)ku_lock, cnt));

    current = insert(list,view);
    return current;
}

struct linked_list* r_rww_lock_routine(struct linked_list* list){

    struct reader_view view;
    struct linked_list* current;

    rwwlock_acquire_readlock((rwwlock_t*)ku_lock);
    view = reader_ops();
    rwwlock_release_readlock((rwwlock_t*)ku_lock);

    current = insert(list,view); 
    return current;
}
struct linked_list* r_rwr_lock_routine(struct linked_list* list){

    struct reader_view view;
    struct linked_list* current;

    rwrlock_acquire_readlock((rwrlock_t*)ku_lock);
    view = reader_ops();
    rwrlock_release_readlock((rwrlock_t*)ku_lock);

    current = insert(list,view); 
    return current;
}















// 실행 3-r : 읽기 스레드가 실행
// reader: 읽기 스레드 함수.
void* reader(void* args){

    struct thread_args* r_args = args;

    struct timespec current;
    struct timespec deadline;

    struct linked_list* list = (struct linked_list*)malloc(sizeof(struct linked_list));
    struct linked_list* current_list = list;

    clock_gettime(CLOCK_MONOTONIC, &current);

    useconds_t period = 1000000/r_args->rhz;
    unsigned long cnt = 0;

    deadline.tv_sec = r_args->duration + current.tv_sec;
    deadline.tv_nsec = current.tv_nsec;

    do
    {
        if (r_args->locktype == seq){
            current_list = r_seq_lock_routine(current_list);
        } 
        else if(r_args->locktype == rwr){
            current_list = r_rwr_lock_routine(current_list);
        } 
        else if(r_args->locktype == rww){
            current_list = r_rww_lock_routine(current_list);
        }

        clock_gettime(CLOCK_MONOTONIC, &current);
        cnt++;
        usleep(period);

    } while (timespec_cmp(deadline, current) > 0); // deadline과 current 구조체를 비교

    traverse(r_args->fp, list);

    return (void*)cnt;
}

// init_lock: 락을 초기화.
// 실행 부분 2
void init_lock(lock_t lock){
    rwrlock_t *rwrlock;
    rwwlock_t *rwwlock;
    seqlock_t *seqlock;

    switch(lock){ // lock 타입에 따라 각각 다른 작업 수행
        case rwr:
            rwrlock = (rwrlock_t*)malloc(sizeof(rwrlock_t));
            ku_lock = rwrlock;
            rwrlock_init(ku_lock); // rwrlock.h 에 있음
            break;
        case rww:
            rwwlock = (rwwlock_t*)malloc(sizeof(rwwlock_t));
            ku_lock = rwwlock;
            rwwlock_init(ku_lock); // rwwlock.h에 있음
            break;
        case seq:
            seqlock = (seqlock_t*)malloc(sizeof(seqlock_t));
            ku_lock = seqlock;
            seqlock_init(ku_lock); // seqlock.h에 있음
            break;
    }
}


// 실행 부분 1
// works: 스레드를 생성하고 실행, 결과를 반환.
struct results works(lock_t lock, int readers, int writers, int whz, int rhz, int duration){

    int status; // 상태
    
    struct results res; //3
    res.rcnt = 0;
    res.wcnt = 0;


    pthread_t *r_thrs = (pthread_t*)malloc(sizeof(pthread_t) * readers); // read thread 메모리 할당 부분
    pthread_t *w_thrs = (pthread_t*)malloc(sizeof(pthread_t) * writers); // write thread 메모리 할당 부분

    FILE* fp = fopen("reader_log.txt", "w");
    if(fp == NULL){
        printf("file open error\n");
        exit(1);
    }

    struct thread_args args = {
        .locktype = lock,
        .whz = whz,
        .rhz = rhz,
        .duration = duration,
        .fp = fp,
    };

     
    init_lock(lock); // 실행 부분 2, 락 초기화

    // 스레드 생성 입력한 reader, writer 만큼
    for(int i = 0; i < writers; i ++)
        pthread_create(&w_thrs[i], NULL, writer, (void*)&args); // 실행 3-w

    for(int i = 0; i < readers; i ++)
        pthread_create(&r_thrs[i], NULL, reader, (void*)&args); // 실행 3-r


    for(int i = 0; i < writers; i++){
        pthread_join(w_thrs[i], (void**)&status); // 이 함수는 w쓰레드를 실행한 후, 쓰레드가 종료될 때까지 기다리게됩니다. 성공 시 0 을 반환하고, 문제가 있을 경우, 에러 코드를 반환합니다.
        res.wcnt+=status;
    }
    for(int i = 0; i < readers; i++){
        pthread_join(r_thrs[i], (void**)&status); // 이 함수는 r쓰레드를 실행한 후, 쓰레드가 종료될 때까지 기다리게됩니다. 성공 시 0 을 반환하고, 문제가 있을 경우, 에러 코드를 반환합니다.
        res.rcnt+=status;
    }


    free(w_thrs);
    free(r_thrs);

    fclose(fp);

    return res;

}

// stats: 실행 결과를 출력.
void stats(int duration, struct results res){
    
    double w_th = (double) res.wcnt / duration;
    double r_th = (double) res.rcnt / duration;

    printf("Local writer counter total sum : %ld\n", res.wcnt);
    printf("Local reader counter total sum : %ld\n", res.rcnt);

    printf("\n");
    printf("Global counter : %ld\n", glob_counter);
    printf("\n");

    printf("Writer throughput: %f ops/sec\n", w_th);
    printf("Reader throughput: %f ops/sec\n", r_th);

}

// usage: 사용법을 출력.
void usage(){

    printf("kurock <lock> <readers> <rhz> <writers> <whz> <s>\n"); // 인자 1: ./kurock
    printf("<lock>: rwr, rww, or seq \n"); // 인자 1: lock 타입
    printf("<readers>: Number of read threads\n"); // 인자 2: read thread 숫자
    printf("<rhz>: Read frequency (1~100000)\n"); // 인자 3: read frequency
    printf("<writers>: Number of writer threads\n"); // 인자 4: writer thread 숫자
    printf("<whz>: Write frequency (1~100000)\n"); // 인자 5: write frequency 
    printf("<s>: Total execution time (seconds)\n"); // 인자 6: 총 실행 시간(초)
}


/**
    *   입력 예시 : ./ kurock rwr 5 100 5 100 3  (인자 7개)
    * 
    * 
**/ 

// main: 프로그램의 진입점, 인자를 파싱하고 프로그램을 실행.
int main(int argc, char* argv[]){

    lock_t locktype; // 1
    int readers, writers; // 2
    int rhz, whz; // 인자3 read frequency ,   인자 4 write frequency
    int duration; //
    struct results res;

    if(argc != 7){
        usage();
        printf("invaild arg numbs!\n");
        exit(1);
    }

    if(strcmp("rwr", argv[1]) == 0)
        locktype = rwr;
    
    else if(strcmp("rww", argv[1]) == 0)
        locktype = rww;

    else if(strcmp("seq", argv[1]) == 0)
        locktype = seq;

    else{
        usage();
        printf("lock type invalid!\n");
        exit(1);
    }
    // kurock <lock> <readers> <rhz> <writers> <whz> <s>
    readers = atoi(argv[2]);
    if(readers < 1){
        usage();
        printf("reader count invalid!\n");
        exit(1);
    }
    // kurock <lock> <readers> <rhz> <writers> <whz> <s>
    rhz = atoi(argv[3]);
    if(rhz < 1 ||  rhz > 100000){
        usage();
        printf("rhz invalid!\n");
    }
    // kurock <lock> <readers> <rhz> <writers> <whz> <s>
    writers = atoi(argv[4]);
    if(writers < 1){
        usage();
        printf("writer count invalid!\n");
        exit(1);
    }
    // kurock <lock> <readers> <rhz> <writers> <whz> <s>
    whz = atoi(argv[5]);
    if(whz < 1 ||  whz > 100000){
        usage();
        printf("whz invalid!\n");
    }
    // kurock <lock> <readers> <rhz> <writers> <whz> <s>
    duration = atoi(argv[6]);


    // 실행 부분
    res = works(locktype, readers, writers, whz, rhz, duration);

    // 실행 결과 출력
    stats(duration,res);

    return 0;

}