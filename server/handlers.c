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

#include "handlers.h"
#include "defination.h"
#include "errors.h"
#include "utils.h"

cmd_info_t cmd_table[] = {
    { "USER", &user,   4 },
    { "PASV", &pasv,  -1 },
    { "PORT", &port,   4 },
    { "RETR", &retr,   4 },
    { "STOR", &stor,   4 },
    { "SYST", &syst,  -1 },
    { "TYPE", &type,   4 },
    { "REST", &rest,   4 },
    { "QUIT", &quit,  -1 },
    { "MKD ", &mkd,    3 },
    { "CWD ", &cwd,    3 },
    { "CDUP", &cdup,  -1 },
    { "PWD ", &pwd,   -1 },
    { "RMD ", &rmd,    3 },
    { "LIST", &list,   4 },
    { "RNFR", &renm,   4 },
    { "",     NULL,   -1 }
};

handler_t get_cmd_handler(char req[BUF_SIZE], int * len) {
    char cmd[5];
    memset(cmd, 0, 5);
    if (strlen(req) < 4) {
        req[3] = ' ';
        req[4] = '\0';
    }
    strncpy(cmd, req, 4);
    cmd[4] = '\0';
    handler_t h = NULL;
    cmd_info_t cmd_info = cmd_table[0];
    int i = 0;
    while (strlen(cmd_info.name) != 0) {
        if (strncmp(cmd, cmd_info.name, 4) == 0) {
            h = cmd_info.handler;
            *len = cmd_info.cmd_len;
            break;
        }
        cmd_info = cmd_table[++i];
    }
    return h;
}

int server_ready(socket_info_t * sockif) {
    int fd = sockif->sockfd;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    strcpy(res, "220 Anonymous FTP server ready.\r\n");
    res[strlen(res)] = '\0';
    send(fd, res, strlen(res), 0);
    return 0;
}

int user(socket_info_t * sockif, char param[PARAM_LEN]) {
    int fd = sockif->sockfd;
    char req[BUF_SIZE];
    char res[BUF_SIZE];
    memset(req, 0, BUF_SIZE);
    int err;
    if (param == NULL) {
        recv(fd, req, BUF_SIZE, 0);
        if (strncmp(req, "USER anonymous", 14) != 0) {
            printf("Invalid command: 'User anonymous' expected\n");
            error501(sockif, "Invalid command: 'User anonymous' expected");
            return 1;
        }
    }
    else {
        if (strncmp(param, "anonymous", 9) != 0) {
            printf("Invalid command: 'User anonymous' expected\n");
            error501(sockif, "Invalid command: 'User anonymous' expected");
            return 1;
        }
    }
    memset(res, 0, BUF_SIZE);
    strcpy(res, "331 Guest login ok, send your complete e-mail address as password.\r\n");
    res[strlen(res)] = '\0';
    err = send(fd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error User(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    if (pass(sockif) == 0) {
        sockif->is_login = 1;
        return 0;
    }
    return 1;
}

int pass(socket_info_t * sockif) {
    int fd = sockif->sockfd;
    char req[BUF_SIZE];
    char res[BUF_SIZE];
    int err;
    memset(req, 0, BUF_SIZE);
    recv(fd, req, BUF_SIZE, 0);
    if (strncmp(req, "PASS", 4) != 0) {
        printf("Invalid command: 'PASS' expected\n");
        error503(sockif, "'PASS' expected");
        return 1;
    }
    if (strlen(req) < 5) {
        error500(sockif, NULL);
        return 1;
    }
    memset(res, 0, BUF_SIZE);
    strcpy(res, "230 Guest login ok, access restrictions apply\r\n");
    res[47] = '\0';
    err = send(fd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error pass(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int pasv(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    int old_fd = sockif->datafd;
    if ((sockif->datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error data socket(): %s(%d))\n", strerror(errno), errno);
        error502(sockif, NULL);
        return 1;
    }
    struct sockaddr_in pasv_addr;
    ushort data_port = PASV_PORT;
    memset(&pasv_addr, 0, sizeof(pasv_addr));
    pasv_addr.sin_family = AF_INET;
    pasv_addr.sin_port = htons(data_port);
    pasv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    while ((bind(sockif->datafd, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr))) == -1) {
        ++data_port;
        if (data_port < 1024) {
            printf("Error Bind data port: %s(%d)\n", strerror(errno), errno);
            error502(sockif, NULL);
            return 1;
        }
        pasv_addr.sin_port = htons(data_port);
    }
    int err;
    char res[BUF_SIZE];
    generate_pasv_response(data_port, res);
    err = send(sockif->sockfd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error response to pasv: %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    err = listen(sockif->datafd, 10);
    if (err < 0) {
        printf("Error Listen to datafd: %s(%d)\n", strerror(errno), errno);
        error502(sockif, NULL);
        return 1;
    }
    // 关闭旧的数据连接
    sockif->mod = 0;
    if (old_fd != -1) {
        close(old_fd);
    }
    return 0;
}

int port(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int old_fd = sockif->datafd;
    int err = parse_port_request(&sockif->port_addr, param);
    if (err < 0) {
        printf("Error PORT: invalid param\n");
        error501(sockif, NULL);
        return 1;
    }
    else if (err == 1) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
        error501(sockif, NULL);
        return 1;
    }

    if ((sockif->datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error data socket(): %s(%d))\n", strerror(errno), errno);
        return 1;
    }

    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    strcpy(res, "200 PORT command successful.\r\n");
    res[strlen(res)] = '\0';
    err = send(sockif->sockfd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error response to PORT: %s(%d)\n", strerror(errno), errno);
    }
    sockif->mod = 1;
    // 关闭旧的数据连接
    if (old_fd != -1) {
        close(old_fd);
    }
    return 0;
}

int retr(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int err;
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
        error425(sockif, NULL);
        return 1;
    }
    FILE * file = fopen(param, "rb");
    if (file == NULL) {
        printf("Error can't open file %s\n", param);
        error450(sockif, NULL);
        clear_data_socket(sockif);
        return 1;
    }
    char block[BUF_SIZE];
    memset(block, 0, BUF_SIZE);
    err = fseek(file, sockif->rest, SEEK_SET);
    if (err != 0) {
        printf("Error fseek() fail\n");
        error451(sockif, NULL);
    }
    else {
        while(!feof(file)) {
            int len = fread(block, 1, sizeof(block), file);
            if (len != send(connfd, block, len, 0)) {
                printf("Error occurs when sending file\n");
                error426(sockif, NULL);
            }
        }
    }
    fclose(file);
    close(connfd);
    clear_data_socket(sockif);
    send(sockif->sockfd, "226 Transfer complete.\r\n", 24, 0);
    return 0;

}

int stor(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
        error425(sockif, NULL);
        return 1;
    }

    char filename[PATH_LEN];
    memset(filename, 0, PATH_LEN);
    strncpy(filename, param, strlen(param));
    filename[strlen(param)] = '\0';

    FILE * file;
    if (sockif->rest == 0) {
        file = fopen(filename, "wb");
    }
    else {
        file = fopen(filename, "rb+");
    }
    if (file == NULL) {
        printf("Error can't open file %s\n", filename);
        error450(sockif, NULL);
        clear_data_socket(sockif);
        return 1;
    }
    char block[BUF_SIZE];
    memset(block, 0, BUF_SIZE);
    err = fseek(file, sockif->rest, SEEK_SET);
    if (err != 0) {
        printf("Error fseek() fail\n");
        error451(sockif, NULL);
    }
    else {
        while(1) {
            int len = recv(connfd, block, sizeof(block), 0);
            if (len == 0) {
                break;
            }
            if (len != fwrite(block, 1, len, file)) {
                printf("Error occurs when reciving file\n");
                error426(sockif, NULL);
            }
        }
    }
    fclose(file);
    close(connfd);
    clear_data_socket(sockif);
    send(sockif->sockfd, "226 Transfer complete.\r\n", 24, 0);
    return 0;
}

int syst(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    int err;
    err = send(sockif->sockfd, "215 UNIX Type: L8\r\n", 19, 0);
    if (err < 0) {
        printf("Error TYPE(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int type(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    if (strncmp(param, "I", 1) == 0) {
        err = send(sockif->sockfd, "200 Type set to I.\r\n", 20, 0);
        if (err < 0) {
            printf("Error TYPE(): %s(%d)\n", strerror(errno), errno);
            return 1;
        }
        return 0;
    }
    else if (strncmp(param, "A", 1) == 0) {
        err = send(sockif->sockfd, "200 Type set to A.\r\n", 20, 0);
        if (err < 0) {
            printf("Error TYPE(): %s(%d)\n", strerror(errno), errno);
            return 1;
        }
        return 0;
    }
    else {
        error501(sockif, NULL);
        return 1;
    }
}

int rest(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    sockif->rest = atoi(param);
    int err = send(sockif->sockfd, "350\r\n", 5, 0);
    if (err < 0) {
        printf("Error REST(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int quit(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    int err = send(sockif->sockfd, "221 Goodbye.\r\n", 14, 0);
    if (err < 0) {
        printf("Error Quit(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    sockif->is_login = 0;
    return 0;
}

int mkd (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    err = mkdir(param, 0777);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (err < 0) {
        printf("Error MKD(): %s(%d)\n", strerror(errno), errno);
        char errinfo[ERROR_LEN];
        memset(errinfo, 0, ERROR_LEN);
        sprintf(errinfo, "Fail to create %s", param);
        errinfo[strlen(errinfo)] = '\0';
        error550(sockif, errinfo);
        return 1;
    }
    sprintf(res, "257 \"%s\"\r\n", param);
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int cwd (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    err = chdir(param);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (err < 0) {
        printf("Error CWD(): %s(%d)\n", strerror(errno), errno);
        char errinfo[ERROR_LEN];
        memset(errinfo, 0, ERROR_LEN);
        sprintf(errinfo, "No such directory %s", param);
        error550(sockif, errinfo);
        return 1;
    }
    sprintf(res, "200 Working directory changed\r\n");
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int cdup (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    char path[PARAM_LEN] = "..";
    return cwd(sockif, path);
}

int pwd (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    char path[PATH_LEN];
    getcwd(path, PATH_LEN);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    sprintf(res, "257 \"%s\"\r\n", path);
    int err = send(sockif->sockfd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error PWD(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int rmd (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    err = rmdir(param);
    char res[BUF_SIZE];
    if (err < 0) {
        printf("Error RMD(): %s(%d)\n", strerror(errno), errno);
        char errinfo[ERROR_LEN];
        memset(errinfo, 0, ERROR_LEN);
        sprintf(errinfo, "Fail to remove %s", param);
        error550(sockif, errinfo);
        return 1;
    }
    sprintf(res, "250 %s removed\r\n", param);
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int list (socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
        error425(sockif, NULL);
        return 1;
    }
    if (param == NULL) {
        char cur[PATH_LEN];
        memset(cur, 0, PATH_LEN);
        getcwd(cur, PATH_LEN);
        err = traverse_dir(cur, connfd);
    }
    else {
        struct stat st;
        stat(param, &st);
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            err = traverse_dir(param, connfd);
        }
        else {
            char * buffer;
            char filename[PATH_LEN];
            char * token_ptr;
            for (token_ptr = strtok_r(param, " ", &buffer);
                token_ptr != NULL;
                token_ptr = strtok_r(NULL, " ", &buffer)) {
                memset(filename, 0, PATH_LEN);
                strncpy(filename, token_ptr, strlen(token_ptr));
                filename[strlen(filename)] = '\0';
                char info[BUF_SIZE];
                err = generate_file_info(filename, info);
                if (err != 0) {
                    break;
                }
                if (send(connfd, info, strlen(info), 0) < 0) {
                    err = 426;
                    break;
                }
            }
        }
    }
    close(connfd);
    clear_data_socket(sockif);
    if (err == 0) {
        send(sockif->sockfd, "226 Transfer complete.\r\n", 24, 0);
    }
    else if (err == 426) {
        error426(sockif, NULL);
    }
    else {
        error450(sockif, NULL);
    }
    return 0;
}

int renm(socket_info_t * sockif, char param[PARAM_LEN]) {
    if (check_login(sockif) != 0) {
        return 1;
    }
    if (param == NULL) {
        error500(sockif, NULL);
        return 1;
    }
    int err;
    char from[PATH_LEN];
    memset(from, 0, PATH_LEN);
    strncpy(from, param, strlen(param));
    err = send(sockif->sockfd, "350 wait for RNTO\r\n", 19, 0);
    if (err < 0) {
        printf("Error RNFR(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    char req[BUF_SIZE];
    memset(req, 0, BUF_SIZE);
    recv(sockif->sockfd, req, BUF_SIZE, 0);
    trim_req(req, strlen(req));
    if (strncmp(req, "RNTO", 4) == 0) {
        int len = strlen(req) - 5;
        if (len <= 0) {
            error500(sockif, NULL);
            return 1;
        }
        char to[PATH_LEN];
        memset(to, 0, PATH_LEN);
        strncpy(to, req + 5, len);
        err = rename(from, to);
        if (err < 0) {
            printf("Error RNFR(): %s(%d)\n", strerror(errno), errno);
            error553(sockif, NULL);
            return 1;
        }
        send(sockif->sockfd, "250\r\n", 5, 0);
        return 0;
    }
    else {
        error503(sockif, NULL);
        return 1;
    }
}
