#ifndef SNAPSHOT_STORAGE_H
#define SNAPSHOT_STORAGE_H

#define SNAPSHOT_PATH_MAX_LEN 256

int snapshot_storage_init(const char *snapshot_dir);
int snapshot_storage_make_path(char *path_buf, int buf_size);
const char *snapshot_storage_get_dir(void);

#endif
