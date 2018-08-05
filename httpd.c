/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

//宏定义：检查参数c是否为空格字符，
//也就是判断是否为空格(' ')、定位字符(' \t ')、CR(' \r ')、换行(' \n ')、垂直定位字符(' \v ')或翻页(' \f ')的情况。
//返回值：若参数c 为空白字符，则返回非 0，否则返回 0。
#define ISspace(x) isspace((int)(x))

//定义server名称
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

/*
 * accept_request:  处理从套接字上监听到的一个 HTTP 请求，在这里可以很大一部分地体现服务器处理请求流程。

 * bad_request: 返回给客户端这是个错误请求，HTTP 状态码 400 BAD REQUEST.

 * cat: 读取服务器上某个文件写到 socket 套接字。

 * cannot_execute: 主要处理发生在执行 cgi 程序时出现的错误。

 * error_die: 把错误信息写到 perror 并退出。

 * execute_cgi: 执行cgi动态解析，也是个主要函数。

 * get_line: 读取套接字的一行，把回车换行等情况都统一为换行符结束。

 * headers: 把 HTTP 响应的头部写到套接字。

 * not_found: 主要处理找不到请求的文件时的情况。

 * sever_file: 调用 cat 把服务器文件返回给浏览器。

 * startup: 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。

 * unimplemented: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。
 */

/* 建议源码阅读顺序：                              
 * main -> startup -> accept_request -> serve_file -> execute_cgi
 * 通晓主要工作流程后再仔细把每个函数的源码看一看。
 */

void *accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
 //处理从套接字上监听到的一个 HTTP 请求，在这里可以很大一部分地体现服务器处理请求流程。
/**********************************************************************/
void *accept_request(void* tclient)
{
    int client = *(int *)tclient;
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI program */
    char *query_string = NULL;

    ////客户端发送给服务器的http请求由三部分组成，分别是：请求行、消息报头、请求正文

    /*获取第一行HTTP报文数据 */
    /////对于HTTP报文来说，第一行的内容即为报文的请求行，格式为<method> <request-URL> <version>，每个字段用空格相连
    ////如果你的http的网址为http://192.168.0.23:47310/index.html
    ////那么你得到的第一行http信息就是GET /index.html HTTP/1.1（注意空格）

    /*get_line参数：1.通信套接字，2.接收数据的缓冲区，3.缓冲区的大小*/
    numchars = get_line(client, buf, sizeof(buf));

    /*把客户端的请求方法存到 method 数组*/
    /*然后判断客户端的请求方法是GET还是POST*/
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
    {     
        method[i] = buf[j];
        i++; j++;
    }
    method[i] = '\0';

    ////HTTP1.0定义了三种请求方法：GET, POST 和 HEAD方法。
    ////HTTP1.1新增了五种请求方法：OPTIONS, PUT, DELETE, TRACE 和 CONNECT 方法。
    ////tinyhttp仅仅实现了GET和POST方法
    ////GET：请求指定的页面信息，并返回实体主体。
    ////POST：向指定资源提交数据进行处理请求（例如提交表单或者上传文件）。数据被包含在请求体中。POST请求可能会导致新的资源的建立或已有资源的修改。

    ////应用举例：
    ////GET方法：在浏览器的地址栏中输入网址的方式访问网页时，浏览器采用GET方法向服务器获取Request-URI所标识的资源，eg:
    ////GET /form.html HTTP/1.1 (CRLF)  //CRLF表示回车和换行

    ////POST方法在Request-URI所标识的资源后附加新的数据，常用于提交表单。eg：
    ////POST /reg.jsp HTTP/ (CRLF)
    ////Accept:image/gif,image/x-xbit,... (CRLF)
    ////...
    ////HOST:www.guet.edu.cn (CRLF)
    ////Content-Length:22 (CRLF)
    ////Connection:Keep-Alive (CRLF)
    ////Cache-Control:no-cache (CRLF)
    ////(CRLF)         //该CRLF表示消息报头已经结束，在此之前为消息报头
    ////user=jeffrey&pwd=1234  //此行以下为提交的数据

    /*strcasecmp函数用于忽略大小写比较字符串*/
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        /*如果既不是 GET 又不是 POST 则无法处理*/
        unimplemented(client);
        return NULL;
    }

#if 1
    /* 如果method是 POST，则把cgi标志位置1，表明要运行CGI程序*/
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    /*将method后面的后边的空白字符略过*/
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;

    ////得到 "/"  如果第一条http信息为GET /index.html HTTP/1.1，
    ////         那么接下来解析得到的就是/index.html

    /*开始读取url*/
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
#endif

#if 1
    ////一个简单的 URL 是这样的：
    ////localhost:/cgi-bin/hello.cgi?username=jelly&password=123456
    ////问号之前的部分可以理解为文件的路径，它指向服务器上的一个脚本文件。
    ////问号之后是传给这个脚本的一些参数。这些参数之间是用 「&」符号分隔的。
    ////每个参数等号前是参数名，等号后是参数值。
    ////在这个例子中我们可以直接看到用户名和密码，很显然这是一种不够安全的方式，
    ////所以我们还可以使用post请求。这样地址栏就看不到这种提交的参数了,相对安全一些。

    /*如果method是GET，url中可能会带有?，有查询参数*/
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;

        /*当query_string所指向的字符不是?或者\0时，后移一位*/
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;

        /*如果GET方法带有查询参数，程序也认为是要执行CGI程序*/
        if (*query_string == '?')
        {           
            cgi = 1;
            *query_string = '\0';   //query_string 指针指向 url 中 ？ 后面的 GET 参数。
            query_string++;
        }
    }
#endif

    ////以上已经将HTTP报文的请求行解析完毕
    /*格式化 url 输出到 path 数组中，在 tinyhttpd 中服务器文件都在 htdocs 文件夹下*/
    sprintf(path, "htdocs%s", url);

    /*默认地址，解析到的路径如果只有"/"，则自动加上index.html */
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

    ////函数定义：int stat(const char *file_name, struct stat *buf);
    ////函数说明：通过文件名filename获取文件信息，并保存在buf所指的结构体stat中
    ////返回值： 执行成功则返回0，失败返回-1，错误代码存于errno
    ////struct stat {
    ////    dev_t         st_dev;       //文件的设备编号
    ////    ino_t         st_ino;       //节点
    ///    mode_t        st_mode;      //文件的类型和存取的权限
    ////    nlink_t       st_nlink;     //连到该文件的硬连接数目，刚建立的文件值为1
    ////    uid_t         st_uid;       //用户ID
    ////    gid_t         st_gid;       //组ID
    ////    dev_t         st_rdev;      //(设备类型)若此文件为设备文件，则为其设备编号
    ////    off_t         st_size;      //文件字节数(文件大小)
    ////    unsigned long st_blksize;   //块大小(文件系统的I/O 缓冲区大小)
    ////    unsigned long st_blocks;    //块数
    ////    time_t        st_atime;     //最后一次访问时间
    ////    time_t        st_mtime;     //最后一次修改时间
    ////    time_t        st_ctime;     //最后一次改变时间(指属性)
    ////};

    ////其中st_mode 则定义了下列数种情况：（8进制）
    ///    S_IFMT   0170000    文件类型的位遮罩（用来跟st_mode进行位与操作，可以屏蔽其他无关位，得到文件的相应模式）
    ///    例如：st_mode为0121421，与S_IFMT进行位与操作后可以得到文件类型为符号连接（0120000）
    ///    除了文件类型位之外其他项都只占1位，所以直接进行与操作就可以判断
    ////    S_IFSOCK 0140000    scoket
    ////    S_IFLNK 0120000     符号连接
    ////    S_IFREG 0100000     一般文件
    ////    S_IFBLK 0060000     区块装置
    ///    S_IFDIR 0040000     目录
    ////    S_IFCHR 0020000     字符装置
    ////    S_IFIFO 0010000     先进先出

    ////    S_ISUID 04000     文件的(set user-id on execution)位
    ////    S_ISGID 02000     文件的(set group-id on execution)位
    ////    S_ISVTX 01000     文件的sticky位

    ////    S_IRUSR(S_IREAD) 00400     文件所有者具可读取权限
    ////    S_IWUSR(S_IWRITE)00200     文件所有者具可写入权限
    ///    S_IXUSR(S_IEXEC) 00100     文件所有者具可执行权限

    ////    S_IRGRP 00040             用户组具可读取权限
    ////    S_IWGRP 00020             用户组具可写入权限
    ///    S_IXGRP 00010             用户组具可执行权限

    ////    S_IROTH 00004             其他用户具可读取权限
    ////    S_IWOTH 00002             其他用户具可写入权限
    ///    S_IXOTH 00001             其他用户具可执行权限

    /*根据path路径寻找对应文件 */
    if (stat(path, &st) == -1) {
        /*假如访问的网页不存在，把消息报头部分读出然后丢弃*/
        ////\n的ASCII码为10，在一切字母数字和常见字符之前，所以strcmp函数的结果在遇到buf中的\n之前大于0
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));

        /*回复客户端网页不存在*/
        not_found(client);
    }
    else
    {
        /*如果路径是个目录（即只有"/"），则默认使用该目录下 index.html 文件*/
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");

        /*判断了给定文件是否有可执权限，如果有，则认为是CGI程序，如果有执行权限但是不能执行，会接受到报错信号*/
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            cgi = 1;

        /*如果不是cgi程序,直接把服务器文件返回，否则执行cgi程序 */
        if (!cgi)
            //读取无需进行交互操作的静态文件，返回给客户端
            serve_file(client, path);
        else
            //执行CGI程序
            execute_cgi(client, path, method, query_string);

    }

    /*断开与客户端的连接（HTTP 特点：无连接）*/
    close(client);

    return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
 //返回给客户端这是个错误请求，HTTP 状态码 400 BAD REQUEST.
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    /*回应客户端错误的 HTTP 请求 */
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");           //状态行
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");            //消息报头部分
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");                                   //空行
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");   //响应正文部分
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
 //读取服务器上某个文件写到 socket 套接字。
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    /*读取文件中的所有数据写到 socket */
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
 //主要处理发生在执行 cgi 程序时出现的错误。
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    /* 回应客户端 cgi 无法执行*/
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n"); //状态行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");            //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");                                   //空行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n"); //响应正文部分
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
 //把错误信息写到 perror 并退出。
/**********************************************************************/
void error_die(const char *sc)
{
    /*出错信息处理 */
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
 //执行cgi动态解析，也是个主要函数。
/**********************************************************************/
void execute_cgi(int client, const char *path, const char *method, const char *query_string)
{
    char buf[1024];
    int cgi_output[2];  //两根管道
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    ////如果是GET请求，对消息报头部分的信息进行忽略
    buf[0] = 'A'; buf[1] = '\0';   
    if (strcasecmp(method, "GET") == 0)
        /*读取并丢弃消息报头部分的信息*/
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else    /* POST */
    {
        ////如果是POST请求，就需要得到Content-Length
        ////Content-Length作为实体报头，位于一个HTTP请求中的消息报头部分
        ////Content-Length：用于指明实体正文的长度，以字节方式存储的十进制数字来表示。
        /* 在 POST 的 HTTP 请求中找出 Content-length */
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            /*利用 \0 进行分隔，"Content-Length:"共15位*/
            buf[15] = '\0';

            if (strcasecmp(buf, "Content-Length:") == 0)
                ////buf中从第17位开始就是长度，将17位开始的所有字符串转成整数就是content_length
                /*atoi() 函数用来将字符串转换成整数(int)*/
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        /*没有找到 content_length */
        if (content_length == -1) {
            /*错误请求*/
            bad_request(client);
            return;
        }
    }

    /* 正确，HTTP 状态码 200 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //此时先向客户端发送状态行，消息报头等部分由cgi程序发送
    send(client, buf, strlen(buf), 0);


    ////int pipe(int filedes[2]);
    ////返回值：成功返回0，否则返回-1。
    ////参数：filedes是一个有两个成员的整形数组，用来保存管道的文件描述符
    ////filedes[0]将用来从管道读取数据，filedes[1]用来向管道写入数据。
    ////管道是半双工的，数据只能向一个方向流动；所以双方需要通信时，需要建立起两个管道；

    /* 建立管道*/
    if (pipe(cgi_output) < 0) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }
    /*建立管道*/
    if (pipe(cgi_input) < 0) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }

    ////在fork函数执行完毕后，如果创建新进程成功，则出现两个进程，一个是子进程，一个是父进程。
    ////在子进程中，fork函数返回0，在父进程中，fork返回新创建子进程的进程ID。

    ////注意：在任何情况下都应该谨慎在多线程中使用fork函数
    ////即使必要这么做，在多线程程序中最好只用fork来执行exec函数，不要对fork出的子进程进行其他任何操作。

    /*创建一个子进程*/
    if ((pid = fork()) < 0 ) {
        /*错误处理*/
        cannot_execute(client);
        return;
    }
    /*子进程用于执行CGI*/
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        ////函数dup2提供了复制文件描述符的功能。通常用于stdin,stdout或进程的stderr的重定向。
        ////一般来说，普通输出函数（如：printf），默认是将某信息写入到文件描述符为1的文件中，普通输入函数都默认从文件描述符为0的文件中读取数据。
        ////而文件描述符为1的文件一般默认为标准输出(stdout)，文件描述符为0的文件一般默认为标准输入(stdin)，
        ////因此重定向操作实际上是关闭某个标准输入输出设备（文件描述符为0、1、2），而将另一个打开的普通文件的文件描述符设置为0、1、2.
        ////输入重定向：关闭标准输入设备，打开（或复制）某普通文件，使其文件描述符为0.
        ////输出重定向：关闭标准输出设备，打开（或复制）某普通文件，使其文件描述符为1.
        ////错误输出重定向：关闭标准错误输入设备，打开（或复制）某普通文件，使其文件描述符为2.

        ////函数定义：int dup2(int oldfd, int newfd);
        ////函数说明：函数dup2用来复制参数oldfd所指的文件描述符，并将oldfd拷贝到参数newfd后一起返回。
        ////若参数newfd为一个打开的文件描述符，则newfd所指的文件会先被关闭，若newfd等于oldfd，则返回newfd,而不关闭newfd所指的文件。
        ////dup2所复制的文件描述符与原来的文件描述符共享各种文件状态。共享所有的锁定，读写位置和各项权限或flags等等.
        ////返回值：如成功则返回新的文件描述符，否则出错返回-1.
        ////注意：由dup2函数返回的新文件描述符一定是当前可用文件描述符中的最小值。

        /* 把 stdout 重定向到 cgi_output 的写入端 */
        dup2(cgi_output[1], 1);

        /* 把 stdin 重定向到 cgi_input 的读取端 */
        dup2(cgi_input[0], 0);

        ////    管道的初始状态：
        ////                      =============
        ////            stdin -->      管道      --> stdout
        ////                      =============
        ////
        ////
        ////     cgi_input[1] --> ============= <-- cgi_input[1]
        ////                           管道
        ////     cgi_input[0] <-- ============= --> cgi_input[0]
        ////
        ////
        ////    cgi_output[1] --> ============= <-- cgi_output[1]
        ////                           管道
        ////    cgi_output[0] <-- ============= --> cgi_output[0]
        ////-----------------------------------------------------------------------
        ////    fork后子进程把管道都复制了一份，显然重复了
        ////    所以父进程和子进程都关闭2个无用的端口，避免浪费
        ////    管道此时的状态：
        ////                      =============
        ////            stdin -->      管道      --> stdout
        ////                      =============
        ////
        ////
        ////                      =============
        ////     cgi_input[1] -->      管道      --> cgi_input[0]
        ////                      =============
        ////
        ////
        ////                      =============
        ////    cgi_output[1] -->      管道      --> cgi_output[0]
        ////                      =============
        ////-----------------------------------------------------------------------
        ////    把 stdout 重定向到 cgi_output 的写入端，把 stdin 重定向到 cgi_input 的读取端
        ////
        ////    管道的最终状态：
        ////    <--------------------------------------------
        ////    |                       =============       |
        ////    |             stdin -->      管道      --> stdout
        ////    |               |       =============
        ////    |               <------------------------------
        ////    |                       =============         |
        ////    |     GET或POST的数据 -->     管道      --> cgi_input[0]
        ////    |     (cgi_input[1])    =============
        ////    |
        ////    |                       =============
        ////    ----> cgi_output[1] -->      管道      --> send到客户端
        ////                            =============    (cgi_output[0])
        ////    此时父子进程已经可以通信

        /* 关闭cgi_output 的读取端 和 cgi_input 的写入端 */
        close(cgi_output[0]);
        close(cgi_input[1]);

        ////当前端页面通过get或post方法向cgi程序提交了数据之后，CGI程序通过从环境变量中取值来获得相应的参数。
        ////下面是几个在本程序中用到的环境变量：
        ////REQUEST_METHOD：【前端页面数据请求方式：get/post】
        ////QUERY_STRING：【采用GET时所传输的信息】
        ////CONTENT_LENGTH：【STDIO中的有效信息长度】

        ////这些环境变量是从何而来，是谁定义的？是Linux吗？POSIX吗？当然不是。
        ////这里就要再次声明一下CGI是一个接口协议，这些环境变量就是属于该协议的内容，
        ////所以不论你的server所在的操作系统是Linux还是Windows，
        ////也不论你的server是Apache还是Nginx，这些变量的名称和含义都是一样的。
        ////实际就是Apache/Nginx在将这些内容填充到环境变量中，而具体填充规范则来自于CGI接口协议。

        ////定义函数：int putenv(const char * string);
        ////函数说明：putenv()用来改变或增加环境变量的内容. 参数string 的格式为name＝value,
        ////如果该环境变量原先存在, 则变量内容会依参数string 改变, 否则此参数内容会成为新的环境变量.
        ////返回值：执行成功则返回0, 有错误发生则返回-1.

        /*设置 request_method 的环境变量*/
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if (strcasecmp(method, "GET") == 0) {
            /*设置 query_string 的环境变量*/
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {   /* POST */
            /*设置 content_length 的环境变量*/
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        ////exec函数族提供了一种在进程中启动另一个程序执行的方法。
        ////它可以根据指定的文件名或目录名找到可执行文件（在本程序中是CGI），并用它来取代原调用进程的数据段、代码段和堆栈段。
        ////在执行完之后，原调用进程的内容除了进程号外，其他全部都被替换了。
        ////如果某个进程想同时执行另一个程序，它就可以调用fork函数创建子进程，然后在子进程中调用任何一个exec函数。
        ////这样看起来就好像通过执行应用程序而产生了一个新进程一样

        ////函数原型：int execl(const char *path, const char *arg, ...);
        ////参数：path参数表示你要启动程序的名称（包括路径名），arg参数表示启动程序所带的参数，必须以NULL结束
        ////返回值:成功返回0,失败返回-1
        ////注意：程序在执行execl后产生的新进程保持了原进程的环境变量

        /*用 execl 运行 cgi 程序*/
        execl(path, path, NULL);
        exit(0);
    }
    /*父进程用于接收数据以及发送子进程处理的回复数据*/
    else {    /* parent */
        /* 关闭 cgi_input 的读取端 和 cgi_output 的写入端 */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            /*接收 POST 过来的数据*/
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                /*把读取的 POST 数据写入 cgi_input，供子进程使用 */
                write(cgi_input[1], &c, 1);
            }

        ////write()一般是向文件描述符中写数据，
        ////而send()是向socket（如建立tcp连接的）中发送数据。

        /*从 cgi_output 读取子进程处理后的信息，然后send到客户端 */
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        /*完成操作后关闭管道*/
        close(cgi_output[0]);
        close(cgi_input[1]);

        ////当使用fork启动一个子进程时候，子进程有了自己的生命，并将独立地运行。
        ////当父进程退出，而它的一个或多个子进程还在运行时，子进程将成为僵尸进程。
        ////僵尸进程将保留在进程表里，直到被init发现并处理掉，释放PCB等
        ////有时候我们需要知道某个子进程是否已经结束了，可以通过调用wait安排父进程结束在子进程结束之后

        ////定义函数：pid_t waitpid(pid_t pid, int * status, int options);
        ////函数说明：waitpid()会暂时停止目前进程的执行, 直到有信号来到或子进程结束.
        ////参数pid 为欲等待的子进程识别码, 其数值意义如下：
        ////1、pid<-1 等待进程组识别码为pid 绝对值的任何子进程.
        ////2、pid=-1 等待任何子进程, 相当于wait().
        ////3、pid=0 等待进程组识别码与目前进程相同的任何子进程.
        ///4、pid>0 等待任何子进程识别码为pid 的子进程.
        ////status是一个整型指针，指向的对象用来保存子进程退出时的状态。
        ////status若为空，表示忽略子进程退出时的状态
        ////status若不为空，表示保存子进程退出时的状态
        ////当options为0时：同wait，阻塞父进程，等待子进程退出。

        /*等待子进程返回*/
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
 //读取套接字的一行，把回车换行等情况都统一为换行符结束。
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    /*把终止条件统一为 \n 换行符，标准化 buf 数组*/
    while ((i < size - 1) && (c != '\n'))
    {
        /*一次仅接收一个字节*/
        /*recv参数：1.通信套接字，2.接收数据的缓冲区首地址，3.接收的字节数，4.接收方式（一般为0）*/
        n = recv(sock, &c, 1, 0);

        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            /*收到 \r 则继续接收下个字节，因为换行符可能是 \r\n */
            if (c == '\r')
            {
                ////recv函数通常flags都设置为0，此时recv函数读取tcp buffer中的数据到buf中，并从tcp buffer中移除已读取的数据。
                ////把flags设置为MSG_PEEK，仅把tcp buffer中的数据读取到buf中，
                ////并不把已读取的数据从tcp buffer中移除，再次调用recv仍然可以读到刚才读到的数据。
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */

                /*但如果是换行符则把它读取掉，否则将\r替换为\n*/
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            /*把读取到的字符存到缓冲区*/
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    /*返回 buf 数组实际的大小（即一行的长度）*/
    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
  //把 HTTP 响应的头部写到套接字。
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    /*正常的 HTTP header，200表示客户端请求成功 */
    strcpy(buf, "HTTP/1.0 200 OK\r\n");                 //状态行
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    strcpy(buf, SERVER_STRING);                         //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");        //消息报头部分
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");                                //空行
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
 //主要处理找不到请求的文件时的情况。
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    ////在接收和解释请求消息后，服务器返回一个HTTP响应消息。
    ////HTTP响应也是由三个部分组成，分别是：状态行、消息报头、响应正文

    /* 404页面，请求资源不存在*/
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");         //状态行
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    sprintf(buf, SERVER_STRING);                        //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");        //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");                               //空行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n"); //响应正文部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
 //调用 cat 把服务器文件返回给浏览器。
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    ////C语言file类，在stdio.h 头文件中，FILE类是一个结构体：定义如下：
    ////struct _iobuf {
    ////    char *_ptr;
    ////    int   _cnt;
    ////    char *_base;
    ////    int   _flag;
    ////    int   _file;
    ////    int   _charbuf;
    ////    int   _bufsiz;
    ////    char *_tmpfname;
    ////};

    ////typedef struct _iobuf FILE;

    ////通过typedef定义了 文件类型 的别名："FILE"，这样以后需要读写文件的时候直接定义FILE就行了。
    ////虽然看不懂参数具体表示的是什么，但是凭借经验不难知道，FILE中的属性无非就是文件名,修改时间，权限等一些信息
    ////而FILE *fp通俗的说，就是定义一个指向文件流的指针，通过操作这个指针就能进行文件读写，打开关闭等操作

    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    ////在本程序中对GET方法的处理很简单，直接忽略消息报头的信息
    /*读取并丢弃消息报头部分的信息*/
    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    /*打开服务器的对应文件*/
    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        /*写 HTTP header */
        headers(client, filename);
        /*复制文件*/
        cat(client, resource);
    }

    /*关闭文件*/
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
 //初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;              //socket返回的文件描述符
    struct sockaddr_in name;    //Internet协议地址结构,用于储存服务器端的端口和IP地址

    /*建立 socket */
    /*PF_INET->internet 协议，SOCK_STREAM->流式套接字，默认协议*/
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    /*绑定端口 */
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");

    /*如果当前指定端口是 0，则动态随机分配一个端口*/
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);

        /*getsockname：返回指定套接字的IP地址 */
        /*参数：1.需要获取名称的套接字，2.存放所获取套接字名称的缓冲区，3.缓冲区的长度*/
        /*此处用来读取绑定后随机生成的端口号*/
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }

    /*开始监听*/
    if (listen(httpd, 5) < 0)
        error_die("listen");
    /*返回 socket id */
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
 //返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。
/**********************************************************************/
void unimplemented(int client)
{
    char buf[1024];

    /* HTTP method 不被支持*/
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");            //状态行
    send(client, buf, strlen(buf), 0);
    /*服务器信息*/
    sprintf(buf, SERVER_STRING);                                        //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");                        //消息报头部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");                                               //空行
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");      //响应正文部分
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;           //监听套接字
    u_short port = 0;               //服务器的端口号
    int client_sock = -1;           //与客户端进行通信的套接字
    struct sockaddr_in client_name; //Internet协议地址结构，用于储存客户端的端口和IP地址
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;

    /*在对应端口建立 httpd 服务*/
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        /*套接字收到客户端的连接请求*/
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);
        if (client_sock == -1)
            error_die("accept");

        /*派生新线程用 accept_request 函数处理新请求*/
        /* accept_request(client_sock); */

        ////函数原型：int pthread_create(pthread_t *restrict tidp, ->创建的线程
        ////               const pthread_attr_t *restrict_attr,  ->指定线程的属性，NULL表示使用缺省属性
        ////                         void*（*start_rtn)(void*),  ->线程执行的函数
        ////                              void *restrict arg);   ->传递给线程执行的函数的参数
        if (pthread_create(&newthread , NULL, accept_request, (void*)&client_sock) != 0)
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}
