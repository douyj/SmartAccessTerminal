#ifndef __DOOR_H
#define __DOOR_H

int door_init(void);
int door_open(int duration_ms);
int door_close(void);
int door_deinit(void);

#endif