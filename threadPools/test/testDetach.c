#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

static pthread_cond_t cond; 
static pthread_mutex_t mutex;

void *product(void *args) {
	printf("pid = %d, pthread_id = %lu\n", (int)getpid(), pthread_self());
	pthread_exit(NULL);
	return NULL;
}

void *consumer(void *args) {
//	pthread_detach(pthread_self());

	struct timeval now;
	gettimeofday(&now, NULL);
	printf("now.tv_sec is  %d, now.tv_usec is %d\n", (int)now.tv_sec, now.tv_usec*1000);
	struct timespec outtime;
	outtime.tv_sec = now.tv_sec + 5;	
	outtime.tv_nsec = now.tv_usec*1000;	

	pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &outtime);	
	printf("pid = %d, pthread_id = %lu\n", (int)getpid(), pthread_self());
	return NULL;
}

int main(int argc, char const **argv) {
	
	pthread_mutex_init(&mutex, NULL);
	 pthread_cond_init(&cond, NULL);


	pthread_t id1, id2, id3;	
	pthread_create(&id1, NULL, product, NULL);
	pthread_create(&id2, NULL, product, NULL);
	pthread_create(&id3, NULL, consumer, NULL);

	pthread_join(id1, NULL);
	pthread_join(id2, NULL);
	pthread_join(id3, NULL);
//	pthread_detach(id3);
}
