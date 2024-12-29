#include <pthread.h>

typedef pthread_t thrd_t;
typedef int (*thrd_start_t) (void*);

#define thrd_success 0

#define thrd_create(a,b,c) pthread_create(a,NULL,(void* (*)(void *))b,c)
#define thrd_join(a,b) pthread_join(a,b)
#define thrd_sleep(a,b) nanosleep(a,b)