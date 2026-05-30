# 运行lvgl编译
rm -rf build_pc
mkdir build_pc
cd build_pc

cmake .. \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DBUILD_LVGL_UI_TEST=ON

make test_lvgl_ui -j4

# 启动ubuntu gui
sudo systemctl start graphical.target