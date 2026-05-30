import socket
import struct
import json
import os
import time


HOST = "0.0.0.0"
PORT = 9000
SAVE_DIR = "recv_images"


def recv_all(conn, size):
    data = b""

    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            raise ConnectionError("client closed connection")
        data += chunk

    return data


def fake_face_recognize(filename):
    """
    第二关：假识别函数
    后面第三/四关可以把这里替换成 OpenCV / face_recognition。
    """
    name = filename.lower()

    if "noface" in name:
        return {
            "type": "result",
            "result": "no_face",
            "name": "none",
            "confidence": 0.0,
            "action": "none",
            "message": "no face detected"
        }

    if "dyj" in name or "allow" in name:
        return {
            "type": "result",
            "result": "allow",
            "name": "DengYangjie",
            "confidence": 0.96,
            "action": "open_door",
            "message": "access granted"
        }

    if "unknown" in name or "deny" in name:
        return {
            "type": "result",
            "result": "deny",
            "name": "unknown",
            "confidence": 0.35,
            "action": "alarm",
            "message": "unknown person"
        }

    return {
        "type": "result",
        "result": "deny",
        "name": "unknown",
        "confidence": 0.30,
        "action": "alarm",
        "message": "default deny"
    }


def handle_client(conn, addr):
    print(f"[INFO] client connected: {addr}")

    try:
        raw_len = recv_all(conn, 4)
        json_len = struct.unpack("!I", raw_len)[0]
        print(f"[INFO] json length: {json_len}")

        json_data = recv_all(conn, json_len)
        header = json.loads(json_data.decode("utf-8"))
        print(f"[INFO] header: {header}")

        image_size = int(header["image_size"])
        filename = header.get("filename", "unknown.jpg")
        device_id = header.get("device_id", "unknown_device")

        image_data = recv_all(conn, image_size)

        os.makedirs(SAVE_DIR, exist_ok=True)

        timestamp = time.strftime("%Y%m%d_%H%M%S")
        save_name = f"{timestamp}_{device_id}_{filename}"
        save_path = os.path.join(SAVE_DIR, save_name)

        with open(save_path, "wb") as f:
            f.write(image_data)

        print(f"[INFO] image saved: {save_path}")
        print(f"[INFO] image size: {len(image_data)} bytes")

        result = fake_face_recognize(filename)
        result["snapshot_path"] = save_path
        result["timestamp"] = time.strftime("%Y-%m-%d %H:%M:%S")

        result_str = json.dumps(result, ensure_ascii=False) + "\n"
        conn.sendall(result_str.encode("utf-8"))

        print(f"[INFO] result sent: {result}")

    except Exception as e:
        print(f"[ERROR] {e}")

    finally:
        conn.close()
        print(f"[INFO] client disconnected: {addr}")


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    server.bind((HOST, PORT))
    server.listen(5)

    print(f"[INFO] TCP image server listening on {HOST}:{PORT}")

    while True:
        conn, addr = server.accept()
        handle_client(conn, addr)


if __name__ == "__main__":
    main()