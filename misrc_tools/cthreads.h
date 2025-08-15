#ifdef _WIN32
  #include <windows.h>
  #include <process.h>
  
  typedef HANDLE thrd_t;
  typedef unsigned (__stdcall *thrd_start_t) (void*);
  #if defined(_TIMESPEC_DEFINED)
    typedef struct timespec timespec_t;
  #else
    typedef struct timespec { long tv_sec; long tv_nsec; } timespec_t;
  #endif

  #define thrd_success 0

  #define thrd_create(a,b,c) (((*(a)=(thrd_t)_beginthreadex(NULL,0,(thrd_start_t)b,c,0,NULL))==0)?-1:thrd_success)
  #define thrd_join(a,b) ((WaitForSingleObject(a,INFINITE)!=WAIT_OBJECT_0||!GetExitCodeThread(a,b))?-1:(CloseHandle(a),thrd_success))
  #define thrd_sleep(a,b) Sleep((a)->tv_sec*1000+(a)->tv_nsec/1000000)

#else
  #include <pthread.h>

  typedef pthread_t thrd_t;
  typedef int (*thrd_start_t) (void*);

  #define thrd_success 0

  #define thrd_create(a,b,c) pthread_create(a,NULL,(void* (*)(void *))b,c)
  #define thrd_join(a,b) pthread_join(a,b)
  #define thrd_sleep(a,b) nanosleep(a,b)

#endif
