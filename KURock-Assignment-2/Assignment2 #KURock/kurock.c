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

// 1. lock_t lock.type ����
typedef enum locks{ // ����� Lock�� Ÿ���� �����մϴ�.
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

struct thread_args{ // �����忡 ������ ���ڸ� ��� �ֽ��ϴ�.
    int locktype;
    int whz;
    int rhz;
    int duration;
    FILE* fp;
};

struct reader_view { // �б� �����尡 ���� �����͸� �����ϴ� ����ü.
    int counter;
    char string[STRING_LEN+1];
};

struct linked_list{ // �б� �����尡 ���� �����͸� ���� ����Ʈ�� �����ϱ� ���� ����ü.
    struct reader_view value;
    struct linked_list* next;
};


// 3
struct results{ // ������ ���� ����� �����ϴ� ����ü.
    unsigned long rcnt;
    unsigned long wcnt;
}; 
 
struct linked_list* insert(struct linked_list* node, struct reader_view value){   // insert: ���� ����Ʈ�� ���ο� ��带 ����.
    struct linked_list* new = (struct linked_list*)malloc(sizeof(struct linked_list));
    node->next = new;
    new->value = value;
    new->next = NULL;
    return new;
}

void traverse(FILE* fp, struct linked_list* node){ // traverse: ���� ����Ʈ�� ��ȸ�ϸ� ���Ͽ� �����͸� ���.

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
timespec_cmp (struct timespec a, struct timespec b){ // timespec_cmp: �� timespec ����ü�� ��.
  return (a.tv_sec < b.tv_sec ? -1
	  : a.tv_sec > b.tv_sec ? 1
	  : a.tv_nsec - b.tv_nsec);
}

// ���� 5
void writer_ops(){ // writer_ops: ���� �۾��� ����.
    char alph = glob_counter % 26 + 'a';
    glob_counter++;
    for(int i = 0; i < STRING_LEN; i++)
        glob_string[i] =  alph;

}  

// w_rww_lock_routine, w_rwr_lock_routine, w_seq_lock_routine: ���� rww, rwr, seq ���� ����Ͽ� ���� �۾��� ����.

void w_rww_lock_routine(){ // ���� 4 w - rww
    rwwlock_acquire_writelock((rwwlock_t*)ku_lock);
    writer_ops();  // ���� 5
    rwwlock_release_writelock((rwwlock_t*)ku_lock);
}
void w_rwr_lock_routine(){ // ���� 4 w - rwr
    rwrlock_acquire_writelock((rwrlock_t*)ku_lock);
    writer_ops();  // ���� 5
    rwrlock_release_writelock((rwrlock_t*)ku_lock);
}
void w_seq_lock_routine(){ // ���� 4 w - seq
    seqlock_write_lock((seqlock_t*)ku_lock);
    writer_ops();  // ���� 5
    seqlock_write_unlock((seqlock_t*)ku_lock);
}










// ���� 3-w  writer �����尡 ����

void* writer(void* args){ // writer: ���� ������ �Լ�.

    struct thread_args *t_args = args; // thread_args: �����忡 ������ ���ڸ� ��� �ֽ��ϴ�.

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
            w_rwr_lock_routine();  // ���� 4 w - rwr  w_rww_lock_routine, w_rwr_lock_routine, w_seq_lock_routine: ���� rww, rwr, seq ���� ����Ͽ� ���� �۾��� ����.
        else if(t_args->locktype == rww)
            w_rww_lock_routine(); // ���� 4 w - rww 
        else if(t_args->locktype == seq)
            w_seq_lock_routine();  // ���� 4 w - seq

        clock_gettime(CLOCK_MONOTONIC, &current);
        cnt++;
        usleep(period);
    } while (timespec_cmp(deadline, current) > 0); // deadline�� current ����ü�� ��
    
    /*
    * timespec_cmp (struct timespec a, struct timespec b){ // timespec_cmp: �� timespec ����ü�� ��.
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

// r_seq_lock_routine, r_rww_lock_routine, r_rwr_lock_routine: ���� seq, rww, rwr ���� ����Ͽ� �б� �۾��� ����.
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















// ���� 3-r : �б� �����尡 ����
// reader: �б� ������ �Լ�.
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

    } while (timespec_cmp(deadline, current) > 0); // deadline�� current ����ü�� ��

    traverse(r_args->fp, list);

    return (void*)cnt;
}

// init_lock: ���� �ʱ�ȭ.
// ���� �κ� 2
void init_lock(lock_t lock){
    rwrlock_t *rwrlock;
    rwwlock_t *rwwlock;
    seqlock_t *seqlock;

    switch(lock){ // lock Ÿ�Կ� ���� ���� �ٸ� �۾� ����
        case rwr:
            rwrlock = (rwrlock_t*)malloc(sizeof(rwrlock_t));
            ku_lock = rwrlock;
            rwrlock_init(ku_lock); // rwrlock.h �� ����
            break;
        case rww:
            rwwlock = (rwwlock_t*)malloc(sizeof(rwwlock_t));
            ku_lock = rwwlock;
            rwwlock_init(ku_lock); // rwwlock.h�� ����
            break;
        case seq:
            seqlock = (seqlock_t*)malloc(sizeof(seqlock_t));
            ku_lock = seqlock;
            seqlock_init(ku_lock); // seqlock.h�� ����
            break;
    }
}


// ���� �κ� 1
// works: �����带 �����ϰ� ����, ����� ��ȯ.
struct results works(lock_t lock, int readers, int writers, int whz, int rhz, int duration){

    int status; // ����
    
    struct results res; //3
    res.rcnt = 0;
    res.wcnt = 0;


    pthread_t *r_thrs = (pthread_t*)malloc(sizeof(pthread_t) * readers); // read thread �޸� �Ҵ� �κ�
    pthread_t *w_thrs = (pthread_t*)malloc(sizeof(pthread_t) * writers); // write thread �޸� �Ҵ� �κ�

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

     
    init_lock(lock); // ���� �κ� 2, �� �ʱ�ȭ

    // ������ ���� �Է��� reader, writer ��ŭ
    for(int i = 0; i < writers; i ++)
        pthread_create(&w_thrs[i], NULL, writer, (void*)&args); // ���� 3-w

    for(int i = 0; i < readers; i ++)
        pthread_create(&r_thrs[i], NULL, reader, (void*)&args); // ���� 3-r


    for(int i = 0; i < writers; i++){
        pthread_join(w_thrs[i], (void**)&status); // �� �Լ��� w�����带 ������ ��, �����尡 ����� ������ ��ٸ��Ե˴ϴ�. ���� �� 0 �� ��ȯ�ϰ�, ������ ���� ���, ���� �ڵ带 ��ȯ�մϴ�.
        res.wcnt+=status;
    }
    for(int i = 0; i < readers; i++){
        pthread_join(r_thrs[i], (void**)&status); // �� �Լ��� r�����带 ������ ��, �����尡 ����� ������ ��ٸ��Ե˴ϴ�. ���� �� 0 �� ��ȯ�ϰ�, ������ ���� ���, ���� �ڵ带 ��ȯ�մϴ�.
        res.rcnt+=status;
    }


    free(w_thrs);
    free(r_thrs);

    fclose(fp);

    return res;

}

// stats: ���� ����� ���.
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

// usage: ������ ���.
void usage(){

    printf("kurock <lock> <readers> <rhz> <writers> <whz> <s>\n"); // ���� 1: ./kurock
    printf("<lock>: rwr, rww, or seq \n"); // ���� 1: lock Ÿ��
    printf("<readers>: Number of read threads\n"); // ���� 2: read thread ����
    printf("<rhz>: Read frequency (1~100000)\n"); // ���� 3: read frequency
    printf("<writers>: Number of writer threads\n"); // ���� 4: writer thread ����
    printf("<whz>: Write frequency (1~100000)\n"); // ���� 5: write frequency 
    printf("<s>: Total execution time (seconds)\n"); // ���� 6: �� ���� �ð�(��)
}


/**
    *   �Է� ���� : ./ kurock rwr 5 100 5 100 3  (���� 7��)
    * 
    * 
**/ 

// main: ���α׷��� ������, ���ڸ� �Ľ��ϰ� ���α׷��� ����.
int main(int argc, char* argv[]){

    lock_t locktype; // 1
    int readers, writers; // 2
    int rhz, whz; // ����3 read frequency ,   ���� 4 write frequency
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


    // ���� �κ�
    res = works(locktype, readers, writers, whz, rhz, duration);

    // ���� ��� ���
    stats(duration,res);

    return 0;

}