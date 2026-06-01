# 运行lvgl编译
cd ~/linux_project/SmartAccessTerminal

rm -rf build_pc
mkdir build_pc
cd build_pc

cmake .. \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DBUILD_LVGL_UI_TEST=ON

make test_app_state_ui -j4
./test_app_state_ui

# 启动ubuntu gui
sudo systemctl start graphical.target

# 启动容器
```shell
docker start -ai atk-x86-build
```

# 编译PC版
```shell
cd ~/linux_project/SmartAccessTerminal

rm -rf build_pc
mkdir build_pc
cd build_pc

cmake .. \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DBUILD_LVGL_UI_TEST=ON

make test_v4l2_rgb565_capture -j4
```

# 编译arm版
```shell
cd ~/linux_project/SmartAccessTerminal

rm -rf build_arm
mkdir build_arm
cd build_arm

cmake .. \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DBUILD_LVGL_UI_TEST=OFF

make test_v4l2_rgb565_capture -j4
```