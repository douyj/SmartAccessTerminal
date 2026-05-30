## 通信协议

### 图片上传协议

客户端向服务端发送：

[4-byte json_len][json_header][image_data]

其中 json_len 使用网络字节序，大端。

JSON Header 示例：

{
  "type": "snapshot",
  "device_id": "imx6ull_001",
  "filename": "dyj.jpg",
  "image_size": 80331,
  "timestamp": "2026-05-30 13:29:13"
}

### 服务端返回协议

服务端返回一行 JSON，并以 \n 结尾：

{
  "type": "result",
  "result": "allow",
  "name": "DengYangjie",
  "confidence": 0.96,
  "action": "open_door",
  "message": "face ok"
}