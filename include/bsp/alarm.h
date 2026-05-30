#ifndef __ALARM_H
#define __ALARM_H

int alarm_init(void);
int alarm_beep(int duration_ms);
int alarm_stop(void);
int alarm_deinit(void);

#endif