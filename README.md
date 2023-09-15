# asio3
Header only c++ network library, based on c++ 20 coroutine and asio

## 设计思路
* 基于c++20, 单线程协程模式, 以简洁易用为目标
* 不再定义其它的namespace, 而是直接在namespace asio中添加功能, 这样用户在自己的代码中通过namespace net=asio;重定向一下命名空间, 然后统一使用net::xxx即可, 代码更干净;
* 避免继承, 尽量使用组合模式, 将各种功能封装成一个个独立的api接口函数, 用户自行选择调用
* 封装的api函数尽量保证同时支持callback,future,coroutine等各种调用方式
* 各种属性类不再采用get set这种方式, 而是采用c++20的指定初始化功能, 类似js的语法, 非常清爽
* 使用concept代替sfinae, 尽量使用c++20的新特性
* 提供轻框架支持
