#include "threadpool.h"
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include <assert.h>

struct threadpool* threadpool_init(int thread_num, 
                                   int queue_max_num, 
                                   void*(*thread_enter)(void* arg))
{
    struct threadpool *pool = NULL;
    do {


        pool = malloc(sizeof(struct threadpool));
        if (NULL == pool) {
            printf("failed to malloc threadpool!\n");
            break;
        }

        if( pthread_mutx_init( &(pool->mutex) , NULL) {
            printf("failed to init mutex\n");
            break;
        }

        pool->thread_num = thread_num;

        pool->pthreads = malloc(sizeof(pthread_t) * thread_num);
        if (NULL == pool->pthreads) {
            printf("failed to malloc pthreads!\n");
            break;
        }

        pool->head_jobs_perthr = malloc(sizeof(jobs_perthread) * thread_num);
        if (NULL == pool->head_jobs_perthr) {
            printf("failed to malloc head_jobs_perthr");
            break;
        }

        pool->pool_close = 0;

        int i = 0;
        for (i = 0; i < pool->thread_num; ++i)
        {
            pthread_create(&(pool->pthreads[i]), NULL, thread_enter, (void *)pool);
        }
        
        return pool;    
    } while (0);
    
    return NULL;
}


//use poll method to add job to every thread
int threadpool_add_job(struct threadpool* pool, void* (*callback_function)(void *arg), void *arg)
{
    static int cur_thread = 0;
    assert(pool != NULL);
    assert(callback_function != NULL);

    //队列关闭或者线程池关闭就退出
    if (pool->pool_close) {
        printf("thread pool was closed\n");
        return -1;
    }

    pthread_mutex_lock(&(pool->mutex));
    if(cur_thread >= pool->thread_num) {
        cur_thread = 0;
    }
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
    pthread_mutex_lock( &( (pool->head_jobs_perthr)[cur_thread].mutex) );  

    if( (pool->head_jobs_perthr)[cur_thread].head == NULL ) {
        (pool->head_jobs_perthr)[cur_thread].head = pjob;
        (pool->head_jobs_perthr)[cur_thread].tail = pjob;
    }
    else {
        (pool->head_jobs_perthr)[cur_thread].tail->next = pjob;
        (pool->head_jobs_perthr)[cur_thread].tail = pjob;
    }

    (pool->head_jobs_perthr)[cur_thread].tail = pjob;

    (pool->head_jobs_perthr)[cur_thread].job_nums++;

    pthread_mutex_lock( &( (pool->head_jobs_perthr)[cur_thread].mutex) );  

    pthread_mutex_unlock(&(pool->mutex));
    return 0;
}

//void* threadpool_function(void* arg)
//{
//    struct threadpool *pool = (struct threadpool*)arg;
//    struct job *pjob = NULL;
//    while (1)  //死循环
//    {
//        pthread_mutex_lock(&(pool->mutex));
//        while ((pool->queue_cur_num == 0) && !pool->pool_close)   //队列为空时，就等待队列非空
//        {
//            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex));
//        }
//        if (pool->pool_close)   //线程池关闭，线程就退出
//        {
//            pthread_mutex_unlock(&(pool->mutex));
//            pthread_exit(NULL);
//        }
//        pool->queue_cur_num--;
//        pjob = pool->head;
//        if (pool->queue_cur_num == 0)
//        {
//            pool->head = pool->tail = NULL;
//        }
//        else 
//        {
//            pool->head = pjob->next;
//        }
//        if (pool->queue_cur_num == 0)
//        {
//            pthread_cond_signal(&(pool->queue_empty));        //队列为空，就可以通知threadpool_destroy函数，销毁线程函数
//        }
//        if (pool->queue_cur_num == pool->queue_max_num - 1)
//        {
//            pthread_cond_broadcast(&(pool->queue_not_full));  //队列非满，就可以通知threadpool_add_job函数，添加新任务
//        }
//        pthread_mutex_unlock(&(pool->mutex));
//        
//        (*(pjob->callback_function))(pjob->arg);   //线程真正要做的工作，回调函数的调用
//        free(pjob);
//        pjob = NULL;    
//    }
//}

int threadpool_destroy(struct threadpool *pool)
{
    assert(pool != NULL);
    pthread_mutex_lock(&(pool->mutex));
    if (pool->queue_close || pool->pool_close)   //线程池已经退出了，就直接返回
    {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
    }
    
    pool->queue_close = 1;        //置队列关闭标志
    while (pool->queue_cur_num != 0)
    {
        pthread_cond_wait(&(pool->queue_empty), &(pool->mutex));  //等待队列为空
    }    
    
    pool->pool_close = 1;      //置线程池关闭标志
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_broadcast(&(pool->queue_not_empty));  //唤醒线程池中正在阻塞的线程
    pthread_cond_broadcast(&(pool->queue_not_full));   //唤醒添加任务的threadpool_add_job函数
    int i;
    for (i = 0; i < pool->thread_num; ++i)
    {
        pthread_join(pool->pthreads[i], NULL);    //等待线程池的所有线程执行完毕
    }
    
    pthread_mutex_destroy(&(pool->mutex));          //清理资源
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_empty));   
    pthread_cond_destroy(&(pool->queue_not_full));    
    free(pool->pthreads);
    struct job *p;
    while (pool->head != NULL)
    {
        p = pool->head;
        pool->head = p->next;
        free(p);
    }
    free(pool);
    return 0;
}