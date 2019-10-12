#ifndef DEFINATION_H
#define DEFINATION_H

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

#define BUF_SIZE 8192
#define PATH_LEN 1024
#define PARAM_LEN 1024
#define ERROR_LEN 64
#define PASV_PORT 23333

typedef unsigned short int ushort;
typedef unsigned int uint;

// ���������Ϣ
typedef struct {
    int sockfd;
    int datafd;
    off_t rest;
    short mod;                 // DEFAULT: -1, PASV: 0, PORT: 1
    ushort is_login;
    struct sockaddr_in port_addr;
} socket_info_t;

// ����ָͬ��ĺ���ָ������
typedef int (*handler_t) (socket_info_t *, char [PARAM_LEN]);

// ָ����������ָ�룬��ָ���
typedef struct {
    char name[5];
    handler_t handler;
    int cmd_len;
} cmd_info_t;

#endif
