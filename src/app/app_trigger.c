#include "app/app_trigger.h"
#include "common/log.h"

#include <pthread.h>
#include <time.h>
#include <errno.h>

#define APP_TRIGGER_COOLDOWN_MS 3000

static pthread_mutex_t g_trigger_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_trigger_cond = PTHREAD_COND_INITIALIZER;

static int g_trigger_pending = 0;
static int g_trigger_stop = 0;

static AppTriggerState g_trigger_state = APP_TRIGGER_IDLE;
static long long g_cooldown_start_ms = 0;


static long long get_time_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}


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
    g_trigger_state = APP_TRIGGER_IDLE;
    g_cooldown_start_ms = 0;

    pthread_mutex_unlock(&g_trigger_mutex);

    LOG_INFO("[TRIGGER] init, state=IDLE");
}


int app_trigger_request(void)
{
    int ret = 0;

    pthread_mutex_lock(&g_trigger_mutex);

    if (g_trigger_stop) {
        LOG_WARN("[TRIGGER] stopped, trigger ignored");
        ret = -1;
    }
    else if (g_trigger_state == APP_TRIGGER_BUSY) {
        LOG_WARN("[TRIGGER] busy, trigger ignored");
        ret = 0;
    }
    else if (g_trigger_state == APP_TRIGGER_COOLDOWN) {
        LOG_WARN("[TRIGGER] cooldown, trigger ignored");
        ret = 0;
    }
    else if (g_trigger_pending) {
        LOG_WARN("[TRIGGER] already pending, trigger ignored");
        ret = 0;
    }
    else {
        g_trigger_pending = 1;
        pthread_cond_signal(&g_trigger_cond);
        LOG_INFO("[TRIGGER] request accepted");
        ret = 1;
    }

    pthread_mutex_unlock(&g_trigger_mutex);

    return ret;
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
                pthread_mutex_unlock(&g_trigger_mutex);
                return 0;
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


void app_trigger_mark_busy(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    if (!g_trigger_stop) {
        g_trigger_state = APP_TRIGGER_BUSY;
        g_trigger_pending = 0;
        LOG_INFO("[TRIGGER] state=BUSY");
    }

    pthread_mutex_unlock(&g_trigger_mutex);
}


void app_trigger_mark_done(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    if (!g_trigger_stop) {
        g_trigger_state = APP_TRIGGER_COOLDOWN;
        g_cooldown_start_ms = get_time_ms();
        g_trigger_pending = 0;
        LOG_INFO("[TRIGGER] state=COOLDOWN, cooldown=%d ms", APP_TRIGGER_COOLDOWN_MS);
    }

    pthread_mutex_unlock(&g_trigger_mutex);
}


void app_trigger_update(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    if (!g_trigger_stop && g_trigger_state == APP_TRIGGER_COOLDOWN) {
        long long now_ms = get_time_ms();

        if (now_ms - g_cooldown_start_ms >= APP_TRIGGER_COOLDOWN_MS) {
            g_trigger_state = APP_TRIGGER_IDLE;
            g_cooldown_start_ms = 0;
            LOG_INFO("[TRIGGER] cooldown finished, state=IDLE");
        }
    }

    pthread_mutex_unlock(&g_trigger_mutex);
}


void app_trigger_stop(void)
{
    pthread_mutex_lock(&g_trigger_mutex);

    g_trigger_stop = 1;
    g_trigger_pending = 0;
    g_trigger_state = APP_TRIGGER_STOPPED;

    pthread_cond_broadcast(&g_trigger_cond);

    pthread_mutex_unlock(&g_trigger_mutex);

    LOG_INFO("[TRIGGER] stopped");
}


AppTriggerState app_trigger_get_state(void)
{
    AppTriggerState state;

    pthread_mutex_lock(&g_trigger_mutex);
    state = g_trigger_state;
    pthread_mutex_unlock(&g_trigger_mutex);

    return state;
}
