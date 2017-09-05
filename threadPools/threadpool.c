#include "threadpool.h"
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include <assert.h>

struct threadpool* threadpool_init(int thread_nums, 
                                   void*(*thread_enter)(void* arg))
{
    struct threadpool *pool = NULL;

    do {
        pool = malloc(sizeof(struct threadpool));
        if (NULL == pool) {
            printf("failed to malloc threadpool!\n");
            break;
        }

        if(pthread_mutex_init(&(pool->mutex) ,NULL )){
            printf("failed to init mutex\n");
            break;
        }

        pool->thread_nums = thread_nums;

        pool->pthreads = malloc(sizeof(pthread_t) * thread_nums);
        if (NULL == pool->pthreads) {
            printf("failed to malloc pthreads!\n");
            break;
        }

        pool->head_jobs_perthr = malloc(sizeof(struct jobs_perthread) * thread_nums);
        if (NULL == pool->head_jobs_perthr) {
            printf("failed to malloc head_jobs_perthr");
            break;
        }

        pool->pool_close = 0;

        //per thread jobs init
        void* (* temp_function) (void *);
        temp_function = thread_enter;
        if(temp_function == NULL) {
            temp_function = threadpool_default_enter;
        }
        int i = 0;
        for (i = 0; i < pool->thread_nums; ++i)
        {
            struct jobs_perthread *cur_jobs = (pool->head_jobs_perthr + i);
            if (pthread_mutex_init( &(cur_jobs->mutex) ,NULL) ) {
                printf("failed to init thread %d mutex\n", i);
                break;
            }

            if (pthread_cond_init( &(cur_jobs->job_empty), NULL) ) {
                printf("falied to init thread %d job_empty\n", i);
                break;
            }
            (pool->head_jobs_perthr)[i].num = i;
            (pool->head_jobs_perthr)[i].job_nums = 0;
            (pool->head_jobs_perthr)[i].head = NULL;
            (pool->head_jobs_perthr)[i].tail = NULL;
            (pool->head_jobs_perthr)[i].thread_close = 0;
            pthread_create(&(pool->pthreads[i]), 
                           NULL, temp_function, 
                           (void *)(pool->head_jobs_perthr+i));
        }
        
        return pool;    
    } while (0);
    //init failed release the resource
    if(pool!=NULL) {
        if(pool->pthreads != NULL) {
            free(pool->pthreads);
        }
        if(pool->head_jobs_perthr != NULL) {
            free(pool->head_jobs_perthr);
        }
        free(pool);
    } 
    return NULL;
}


//use poll method to add job to every thread
int threadpool_add_job(struct threadpool* pool, 
                       int target_thread,
                       void* (*callback_function)(void *arg), 
                       void *arg)
{
    assert(pool != NULL);
    assert(callback_function != NULL);
    assert(target_thread< (pool->thread_nums));
    //队列关闭或者线程池关闭就退出
    if (pool->pool_close) {
        printf("thread pool was closed, can't add job\n");
        return -1;
    }

    pthread_mutex_lock(&(pool->mutex));
    //if(target_thread >= pool->thread_nums) {
    //    target_thread = 0;
    //}
    struct job *pjob =(struct job*) malloc(sizeof(struct job));
    if (NULL == pjob)
    {
        pthread_mutex_unlock(&(pool->mutex));
        printf("can't malloc for job\n");
        return -1;
    } 
    pjob->callback_function = callback_function;    
    pjob->arg = arg;
    pjob->next = NULL;
    pthread_mutex_lock( &( (pool->head_jobs_perthr)[target_thread].mutex) );  

    if( (pool->head_jobs_perthr)[target_thread].head == NULL ) {
        assert((pool->head_jobs_perthr)[target_thread].tail == NULL);
        (pool->head_jobs_perthr)[target_thread].head = pjob;
        (pool->head_jobs_perthr)[target_thread].tail = pjob;
    }
    else {
        (pool->head_jobs_perthr)[target_thread].tail->next = pjob;
        (pool->head_jobs_perthr)[target_thread].tail = pjob;
    }

    (pool->head_jobs_perthr)[target_thread].tail = pjob;

    (pool->head_jobs_perthr)[target_thread].job_nums++;
    //if((pool->head_jobs_perthr)[target_thread].job_nums == 1) {
        printf("add a job and signal\n");
        pthread_cond_signal(&((pool->head_jobs_perthr)[target_thread].job_empty));
    //}
    pthread_mutex_unlock( &( (pool->head_jobs_perthr)[target_thread].mutex) );  

    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

void* threadpool_default_enter(void* arg)
{
    assert(arg != NULL);

    struct jobs_perthread *thread_jobs = (struct jobs_perthread*)arg;
    struct job *pjob = NULL;
    while (1) {
        pthread_mutex_lock(&(thread_jobs->mutex));
        //when jobs list is empty, wait 
        while(thread_jobs->job_nums == 0 && !thread_jobs->thread_close) {
            printf("job is empty\n");
            pthread_cond_wait(&(thread_jobs->job_empty), &(thread_jobs->mutex));
        }
        if(thread_jobs->job_nums == 0 && thread_jobs->thread_close) {
            printf("thread num is %d, ID is %lu exit\n", 
                    thread_jobs->num, 
                    pthread_self());
            pthread_mutex_unlock(&(thread_jobs->mutex));
            pthread_exit(NULL);
        }

        //get job
        assert(thread_jobs->head != NULL);
        struct job *cur_job = thread_jobs->head;
        thread_jobs->head = cur_job->next;
        if(thread_jobs->head == NULL) {
            thread_jobs->tail = NULL;
        }
        thread_jobs->job_nums--;
        pthread_mutex_unlock(&(thread_jobs->mutex));

        //do job
        (*(cur_job->callback_function))(cur_job->arg);
        free(cur_job);
        pjob = NULL;    
    }
}

int threadpool_destroy(struct threadpool *pool)
{
    assert(pool != NULL);
    pthread_mutex_lock(&(pool->mutex));
    if (pool->pool_close)   //线程池已经退出了，就直接返回
    {
        printf("pool already closed\n");
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    pool->pool_close = 1;      //置线程池关闭标志

    int i = 0;
    for(i = 0; i < pool->thread_nums; i++) {
        struct jobs_perthread *cur_jobs = (pool->head_jobs_perthr + i);
        pthread_mutex_lock(&(cur_jobs->mutex));
        cur_jobs->thread_close = 1;
        pthread_cond_signal(&(cur_jobs->job_empty));
        pthread_mutex_unlock(&(cur_jobs->mutex));
    }
    
    for (i = 0; i < pool->thread_nums; ++i)
    {
        pthread_join(pool->pthreads[i], NULL);    //等待线程池的所有线程执行完毕
    }
    
    //all threads have been closed, now release the resource
    for(i = 0; i < pool->thread_nums; i++) {
        struct jobs_perthread *cur_jobs = (pool->head_jobs_perthr + i);
        pthread_mutex_destroy(&(cur_jobs->mutex));
        pthread_cond_destroy(&(cur_jobs->job_empty));
    }

    free(pool->pthreads);
    free(pool->head_jobs_perthr);

    free(pool);
    return 0;
}