#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<sys/socket.h>
#include<sqlite3.h>
#include<stdbool.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<sys/uio.h>
#include<execinfo.h>
#include<string.h>

#define NAME_MAX 512
#define USER_MAX 1000
#define WORD_MAX 1024
#define ANS_MAX 5096

int client_fd;
struct sockaddr_in server_addr;
int port;
char ip[16];


//ctrl+c退出函数 
void sig_handler(int signo)
{
    if (signo == SIGINT) 
    {
        printf("Server closing...\n");
        close(client_fd);
        exit(0);
    }
}

//清空缓冲区
void ClearInputBuffer();;
//菜单
void Meun();
//登录
bool Log();
//注册
bool Sign();
//查询
void Query();
//退出
void Quit();
//主控
void MainController();

int main(int argc,char *argv[])
{
    if(argc<3)
    {
        printf("Usage:%s ip port",argv[0]);
        exit(1);
    }
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    port=atoi(argv[2]);
    memset(ip,0,sizeof ip);
    strcpy(ip,argv[1]);
    MainController();

    return 0;
}

void Meun()
{
    printf("--------------------------------------------\n");
    printf("------------------1.登录---------------------\n");
    printf("------------------2.注册---------------------\n");
    printf("------------------3.翻译---------------------\n");
    printf("------------------4.退出---------------------\n");
    printf("--------------------------------------------\n");
    printf("请选择：");
}

void MainController()
{
    if((client_fd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &serveraddr.sin_addr) <= 0) {
        perror("Invalid address");
        return;
    }
    
    if (connect(client_fd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("connect");
        return;
    }
    
    char buffer[1024];
    memset(buffer,0,sizeof buffer);
    recv(client_fd,buffer,sizeof(buffer),0);

    if(strcmp(buffer,"ERROR:user limit...")==0)
    {
        printf("%s",buffer);
        exit(1);
    }
    printf("%s\n",buffer);

    int choice;
    while(1)
    {
        Meun();
        usleep(100);
        scanf("%d",&choice);
        switch(choice)
        {
            case 1:
                send(client_fd,"Log",3,0);
                usleep(100);
                if(Log())
                {
                    printf("登录成功\n");
                }
                else
                {
                    printf("登录失败，请检查帐号和密码是否错误\n");
                }
                break;
            case 2:
                send(client_fd,"Sign",4,0);
                usleep(100);
                if(Sign())
                {
                    printf("注册成功，你用此帐号可以登录\n");
                }
                else
                {
                    printf("出现了未知错误，联系管理员\n");
                }
                break;
            case 3:
                send(client_fd,"Query",5,0);
                usleep(100);
                Query();
                break;
            case 4:
                send(client_fd,"Quit",4,0);
                usleep(100);
                Quit();
                return;
        }
    }
}

bool Log()
{
    char name[USER_MAX],passwd[USER_MAX];
    memset(name,0,sizeof name);
    memset(passwd,0,sizeof passwd);
    printf("请输入您的帐号：");
    ClearInputBuffer();
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';//把fgets获取字符串的\n去掉
    send(client_fd, name, strlen(name), 0);
    
    printf("请输入您的密码：");
    fgets(passwd, sizeof(passwd), stdin);
    passwd[strcspn(passwd, "\n")] = '\0';//把fgets获取字符串的\n去掉
    send(client_fd, passwd, strlen(passwd), 0);

    char islog[USER_MAX];
    memset(islog,0,sizeof islog);
    recv(client_fd,islog,sizeof(islog),0);
    if(strcmp(islog,"Log success")==0)
        return true;
    else
        return false;
}

bool Sign()
{
    char name[USER_MAX],passwd[USER_MAX];
    memset(name,0,sizeof name);
    memset(passwd,0,sizeof passwd);
    printf("请输入您要注册帐号的名字：");
    ClearInputBuffer();
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = '\0';//把fgets获取字符串的\n去掉
    send(client_fd, name, strlen(name), 0);
    
    printf("请输入您的注册帐号的密码：");
    fgets(passwd, sizeof(passwd), stdin);
    passwd[strcspn(passwd, "\n")] = '\0';//把fgets获取字符串的\n去掉
    send(client_fd, passwd, strlen(passwd), 0);

    char islog[USER_MAX];
    memset(islog,0,sizeof islog);
    recv(client_fd,islog,sizeof(islog),0);
    if(strcmp(islog,"Sign success")==0)
        return true;
    else
        return false;
}

void Query()
{
    char que[WORD_MAX];
    memset(que,0,sizeof que);
    printf("请输入你要翻译的单词：");
    ClearInputBuffer();
    fgets(que, sizeof(que), stdin);
    send(client_fd,que,sizeof(que),0);
    char ans[ANS_MAX];
    memset(ans,0,sizeof ans);
    sleep(1);
    if(recv(client_fd,ans,sizeof(ans),0)<0)
    {
        perror("recv");
        return;
    }
    printf("%s",ans);
}

void Quit()
{
    close(client_fd);
    exit(0);
}

void ClearInputBuffer()
{
    int c;
    // 检查缓冲区是否有残留数据（通过getchar()的返回值判断）
    // 如果缓冲区为空，getchar()会阻塞，所以需要配合feof避免无意义等待
    while (1) 
    {
        c = getchar();
        // 若读到换行符或文件结束符，说明缓冲区已清空
        if (c == '\n' || c == EOF) 
        {
            break;
        }
        // 其他字符直接丢弃（不做处理）
    }
}