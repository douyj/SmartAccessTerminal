# V1版本
```shell
客户端发送 jpg → 服务端保存图片 → 服务端固定返回 allow
```

# V2版本
```shell
客户端发送 jpg
        ↓
服务端根据文件名模拟识别
        ↓
返回 allow / deny / no_face
        ↓
客户端解析 JSON
        ↓
打印：开门 / 拒绝 / 未检测到人脸
```