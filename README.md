# simple
c++20协程框架

## 说明
仅用于学习使用  
log库修改自 [spdlog](https://github.com/gabime/spdlog) ，主要是为了学习下`spdlog`，实际项目请使用`spdlog`  
协程部分参考自 [cppcoro](https://github.com/lewissbaker/cppcoro) 以及[asio](https://github.com/chriskohlhoff/asio)的部分内容  

游戏服务器的逻辑部分一般是单线程，因此，框架中的协程只会运行在一个单线程的调度器中
如果要用于一些其他通用的地方，可以实现一个抢占式的协程调度器，类似golang的gpm，或者rust的tokio的调度方式，当然直接使用
golang或者rust也不错。c++的协程库还有 [async_simple](https://github.com/alibaba/async_simple)

除了协程的调度器，还提供一个可配置线程数的线程池，用于读写数据库、读写文件、异步的ai计算、异步的寻路等  
此外，还有一个线程专门用于写日志文件，一个线程对日志进行lz4压缩，一个asio网络线程，一个检查共享内存通道的线程  

服务为一个独立的功能，需要继承`simple::service_base` 实现 `awake()`，
如果需要每几帧调用一次某个逻辑，可以重写`update()`，并在配置文件中设置调用的间隔`interval`（帧）。
服务的动态库，需要导出两个函数分别为 `xxx_create` 和 `xxx_release`, `xxx` 为服务的类型名字与配置文件中的`type`对应。  
可以参看`config/test.toml`

框架根据配置文件加载服务，所有的服务与协程调度器运行在同一个单线程中

## 编译需要
```
vcpkg install protobuf
vcpkg install asio
vcpkg install fmt
vcpkg install kcp
vcpkg install lz4
vcpkg install openssl
vcpkg install toml11
vcpkg install gtest
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
