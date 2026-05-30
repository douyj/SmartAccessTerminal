#ifndef ACCESS_PROTOCOL_H
#define ACCESS_PROTOCOL_H

int access_build_snapshot_header(
    char *json_buf,
    int buf_size,
    const char *device_id,
    const char *filename,
    int image_size
);

#endif