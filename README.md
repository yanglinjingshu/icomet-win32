# icomet-win32
本项目基于ideawu的icomet项目，但是运行在windows系统之上。相当于是windows版本的icomet的服务器。文件名可能与icomet原始版本的不一致，请见谅。如有侵权，原作者如告知将删除。此项目仅用于验证和学习libevent。原作者的icomet源码地址：https://github.com/ideawu/icomet。

部分功能也做了更改：
1、去掉了-d参数
2、无需指定配置文件的路径，默认路径为可执行文件的路径。
3、不会读写pid文件

因本程序用于测试实现网页聊天功能，因此项目以chatserv命名。相关的配置文件也未采用icomet作者的命名方式。


使用的第三方库包括：
libevent 2.0.22版本：http://libevent.org/
pthread-win：http://www.sourceware.org/pthreads-win32/


开发工具采用vs2013，需要在项目中设置头文件以及库文件的路径。
