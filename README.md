# simple
c++20协程框架

## 说明
仅用于学习使用  
log库修改自 https://github.com/gabime/spdlog ，主要是为了学习下spdlog，实际项目请使用spdlog  
协程部分参考自 https://github.com/lewissbaker/cppcoro 以及asio的部分内容  

## 编译需要
```
vcpkg install protobuf
vcpkg install asio
vcpkg install fmt
vcpkg install kcp
vcpkg install lz4
vcpkg install openssl
vcpkg install toml11
```

可以使用命令`vcpkg integrate install`, 然后vs2022直接打开文件夹  
或者在vs2022的开发者命令行中，使用下面的命令生成解决方案
```
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=XXX/vcpkg/scripts/buildsystems/vcpkg.cmake ..
```

## 运行例子
```
cd bin\Debug
.\executor.exe ..\..\config\test.toml
```

启动五子棋服务器（未完成）
```
.\executor.exe ..\..\config\server.toml
```
