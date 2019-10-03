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

#include "errors.h"

void error425(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "425 Can't open data connection\r\n", 32, 0);
}

void error426(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "426 Connection closed\r\n", 23, 0);
}

void error450(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "450 Requested file action not taken\r\n", 37, 0);
}

void error451(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "451 Requested action aborted.\r\n", 31, 0);
}

void error500(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "500 Syntax error, command unrecognized\r\n", 40, 0);
}

void error501(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (errinfo != NULL) {
        sprintf(res, "501 %s\r\n", errinfo);
    }
    else {
        sprintf(res, "501 Syntax error in parameters or arguments\r\n");
    }
    res[strlen(res)] = '\0';
    send(sockif->sockfd, res, strlen(res), 0);
}

void error502(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "502 Command not implemented\r\n", 19, 0);
}

void error503(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (errinfo != NULL) {
        sprintf(res, "503 %s\r\n", errinfo);
    }
    else {
        sprintf(res, "503 Bad sequence of commands\r\n");
    }
    res[strlen(res)] = '\0';
    send(sockif->sockfd, res, strlen(res), 0);
}

void error530(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "530 Not logged in\r\n", 19, 0);
}

void error550(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    if (errinfo == NULL) {
        send(sockif->sockfd, "550 Requested action not taken\r\n", 32, 0);
    }
    else {
        char res[BUF_SIZE];
        memset(res, 0, BUF_SIZE);
        sprintf(res, "550 %s\r\n", errinfo);
        res[strlen(res)] = '\0';
        send(sockif->sockfd, res, strlen(res), 0);
    }
}

void error553(socket_info_t * sockif, char errinfo[ERROR_LEN]) {
    send(sockif->sockfd, "553 Requested action not taken\r\n", 32, 0);
}
