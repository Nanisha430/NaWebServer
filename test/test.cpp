
#include "../code/log/log.h"
#include "../code/pool/threadpool.h"
#include <cstddef>
#include <features.h>
#include <mysql/mysql.h>
#include <iostream>

using namespace std;

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

void TestLog() {
    int cnt = 0, level = 0;
    Log::Instance()->init(level, "./testlog1", ".log", 0);
    for (level = 3; level >= 0; level--) {
        Log::Instance()->SetLevel(level);
        for (int j = 0; j < 10000; j++) {
            for (int i = 0; i < 4; i++) {
                LOG_BASE(i, "%s 111111111 %d ============= ", "Test", cnt++);
            }
        }
    }
    cnt = 0;
    Log::Instance()->init(level, "./testlog2", ".log", 5000);
    for (level = 0; level < 4; level++) {
        Log::Instance()->SetLevel(level);
        for (int j = 0; j < 10000; j++) {
            for (int i = 0; i < 4; i++) {
                LOG_BASE(i, "%s 222222222 %d ============= ", "Test", cnt++);
            }
        }
    }
}

void ThreadLogTask(int i, int cnt) {
    for (int j = 0; j < 10000; j++) {
        LOG_BASE(i, "PID:[%04d]======= %05d ========= ", gettid(), cnt++);
    }
}

void TestThreadPool() {
    Log::Instance()->init(0, "./testThreadpool", ".log", 5000);
    ThreadPool threadpool(6);
    for (int i = 0; i < 18; i++) {
        threadpool.AddTask(std::bind(ThreadLogTask, i % 4, i * 10000));
    }
    getchar();
}

int main() {
    //必备数据结构
    MYSQL myconn; //=mysql_init((MYSQL*)0);

    //初始化数据结构
    if (NULL != mysql_init(&myconn)) {
        cout << "mysql_init()succeed" << endl;
    } else {
        cout << "mysql_init()failed" << endl;
        return -1;
    }
    //设置编码方式
    mysql_options(&myconn, MYSQL_SET_CHARSET_NAME, "gbk");

    //初始化数据库
    if (0 == mysql_library_init(0, NULL, NULL)) {
        cout << "mysql_library_init()succeed" << endl;
    } else {
        cout << "mysql_library_init()failed" << endl;
        return -1;
    }
    //连接数据库
    if (NULL != mysql_real_connect(&myconn, "127.0.0.1", "root", "430430", "webdb", 3306, NULL, 0))
    //这里的地址，用户名，密码，数据库，端口可以根据自己本地的情况更改
    {
        cout << "mysql_real_connect()succeed" << endl;
    } else {
        cout << "mysql_real_connect()failed" << endl;
        return -1;
    }
    //操作……

    MYSQL_RES *res; //查询结果集
    MYSQL_ROW row;  //存放一条数据记录，二维数组

    const char *sql = "select * from user";
    mysql_query(&myconn, sql);
    res = mysql_store_result(&myconn);
    while (row = mysql_fetch_row(res)) {
        cout << row[0] << "|" << row[1] << endl;
    }
    mysql_free_result(res);
    mysql_close(&myconn);

    system("pause");
    return 0;
}