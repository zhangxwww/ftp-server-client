#ifndef HANDLERS_H
#define HANDLERS_H

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

handler_t get_cmd_handler(char cmd[5], int * len);

int server_ready(socket_info_t * sockif);
int user (socket_info_t * sockif, char param[PARAM_LEN]);
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

#endif