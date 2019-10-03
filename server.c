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
#define PASS_LEN 64
#define PARAM_LEN 1024
#define PASV_PORT 20000

ushort listen_port;
char root[PATH_LEN];
char ip_addr[] = "127.0.0.1";

typedef unsigned short int ushort;
typedef unsigned int uint;

typedef struct {
    int sockfd;
    int datafd;
    char dir[PATH_LEN];
    off_t rest;
    char password[PASS_LEN];
    short mod;                 // DEFAULT: -1, PASV: 0, PORT: 1
    ushort is_login;
    struct sockaddr_in port_addr;  // !!! TODO !!! RETR, STOR, LIST后清除
} socket_info_t;

typedef int (*handler_t) (socket_info_t *, char [PARAM_LEN]);

typedef struct {
    char name[5];
    handler_t handler;
    int cmd_len;
} cmd_info_t;

void parseArgs(int argc, char** argv);

socket_info_t * new_socket_info(int sfd);

void process_func(socket_info_t * sockif);

void exit_process(socket_info_t * sockif);

handler_t get_cmd_handler(char cmd[5], int * len);

int server_ready(socket_info_t * sockif);
int login (socket_info_t * sockif);
int user (socket_info_t * sockif);
int pass (socket_info_t * sockif);
int pasv (socket_info_t * sockif, char param[PARAM_LEN]);
int port (socket_info_t * sockif, char param[PARAM_LEN]);
int retr (socket_info_t * sockif, char param[PARAM_LEN]);
int stor (socket_info_t * sockif, char param[PARAM_LEN]);
int syst (socket_info_t * sockif, char param[PARAM_LEN]);
int type (socket_info_t * sockif, char param[PARAM_LEN]);
int rest (socket_info_t * sockif, char param[PARAM_LEN]);
int quit (socket_info_t * sockif, char param[PARAM_LEN]);
int mkd  (socket_info_t * sockif, char param[PARAM_LEN]);
int cwd  (socket_info_t * sockif, char param[PARAM_LEN]);
int cdup (socket_info_t * sockif, char param[PARAM_LEN]);
int pwd  (socket_info_t * sockif, char param[PARAM_LEN]);
int rmd  (socket_info_t * sockif, char param[PARAM_LEN]);
int list (socket_info_t * sockif, char param[PARAM_LEN]);
int renm (socket_info_t * sockif, char param[PARAM_LEN]);

cmd_info_t cmd_table[] = {
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

void generate_pasv_response( ushort port, char res[BUF_SIZE]);

int parse_port_request(
    struct sockaddr_in * addr,
    char param[PARAM_LEN]
);

int build_data_connect(socket_info_t * sockif, int * connfd);

void clear_data_socket(socket_info_t * sockif);

void trim_req(char str[BUF_SIZE], int len);

int traverse_dir(char dirname[PATH_LEN], int connfd);

int generate_file_info(char filename[PATH_LEN], char info[BUF_SIZE]);

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

        // pthread_t conn_thread;
        socket_info_t * sockif = new_socket_info(connfd);
        // !!! TODO !!! Multi Client
        // pthread_create(&conn_thread, NULL, thread_func, (void *)sockif);
        if (fork() == 0) {
            process_func(sockif);
        }

    /*
        //榨干socket传来的内容
        p = 0;
        while (1) {
            int n = read(connfd, sentence + p, 8191 - p);
            if (n < 0) {
                printf("Error read(): %s(%d)\n", strerror(errno), errno);
                close(connfd);
                continue;
            } else if (n == 0) {
                break;
            } else {
                p += n;
                if (sentence[p - 1] == '\n') {
                    break;
                }
            }
        }
        //socket接收到的字符串并不会添加'\0'
        sentence[p - 1] = '\0';
        len = p - 1;

        //字符串处理
        for (p = 0; p < len; p++) {
            sentence[p] = toupper(sentence[p]);
        }

        //发送字符串到socket
         p = 0;
        while (p < len) {
            int n = write(connfd, sentence + p, len + 1 - p);
            if (n < 0) {
                printf("Error write(): %s(%d)\n", strerror(errno), errno);
                return 1;
             } else {
                p += n;
            }
        }

        close(connfd);
    */
    }

    close(listenfd);
}

void parseArgs(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-port") == 0) {
            ++i;
            if (i == argc) {
                break;
            }
            if (strcmp(argv[i], "-root") == 0) {
                continue;
            }
            listen_port = (ushort) atoi(argv[i]);
            if (listen_port < 1024) {
                listen_port = 21;
            }
        }
        else if (strcmp(argv[i], "-root") == 0) {
            ++i;
            if (i == argc) {
                break;
            }
            if (strcmp(argv[i], "-port") == 0) {
                continue;
            }
            // !!! TODO !!! absolute path OR relative path
            strncpy(root, argv[i], PATH_LEN);
        }
    }
}

socket_info_t * new_socket_info(int sfd) {
    socket_info_t * info;
    info = malloc(sizeof(* info));
    info->sockfd = sfd;
    info->datafd = -1;
    strcpy(info->dir, root);
    info->dir[strlen(info->dir)] = '\0';
    info->rest = 0;
    memset(info->password, 0, PASS_LEN);
    info->mod = -1;
    info->is_login = 0;
    return info;
}

void process_func(socket_info_t * sockif) {
    printf("Thread func()\n");
    socket_info_t * socket_info = (socket_info_t *) sockif;
    int err;
    login(socket_info);
    while (socket_info->is_login) {
        char req[BUF_SIZE];
        memset(req, 0, BUF_SIZE);
        err = recv(socket_info->sockfd, req, BUF_SIZE, 0);
        if (err < 0) {
            printf("Error listening: %s(%d)\n", strerror(errno), errno);
            exit_process(socket_info);
        }
        if (err == 0) {
            quit(sockif, NULL);
        }

        trim_req(req, strlen(req));
        int cmd_len = 0;
        handler_t hdl = get_cmd_handler(req, &cmd_len);
        if (hdl == NULL) {
            continue;
        }
        if (cmd_len > 0) {
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
            hdl(socket_info, NULL);
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
    printf("Ready()\n");
    int fd = sockif->sockfd;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    strcpy(res, "220 Anonymous FTP server ready.\r\n");
    res[strlen(res)] = '\0';
    int err;
    err = send(fd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error data socket(): %s(%d))\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int login(socket_info_t * sockif) {
    if (server_ready(sockif) == 0 && user(sockif) == 0 && pass(sockif) == 0) {
        sockif->is_login = 1;
        return 0;
    }
    return 1;
}

int user(socket_info_t * sockif) {
    printf("User()\n");
    int fd = sockif->sockfd;
    char req[BUF_SIZE];
    char res[BUF_SIZE];
    memset(req, 0, BUF_SIZE);
    int err;
    err = recv(fd, req, BUF_SIZE, 0);
    if (err < 0) {
        printf("Error pass(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    if (strncmp(req, "USER anonymous", 14) != 0) {
        printf("Invalid command: 'User anonymous' expected\n");
        return 1;
    }
    memset(res, 0, BUF_SIZE);
    strcpy(res, "331 Guest login ok, send your complete e-mail address as password.\r\n");
    res[strlen(res)] = '\0';
    err = send(fd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error pass(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int pass(socket_info_t * sockif) {
    int fd = sockif->sockfd;
    printf("Pass()\n");
    char req[BUF_SIZE];
    char res[BUF_SIZE];
    char password[PASS_LEN];
    int err;
    memset(req, 0, BUF_SIZE);
    err = recv(fd, req, BUF_SIZE, 0);
    if (err < 0) {
        printf("Error pass(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    if (strncmp(req, "PASS", 4) != 0) {
        printf("Invalid command: 'User anonymous' expected\n");
        return 1;
    }
    memset(password, 0, PASS_LEN);
    strcpy(password, req + 5);
    password[strlen(password)] = '\0';
    strcpy(sockif->password, password);
    sockif->password[strlen(sockif->password)] = '\0';
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
    printf("Pasv()\n");
    int old_fd = sockif->datafd;
    if ((sockif->datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error data socket(): %s(%d))\n", strerror(errno), errno);
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
            return 1;
        }
        pasv_addr.sin_port = htons(data_port);
    }
    printf("data port: %hu", data_port);
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
        return 1;
    }
    sockif->mod = 0;
    if (old_fd != -1) {
        close(old_fd);
    }
    return 0;
}

int port(socket_info_t * sockif, char param[PARAM_LEN]) {
    printf("Port()\n");
    int old_fd = sockif->datafd;
    int err = parse_port_request(&sockif->port_addr, param);
    if (err < 0) {
        printf("Error PORT: invalid param\n");
        return 1;
    }
    else if (err == 1) {
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
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
        exit_process(sockif);
    }
    sockif->mod = 1;
    if (old_fd != -1) {
        close(old_fd);
    }
    return 0;
}

int retr(socket_info_t * sockif, char param[PARAM_LEN]) {
    printf("Retr()\n");
    char cwd[64];
    getcwd(cwd, 64);
    printf("Current dir: %s\n", cwd);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int err;
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
        return 1;
    }

    printf("Retr opening file\n");
    FILE * file = fopen(param, "rb");
    if (file == NULL) {
        printf("Error can't open file %s\n", param);
        send(sockif->sockfd, "550\r\n", 5, 0);
        clear_data_socket(sockif);
        return 1;
    }

    printf("Retr sending\n");
    char block[BUF_SIZE];
    memset(block, 0, BUF_SIZE);
    err = fseek(file, sockif->rest, SEEK_SET);
    if (err != 0) {
        printf("Error fseek() fail\n");
        send(sockif->sockfd, "451\r\n", 5, 0);
    }
    else {
        while(!feof(file)) {
            int len = fread(block, 1, sizeof(block), file);
            if (len != send(connfd, block, len, 0)) {
                printf("Error occurs when sending file\n");
                send(sockif->sockfd, "426\r\n", 5, 0);
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
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
        return 1;
    }

    char filename[PATH_LEN];
    memset(filename, 0, PATH_LEN);
    strncpy(filename, param, strlen(param));
    filename[strlen(param)] = '\0';

    FILE * file = fopen(filename, "rb+");
    if (file == NULL) {
        printf("Error can't open file %s\n", filename);
        send(sockif->sockfd, "550\r\n", 5, 0);
        clear_data_socket(sockif);
        return 1;
    }
    char block[BUF_SIZE];
    memset(block, 0, BUF_SIZE);
    err = fseek(file, sockif->rest, SEEK_SET);
    if (err != 0) {
        printf("Error fseek() fail\n");
        send(sockif->sockfd, "451\r\n", 5, 0);
    }
    else {
        while(1) {
            int len = recv(connfd, block, sizeof(block), 0);
            if (len == 0) {
                break;
            }
            if (len != fwrite(block, 1, sizeof(block), file)) {
                printf("Error occurs when sending file\n");
                send(sockif->sockfd, "426\r\n", 5, 0);
            }
        }
    }
    fclose(file);
    close(connfd);
    clear_data_socket(sockif);
    send(sockif->sockfd, "226\r\n", 5, 0);
    return 0;
}

int syst(socket_info_t * sockif, char param[PARAM_LEN]) {
    int err;
    err = send(sockif->sockfd, "215 UNIX Type: L8\r\n", 19, 0);
    if (err < 0) {
        printf("Error TYPE(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int type(socket_info_t * sockif, char param[PARAM_LEN]) {
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
        // TODO other error code
        return 1;
    }
}

int rest(socket_info_t * sockif, char param[PARAM_LEN]) {
    sockif->rest = atoi(param);
    int err = send(sockif->sockfd, "350\r\n", 5, 0);
    if (err < 0) {
        printf("Error REST(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    return 0;
}

int quit(socket_info_t * sockif, char param[PARAM_LEN]) {
    int err = send(sockif->sockfd, "221 Goodbye.\r\n", 14, 0);
    if (err < 0) {
        printf("Error Quit(): %s(%d)\n", strerror(errno), errno);
        return 1;
    }
    sockif->is_login = 0;
    return 0;
}

int mkd (socket_info_t * sockif, char param[PARAM_LEN]) {
    int err;
    err = mkdir(param, 0777);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (err < 0) {
        printf("Error MKD(): %s(%d)\n", strerror(errno), errno);
        sprintf(res, "521 Fail to create %s\r\n", param);
        send(sockif->sockfd, res, strlen(res), 0);
        return 1;
    }
    sprintf(res, "257 \"%s\"\r\n", param);
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int cwd (socket_info_t * sockif, char param[PARAM_LEN]) {
    int err;
    err = chdir(param);
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    if (err < 0) {
        printf("Error CWD(): %s(%d)\n", strerror(errno), errno);
        sprintf(res, "431 No such directory %s\r\n", param);
        send(sockif->sockfd, res, strlen(res), 0);
        return 1;
    }
    sprintf(res, "200 Working directory changed\r\n");
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int cdup (socket_info_t * sockif, char param[PARAM_LEN]) {
    char path[PARAM_LEN] = "..";
    return cwd(sockif, path);
}

int pwd (socket_info_t * sockif, char param[PARAM_LEN]) {
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
    int err;
    err = rmdir(param);
    char res[BUF_SIZE];
    if (err < 0) {
        printf("Error PWD(): %s(%d)\n", strerror(errno), errno);
        sprintf(res, "250 Fail to remove %s\r\n", param);
        send(sockif->sockfd, res, strlen(res), 0);
        return 1;
    }
    sprintf(res, "250 %s removed\r\n", param);
    send(sockif->sockfd, res, strlen(res), 0);
    return 0;
}

int list (socket_info_t * sockif, char param[PARAM_LEN]) {
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    int connfd;
    err = build_data_connect(sockif, &connfd);
    if (err != 0) {
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
            printf("%s is a dir\n", param);
            err = traverse_dir(param, connfd);
        }
        else {
            char * buffer;
            char filename[PATH_LEN];
            char * token_ptr;
            for (token_ptr = strtok_r(param, " ", &buffer);
                token_ptr != NULL;
                token_ptr = strtok_r(NULL, " ", &buffer)) {

                printf("token: %s\n", token_ptr);
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
    else {
        char res[BUF_SIZE];
        sprintf(res, "%d\r\n", err);
        send(sockif->sockfd, res, strlen(res), 0);
    }
    return 0;
}

int renm(socket_info_t * sockif, char param[PARAM_LEN]) {
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
            // TODO systex error
            return 1;
        }
        char to[PATH_LEN];
        memset(to, 0, PATH_LEN);
        strncpy(to, req + 5, len);
        err = rename(from, to);
        if (err < 0) {
            printf("Error RNFR(): %s(%d)\n", strerror(errno), errno);
            return 1;
        }
        send(sockif->sockfd, "250\r\n", 5, 0);
        return 0;
    }
    else {
        // TODO wrong req
        return 1;
    }
}

void generate_pasv_response( ushort port, char res[BUF_SIZE]) {
    memset(res, 0, BUF_SIZE);
    sprintf(res, "227 (%s,%hu,%hu)\r\n", ip_addr, port >> 8, port % 256);
    res[strlen(res)] = '\0';
    int i;
    for (i = 0;; ++i) {
        if (res[i] == '\0') {
            break;
        }
        if (res[i] == '.') {
            res[i] = ',';
        }
    }
}

int parse_port_request(
    struct sockaddr_in * addr,
    char param[PARAM_LEN]) {

    int addr_len;
    int comma_count = 0;
    int i;
    int param_len = strlen(param);
    for (i = 0; i < param_len; ++i) {
        if (param[i] == ',') {
            ++comma_count;
            param[i] = '.';
            if (comma_count == 4) {
                addr_len = i;
                break;
            }
        }
    }
    if (i >= param_len - 3) {
        return -1;
    }
    char addr_s[16];
    memset(addr_s, 0, 16);
    strncpy(addr_s, param, addr_len);
    addr_s[addr_len] = '\0';

    char hport_s[4];
    int hport_start = addr_len + 1;
    int lport_start = -1;
    for (i = hport_start; i < param_len; ++i) {
        if (param[i] == ',') {
            lport_start = i + 1;
            break;
        }
    }
    if (i >= param_len - 1) {
        return -1;
    }
    if (lport_start < 0) {
        return -1;
    }
    memset(hport_s, 0, 4);
    strncpy(hport_s, param + hport_start, i - hport_start);
    hport_s[strlen(hport_s)] = '\0';
    ushort hport = atoi(hport_s);

    char lport_s[4];
    memset(lport_s, 0, 4);
    strncpy(lport_s, param + lport_start, param_len - lport_start);
    lport_s[strlen(lport_s)] = '\0';
    ushort lport = atoi(lport_s);

    memset(addr, 0, sizeof(*addr));
    if (inet_pton(AF_INET, addr_s, &(addr->sin_addr)) <= 0) {
        return 1;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons((hport << 8) + lport);

    return 0;
}

int build_data_connect(socket_info_t * sockif, int * connfd) {
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    strncpy(res, "150 Opening BINARY mode data connection.\r\n", 42);
    res[strlen(res)] = '\0';
    err = send(sockif->sockfd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error Send 150 for RETR: %s(%d)\n", strerror(errno), errno);
        return 1;
    }

    if (sockif->mod == 0) { // PASV
        struct sockaddr_in conn_addr;
        socklen_t conn_len = sizeof(conn_addr);
        if ((*connfd = accept(sockif->datafd, (struct sockaddr *)&conn_addr, &conn_len)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            return 1;
        }
    }
    else if (sockif->mod == 1) { // PORT
        if ((*connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return 1;
        }
        struct sockaddr_in addr = sockif->port_addr;
        if (connect(*connfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            send(sockif->sockfd, "425\r\n", 5, 0);
            clear_data_socket(sockif);
            return 1;
        }
    }
    else {
        // TODO if not PASV or PORT before RETR
        clear_data_socket(sockif);
        return 1;
    }
    return 0;
}

void clear_data_socket(socket_info_t * sockif) {
    if (sockif->datafd >= 0) {
        close(sockif->datafd);
    }
    sockif->datafd = -1;
    sockif->mod = -1;
    sockif->rest = 0;
    memset(&(sockif->port_addr), 0, sizeof(sockif->port_addr));
}

void trim_req(char str[BUF_SIZE], int len) {
    int i = len - 1;
    for (; i >= 0; --i) {
        if (str[i] == '\r' || str[i] == '\n') {
            str[i] = '\0';
        }
        else {
            break;
        }
    }
}

int traverse_dir(char dirname[PATH_LEN], int connfd) {
    int err;
    DIR * dir = NULL;
    struct dirent * entry;
    if ((dir = opendir(dirname)) == NULL) {
        printf("Error opendir %s\n", dirname);
        return 450;
    }
    while ((entry = readdir(dir))) {
        char filename[PATH_LEN];
        memset(filename, 0, PATH_LEN);
        strncpy(filename, entry->d_name, strlen(entry->d_name));
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
    closedir(dir);
    return err;
}

int generate_file_info(char filename[PATH_LEN], char info[BUF_SIZE]) {
    memset(info, 0, BUF_SIZE);
    struct stat st;
    int err = stat(filename, &st);
    if (err < 0) {
        printf("Error File info: %s(%d)\n", strerror(errno), errno);
        return 450;
    }
    char permission[11];
    memset(permission, 0, 11);
    switch (st.st_mode & S_IFMT) {
        case S_IFSOCK:
            permission[0] = 's';
            break;
        case S_IFLNK:
            permission[0] = 'l';
            break;
        case S_IFREG:
            permission[0] = '-';
            break;
        case S_IFBLK:
            permission[0] = 'b';
            break;
        case S_IFDIR:
            permission[0] = 'd';
            break;
        case S_IFIFO:
            permission[0] = 'p';
            break;
        case S_IFCHR:
            permission[0] = 'c';
            break;
    }
    permission[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    permission[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    permission[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
    permission[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    permission[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    permission[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
    permission[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
    permission[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    permission[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';

    int links = st.st_nlink;
    char * user = getpwuid(st.st_uid)->pw_name;
    char * group = getgrgid(st.st_gid)->gr_name;
    int size = (int) st.st_size;
    char * time = ctime(&st.st_mtime);
    char mtime[512] = "";
    strncpy(mtime, time, strlen(time) - 1);
    sprintf(info, "%s %d %s %s    %d %s %s\r\n", permission, links, user, group, size, mtime, filename);

    return 0;
}
