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
#include<pthread.h>
#include<bits/waitflags.h>
#include <stdint.h>


#define DATABASE "Users.db"
#define NAME_MAX 512
#define USER_MAX 1000
#define WORD_MAX 1024
#define ANS_MAX 5096

struct User
{
    //char name[NAME_MAX]; //后续扩展用，暂时没用到
    int fd;
};

struct User users[USER_MAX];    //用户信息
int now_user_num=0;    //现在的用户数量
int server_fd;     //服务器fd
struct sockaddr_in server_addr; //服务器属性
int port;       //端口

//ctrl+c退出函数 
void sig_handler(int signo)
{
    if (signo == SIGINT) 
    {
        printf("Server closing...\n");
        close(server_fd);
        for (int i = 0; i < now_user_num; i++) 
        {
            if (users[i].fd != 0) 
                close(users[i].fd);
            users[i].fd=0;
        }
        exit(0);
    }
}

//登录
bool LogIn(char *name,char *passwd);
//注册
bool SignUp(char *name,char *passwd);
//查询
void Query(char *que,char *ans);
//退出客户端
void Quit(int fd);
//主控函数
void MainController();
//线程处理
void *ThreadOp(void *arg);


int main(int argc,char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        perror("signal");
        exit(1);
    }
    port=atoi(argv[1]);

    MainController();

    return 0;
}

bool LogIn(char *name, char *passwd) 
{
    // 打印收到的用户名和密码（关键调试信息）
    printf("尝试登录 - 收到的用户名: [%s] (长度: %zu), 密码: [%s] (长度: %zu)\n",
           name, strlen(name), passwd, strlen(passwd));

    sqlite3 *sq;
    int rc;
    sqlite3_stmt *stmt;

    // 打开数据库
    if (sqlite3_open(DATABASE, &sq) != SQLITE_OK) 
    {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(sq));
        return false;
    }

    // 准备SQL语句（使用参数绑定）
    const char *sql = "SELECT name, passwd FROM us WHERE name = ? AND passwd = ?";
    rc = sqlite3_prepare_v2(sq, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "SQL准备失败: %s\n", sqlite3_errmsg(sq));
        sqlite3_close(sq);
        return false;
    }

    // 绑定参数并打印调试信息
    printf("绑定参数 - 用户名: [%s], 密码: [%s]\n", name, passwd);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwd, -1, SQLITE_TRANSIENT);

    // 执行查询
    bool found = false;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) 
    {
        // 从结果中获取实际存储的值并打印
        const char *dbName = (const char *)sqlite3_column_text(stmt, 0);
        const char *dbPass = (const char *)sqlite3_column_text(stmt, 1);
        printf("数据库中找到匹配记录 - 用户名: [%s], 密码: [%s]\n", dbName, dbPass);
        
        // 额外的字符串比较验证
        if (strcmp(dbName, name) == 0 && strcmp(dbPass, passwd) == 0) 
        {
            found = true;
            printf("登录验证成功\n");
        } 
        else 
        {
            printf("字符串比较失败 - 数据库值与输入值不匹配\n");
        }
    } 
    else if (rc == SQLITE_DONE) 
    {
        printf("数据库中未找到匹配记录\n");
    } 
    else 
    {
        fprintf(stderr, "查询执行错误: %s\n", sqlite3_errmsg(sq));
    }

    // 清理资源
    sqlite3_finalize(stmt);
    sqlite3_close(sq);

    return found;
}

bool SignUp(char *name, char *passwd)
{
    // 1. 检查用户是否已存在
    bool found = false;
    sqlite3 *sq;
    int rc;
    sqlite3_stmt *stmt;

    if (sqlite3_open(DATABASE, &sq) != SQLITE_OK) 
    {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(sq));
        return false;
    }

    const char *sql_check = "SELECT name FROM us WHERE name = ?";
    rc = sqlite3_prepare_v2(sq, sql_check, -1, &stmt, 0);
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "SQL准备失败: %s\n", sqlite3_errmsg(sq));
        sqlite3_close(sq);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) 
    {
        const char *dbName = (const char *)sqlite3_column_text(stmt, 0);
        if (strcmp(dbName, name) == 0) 
        {
            found = true;
        }
    } 
    else if (rc != SQLITE_DONE) 
    {
        fprintf(stderr, "查询执行错误: %s\n", sqlite3_errmsg(sq));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(sq);

    if (found) 
    {
        return false; // 用户名已存在，注册失败
    }

    // 2. 插入新用户（修复核心逻辑）
    sqlite3 *sq_in;
    int rc_in;
    sqlite3_stmt *stmt_in;

    if (sqlite3_open(DATABASE, &sq_in) != SQLITE_OK) 
    {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(sq_in));
        return false;
    }

    const char *sql_insert = "INSERT INTO us (name, passwd) VALUES (?, ?)";
    rc_in = sqlite3_prepare_v2(sq_in, sql_insert, -1, &stmt_in, NULL);
    if (rc_in != SQLITE_OK) 
    {
        fprintf(stderr, "SQL插入准备失败: %s\n", sqlite3_errmsg(sq_in));
        sqlite3_close(sq_in);
        return false;
    }

    sqlite3_bind_text(stmt_in, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt_in, 2, passwd, -1, SQLITE_TRANSIENT);

    rc_in = sqlite3_step(stmt_in); // 仅执行一次插入
    if (rc_in != SQLITE_DONE) 
    {    // 插入成功返回 SQLITE_DONE
        fprintf(stderr, "插入失败: %s\n", sqlite3_errmsg(sq_in));
        sqlite3_finalize(stmt_in);
        sqlite3_close(sq_in);
        return false;
    }

    sqlite3_finalize(stmt_in);
    sqlite3_close(sq_in);
    return true; // 注册成功
}

void Query(char *que,char *ans)
{
    char to_search[ANS_MAX]="python3 get_chinese.py ";
    strcat(to_search,que);
    FILE* pipe = popen(to_search, "r");
    if (!pipe) 
    {
        return; // 执行失败返回NULL
    }

    // 缓冲区和结果字符串
    char buffer[ANS_MAX];
    char* result = NULL;
    size_t result_size = 0;

    // 循环读取命令输出
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) 
    {
        size_t buffer_len = strlen(buffer);
        // 重新分配内存（预留终止符位置）
        char* temp = realloc(result, result_size + buffer_len + 1);
        if (!temp) 
        {
            free(result);
            pclose(pipe);
            return;
        }
        result = temp;
        // 拼接新读取的内容
        strcpy(result + result_size, buffer);
        result_size += buffer_len;
    }

    // 关闭管道
    int exit_status = pclose(pipe);
    
    // 检查命令是否执行成功
    if (exit_status == -1) 
    {
        free(result);
        return;
    }

    // 如果有结果，确保以'\0'结尾
    if (result) 
    {
        result[result_size] = '\0';
    }
    strcpy(ans,result);
}

void Quit(int fd)
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    if(getpeername(fd,(struct sockaddr*)&addr,&len)<0)
    {
        perror("getpeername");
        return;
    }
    char ip[16];
    memset(ip,0,sizeof ip);
    int port=ntohs(addr.sin_port);
    inet_ntop(AF_INET,&addr.sin_addr.s_addr,ip,sizeof(ip));
    printf("%s(%d) Closed.\n",ip,port);
    close(fd);
}

void MainController()
{
    if((server_fd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket");
        exit(1);
    }
    int opt=1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0)
    {
        perror("setsocketopt");
        exit(1);
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("bind");
        exit(1);
    }
    if(listen(server_fd,USER_MAX)<0)
    {
        perror("listen");
        exit(1);
    }
    printf("server listening on port %d\n",port);

    //设置线程分离属性
    pthread_attr_t attr;
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

    while(1)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len=sizeof(client_addr);
        int client_fd=accept(server_fd,(struct sockaddr*)&client_addr,&addr_len);
        if(client_fd<0)
        {
            perror("accept");
            continue;
        }
        if(now_user_num>=USER_MAX)
        {
            char err_info[WORD_MAX]="ERROR:user limit...";
            send(client_fd,err_info,sizeof(err_info),0);
            continue;
        }
        printf("New connection from %s:%d (fd=%d)\n",
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
        users[now_user_num].fd=client_fd;
        now_user_num++;
        char welcome_info[1024]="Welcome translation system";
        send(client_fd,welcome_info,sizeof(welcome_info),0);
        pthread_t th;
        int err;
        if ((err = pthread_create(&th, &attr, ThreadOp, (void*)(intptr_t)client_fd)) < 0)
        {
            perror("pthread_create");
            exit(1);
        }
    }
    pthread_attr_destroy(&attr);

    close(server_fd);
}

void *ThreadOp(void *arg)
{
    bool Signed=false;    //没有登录不能查单词
    int now_fd = (int)(intptr_t)arg;  // 安全转换，无警告
    char choice[WORD_MAX];
    while(1)
    {
        memset(choice, 0, sizeof(choice));
        usleep(100);
        ssize_t recv_len = recv(now_fd, choice, sizeof(choice), 0); // 留1字节存'\0'
        if (recv_len < 0) 
        {
            perror("recv choice失败");
            send(now_fd, "ERROR:接收请求失败", 20, 0);
            close(now_fd);
            return NULL;
        } 
        else if (recv_len == 0) 
        {
            printf("客户端主动断开连接\n");
            close(now_fd);
            return NULL;
        }
        usleep(10);
        if(strcmp(choice,"Log")==0)
        {
            char name[NAME_MAX] = {0};
            char password[NAME_MAX] = {0};
            // 接收用户名
            size_t recv_len = recv(now_fd, name, sizeof(name) - 1, 0);
            if (recv_len <= 0) 
            {
                perror("接收用户名失败");
                close(now_fd);
                return (void*)0;
            }
            name[recv_len] = '\0'; // 确保字符串结尾
            // 接收密码
            recv_len = recv(now_fd, password, sizeof(password) - 1, 0);
            if (recv_len <= 0) 
            {
                perror("接收密码失败\n");
                close(now_fd);
                return (void*)0;
            }
            password[recv_len] = '\0'; // 确保字符串结尾
            
            // 调用登录函数
            if(LogIn(name, password)) 
            {
                send(now_fd, "Log success", 11, 0);
                Signed=true;
            }
            else 
            {
                send(now_fd, "Log fail", 8, 0);
            }
        }
        else if(strcmp(choice,"Sign")==0)
        {
            char name[NAME_MAX] = {0};
            char password[NAME_MAX] = {0};
            // 接收用户名
            size_t recv_len = recv(now_fd, name, sizeof(name) - 1, 0);
            if (recv_len <= 0) 
            {
                perror("接收用户名失败\n");
                close(now_fd);
                return (void*)0;
            }
            name[recv_len] = '\0'; // 确保字符串结尾
            // 接收密码
            recv_len = recv(now_fd, password, sizeof(password) - 1, 0);
            if (recv_len <= 0) 
            {
                perror("接收密码失败\n");
                close(now_fd);
                return (void*)0;
            }
            password[recv_len] = '\0'; // 确保字符串结尾

            // 调用注册函数
            if(SignUp(name, password)) 
            {
                send(now_fd, "Sign success", 12, 0);
            }
            else 
            {
                send(now_fd, "Sign fail", 9, 0);
            }
        }
        else if(strcmp(choice,"Query")==0)
        {
            char que[WORD_MAX];
            char ans[ANS_MAX];
            memset(que,0,sizeof que);
            memset(ans,0,sizeof ans);
            size_t recv_len = recv(now_fd, que, sizeof(que) - 1, 0);
            if (recv_len <= 0) 
            {
                perror("接收单词失败");
                close(now_fd);
                return (void*)0;
            }
            que[recv_len] = '\0'; // 确保字符串结尾
            if(!Signed)   //没有登录不能查单词
            {
                char err_info[20]="请先登录\n";
                send(now_fd,err_info,sizeof(err_info),0);
                continue;
            }
            Query(que,ans);
            send(now_fd,ans,sizeof(ans),0);//把答案发回去
        }
        else if(strcmp(choice,"Quit")==0)
        {
            Quit(now_fd);
            return (void*)0;//退出就退出线程
        }
    }
    
}