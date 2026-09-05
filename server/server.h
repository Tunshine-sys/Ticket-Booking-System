#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/epoll.h>
#include <fstream>
#include <sstream>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <random>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <mysql/mysql.h>

#define EV_MAX 128

enum OPTYPE
{
    DL = 1,
    ZC,
    CKYY,
    YD,
    WDYY,
    QXYY,
    TC = 0
};

using namespace std;

// 配置文件的信息
class SerConf
{
public:
    SerConf()
    {
        ips = "127.0.0.1";
        port = 6000;
        lismax = 128;
        taskmax = 512;
        threadnum = 4;
        db_ip = "127.0.0.1";
        db_port = 3306;
        db_username = "root";
        db_passwd = "123456";
        db_name = "project2";
        db_poolsize = 10;
    }

    bool ReadConf(const string filename);
    void PrintInfo();
    string GetIps() const { return ips; }
    short GetPort() const { return port; }
    int GetLisMax() const { return lismax; }
    int GetTaskMax() const { return taskmax; }
    int GetThreadNum() const { return threadnum; }

    string GetDbIp() const { return db_ip; }
    int GetDbPort() const { return db_port; }
    string GetDbUser() const { return db_username; }
    string GetDbPasswd() const { return db_passwd; }
    string GetDbName() const { return db_name; }
    int GetDbPoolSize() const { return db_poolsize; }

private:
    string ips;
    short port;
    int lismax;
    int taskmax;
    int threadnum;

    string db_ip;
    int db_port;
    string db_username;
    string db_passwd;
    string db_name;
    int db_poolsize;
};

// socket基类
class Socket
{
public:
    Socket(int fd, int epfd) : m_fd(fd), m_epfd(epfd)
    {
    }
    virtual ~Socket()
    {
        close(m_fd);
    }
    virtual void Handle_data() = 0;
    int m_fd; // socket套接字描述符
    int m_epfd;
};

// 监听套接字
class LisSocket : public Socket
{
public:
    LisSocket(int fd, int epfd) : Socket(fd, epfd)
    {
    }
    void Handle_data() override; // accept
private:
    void RestEvent();
    unsigned int m_Count;
};

// 连接套接字
class ConSocket : public Socket
{
public:
    ConSocket(int fd, int epfd) : Socket(fd, epfd)
    {
    }

    void Handle_data() override; // recv
private:
    void RestEvent(); // 重置或重新注册该套接字的 epoll 监听事件
    void DeleteFromEpoll();

    void Get_OpType(char buff[]);
    void Send_Ok();
    void Send_Err();
    void Send_JsonObj(Json::Value &root);

    void User_Register();
    void User_Login();

    void Show_Ticket();
    void Yd_Ticket();
    void WDYY_Ticket(); // 我的预约
    void QXYY_Ticket(); // 取消预定

private:
    int m_OpType;
    Json::Value m_val;
};

// 数据库连接池
class MysqlPool
{
public:
    static MysqlPool *GetInstance(); // 获取单例对象
    bool Init(string ip, string username, string passwd, string dbName, int port, int poolSize);
    MYSQL *GetConnection();
    void ReleaseConnection(MYSQL *con);
    ~MysqlPool();

private:
    MysqlPool() {}
    queue<MYSQL *> m_connQ;    // 存储连接的队列
    mutex m_mtx;               // 互斥锁，保护线程安全
    condition_variable m_cond; // 条件变量
};

// MySQL
/*class MysqlClient
{
public:
    MysqlClient()
    {
        db_ip = "127.0.0.1";
        db_port = 3306;
        db_name = "project2";
        db_username = "root";
        db_passwd = "123456";
    }
    MysqlClient(string ip, short port, string dbname, string dbusername, string dbpasswd)
    {
        db_ip = ip;
        db_port = port;
        db_name = dbname;
        db_username = dbusername;
        db_passwd = dbpasswd;
    }
    ~MysqlClient()
    {
        mysql_close(&mysql_con);
    }

    bool Connect_MysqlServer();
    bool Db_User_Register(const string &tel, const string &name, const string passwd);
    bool Db_User_Login(const string &tel, string &name, const string &passwd);
    bool Db_Show_ticket(Json::Value &res);

private:
    string db_ip;
    short db_port;
    string db_name;
    string db_username;
    string db_passwd;

    MYSQL mysql_con; // 连接句柄
    // 数据加密
    string Generate_Salt(int length = 16);
    string SHA256_Hash(const string &str);
};*/
class MysqlClient
{
public:
    // 默认构造函数：直接从连接池获取连接
    MysqlClient()
    {
        m_con = MysqlPool::GetInstance()->GetConnection();
    }

    // 析构函数：将连接归还给连接池
    ~MysqlClient()
    {
        if (m_con != nullptr)
        {
            MysqlPool::GetInstance()->ReleaseConnection(m_con);
        }
    }

    // 检查连接是否有效（防止连接池超时返回空指针）
    bool IsValid() { return m_con != nullptr; }

    // 原有的业务函数保持不变
    bool Db_User_Register(const string &tel, const string &name, const string passwd);
    bool Db_User_Login(const string &tel, string &name, const string &passwd);
    bool Db_Show_ticket(Json::Value &res);
    bool Db_Yd_Ticket(const string &tel, const string &tk_id);
    bool Db_WDYY_Ticket(const string &tel, Json::Value &res);    // 我的预约
    bool Db_QXYY_Ticket(const string &tel, const string &tk_id); // 取消预定

private:
    void Mysql_Begin();    // 开启事物
    void Mysql_RollBack(); // 回滚事物
    void Mysql_Commit();   // 提交事物
private:
    // 唯一的成员变量：从连接池借出来的数据库连接指针
    MYSQL *m_con;

    // 数据加密相关的工具函数
    string Generate_Salt(int length = 16);
    string SHA256_Hash(const string &str);
};

// 任务队列
typedef struct
{
    Socket *con;
} task_t;

// 线程池
class ThreadPool
{
public:
    ThreadPool(int num, int task_num) : thread_num(num), max_queue(task_num)
    {
    }
    bool Thread_Pool_Init();
    void Add_Task(Socket *csocket); // 向任务队列添加任务
    void Start_Thread();            // 启动线程池
    void Work();                    // 工作线程

private:
    task_t *queue; // 任务队列
    int front;     // 队头
    int rear;      // 队尾
    int count;     // 当前任务数量

    int stop; // 线程是否退出

    int max_queue = 1024; // 任务队列大小
    int thread_num = 4;   // 线程数目

    pthread_mutex_t mutex; // 互斥锁
    pthread_cond_t cond;   // 通过条件变量，唤醒线程
};

// TCP服务
class TcpSer
{
public:
    TcpSer(SerConf &sc) : m_conf(sc), m_pool(sc.GetThreadNum(), sc.GetTaskMax())
    {
        m_sockfd = -1; // socket文件描述符
        m_epfd = -1;   // epoll文件描述符
    }
    bool Ser_init(); // 服务器初始化
    void Run();      // 运行

private:
    bool create_socket(); // 创建套接字
    void do_events();     // 创建内核事件

private:
    SerConf m_conf; // 读取配置文件的信息
    int m_sockfd;   // 监听套接字
    int m_epfd;
    struct epoll_event m_evs[EV_MAX];
    int m_num; // 就绪事件的个数

    ThreadPool m_pool;
};