#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "definition.h"
#include "utils.h"
#include "errors.h"
#include "handlers.h"

// 监听端口
ushort listen_port;
// 根目录
char root[PATH_LEN];
// 服务器ip
char ip_addr[16] = "127.0.0.1";

// 处理每一个进程
void process_func(socket_info_t * sockif);

// 退出进程前的清理
void exit_process(socket_info_t * sockif);

int main(int argc, char **argv) {
    int listenfd;		//监听socket和连接socket不一样，后者用于数据传输
    struct sockaddr_in addr;

    listen_port = 21;
    strcpy(root, "/tmp");
    parseArgs(argc, argv);
    printf("port: %hu\n", listen_port);
    printf("root: %s\n", root);
    if (chdir(root) != 0) {
        printf("Error chdir(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    //创建socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    //设置本机的ip和port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);	//监听"0.0.0.0"

    //将本机的ip和port与socket绑定
    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Error bind(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    //开始监听socket
    if (listen(listenfd, 10) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    //持续监听连接请求
    while (1) {
        //等待client的连接 -- 阻塞函数
        int connfd;
        struct sockaddr_in conn_addr;
        socklen_t conn_len = sizeof(conn_addr);
        memset(&conn_addr, 0, conn_len);
        if ((connfd = accept(listenfd, (struct sockaddr *) &conn_addr, &conn_len)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            continue;
        }
        socket_info_t * sockif = new_socket_info(connfd);
        if (fork() == 0) {
            process_func(sockif);
        }
    }
    close(listenfd);
}

void process_func(socket_info_t * sockif) {
    printf("New process\n");
    socket_info_t * socket_info = (socket_info_t *) sockif;
    int err;
    server_ready(socket_info);
    while (1) {
        char req[BUF_SIZE];
        memset(req, 0, BUF_SIZE);
        err = recv(socket_info->sockfd, req, BUF_SIZE, 0);
        if (err < 0) {
            printf("Error listening: %s(%d)\n", strerror(errno), errno);
            exit_process(socket_info);
        }
        if (err == 0) {
            quit(sockif, NULL);
            break;
        }
        trim_req(req, strlen(req));
        printf("Request: %s\n", req);
        int cmd_len = 0;
        // 获得处理当前命令的函数
        handler_t hdl = get_cmd_handler(req, &cmd_len);
        if (hdl == NULL) {
            error500(sockif, NULL);
            continue;
        }
        if (cmd_len > 0) {
            // 提取命令参数
            char param[PARAM_LEN];
            memset(param, 0, PARAM_LEN);
            int len = strlen(req) - cmd_len - 1;
            if (len <= 0) {
                hdl(socket_info, NULL);
            }
            else {
                len = len < PARAM_LEN ? len : PARAM_LEN;
                strncpy(param, req + cmd_len + 1, len);
                param[strlen(param)] = '\0';
                hdl(socket_info, param);
            }
        }
        else {
            if (strlen(req) > 4) {
                error500(socket_info, NULL);
            }
            else {
                hdl(socket_info, NULL);
            }
        }
    }
    free(sockif);
    exit_process(socket_info);
}

void exit_process(socket_info_t * sockif) {
    close(sockif->sockfd);
    if (sockif->datafd >= 0) {
        close(sockif->datafd);
    }
    exit(0);
}
