#include<sys/time.h>
#include<time.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include "threadpool.h"


void* work(void* arg)
{
    int *p = (int*) arg;
    printf("threadID is %lu,threadpool callback fuction:%d.\n",
           pthread_self(),
           *p);
    int k = 2000000;
    while(k > 0) {
        k--;
    }
    free(p);
}

int main(void)
{
    struct timeval time_start;
    gettimeofday(&time_start, NULL);
    printf("start now, sec is %ld, m_sec is %ld \n", 
            time_start.tv_sec, 
            time_start.tv_usec);
    int thread_nums = 4;
    struct threadpool *pool = threadpool_init(thread_nums, NULL);

    int tasks = 50000;
    int i = 0;
    int j = 0;
    for(j=0; j<tasks; ) {
        for(i=0; i<thread_nums && j<tasks; i++ ) {
            int *par = malloc(sizeof(int));
            *par = j;
            threadpool_add_job(pool, i, work, par);
            j++;
        }
    }
    struct timeval time_add_task_end;
    gettimeofday(&time_add_task_end, NULL);
    threadpool_destroy(pool);

    struct timeval time_end;
    gettimeofday(&time_end, NULL);
    printf("end now, sec is %ld, m_sec is %ld \n", 
            time_end.tv_sec, 
            time_end.tv_usec);

    printf("add tasks end at sec is %ld, m_sec is %ld \n", 
            time_add_task_end.tv_sec - time_start.tv_sec,
            time_add_task_end.tv_usec - time_start.tv_usec);

    printf("interval is %ld, m_sec is %ld \n", 
            time_end.tv_sec - time_start.tv_sec,
            time_end.tv_usec - time_start.tv_usec);

    return 0;
}