//Ex3: threadpool.c file
//Name: Yahya Saad, ID: 322944869
#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//---------some macros---------
#define TRUE 		1
#define EMPTY		0
#define SUCCESS 	0
//---------private functions---------
void *error(char*); //for perror message and exit
//---------threadpool.c functions---------
threadpool* create_threadpool(int num_threads_in_pool){
    if(num_threads_in_pool>MAXT_IN_POOL || num_threads_in_pool<=0){//flag number of threads is allowable
        printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
        return NULL;
    }
    threadpool* result=(threadpool*)malloc(sizeof(threadpool));
    if(result==NULL){
        error("Cannot allocate memory\n");
    }
    // init pool
    result->num_threads=num_threads_in_pool;
    result->dont_accept=result->shutdown = result->qsize = 0;
    result->qhead=result->qtail = NULL;
    pthread_mutex_init(&result->qlock,NULL);
    pthread_cond_init(&result->q_not_empty,NULL);
    pthread_cond_init(&result->q_empty,NULL);


    result->threads=(pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
    int flag;
    for(int i=0 ; i<num_threads_in_pool ; i++){
        //Create the threads with do_work as the execution function and the pool as an argument
        flag = pthread_create(&result->threads[i], NULL, do_work,result);
        if(flag != SUCCESS){
            error("Create Failed\n");
        }
    }
    return result;
}
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(from_me == NULL || dispatch_to_here == NULL){
        error("Null giving input\n");
    }
    if(from_me->dont_accept==TRUE){
        printf("Can't accept more tasks\n");
        return;
    }

    work_t* work=(work_t*)malloc(sizeof(work_t));
    if(work==NULL)
    {
        error("Cannot allocate memory\n");
    }
    //Create work_t structure and init it with the routine and argument.
    work->routine=dispatch_to_here;
    work->arg=arg;
    work->next=NULL;
    // add the new job - add an item to the queue
    pthread_mutex_lock(&from_me->qlock);

    if(from_me->qhead==NULL){
        from_me->qhead=work;
        from_me->qtail=work;
    }
    //flag next
    else{
        from_me->qtail->next=work;
        from_me->qtail=from_me->qtail->next;
    }
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    //conjunction
    pthread_cond_signal(&from_me->q_not_empty);
}

void* do_work(void* p){
    threadpool* th=(threadpool*)p;

    while(TRUE){ // endless loop
        //If the destruction process has begun, exit the thread
        if(th ==NULL){
            error("Failed to cast\n");
        }
        pthread_mutex_lock(&th->qlock);
        if(th->shutdown==TRUE){
            pthread_mutex_unlock(&th->qlock);
            return NULL;

        }

        if(th->qsize==EMPTY) //If the queue is empty, wait (no job to make)
            pthread_cond_wait(&th->q_not_empty,&th->qlock);

        if(th->shutdown==TRUE) { //flag again destruction flag
            pthread_mutex_unlock(&th->qlock);
            pthread_exit(SUCCESS);
        }

        work_t* work=th->qhead;//Take the first element from the queue (*work_t)
        if(work == NULL)
        {
            error("The first element not found!\n");
        }
        th->qsize--;
        if(th->qsize==EMPTY){
            //the queue is empty, so we set the head and the tail to null again
            th->qhead=NULL;
            th->qtail=NULL;
            //If the queue becomes empty and the destruction process wait to begin, signal the
            //destruction process.
            if(th->dont_accept==TRUE)
                pthread_cond_signal(&th->q_empty);
        }
        else
            th->qhead=th->qhead->next; //next element

        pthread_mutex_unlock(&th->qlock);
        //Call the thread routine
        work->routine(work->arg);
        free(work);
    }
}

//finish thread pool
void destroy_threadpool(threadpool* destroyme){
    if(destroyme == NULL || destroyme->threads == NULL)
        return;
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept=TRUE; //Set the don’t_accept flag to 1
    if(destroyme->qsize!=EMPTY)//Wait for the queue to become empty -> threads finish the queue
        pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock);
    destroyme->shutdown=TRUE; //Set the shutdown flag to 1
    pthread_mutex_unlock(&destroyme->qlock);
    //Signal threads that wait on ‘empty queue’, so they can wake up, see the shutdown
    //flag, and exit
    //wake all threads that are waiting
    pthread_cond_broadcast(&destroyme->q_not_empty);

    int flag;
    for(int i=0;i< destroyme->num_threads; i++) {
        //Join all threads
        flag = pthread_join(destroyme->threads[i],NULL);
        if (flag != SUCCESS) {
            error("Join failed\n");
        }
    }
    free(destroyme->threads);
    destroyme->threads = NULL;
    free(destroyme);
}
//perror and exit
void * error(char* str){
    perror(str);
    return NULL;
}
