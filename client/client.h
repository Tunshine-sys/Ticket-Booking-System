#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>
#include <signal.h>

using namespace std;

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

class TcpClient
{
public:
    TcpClient()
    {
        ips = "127.0.0.1";
        port = 6000;
        runing = true;
        login_status = false;
        op_type = -1;
    }
    TcpClient(string ser_ip, short ser_port) : ips(ser_ip), port(ser_port)
    {
        runing = true;
        login_status = false;
        op_type = -1;
    }
    ~TcpClient()
    {
        close(sockfd);
    }
    bool Socket_Init();
    void Run();

private:
    void Print_info();
    void User_Register();
    void User_Login();
    void Show_Ticket();
    void Yd_Ticket();   // 预定
    void WDYY_Ticket(); // 我的预约
    void QXYY_Ticket(); // 取消预定

private:
    string ips;
    short port;
    int sockfd;
    bool runing;
    bool login_status;
    string username;
    string usertel;

    int op_type;
};
