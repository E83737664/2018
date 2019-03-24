/*************************************************************************
	> File Name: download.cpp
	> Author: 
	> Mail: 
	> Created Time: 日  3/24 10:44:42 2019
 ************************************************************************/

#include"download.h"

/*解析HTTP响应码函数*/
HTTPCODE parse_HTTPCODE(const char *http_respond)
{
    char *http;
    char *get;
    char code[4];
    int len = strlen(http_respond);
    http = new char [len];
    strcpy(http, http_respond);
    get = strstr(http," ");
    get++;
    int i=0;
    while(*get != ' ')
    {
        code[i++] = *get;
        get++;
    }
    code[3] = '\0';
    delete [] http;
    cout << "code:"<< code << endl;
    if(strcmp(code,"200")==0) return OK;
    if(strcmp(code,"206")==0) return PARTIAL_OK;
    if(strcmp(code,"403")==0) return FORBIDDEN;
    if(strcmp(code, "400")==0) return NOTFOUND;
    else return UNKNOWN;
}

/*对HTTP响应码作出相应的处理*/
void deal_with_code(HTTPCODE code)
{
    int my = code;
    switch(my)
    {
        case OK:
        {
            cout << "OK\n";
            return;
        }
        case PARTIAL_OK:
        {
            cout << "PARTIAL_OK\n";
            return;
        }
        case FORBIDDEN:
        {
            cout << "该资源无权访问!\n";
            exit(0);
        }
        case NOTFOUND:
        {
            cout << "未找到该资源,请检查下载地址是否填写正确!\n";
            exit(0);
        }
    }
}

long int get_file_size(int fd)
{
    struct stat st;
    int ret = fstat(fd, &st);
    assert(ret != -1);
    return st.st_size;
}

Baseclient :: ~Baseclient()
{
    close(sockfd);
    delete [] myfile_information.file_path;
    
}
/*解析用户输入的下载地址*/
Baseclient :: STATUS Baseclient :: parse_address()
{
    char *get;
    /*判断下载地址的状态*/
    if(strstr(address,"https") != NULL)
    {
        return HTTPS;
    }
    
    /*获取FQDN*/
    get = address + 7;
    fqdn = get;//获取FQDN的起始位置
    get = strstr(get, "/");//解析出FQDN地址
    *get++ = '\0';
    host = gethostbyname(fqdn); //通过名字获取hostIP地址
    
    /*获取文件的绝对路径*/
    int len = strlen(get)+2;
    myfile_information.file_path = new char[len];
    sprintf(myfile_information.file_path, "/%s",get);
    myfile_information.file_path[len-1] = '\0';
    len = strlen(myfile_information.file_path);
    
    /*获取文件原来的名称*/
    int i = len;
    for(i = len-1; i>=0; i--)
    {
        if(myfile_information.file_path[i] == '/')
        {
            get = myfile_information.file_path + i + 1;
            break;
        }
    }
    len = strlen(get);
    strcpy(myfile_information.file_name,get);
    myfile_information.file_name[strlen(get)] = '\0';
    
    /*获取.*td文件名称*/
    len = strlen(myfile_information.file_name);
    for(int i=0; i<len; i++)
    {
        if(myfile_information.file_name[i]=='.')
        {
            myfile_information.file_name_td[i] = myfile_information.file_name[i];
            break;
        }
        myfile_information.file_name_td[i] = myfile_information.file_name[i];
    }
    sprintf(myfile_information.file_name_td, "%s*td",myfile_information.file_name_td);
    return HTTP;
}

/*发送HTTP请求头，接收HTTP响应头，对头部内容进行解析*/
void Baseclient :: parse_httphead()
{
    //cout << "发送HTTP请求头：\n";
    //cout << http_request << endl;
    //cout << "接收HTTP响应头:\n";
    int ret = write(sockfd, http_request, strlen(http_request));
    if(ret <= 0)
    {
        cout << "wrong http_request\n";
        exit(0);
    }
    int k = 0;
    char ch[1];
    /*解析出HTTP响应头部分*/
    while(read(sockfd, ch, 1) != 0)
    {
        http_respond[k] = ch[0];
        if(k>4 && http_respond[k]=='\n' && http_respond[k-1]=='\r' && http_respond[k-2]=='\n' && http_respond[k-3]=='\r')
        {
            break;
        }
        k++;
    }
    int len = strlen(http_respond);
    http_respond[len] = '\0';
    cout << http_respond<< endl;
    
    /*分析HTTP响应码*/
    HTTPCODE code;
    code = parse_HTTPCODE(http_respond);
    deal_with_code(code);
    
    /*解析出content-length:字段*/
    char *length;
    length = strstr(http_respond,"Content-Length:");
    if(length == NULL)
    {
        length = strstr(http_respond,"Content-length:");
        if(length == NULL)
        {
            length = strstr(http_respond, "content-Length:");
            if(length == NULL)
            {
                length = strstr(http_respond,"content-length:");
                if(length == NULL)
                {
                    cout << "NOT FOUND  Content-Length\n";
                    exit(0);
                }
            }
        }
    }
    char buf[10];
    char *get = strstr(length,"\r");
    *get = '\0';
    length = length + 16;;
    myfile_information.file_length = atol(length);
    int r_ret = read(sockfd,buf,1);
}

void* Baseclient :: work(void *arg)
{
    char *buffer;
    struct thread_package *my = (struct thread_package *)arg;
    
    /*设置套接字*/
    struct sockaddr_in client;
    struct hostent *thread_host;
    thread_host = gethostbyname(my->fqdn);
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = *(int *)thread_host->h_addr_list[0];
    client.sin_port = htons(80);
    
    /*创建套接字*/
    my->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(my->sockfd>=0);
    
    /*建立连接*/
    int ret = connect(my->sockfd, (struct sockaddr*)&client, sizeof(client));
    assert(ret != -1);
    //cout << "成功连接服务器!\n";
    //cout << "my->url:" << my->url << endl;
    /*填充HTTP GET方法的请求头*/
    char http_head_get[1000];
    sprintf(http_head_get,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\nRange: bytes=%ld-%ld\r\n\r\n",my->url, my->fqdn, my->start, my->end);
    // cout << "http_head_get:\n"  << http_head_get << endl;
    
    /*发送HTTP GET方法的请求头*/
    int r = write(my->sockfd, http_head_get, strlen(http_head_get));
    assert(r>0);
    //cout << "发送HTTP请求成功\n";
    /*处理HTTP请求头*/
    char c[1];
    char buf[2000];
    int k = 0;
    /*处理响应头函数，判断是否为HTTPS或者不合法HTTP响应头*/
    while(read(my->sockfd, c, 1) != 0)
    {
        buf[k] = c[0];
        if(k>4 && buf[k]=='\n' && buf[k-1]=='\r' && buf[k-2]=='\n' && buf[k-3]=='\r')
        {
            break;
        }
        k++;
    }
    int l = strlen(buf);
    buf[l] = '\0';
    cout << buf<< endl;
    HTTPCODE mycode = parse_HTTPCODE(buf);
    deal_with_code(mycode);
    
    int len = (my->end) - (my->start);
    buffer = new char[len];
    int fd = open(my->file_name, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
    assert(fd > 0);
    off_t offset;
    if((offset = lseek(fd, my->start, SEEK_SET)) < 0)
    {
        cout << "lseek is wrong!\n";
    }
    int ave = len;
    int r_ret = 0;
    int w_ret = 0;
    while((r_ret = read(my->sockfd, buffer, len))>0 && my->read_ret!=ave)
    {
        my->read_ret = my->read_ret + r_ret;
        len = ave - my->read_ret;
        w_ret = write(fd, buffer, r_ret);
        my->write_ret = my->write_ret + w_ret;
    }
    if(r_ret < 0)
    {
        cout << "read is wrong!\n";
    }
    delete [] buffer;
    close(fd);
    close(my->sockfd);
    return 0;
}
void Baseclient :: thread_download()
{
    void *statu;
    long int ave_bit;//线程平均字节数目
    struct thread_package *Thread_package;
    Thread_package = new struct thread_package[thread_number];
    
    /*如果.*td文件不存在，则为一个新的下载*/
    if(access(myfile_information.file_name_td, F_OK) != 0)
    {
        ave_bit = myfile_information.file_length / thread_number;
        
    }
    
    /*如果.*td文件存在,则属于断点下载*/
    else
    {
        int fd = open(myfile_information.file_name_td, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
        /*获取已经读过的文件大小*/
        long int file_size = get_file_size(fd);
        cout << "已经读取的字节数目:"<< file_size << endl;
        close(fd);
        /*计算出剩下的文件大小,计算每个线程应该读多少字节*/
        myfile_information.file_length = myfile_information.file_length - file_size;
        cout << "剩余字节数:" << myfile_information.file_length << endl;
        ave_bit = myfile_information.file_length / thread_number;
    }
    
    long int start = 0;
    int i = 0;
    /*多线程下载*/
    for(i=0; i<thread_number; i++)
    {
        Thread_package[i].read_ret = 0;//该线程已经从sockfd读取的字节数目
        Thread_package[i].write_ret = 0;//该线程已经写入文件的字节数目
        Thread_package[i].sockfd = -1;//该线程的socket
        Thread_package[i].start = start;//该线程读取文件内容的开始位置
        start = start + ave_bit;
        Thread_package[i].end = start;//该线程读取文件内容的结束位置
        Thread_package[i].fqdn = fqdn;//该线程存取访问的fqdn
        Thread_package[i].url = address_buf;//该线程存取下载地址
        strcpy(Thread_package[i].file_name, myfile_information.file_name_td);//该线程存取文件名称CIF文件，以判断是否为断点下载
    }
    int Sum = 0;
    for(i=0; i<thread_number; i++)
    {
        /*pthread_create(&pid, NULL, work, &Thread_package[i]);
         pthread_join(pid, &statu);*/
        pthread_create(&Thread_package[i].pid, NULL, work, &Thread_package[i]);
        pthread_detach(Thread_package[i].pid);
    }
    
    /*打印进度条*/
    cout << "打印进度条\n";
    char bar[120];
    char lable[4]="/|\\";
    int k=0;
    int count = 0;
    /*主线程反复循环，查看各线程是否完成下载,若所有线程完成下载,则退出循环*/
    while(1)
    {
        count = 0;
        for(auto i=0; i<thread_number; i++)
        {
            count = count + Thread_package[i].write_ret;
        }
        /*按照百分比打印下载进度条*/
        double percent = ((double)count / (double)myfile_information.file_length)*100;
        while(k <= (int)percent)
        {
            printf("[%-100s][%d%%][%c]\r", bar, (int)percent, lable[k % 4]);
            fflush(stdout);
            bar[k] = '#';
            k++;
            bar[k] = 0;
            usleep(10000);
        }
        if(count == myfile_information.file_length)
        {
            cout << "\n下载结束\n";
            break;
        }
    }
    if(count != myfile_information.file_length)
    {
        int r = remove(myfile_information.file_name_td);
        if(r == 0)
        {
            cout << "下载失败!\n";
        }
        exit(0);
    }
    else{
        rename(myfile_information.file_name_td, myfile_information.file_name);
        cout << "下载成功!\n";
    }
    
}
void Baseclient :: mysocket()
{
    STATUS mystatu;
    int len = strlen(address);
    address_buf = new char[len];
    strcpy(address_buf, address);
    
    mystatu = parse_address();//解析输入的下载地址,仅仅支持HTTP下载
    if(mystatu==HTTPS)
    {
        cout << "该程序仅支持HTTP下载\n";
        exit(0);
    }
    if(host == NULL)
    {
        cout << "无法解析FQDN的IP地址,请检查下载地址是否输入正确\n";
        exit(0);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = *(int *)host->h_addr_list[0];
    server.sin_port = htons(port);
    
    /*创建套接字*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd>=0);
    
    /*创建连接*/
    int ret = connect(sockfd, (struct sockaddr*)&server, sizeof(server));
    assert(ret != -1);
    cout << "成功连接服务器!\n";
    
    /*填充HTTP请求头*/
    sprintf(http_request,"HEAD %s HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n\r\n ",address_buf ,fqdn);
    //cout << "http_request:\n" << http_request << endl;
    
    /*分析收到的HTTP响应头*/
    parse_httphead();
    
    /*根据线程数量进行下载文件*/
    thread_download();
    
}




