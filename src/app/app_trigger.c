#include "app/app_trigger.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>

static pthread_mutex_t g_trigger_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_trigger_cond = PTHREAD_COND_INITIALIZER;

static int g_trigger_pending = 0;
static int g_trigger_stop = 0;

static void make_abs_timespec(struct timespec *ts, int timeout_ms)
{
    clock_gettime(CLOCK_REALTIME, ts);

    ts->tv_sec += timeout_ms / 1000;
    ts->tv_nsec += (timeout_ms % 1000) * 1000000L;

    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

void app_trigger_init(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    g_trigger_pending = 0;
    g_trigger_stop = 0;

    pthread_mutex_unlock(&g_trigger_mutex);
}

void app_trigger_request(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    g_trigger_pending = 1;
    pthread_cond_signal(&g_trigger_cond);

    pthread_mutex_unlock(&g_trigger_mutex);
}

int app_trigger_wait(int timeout_ms)
{
    int ret = 0;

    pthread_mutex_lock(&g_trigger_mutex);

    while (!g_trigger_pending && !g_trigger_stop) {
        if (timeout_ms < 0) {
            pthread_cond_wait(&g_trigger_cond, &g_trigger_mutex);
        } else {
            struct timespec ts;
            make_abs_timespec(&ts, timeout_ms);

            int wait_ret = pthread_cond_timedwait(&g_trigger_cond,
                                                  &g_trigger_mutex,
                                                  &ts);
            if (wait_ret == ETIMEDOUT) {
                ret = 0;
                pthread_mutex_unlock(&g_trigger_mutex);
                return ret;
            }
        }
    }

    if (g_trigger_stop) {
        ret = -1;
    } else if (g_trigger_pending) {
        g_trigger_pending = 0;
        ret = 1;
    }

    pthread_mutex_unlock(&g_trigger_mutex);

    return ret;
}

void app_trigger_stop(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    g_trigger_stop = 1;
    pthread_cond_broadcast(&g_trigger_cond);

    pthread_mutex_unlock(&g_trigger_mutex);
}
