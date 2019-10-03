#ifndef UTILS_H
#define UTILS_H

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

#include "defination.h"

// 解析命令行参数
void parseArgs(int argc, char** argv);

// 生成新的连接信息
socket_info_t * new_socket_info(int sfd);

// 检查是否登录
int check_login(socket_info_t * sockif);

// 生成PASV命令的response
void generate_pasv_response( ushort port, char res[BUF_SIZE]);

// 解析PORT命令参数
int parse_port_request(struct sockaddr_in * addr, char param[PARAM_LEN]);

// 建立数据连接
int build_data_connect(socket_info_t * sockif, int * connfd);

// 清理数据连接相关信息
void clear_data_socket(socket_info_t * sockif);

// 删除命令末尾CRLF
void trim_req(char str[BUF_SIZE], int len);

// 遍历目录并通过数据连接发送
int traverse_dir(char dirname[PATH_LEN], int connfd);

// 生成文件详细信息
int generate_file_info(char filename[PATH_LEN], char info[BUF_SIZE]);

#endif