#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

/* 将普通字符串安全写入 buf */
void write_text(char *buf, size_t buf_size, const char *text);

/* 将 cJSON 对象转成 JSON 字符串，并写入 buf */
bool write_json_string(char *buf, size_t buf_size, cJSON *root);

/* 返回 {"status":"error","message":"xxx"} */
bool write_error_json(char *buf, size_t buf_size, const char *message);

/* 返回 {"status":"ok"} */
bool write_ok_json(char *buf, size_t buf_size);

/* 返回 {"status":"ok","data":{...}} */
bool write_ok_data_json(char *buf, size_t buf_size, cJSON *data);

/* 从 JSON 对象中取 int */
bool json_get_int(cJSON *obj, const char *key, int *out_value);

/* 从 JSON 对象中取 bool-like：true/false/0/1 */
bool json_get_bool_like(cJSON *obj, const char *key, int *out_value);

/* 从 JSON 对象中取字符串 */
bool json_get_string(cJSON *obj, const char *key, const char **out_value);

/* 从 JSON 对象中取 object */
bool json_get_object(cJSON *obj, const char *key, cJSON **out_obj);

/* 从 JSON 对象中取 array */
bool json_get_array(cJSON *obj, const char *key, cJSON **out_array);

#endif