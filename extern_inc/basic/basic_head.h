#ifndef __BASIC_HEAD_H__
#define __BASIC_HEAD_H__

/////////////// c++ header file //////////////////////

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <regex>
#include <csignal>
#include <set>

//using namespace std;

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) 
    #define __RJF_WINDOWS__
#elif defined(__gnu_linux__) || defined(__linux__)
    #define __RJF_LINUX__
#endif

#if defined(__RJF_LINUX__)
////////////// linux system header file ////////////////
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>

#elif defined(__RJF_WINDOWS__)
#include <window.h>
#endif

#endif
