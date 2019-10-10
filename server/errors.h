#ifndef ERRORS_H
#define ERRORS_H

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

void error425(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error426(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error450(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error451(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error500(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error501(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error502(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error503(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error530(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error550(socket_info_t * sockif, char errinfo[ERROR_LEN]);
void error553(socket_info_t * sockif, char errinfo[ERROR_LEN]);

#endif