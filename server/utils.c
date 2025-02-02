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
#include <pthread.h>

#include "definition.h"
#include "utils.h"
#include "errors.h"

extern ushort listen_port;
extern char root[PATH_LEN];
extern char ip_addr[16];

int parseArgs(int argc, char** argv) {
    memset(root, 0, PATH_LEN);
    if (argc == 1) {
        listen_port = 21;
        strncpy(root, "/tmp", 4);
        root[strlen(root)] = '\0';
        return 0;
    }
    else if (argc == 3) {
        if (strcmp(argv[1], "-port") == 0) {
            listen_port = (ushort) atoi(argv[2]);
            if (listen_port == 0) {
                return -1;
            }
            strncpy(root, "/tmp", 4);
            root[strlen(root)] = '\0';
            return 0;
        }
        else if (strcmp(argv[1], "-root") == 0) {
            strncpy(root, argv[2], strlen(argv[2]));
            root[strlen(root)] = '\0';
            listen_port = 21;
            return 0;
        }
    }
    else if (argc == 5) {
        if (strcmp(argv[1], "-port") == 0 && strcmp(argv[3], "-root") == 0) {
            listen_port = (ushort) atoi(argv[2]);
            if (listen_port == 0) {
                return -1;
            }
            strncpy(root, argv[4], strlen(argv[4]));
            root[strlen(root)] = '\0';
            return 0;
        }
        else if (strcmp(argv[3], "-port") == 0 && strcmp(argv[1], "-root") == 0) {
            listen_port = (ushort) atoi(argv[4]);
            if (listen_port == 0) {
                return -1;
            }
            strncpy(root, argv[2], strlen(argv[2]));
            root[strlen(root)] = '\0';
            return 0;
        }
    }
    return -1;
}

int valid_email_char(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_';
}

int check_email(char email[PARAM_LEN]) {
    int len = strlen(email);
    int i;
    int found_at = 0;
    for (i = 0; i < len; ++i) {
        if (email[i] == '@') {
            // '@' or more than 1 '@'
            if (found_at != 0) {
                printf("1\n");
                return -1;
            }
            found_at = 1;
        }
        else if (email[i] == '.') {
            // Starts/ends with '.'
            if (i == 0 || i == len -1) {
                printf("2\n");
                return -1;
            }
            // Continuing '.'
            else if (i > 0 || email[i - 1] == '.') {
                printf("3\n");
                return -1;
            }
        }
        else if (!valid_email_char(email[i])) {
            printf("4\n");
            return -1;
        }
    }
    if (found_at != 1) {
        // Can't find '@'
        printf("5\n");
        return -1;
    }
    return 0;
}

socket_info_t * new_socket_info(int sfd) {
    socket_info_t * info;
    info = malloc(sizeof(* info));
    info->sockfd = sfd;
    info->datafd = -1;
    info->rest = 0;
    info->mod = -1;
    info->is_login = 0;
    return info;
}

int check_login(socket_info_t * sockif) {
    if (sockif->is_login == 0) {
        error530(sockif, NULL);
        return 1;
    }
    return 0;
}

void generate_pasv_response(ushort port, char res[BUF_SIZE]) {
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

int parse_port_request(struct sockaddr_in * addr, char param[PARAM_LEN]) {
    // 判断是否符合 h1,h2,h3,h4,p1,p2的形式，并提取相应数值
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
    for (i = lport_start; i < param_len; ++i) {
        if (param[i] == ',') {
            return 1;
        }
    }
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

int build_data_connect(socket_info_t * sockif, int * connfd, char info[INFO_LEN]) {
    int err;
    char res[BUF_SIZE];
    memset(res, 0, BUF_SIZE);
    sprintf(res, "150 Opening BINARY mode data connection%s.\r\n", info);
    res[strlen(res)] = '\0';
    err = send(sockif->sockfd, res, strlen(res), 0);
    if (err < 0) {
        printf("Error Send 150: %s(%d)\n", strerror(errno), errno);
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
        error503(sockif, "PASV or PORT requested");
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
    char info[BUF_SIZE];
    while ((entry = readdir(dir))) {
        char filename[PATH_LEN];
        memset(filename, 0, PATH_LEN);
        strncpy(filename, entry->d_name, strlen(entry->d_name));
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

    struct tm * t = localtime(&st.st_mtime);
    char mtime[512];
    memset(mtime, 0, 512);
    char month[5];
    memset(month, 0, 5);
    switch (t->tm_mon) {
        case 0:  strncpy(month, "Jan", 3); break;
        case 1:  strncpy(month, "Feb", 3); break;
        case 2:  strncpy(month, "Mar", 3); break;
        case 3:  strncpy(month, "Apr", 3); break;
        case 4:  strncpy(month, "May", 3); break;
        case 5:  strncpy(month, "Jun", 3); break;
        case 6:  strncpy(month, "July", 4); break;
        case 7:  strncpy(month, "Aug", 3); break;
        case 8:  strncpy(month, "Sept", 4); break;
        case 9:  strncpy(month, "Oct", 3); break;
        case 10: strncpy(month, "Nov", 3); break;
        case 11: strncpy(month, "Dec", 3); break;
    }
    month[strlen(month)] = '\0';
    if (t->tm_year == 19) {
        sprintf(mtime, "%s %d %d:%d", month, t->tm_mday, t->tm_hour, t->tm_min);
    }
    else {
        sprintf(mtime, "%s %d %d", month, t->tm_mday, t->tm_year + 1900);
    }
    mtime[strlen(mtime)] = '\0';
    char tmp[BUF_SIZE];
    memset(tmp, 0, BUF_SIZE);
    sprintf(tmp, "%s %d %s %s %13d %s %s\r\n", permission, links, user, group, size, mtime, filename);
    tmp[strlen(tmp)] = '\0';
    strcat(info, tmp);
    return 0;
}
