#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define SMALL_BUF 100

void* request_handler(void* arg);
void send_data(FILE* fp, char* ct, char* file_name);
char* content_type(char* file);
void send_error(FILE* fp);
void error_handling(char* message);

int main(int argc, char* argv[])
{
    int serv_sock, clnt_sock;          //web就是服务器端，浏览器就是客户端
    struct sockaddr_in serv_adr, clnt_adr;   //定义sockaddr_in结构体 该结构体解决了sockaddr的缺陷，把port和addr 分开储存在两个变量中
    int clnt_adr_size;
    char buf[BUF_SIZE];
    pthread_t t_id;

    if (argc != 2)
    {
        printf("Usage : %s<port>\n", argv[0]);
        exit(1);
    }
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);  //htonl 把本机的字节顺序转换成网络字节顺序  这里存的是IP
    serv_adr.sin_port = htons(atoi(argv[1]));     //int atoi(const char *str) 把参数 str 所指向的字符串转换为一个整数（类型为 int 型）。 
                                                  // htons()将主机的无符号短整形数转换成网络字节顺序

    if (bind(serv_sock, (struct sockaddr*) & serv_adr, sizeof(serv_adr)) == -1)  //bind() 将serv_sock套接字和 serv_adr里面的数据地址绑定
    {
        error_handling("bind() error!");
    }
    if (listen(serv_sock, 20) == -1)    //监听
    {
        error_handling("listen() error");
    }
    while (1)
    {
        clnt_adr_size = sizeof(clnt_adr);
        clnt_sock = accept(serv_sock, (struct sockaddr*) & clnt_adr, (socklen_t*)&clnt_adr_size);  //接受客户端的连线 返回连线的那个客户端的套接字
        printf("Connection Request : %s:%d\n", inet_ntoa(clnt_adr.sin_addr), ntohs(clnt_adr.sin_port));
        pthread_create(&t_id, NULL, request_handler, &clnt_sock);  //创造一个线程去执行request_handler函数 输入进去的参数就是clnt_sock
        pthread_detach(t_id);
    }
    close(serv_sock);
    return 0;
}
void* request_handler(void* arg)
{
    int clnt_sock = *((int*)arg);
    char req_line[SMALL_BUF];
    FILE* clnt_read;
    FILE* clnt_write;

    char method[10];   //方法 get
    char ct[15];    //内容类型
    char file_name[30];

    clnt_read = fdopen(clnt_sock, "r");  //将文件描述符转化为指针
    clnt_write = fdopen(dup(clnt_sock), "w");  //dup() 用来复制clnt_sock所指的描述符
    fgets(req_line, SMALL_BUF, clnt_read);   //从clnt_read这个文件指针上读取一行存到req_line上


    if (strstr(req_line, "HTTP/") == NULL)  //如果 HTTP/ 不是req_line 说明不是HTTP请求 那么就发送错误 
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }

    strcpy(method, strtok(req_line, " /")); //首次调用时，s指向要分解的字符串，之后再次调用要把s设成NULL
    strcpy(file_name, strtok(NULL, " /"));
    strcpy(ct, content_type(file_name));
    if (strcmp(method, "GET") != 0)
    {
        send_error(clnt_write);
        fclose(clnt_read);
        fclose(clnt_write);
        return 0;
    }
    fclose(clnt_read);
    send_data(clnt_write, ct, file_name);  //给客户端发送信息

}
void send_data(FILE* fp, char* ct, char* file_name)
{
    char protocol[] = "HTTP/1.0 200 OK\r\n";
    char server[] = "Server:Linux Web Server\r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[SMALL_BUF];   //缓冲区
    char buf[BUF_SIZE];
    FILE* send_file;

    sprintf(cnt_type, "Content-type:%s\r\n\r\n", ct);  //所以说这个ct应该是内容类型
    send_file = fopen(file_name, "r");
    if (send_file == NULL)
    {
        send_error(fp);
        return;
    }

    //传输头信息
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    //传输请求数据
    while (fgets(buf, BUF_SIZE, send_file) != NULL)  //读出send_file的东西到buf中
    {
        fputs(buf, fp);  //将buf写道 fp中
        fflush(fp);
    }
    fflush(fp);
    fclose(fp);


}

char* content_type(char* file)
{
    char extension[SMALL_BUF];
    char file_name[SMALL_BUF];
    strcpy(file_name, file);  //例如 index.html
    strtok(file_name, ".");
    strcpy(extension, strtok(NULL, "."));
    if (!strcmp(extension, "html") || !strcmp(extension, "htm"))  //检查要求的文件是不是html
    {
        return "text/html";
    }
    else
    {
        return "text/plain";
    }
}

void send_error(FILE* fp)
{
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server\r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[] = "<html><head><title>NETWORK</title></head>"
        "<body><font size =+5><br>发生错误！查看请求文件名和方式！"
        "</font></body></html>";

    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fflush(fp);
}

void error_handling(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}