#include "client.h"

// TcpClient初始化
bool TcpClient::Socket_Init()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cout << "create sockfd err" << endl;
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());

    int res = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (-1 == res)
    {
        cout << "connect ser err" << endl;
        return false;
    }

    return true;
}

void TcpClient::Print_info()
{
    if (!login_status)
    {
        cout << "------------用户名：游客------ 状态: 未登录--------" << endl;
        cout << " 1 登陆  2  注册   3 退出" << endl;
        cout << "------------------------------------------------" << endl;
        cout << "请输入要执行的操作:" << endl;
        cin >> op_type;
        if (op_type == 3)
        {
            op_type = TC;
        }
    }
    else
    {
        cout << "------------用户名：" << username << " ------ 状态: 已登录--------" << endl;
        cout << " 1 查看可预约的信息    2  预定" << endl;
        cout << " 3 查看我的预约       4  取消预约" << endl;
        cout << " 5 退出" << endl;
        cout << "请输入要执行的操作:" << endl;
        cin >> op_type;
        if (op_type == 5)
        {
            op_type = TC;
        }
        else
        {
            op_type += 2;
        }
    }
}

// 用户注册
void TcpClient::User_Register()
{
    cout << "请输入手机号码" << endl;
    cin >> usertel;
    cout << "请输入用户名" << endl;
    cin >> username;
    string passwd1, passwd2;
    cout << "请输入密码" << endl;
    cin >> passwd1;
    cout << "请再次输入密码" << endl;
    cin >> passwd2;
    if (passwd1 != passwd2)
    {
        cout << "密码不一致" << endl;
        return;
    }

    // JSON包
    Json::Value val;
    val["type"] = ZC;
    val["user_tel"] = usertel;
    val["user_name"] = username;
    val["user_passwd"] = passwd1;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    char buff[128]{0};
    int res = recv(sockfd, buff, 127, 0);
    if (res <= 0)
    {
        cout << "ser close" << endl;
        cout << "注册失败" << endl;
        return;
    }

    // 解析JSON包
    Json::Value ra;
    Json::Reader Read;
    if (!Read.parse(buff, ra))
    {
        cout << "解析失败" << endl;
        return;
    }
    string s = ra["status"].asString();
    if (s != "OK")
    {
        cout << "注册失败" << endl;
        return;
    }

    cout << "注册成功" << endl;
    login_status = true;
    return;
}

// 用户登陆
void TcpClient::User_Login()
{
    cout << "请输入手机号码" << endl;
    cin >> usertel;
    string passwd;
    cout << "请输入密码" << endl;
    cin >> passwd;

    // JSON包
    Json::Value val;
    val["type"] = DL;
    val["user_tel"] = usertel;
    val["user_passwd"] = passwd;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    char buff[256]{0};
    int n = recv(sockfd, buff, 255, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    // 解析JSON包
    Json::Value ra;
    Json::Reader Read;
    if (!Read.parse(buff, ra))
    {
        cout << "解析失败" << endl;
        return;
    }
    string s = ra["status"].asString();
    if (s != "OK")
    {
        cout << "登陆失败" << endl;
        return;
    }
    username = ra["user_name"].asString();
    login_status = true; // 代表已经登陆
    cout << "登陆成功" << endl;
    return;
}

// 查看预约信息
void TcpClient::Show_Ticket()
{
    Json::Value val;
    val["type"] = CKYY;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);
    char buff[8192]{0};
    int n = recv(sockfd, buff, 1023, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    // 解析JSON包
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "解析失败" << endl;
        return;
    }
    string s = res_val["status"].asString();
    if (s != "OK")
    {
        cout << "查看预约失败" << endl;
    }
    int num = res_val["num"].asInt();
    if (num == 0)
    {
        cout << "暂时没有可以预约的信息" << endl;
        return;
    }

    // 打印
    cout << "|序号 |  地点名称  |  总票数  |  已预定  |  日期  |" << endl;
    for (int i = 0; i < num; i++)
    {
        cout << " " << res_val["ticket_arr"][i]["ticket_id"].asString();
        cout << "   " << res_val["ticket_arr"][i]["ticket_name"].asString();
        cout << "   " << res_val["ticket_arr"][i]["ticket_max"].asString();
        cout << "   " << res_val["ticket_arr"][i]["ticket_count"].asString();
        cout << "   " << res_val["ticket_arr"][i]["ticket_date"].asString();
        cout << endl;
    }
}

// 预定票
void TcpClient::Yd_Ticket()
{
    Show_Ticket();
    cout << "请输入要预定的序号" << endl;
    int index = -1;
    cin >> index;

    Json::Value val;
    val["type"] = YD;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "json 无法解析" << endl;
        return;
    }

    string s = res_val["status"].asString();
    if (s != "OK")
    {
        cout << "预定失败" << endl;
        return;
    }

    cout << "预订成功" << endl;
    return;
}

// 我的预约
void TcpClient::WDYY_Ticket()
{
    Json::Value val;
    val["type"] = WDYY;
    val["user_tel"] = usertel;
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[8192] = {0};
    if (recv(sockfd, buff, 1023, 0) <= 0)
    {
        cout << "ser close" << endl;
        return;
    }
    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "JSON解析失败" << endl;
        return;
    }

    string s = res_val["status"].asString();
    if (s != "OK")
    {
        cout << "查看我的预约失败" << endl;
        return;
    }

    int num = res_val["num"].asInt();
    if (num == 0)
    {
        cout << "暂时没有我的可预约的信息" << endl;
        return;
    }

    // 打印
    cout << "|    预约ID    |   票ID    |   票名  |   日期  |     预约时间     |" << endl;
    for (int i = 0; i < num; i++)
    {
        cout << "        " << res_val["yd_arr"][i]["yd_id"].asString();
        cout << "        " << res_val["yd_arr"][i]["ticket_id"].asString();
        cout << "          " << res_val["yd_arr"][i]["ticket_name"].asString();
        cout << "           " << res_val["yd_arr"][i]["ticket_date"].asString();
        cout << "        " << res_val["yd_arr"][i]["ctime"].asString();
        cout << endl;
    }
}

// 取消预约
void TcpClient::QXYY_Ticket()
{
    Show_Ticket();
    cout << "请输入要取消预约的序号" << endl;
    int index = -1;
    cin >> index;

    Json::Value val;
    val["type"] = QXYY;
    val["user_tel"] = usertel;
    val["ticket_id"] = to_string(index);
    send(sockfd, val.toStyledString().c_str(), strlen(val.toStyledString().c_str()), 0);

    char buff[128] = {0};
    int n = recv(sockfd, buff, 127, 0);
    if (n <= 0)
    {
        cout << "ser close" << endl;
        return;
    }

    Json::Value res_val;
    Json::Reader Read;
    if (!Read.parse(buff, res_val))
    {
        cout << "json 无法解析" << endl;
        return;
    }

    string s = res_val["status"].asString();
    if (s != "OK")
    {
        cout << "取消预约失败" << endl;
        return;
    }

    cout << "取消预约成功" << endl;
    return;
}

// 用户端运行
void TcpClient::Run()
{
    while (runing)
    {
        Print_info(); // 打印菜单
        switch (op_type)
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
        case TC:
            runing = false;
            break;
        default:
            cout << "无效操作" << endl;
            break;
        }
    }
}

int main()
{
    TcpClient cli;
    if (!cli.Socket_Init())
    {
        exit(1);
    }

    cli.Run();
    exit(0);
}