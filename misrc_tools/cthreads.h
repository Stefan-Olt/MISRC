#ifdef _WIN32
#include <windows.h>
#include <process.h>
typedef HANDLE thrd_t;
typedef unsigned (__stdcall *thrd_start_t) (void*);
#define thrd_success 0
typedef struct timespec {
    long tv_sec;
    long tv_nsec;
} timespec_t;
#define thrd_create(thrd, func, arg) (
    ((*(thrd) = (HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL)) == 0) ? -1 : thrd_success
)
#define thrd_join(thrd, res) (
    (WaitForSingleObject(thrd, INFINITE) != WAIT_OBJECT_0 || !GetExitCodeThread(thrd, res)) ? -1 : (CloseHandle(thrd), thrd_success)
)
#define thrd_sleep(ts, rem) (Sleep((ts)->tv_sec * 1000 + (ts)->tv_nsec / 1000000), 0)
#else
#include <pthread.h>
typedef pthread_t thrd_t;
typedef int (*thrd_start_t)(void*);
#define thrd_success 0
#define thrd_create(a,b,c) pthread_create(a,NULL,(void* (*)(void *))b,c)
#define thrd_join(a,b) pthread_join(a,b)
#define thrd_sleep(a,b) nanosleep(a,b)
#endif
