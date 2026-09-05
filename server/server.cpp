#include "server.h"

// 读取配置文件
bool SerConf::ReadConf(const string filename)
{
    ifstream ifs(filename);
    if (!ifs.is_open())
    {
        cout << "open file:" << filename << "err" << endl;
        return false;
    }

    string line;
    int num = 1;

    while (getline(ifs, line)) // 使用getline逐行读取
    {
        if (line.empty() || line.front() == '#')
        {
            num++;
            continue;
        }

        istringstream iss(line);
        string k, v;

        if (!(iss >> k >> v))
        {
            cout << "配置文件第：" << num++ << "无法解析" << endl;
            continue;
        }

        if (k == "ip")
        {
            ips = v;
        }
        else if (k == "port")
        {
            port = stoi(v);
        }
        else if (k == "lismax")
        {
            lismax = stoi(v);
        }
        else if (k == "taskmax")
        {
            taskmax = stoi(v);
        }
        else if (k == "threadnum")
        {
            threadnum = stoi(v);
        }
        else if (k == "db_ip")
        {
            db_ip = v;
        }
        else if (k == "db_port")
        {
            db_port = stoi(v);
        }
        else if (k == "db_username")
        {
            db_username = v;
        }
        else if (k == "db_passwd")
        {
            db_passwd = v;
        }
        else if (k == "db_name")
        {
            db_name = v;
        }
        else if (k == "db_poolsize")
        {
            db_poolsize = stoi(v);
        }
        else
        {
            cout << "第：" << num << "无效:" << k << endl;
        }
        num++;
    }
    ifs.close();
    return true;
}

// 打印配置文件信息
void SerConf::PrintInfo()
{
    cout << "------serconf------" << endl;
    cout << "ip: " << ips << endl;
    cout << "port: " << port << endl;
    cout << "lismax: " << lismax << endl;
    cout << "taskmax: " << taskmax << endl;
    cout << "threadnum:" << threadnum << endl;
    cout << "--------------------" << endl;
}

// 重置事件监听套接字
void LisSocket::RestEvent()
{
    struct epoll_event ev;
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLONESHOT;
    if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, m_fd, &ev) == -1)
    {
        cout << "restEvent err" << endl;
    }
}

// lis sockfd accept
void LisSocket::Handle_data()
{
    int c = accept(m_fd, NULL, NULL);
    if (c < 0) // 成功会返回连接套接字的描述符
    {
        return;
    }

    RestEvent(); // 重置事件

    cout << "accept" << c << endl;
    ConSocket *cs = new ConSocket(c, m_epfd);
    if (cs == nullptr)
    {
        close(c);
        return;
    }
    struct epoll_event ev;
    ev.data.ptr = cs;
    ev.events = EPOLLIN | EPOLLONESHOT;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, c, &ev) == -1) // 往事件表新增文件描述符 fd，同时登记想要监听的事件
    {
        cout << "epoll ctl add c err" << endl;
        return;
    }
}

// 重置事件连接套接字
void ConSocket::RestEvent()
{
    struct epoll_event ev;
    ev.data.ptr = this;
    ev.events = EPOLLIN | EPOLLONESHOT;
    if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, m_fd, &ev) == -1)
    {
        cout << "restEvent err" << endl;
    }
}

// 删除事件
void ConSocket::DeleteFromEpoll()
{
    if (epoll_ctl(m_epfd, EPOLL_CTL_DEL, m_fd, NULL) == -1)
    {
        cout << "epoll delete err" << endl;
    }
}

// 获取用户操作类型
void ConSocket::Get_OpType(char buff[])
{
    Json::Reader Read;
    m_val.clear();

    if (!Read.parse(buff, m_val))
    {
        cout << "json解析失败" << endl;
        m_OpType = -1;
        return;
    }
    m_OpType = m_val["type"].asInt(); // 转换为整数
    return;
}

// 发送成功
void ConSocket::Send_Ok()
{
    Json::Value val;
    val["status"] = "OK";
    send(m_fd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
}

// 发送失败
void ConSocket::Send_Err()
{
    Json::Value val;
    val["status"] = "ERR";
    send(m_fd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
}

// 发送JSON
void ConSocket::Send_JsonObj(Json::Value &root)
{ // 把 Json::Value 对象序列化为 JSON 字符串，通过 socket 文件描述符 `m_fd` 发送出去。
    std::string jsonStr = root.toStyledString();
    send(m_fd, jsonStr.c_str(), strlen(jsonStr.c_str()), 0);
}

// 用户注册功能
void ConSocket::User_Register()
{
    string usertel = m_val["user_tel"].asString();
    string username = m_val["user_name"].asString();
    string userpasswd = m_val["user_passwd"].asString();
    MysqlClient cli; // 自动从连接池拿一个连接
    // if (!cli.Connect_MysqlServer()) // 数据库连接失败
    if (!cli.IsValid()) // 数据库连接失败
    {
        Send_Err();
        return;
    }
    if (!cli.Db_User_Register(usertel, username, userpasswd))
    {
        Send_Err();
        return;
    }
    Send_Ok();
    return;
}

// 用户登陆功能
void ConSocket::User_Login()
{
    string usertel = m_val["user_tel"].asString();
    string userpasswd = m_val["user_passwd"].asString();
    string username;

    MysqlClient cli;
    if (!cli.IsValid()) // 数据库连接失败
    {
        Send_Err();
        return;
    }

    if (!cli.Db_User_Login(usertel, username, userpasswd))
    {
        Send_Err();
        return;
    }
    Json::Value res;
    res["status"] = "OK";
    res["user_name"] = username;
    Send_JsonObj(res);
    return;
}

// 查看票的信息
void ConSocket::Show_Ticket()
{
    Json::Value res;
    MysqlClient cli;
    if (!cli.IsValid())
    {
        Send_Err();
        return;
    }
    if (!cli.Db_Show_ticket(res))
    {
        Send_Err();
        return;
    }
    Send_JsonObj(res);
}

// 预定票
void ConSocket::Yd_Ticket()
{
    string usertel = m_val["user_tel"].asString();
    string tk_id = m_val["ticket_id"].asString();
    MysqlClient cli;
    if (!cli.IsValid())
    {
        Send_Err();
        return;
    }
    if (!cli.Db_Yd_Ticket(usertel, tk_id))
    {
        Send_Err();
        return;
    }
    Send_Ok();
}

// 我的预约
void ConSocket::WDYY_Ticket()
{
    Json::Value res;
    string usertel = m_val["user_tel"].asString();
    MysqlClient cli;
    if (!cli.IsValid())
    {
        Send_Err();
        return;
    }

    if (!cli.Db_WDYY_Ticket(usertel, res))
    {
        Send_Err();
        return;
    }

    Send_JsonObj(res); // 发送一个json对象给客户端
}

// 取消预约
void ConSocket::QXYY_Ticket()
{
    string usertel = m_val["user_tel"].asString();
    string tk_id = m_val["ticket_id"].asString();
    MysqlClient cli;
    if (!cli.IsValid())
    {
        Send_Err();
        return;
    }

    if (!cli.Db_QXYY_Ticket(usertel, tk_id))
    {
        Send_Err();
        return;
    }
    Send_Ok();

    return;
}

void ConSocket::Handle_data()
{
    char buff[1024]{0};
    int n = recv(m_fd, buff, 1023, 0);
    if (n <= 0)
    {
        DeleteFromEpoll();
        delete this;
        cout << "client close" << endl;
        return;
    }
    Get_OpType(buff);
    switch (m_OpType)
    {
    case DL:
        User_Login();
        break;
    case ZC:
        User_Register();
        break;
    case CKYY:
        Show_Ticket();
        break;
    case YD:
        Yd_Ticket();
        break;
    case WDYY:
        WDYY_Ticket();
        break;
    case QXYY:
        QXYY_Ticket();
        break;
    default:
        break;
    }
    RestEvent();
    return;
    // cout << "buff:" << buff << endl;
    // send(m_fd, "ok", 2, 0);
}

// 数据库连接池
MysqlPool *MysqlPool::GetInstance()
{
    static MysqlPool pool;
    return &pool;
}
bool MysqlPool::Init(string ip, string username, string passwd, string dbName, int port, int poolSize)
{
    for (int i = 0; i < poolSize; i++)
    {
        MYSQL *con = mysql_init(nullptr);
        if (con == nullptr)
        {
            cout << "MYSQL init err" << endl;
            return false;
        }
        con = mysql_real_connect(con, ip.c_str(), username.c_str(), passwd.c_str(), dbName.c_str(), port, nullptr, 0);
        if (con == nullptr)
        {
            cout << "mysql connect err" << endl;
            return false;
        }
        m_connQ.push(con);
    }
    return true;
}
MYSQL *MysqlPool::GetConnection()
{
    unique_lock<mutex> lock(m_mtx);
    // 队列中有连接时会跳过等待，没有连接说明队列为空返回false，继续等待
    bool success = m_cond.wait_for(lock, std::chrono::seconds(3), [this]()
                                   { return !m_connQ.empty(); });
    if (!success)
    {
        cout << "获取数据库连接超时！" << endl;
        return nullptr; // 超时没拿到，返回空指针
    }
    MYSQL *con = m_connQ.front();
    m_connQ.pop();
    return con;
}
void MysqlPool::ReleaseConnection(MYSQL *con)
{
    if (con != nullptr)
    {
        unique_lock<mutex> lock(m_mtx);
        m_connQ.push(con);
        m_cond.notify_one(); // 唤醒一个等待连接的现存
    }
}
MysqlPool::~MysqlPool()
{
    unique_lock<mutex> lock(m_mtx);
    while (!m_connQ.empty()) // 队列不空
    {
        MYSQL *con = m_connQ.front();
        m_connQ.pop();
        mysql_close(con);
    }
}

// MySQL数据库
#if 0
bool MysqlClient::Connect_MysqlServer()
{
    // 初始化连接句柄
    if (mysql_init(&mysql_con) == NULL)
    {
        return false;
    }
    // 连接MySQL服务器
    if (mysql_real_connect(&mysql_con, db_ip.c_str(), db_username.c_str(), db_passwd.c_str(), db_name.c_str(), db_port, NULL, 0) == NULL)
    {
        cout << "connect mysql err" << endl;
        return false;
    }
    return true;
}
#endif
// MySQL用户注册
bool MysqlClient::Db_User_Register(const string &tel, const string &name, const string passwd)
{
    string salt = Generate_Salt(); // 生成随机盐值
    string hashed_passwd = SHA256_Hash(passwd + salt);
    // string sql = string("insert into user_info values(0,'") + tel + string("','") + name + string("','") + passwd + string("',1,curdate())");
    string sql = string("insert into user_info (tel, name, passwd, status, ztime, salt) values('") + tel + string("','") + name + string("','") + hashed_passwd + string("',1,curdate(),'") + salt + string("')");
    // if (mysql_query(&mysql_con, sql.c_str()) != 0)
    if (mysql_query(m_con, sql.c_str()) != 0)
    {
        return false;
    }
    return true;
}

// MySQL用户登录
bool MysqlClient::Db_User_Login(const string &tel, string &name, const string &passwd)
{
    // select name from user_info where tel=13577888899 and passwd=123456;
    // string sql = string("select name from user_info where tel=") + tel + string("and passwd=") + passwd;
    string sql = string("select name, passwd, salt from user_info where tel='") + tel + string("'");
    // if (mysql_query(&mysql_con, sql.c_str()) != 0)
    if (mysql_query(m_con, sql.c_str()) != 0)
    {
        return false;
    }
    // MYSQL_RES *r = mysql_store_result(&mysql_con);
    MYSQL_RES *r = mysql_store_result(m_con);
    if (r == nullptr)
    {
        return false;
    }
    int num = mysql_num_rows(r); // 获取结果集中有几条记录
    if (num != 1)
    {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r); // 读取这一行的内容
    name = row[0];                      // 获取数据库中的用户名
    string db_hashed_passwd = row[1];   // 提取数据库里存的【密文】
    string db_salt = row[2];            // 提取数据库里存的【盐值】
    mysql_free_result(r);               // 释放结果集中占用的内存

    string current_hash = SHA256_Hash(passwd + db_salt);

    if (current_hash != db_hashed_passwd) // 比对我们刚刚算出的密文，和数据库里存的密文是否一模一样
    {
        return false;
    }
    return true;
}

// MySQL查看票
bool MysqlClient::Db_Show_ticket(Json::Value &res)
{
    string sql = string("select*from ticket_table");
    // if (mysql_query(&mysql_con, sql.c_str()) != 0)
    if (mysql_query(m_con, sql.c_str()) != 0)
    {
        return false;
    }
    MYSQL_RES *r = mysql_store_result(m_con);
    if (r == nullptr)
    {
        return false;
    }
    int num = mysql_num_rows(r); // 获取结果集中有几条记录
    res["status"] = "OK";
    res["num"] = num;
    if (num == 0)
    {
        mysql_free_result(r);
        return false;
    }

    for (int i = 0; i < num; i++)
    {
        Json::Value tmp;
        MYSQL_ROW row = mysql_fetch_row(r);
        tmp["ticket_id"] = row[0];
        tmp["ticket_name"] = row[1];
        tmp["ticket_max"] = row[2];
        tmp["ticket_count"] = row[3];
        tmp["ticket_date"] = row[4];
        res["ticket_arr"].append(tmp);
    }
    mysql_free_result(r); // 释放结果集占用的内存
    return true;
}

// MYSQL预定票
bool MysqlClient::Db_Yd_Ticket(const string &tel, const string &tk_id)
{
    // create table yd_table(yd_id int primary key not null unique auto_increment,tel char(11) not null,tk_id int not null,ctime datetime,status tinyint);
    //  select tk_max,count from ticket_table where tk_id=2;
    //  update ticket_table set count=1 where tk_id=2;
    //  insert into yd_table values(0,'13700000001',2,now(),1);

    string sql_max_count = string("select tk_max,count from ticket_table where tk_id=") + tk_id;
    if (mysql_query(m_con, sql_max_count.c_str()) != 0)
    {
        return false;
    }
    MYSQL_RES *r = mysql_store_result(m_con); // 获取查询结果max，count
    if (r == nullptr)
    {
        return false;
    }
    int num = mysql_num_rows(r); // 获取结果集中有几条记录
    if (num == 0)
    {
        mysql_free_result(r);
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r);
    int max = atoi(row[0]);
    int count = atoi(row[1]);

    mysql_free_result(r); // 释放结果集占用的内存

    if (count >= max)
    {
        cout << "票不足" << endl;
        return false;
    }

    count++;

    Mysql_Begin();

    string sql_setcount = string("update ticket_table set count=") + to_string(count) + string(" where tk_id=") + tk_id;
    if (mysql_query(m_con, sql_setcount.c_str()) != 0)
    {
        Mysql_RollBack();
        return false;
    }

    string sql_yd = string("insert into yd_table values(0,'") + tel + string("',") + tk_id + string(",now(),1)");
    if (mysql_query(m_con, sql_yd.c_str()) != 0)
    {
        Mysql_RollBack();
        return false;
    }

    Mysql_Commit();
    return true;
}

// MYSQL我的预约
bool MysqlClient::Db_WDYY_Ticket(const string &tel, Json::Value &res)
{
    // ticket_table 列名：tk_id / tk_name / tk_max / count / dtime / status
    string sql = string("select y.yd_id, y.tk_id, y.ctime, t.tk_name, t.dtime ") + string("from yd_table y, ticket_table t ") + string("where y.tel='") + tel + string("' and y.status=1 and y.tk_id=t.tk_id and t.status=1");
    if (mysql_query(m_con, sql.c_str()) != 0)
    {
        return false;
    }

    MYSQL_RES *r = mysql_store_result(m_con);
    if (r == NULL)
    {
        return false;
    }

    int num = mysql_num_rows(r); // 获取查询到多少条记录
    res["status"] = "OK";
    res["num"] = num;

    if (num == 0)
    {
        mysql_free_result(r);
        return true;
    }

    for (int i = 0; i < num; i++)
    {
        Json::Value tmp;
        MYSQL_ROW row = mysql_fetch_row(r);
        tmp["yd_id"] = row[0]; // 预约记录 id
        tmp["ticket_id"] = row[1];
        tmp["ctime"] = row[2]; // 预约时间
        tmp["ticket_name"] = row[3];
        tmp["ticket_date"] = row[4]; // 对应 dtime 列
        res["yd_arr"].append(tmp);   // 每查出一行就做一个tmp，追加进yd_arr数组；
    }

    mysql_free_result(r);
    return true;
}

// 取消预约
bool MysqlClient::Db_QXYY_Ticket(const string &tel, const string &tk_id)
{
    string sql_find = string("select count(*) from yd_table where tel='") + tel + string("' and tk_id=") + tk_id;
    if (mysql_query(m_con, sql_find.c_str()) != 0)
    {
        return false;
    }
    MYSQL_RES *r = mysql_store_result(m_con);
    if (r == NULL)
    {
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(r);
    int yd_num = atoi(row[0]);
    mysql_free_result(r);
    if (yd_num == 0) // 没有预约记录，不允许取消预约
    {
        return false;
    }

    // 2.事物：删除预约记录+票数减去对应数量
    Mysql_Begin();
    string sql_del = string("delete from yd_table where tel='") + tel + string("' and tk_id=") + tk_id + string(" and status=1");
    if (mysql_query(m_con, sql_del.c_str()) != 0) // 删除预约记录
    {
        Mysql_RollBack();
        return false;
    }

    string sql_sub = string("update ticket_table set count=count-") + to_string(yd_num) + string(" where tk_id=") + tk_id;
    if (mysql_query(m_con, sql_sub.c_str()) != 0) // 票数减去对应数量
    {
        Mysql_RollBack();
        return false;
    }

    Mysql_Commit();
    return true;
}
// 开启事物
void MysqlClient::Mysql_Begin()
{
    if (mysql_query(m_con, "begin") != 0)
    {
        cout << "开启事物失败" << endl;
    }
}
// 回滚事物
void MysqlClient::Mysql_RollBack()
{
    if (mysql_query(m_con, "rollback") != 0)
    {
        cout << "回滚事物失败" << endl;
    }
}
// 提交事物
void MysqlClient::Mysql_Commit()
{
    if (mysql_query(m_con, "commit") != 0)
    {
        cout << "提交事物失败" << endl;
    }
}
// mysql加密
string MysqlClient::Generate_Salt(int length)
{
    // 生成随机盐值
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string salt;
    std::random_device rd; // 向操作系统要一个“随机种子”
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    for (int i = 0; i < length; ++i)
    {
        salt += charset[dis(gen)];
    }
    return salt;
}
string MysqlClient::SHA256_Hash(const string &str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for (auto c : hash)
    {
        ss << hex << setw(2) << setfill('0') << (int)c;
    }
    return ss.str();
}

// 线程池
bool ThreadPool::Thread_Pool_Init()
{
    // queue = (task_t *)malloc(sizeof(task_t) * max_queue);
    queue = new task_t[max_queue]; // 创建一个任务队列
    if (queue == nullptr)
    {
        return false;
    }

    front = 0;
    rear = 0;
    count = 0;

    pthread_mutex_init(&mutex, NULL); // 互斥锁
    pthread_cond_init(&cond, NULL);   // 条件变量

    stop = 0; // 表示不退出,1表示停止

    return true;
}

// 向任务队列添加任务
void ThreadPool::Add_Task(Socket *csocket)
{
    pthread_mutex_lock(&mutex); // 上锁
    if (count >= max_queue)     // 已超出任务队列上限
    {
        delete csocket;
        pthread_mutex_unlock(&mutex); // 释放锁
        return;
    }
    queue[rear].con = csocket; // 添加任务
    rear = (rear + 1) % max_queue;
    count++;                    // 更新当前任务数量
    pthread_cond_signal(&cond); // 唤醒条件变量
    pthread_mutex_unlock(&mutex);
}

// 全局的线程函数
void *worker_thread(void *arg)
{
    ThreadPool *pool = (ThreadPool *)arg;
    pool->Work();
    return NULL;
}

// 启动线程池
void ThreadPool::Start_Thread()
{
    pthread_t ids[thread_num];
    for (auto &id : ids)
    {
        pthread_create(&id, NULL, worker_thread, this);
    }
}

// 工作线程
void ThreadPool::Work()
{

    while (true)
    {
        pthread_mutex_lock(&mutex); // 保护条件变量cond
        while (count == 0 && !stop)
        {
            pthread_cond_wait(&cond, &mutex);
        }

        if (stop)
        {
            break;
        }

        Socket *con = queue[front].con; // 从队列中取出任务
        front = (front + 1) % max_queue;
        count--; // 任务数量减1

        pthread_mutex_unlock(&mutex);
        if (con != nullptr)
        {
            con->Handle_data();
        }
    }
}

// tcpser
bool TcpSer::create_socket()
{
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == m_sockfd)
    {
        return false;
    }
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(m_conf.GetPort());
    saddr.sin_addr.s_addr = inet_addr(m_conf.GetIps().c_str());

    int res = bind(m_sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (-1 == res)
    {
        cout << "bind err!" << endl;
        return false;
    }

    res = listen(m_sockfd, m_conf.GetLisMax());
    if (-1 == res)
    {
        return false;
    }
    return true;
}

bool TcpSer::Ser_init()
{
    if (!create_socket()) // 没有创建好套接字
    {
        return false;
    }

    m_epfd = epoll_create1(0);
    // 失败：返回-1，并设置errno错误原因,flag 只支持 0 或者 EPOLL_CLOEXEC
    if (-1 == m_epfd)
    {
        return false;
    }

    // 创建处理m_sockfd的类的对象
    LisSocket *sock = new LisSocket(m_sockfd, m_epfd);
    if (sock == nullptr)
    {
        cout << "create LisSocket err" << endl;
        return false;
    }

    struct epoll_event ev;
    ev.data.ptr = sock;
    ev.events = EPOLLIN | EPOLLONESHOT; // 只触发一次

    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_sockfd, &ev) == -1)
    {
        cout << "epoll add sockfd err" << endl;
        return false;
    }
    // 创建线程池
    m_pool.Thread_Pool_Init();
    m_pool.Start_Thread();

    return true;
}

void TcpSer::do_events()
{
    for (int i = 0; i < m_num; i++)
    {
        if (m_evs[i].events & EPOLLIN)
        {
            Socket *s = (Socket *)m_evs[i].data.ptr;
            if (s != nullptr)
            {
                // s->Handle_data();
                m_pool.Add_Task(s);
            }
        }
    }
}

void TcpSer::Run()
{
    while (true)
    {
        m_num = epoll_wait(m_epfd, m_evs, EV_MAX, 5000);
        if (m_num == -1)
        {
            cout << "epoll wait err" << endl;
        }
        else if (m_num == 0)
        {
            cout << "time out" << endl;
        }
        else
        {
            do_events();
        }
    }
}

int main()
{
    // 读取配置文件
    SerConf sconf;
    if (!sconf.ReadConf("my.cnf"))
    {
        cout << "读取配置文件失败！" << endl;
        return 1;
    }
    sconf.PrintInfo();
    bool pool_init = MysqlPool::GetInstance()->Init(
        sconf.GetDbIp(),
        sconf.GetDbUser(),
        sconf.GetDbPasswd(),
        sconf.GetDbName(),
        sconf.GetDbPort(),
        sconf.GetDbPoolSize());

    if (!pool_init)
    {
        cout << "数据库连接池初始化失败！请检查 my.cnf 配置。" << endl;
        return 1;
    }
    TcpSer ser(sconf);
    if (!ser.Ser_init())
    {
        return 1;
    }

    ser.Run();
    return 0;
}