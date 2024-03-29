/*
 * @Author       : mark
 * @Date         : 2020-06-18
 * @copyleft GPL 2.0
 */
#include "./server/webserver.h"
//#include "log/log.h"
//#include "http/epoll.h"
//#include <stdio.h>

int main() {
    WebServer server(
        1316, 3, 60000, false,           /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "430430", "webdb", /* Mysql配置 */
        12, 6, true, 0, 1024);           /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    server.Start();
}