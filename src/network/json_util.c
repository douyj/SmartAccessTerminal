#include "json_util.h"

#include <stdio.h>
#include <string.h>

/*
作用：
将普通字符串 text 安全写入 buf。

特点：
1. 会检查 buf 是否为空
2. 会检查 buf_size 是否为 0
3. 使用 snprintf，避免缓冲区溢出
4. text 为 NULL 时，写入空字符串
*/
void write_text(char *buf, size_t buf_size, const char *text)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (text == NULL) {
        buf[0] = '\0';
        return;
    }

    snprintf(buf, buf_size, "%s", text);
}

/*
作用：
把 cJSON 对象 root 转成 JSON 字符串，
然后写入 buf 响应缓冲区。

注意：
1. 使用 cJSON_PrintUnformatted 生成单行 JSON
2. 末尾加 '\n'，方便 TCP/Qt 按行分帧
3. 最后加 '\0'，保证是 C 字符串
*/
bool write_json_string(char *buf, size_t buf_size, cJSON *root)
{
    char *json_str;
    size_t len;

    if (buf == NULL || buf_size == 0 || root == NULL) {
        return false;
    }

    json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        write_text(buf, buf_size,
                   "{\"status\":\"error\",\"message\":\"internal json error\"}\n");
        return true;
    }

    len = strlen(json_str);

    /*
    len 是 JSON 字符串长度。
    还需要额外 2 个字节：
    1 个放 '\n'
    1 个放 '\0'
    */
    if (len + 2 > buf_size) {
        cJSON_free(json_str);
        write_text(buf, buf_size,
                   "{\"status\":\"error\",\"message\":\"response too large\"}\n");
        return true;
    }

    memcpy(buf, json_str, len);
    buf[len] = '\n';
    buf[len + 1] = '\0';

    cJSON_free(json_str);
    return true;
}

/*
作用：
生成统一格式的错误 JSON 响应。

生成格式：
{
    "status": "error",
    "message": "xxx"
}
*/
bool write_error_json(char *buf, size_t buf_size, const char *message)
{
    cJSON *root;
    bool ok;

    root = cJSON_CreateObject();
    if (root == NULL) {
        write_text(buf, buf_size,
                   "{\"status\":\"error\",\"message\":\"internal alloc error\"}\n");
        return true;
    }

    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "message",
                            (message != NULL) ? message : "unknown error");

    ok = write_json_string(buf, buf_size, root);

    cJSON_Delete(root);
    return ok;
}

/*
作用：
生成统一格式的成功 JSON 响应。

生成格式：
{
    "status": "ok"
}
*/
bool write_ok_json(char *buf, size_t buf_size)
{
    cJSON *root;
    bool ok;

    root = cJSON_CreateObject();
    if (root == NULL) {
        write_text(buf, buf_size,
                   "{\"status\":\"error\",\"message\":\"internal alloc error\"}\n");
        return true;
    }

    cJSON_AddStringToObject(root, "status", "ok");

    ok = write_json_string(buf, buf_size, root);

    cJSON_Delete(root);
    return ok;
}

/*
作用：
生成带 data 字段的成功响应。

生成格式：
{
    "status": "ok",
    "data": {
        ...
    }
}

重要：
如果 data != NULL，调用 cJSON_AddItemToObject 后，
data 的所有权会交给 root。
所以外部调用后不要再 cJSON_Delete(data)。
*/
bool write_ok_data_json(char *buf, size_t buf_size, cJSON *data)
{
    cJSON *root;
    bool ok;

    root = cJSON_CreateObject();
    if (root == NULL) {
        if (data != NULL) {
            cJSON_Delete(data);
        }

        write_text(buf, buf_size,
                   "{\"status\":\"error\",\"message\":\"internal alloc error\"}\n");
        return true;
    }

    cJSON_AddStringToObject(root, "status", "ok");

    if (data != NULL) {
        cJSON_AddItemToObject(root, "data", data);
    }

    ok = write_json_string(buf, buf_size, root);

    cJSON_Delete(root);
    return ok;
}

/*
作用：
从 JSON 对象 obj 中取指定 key 的整数值。

例如：
{
    "id": 1
}

json_get_int(obj, "id", &id);
*/
bool json_get_int(cJSON *obj, const char *key, int *out_value)
{
    cJSON *item;

    if (obj == NULL || key == NULL || out_value == NULL) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    *out_value = item->valueint;
    return true;
}

/*
作用：
从 JSON 对象 obj 中取 bool-like 值。

支持：
true  -> 1
false -> 0
1     -> 1
0     -> 0
非 0 数字 -> 1
*/
bool json_get_bool_like(cJSON *obj, const char *key, int *out_value)
{
    cJSON *item;

    if (obj == NULL || key == NULL || out_value == NULL) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        return false;
    }

    if (cJSON_IsBool(item)) {
        *out_value = cJSON_IsTrue(item) ? 1 : 0;
        return true;
    }

    if (cJSON_IsNumber(item)) {
        *out_value = (item->valueint != 0) ? 1 : 0;
        return true;
    }

    return false;
}

/*
作用：
从 JSON 对象 obj 中取字符串。

例如：
{
    "ip": "192.168.1.10"
}

json_get_string(obj, "ip", &ip);
*/
bool json_get_string(cJSON *obj, const char *key, const char **out_value)
{
    cJSON *item;

    if (obj == NULL || key == NULL || out_value == NULL) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }

    *out_value = item->valuestring;
    return true;
}

/*
作用：
从 JSON 对象 obj 中取子对象 object。

例如：
{
    "data": {
        "id": 1
    }
}

json_get_object(obj, "data", &data);
*/
bool json_get_object(cJSON *obj, const char *key, cJSON **out_obj)
{
    cJSON *item;

    if (obj == NULL || key == NULL || out_obj == NULL) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsObject(item)) {
        return false;
    }

    *out_obj = item;
    return true;
}

/*
作用：
从 JSON 对象 obj 中取数组 array。

例如：
{
    "items": [
        {"id": 1, "stock": 10},
        {"id": 2, "stock": 5}
    ]
}

json_get_array(obj, "items", &items);
*/
bool json_get_array(cJSON *obj, const char *key, cJSON **out_array)
{
    cJSON *item;

    if (obj == NULL || key == NULL || out_array == NULL) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsArray(item)) {
        return false;
    }

    *out_array = item;
    return true;
}