#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<errno.h>
#include<pthread.h>
#include<stdio.h>
#include<mysql/mysql.h>
#include"Data.h"

void send_data(int conn_fd,const char *string) //传入一个连接套接字和字符串数据   
{
    if(send(conn_fd,string,strlen(string),0)<0)
    {
        perror("send");
        exit(1);
    }
}

void Delete_for_friend_third(char *a,char *b,char *c) //为了和并出一个唯一的字符串删除好友关系
{
    
    if(strcmp(a,b)<=0)
    {
        strcat(c,a);
        strcat(c,b);
    }else
    {
        strcat(c,b);
        strcat(c,a);
    }  
}

int login(recv_t *sock,MYSQL *mysql)  //sock_fd是要被发送数据的套接字
{
    int ret;
    char recv_buf[MAX_USERNAME];//登录时默认使用字符串
    int flag_recv=USERNAME;
    char buf[256];
    sprintf(buf,"select *from Data where Account = %s",sock->send_Account);
    mysql_query(mysql,buf);
    MYSQL_RES *result = mysql_store_result(mysql);
    MYSQL_ROW row=mysql_fetch_row(result);
    printf("%s || %s\n",sock->message,row[1]);
    mysql_free_result(result);
    MYSQL_RES *res=NULL;
    int row_in_messages_box=0;
    if(!strcmp(sock->message,row[1]))//在数据库中检测账号密码是否匹配 返回名称　密码在message中
    {
        send_data(sock->send_fd,row[3]);//发送名称
        sprintf(buf,"update Data set status = \"1\" where Account = \"%s\"",sock->send_Account);
        mysql_query(mysql,buf); //改变登录状态
        //查询消息盒子 把离线期间发送给send_account的消息提取并发送
        sprintf(buf,"select *from messages_box where recv_account = '%s'",sock->send_Account);
        mysql_query(mysql,buf);
        res=mysql_store_result(mysql);
        if(res==NULL)  //等于空就是出现错误　成功不会为NULL 查询的行为０也不会为NULL
        {
            perror("error in mysql_store_result\n");
            return 0;
        }
        //先发送一个代表消息盒子是否有信息的包　客户端做出接收　
        //两种情况分情况编写代码 因为发信息不知道什么时候结束　只能在结束时发送一个代表消息结束的包
        if((row_in_messages_box=mysql_num_rows(res))==0)
        {
            send_data(sock->send_fd,BOX_NO_MESSAGES);
        }else
        {
            send_data(sock->send_fd,BOX_HAVE_MESSAGS);
        }
        printf("标志消息盒子　是否有数据的包发送成功  %d\n",row_in_messages_box);
        //开始发送消息
        Box_t box;
        printf("%d\n",row_in_messages_box);
        int flag=0;
        if(row_in_messages_box==0) flag=1;
        while(row_in_messages_box--)
        {
            row=mysql_fetch_row(res);
            box.type=ADD_FRIENDS;      //时间类型　离线消息不止添加好友
            strcpy(box.message,row[3]);//消息
            strcpy(box.account,row[1]);//发送者
            if(send(sock->send_fd,&box,sizeof(Box_t),0)<0)
            perror("error in send\n");
            sprintf(buf,"delete from messages_box where recv_account = '%s' and send_acount = '%s' and message = '%s'",
            sock->send_Account,box.account,box.message);
            //printf("%s\n",buf);
            mysql_query(mysql,buf);
        }
        if(flag!=1)
        {
            box.type=EOF_OF_BOX;
            strcpy(box.message,row[3]);
            send(sock->send_fd,&box,sizeof(Box_t),0);
        }
        printf("全部信息发送完成\n");
    }
    else 
    send_data(sock->send_fd,"@@@");//密码账号不匹配　返回错误
    mysql_free_result(res);

    //发送好友列表的函数所需要的值登录函数中已设置　所以这个数据包可直接使用　
    //有效位为其中的　send_Account 与 send_fd 
    //谁发的　以及　套接字是多少
    printf("函数进行到这里数据库查找数据\n");
    List_friends_server(sock,mysql);
}

int register_server(recv_t * sock,MYSQL *mysql)
{
    char account[MAX_ACCOUNT];
    char buf[256];
    memset(account,0,sizeof(account));
    mysql_query(mysql,"select *from Account");
    //perror("error in mysql_query\n");
    MYSQL_RES *result = mysql_store_result(mysql);
    MYSQL_ROW row=mysql_fetch_row(result);
    //itoa(row[0]+1,account,10);    //atoi字符串转数字
    //数字转化为字符串必须用sprintf itoa不标准
    sprintf(account,"%d",atoi(row[0])+1);
    sprintf(buf,"update Account set Account = \"%s\" where Account = \"%s\"",account,row[0]);
    mysql_query(mysql,buf);
    send_data(sock->send_fd,account);//注册时返回一个账号                                       //存一次昵称
    sprintf(buf,"insert into Data values('%s','%s','%s','%s',0,%d)",
    account,sock->message,sock->message_tmp,sock->recv_Acount,sock->send_fd);
    printf("%s\n",buf);
    mysql_query(mysql,buf);
    mysql_free_result(result);
}

int Retrieve_server(recv_t *sock,MYSQL *mysql)
{
    int ret;
    char recv_buf[MAX_USERNAME];
    char buf[256];
    sprintf(buf,"select *from Data where Account = %s",sock->send_Account);
    mysql_query(mysql,buf);
    MYSQL_RES *result = mysql_store_result(mysql);
    MYSQL_ROW row;
    row=mysql_fetch_row(result);
    if(!strcmp(sock->message_tmp,row[2]))
    {
        sprintf(buf,"update Data set password = \"%s\" where Account = \"%s\"",sock->message,sock->send_Account);
        mysql_query(mysql,buf);
        send_data(sock->send_fd,"y");
    }
    else 
    send_data(sock->send_fd,"@@@");
}

int add_friend_server(recv_t *sock,MYSQL *mysql)
{
    int ret;
    char recv_buf[MAX_USERNAME];
    char buf[256];
    sprintf(buf,"select *from Data where Account = %s",sock->recv_Acount);
    mysql_query(mysql,buf);
    MYSQL_RES *result = mysql_store_result(mysql);
    MYSQL_ROW row=mysql_fetch_row(result);
    int tmp=atoi(row[5]);
    if(atoi(row[4])==1)  //在线
    {
        printf("11\n");
        if(send(tmp,sock,sizeof(recv_t),0)<0)  //根据账号查找到接收者的套接字
        perror("error in send\n");//需要在线消息盒子　否则无法实现
    }else  //不在线把数据放到消息盒子
    {
        printf("212\n");
        sprintf(buf,"insert into messages_box values('%d','%s','%s','%s')",tmp,sock->send_Account,sock->recv_Acount,sock->message);
        printf("%s\n",buf);
        mysql_query(mysql,buf);
    }
    //成功后不发送消息
}

int add_friend_server_already_agree(recv_t *sock,MYSQL *mysql)//向朋友数据库加入消息
{
    //friend数据表中第三项　是为了在删除时仅删除一项就把一对好友关系进行删除　
    //这个函数只需要操作下数据库就好
    char buf[512];
    char unique_for_del[64];
    Delete_for_friend_third(sock->recv_Acount,sock->send_Account,unique_for_del);
    unique_for_del[strlen(sock->recv_Acount)+strlen(sock->send_Account)+1]='\0';
    printf("%s\n",unique_for_del);
    sprintf(buf,"insert into friend values('%s','%s','%s')",sock->recv_Acount,sock->send_Account,unique_for_del);
    printf("加入数据库:%s\n",buf);
    mysql_query(mysql,buf);
    return 1;
}

int del_friend_server(recv_t *sock,MYSQL *mysql)
{
    char buf[256];
    char unique_for_del[64];
    Delete_for_friend_third(sock->recv_Acount,sock->send_Account,unique_for_del);
    unique_for_del[strlen(sock->recv_Acount)+strlen(sock->send_Account)+1]='\0';
    sprintf("delete from friend where del = '%s'",unique_for_del);
    mysql_query(mysql,buf);
    return 1;
}

int List_friends_server(recv_t *sock,MYSQL *mysql) //因为数据库表建的不好　导致查找效率较低
{
    recv_t packet;
    packet.type=LIST_FRIENDS;    //区别与EOF包的差别
    char send_account[MAX_ACCOUNT];  //请求好友列表者　
    strcpy(send_account,sock->send_Account);
    char buf[256];
    sprintf(buf,"select *from friend where account1 = '%s'",sock->send_Account);
    mysql_query(mysql,buf);
    MYSQL_RES *result = mysql_store_result(mysql);
    MYSQL_RES *res=NULL;
    int number=mysql_num_rows(result);
    MYSQL_ROW row,wor;
    //printf("第一遍搜索：%d:\n",number);
    while(number--)//第一遍搜索的好友总数
    {
        row=mysql_fetch_row(result);
        //printf("开始搜索好友！\n");
        sprintf(buf,"select *from Data where Account = '%s'",row[1]);//每一个好友的信息
        //printf("%s\n",buf);
        mysql_query(mysql,buf);
        res=mysql_store_result(mysql);
        wor=mysql_fetch_row(res);
        strcpy(packet.message,wor[3]);//昵称
        strcpy(packet.message_tmp,row[1]);//好友账号
        //printf("%s\n",packet.message); //测试用
        packet.conn_fd=atoi(wor[4]);//是否在线
        packet.send_fd=atoi(wor[5]);//好友套接字
        if((send(sock->send_fd,&packet,sizeof(recv_t),0))<0)
        perror("error in list_friend send\n");
        printf("hello!\n");
    }
    mysql_free_result(result);
    mysql_free_result(res);  //释放一遍空间
    //开始第二遍搜索　数据库表建的不好　不然可以一遍ok的


    sprintf(buf,"select *from friend where account2 = '%s'",sock->send_Account);
    mysql_query(mysql,buf);
    result = mysql_store_result(mysql);
    res=NULL;
    number=mysql_num_rows(result);   //获取好友
    //printf("第二遍搜索：%d:\n",number);
    while(number--)//第二遍搜索的好友总数
    {
        row=mysql_fetch_row(result);

        sprintf(buf,"select *from Data where Account = '%s'",row[0]);//每一个好友的信息
        mysql_query(mysql,buf);
        res=mysql_store_result(mysql);
        wor=mysql_fetch_row(res);
        strcpy(packet.message,wor[3]);//昵称
        strcpy(packet.message_tmp,row[1]);//好友账号
        packet.conn_fd=atoi(wor[4]);//是否在线
        packet.send_fd=atoi(wor[5]);//好友套接字
        if((send(sock->send_fd,&packet,sizeof(recv_t),0))<0)
        perror("error in list_friend send\n");
        printf("hello!\n");
    }
    packet.type=EOF_OF_BOX;//好友消息的结束包
    if((send(sock->send_fd,&packet,sizeof(recv_t),0))<0)
    perror("error in EOF list_friend\n");
    mysql_free_result(result);
    mysql_free_result(res);
    return 1;
}

int *solve(void *arg)
{
    MYSQL mysql;
    mysql_init(&mysql);  //初始化一个句柄
    mysql_library_init(0,NULL,NULL);//初始化数据库
    mysql_real_connect(&mysql,"127.0.0.1","root","lzl213260C","Login_Data",0,NULL,0);//连接数据库
    mysql_set_character_set(&mysql,"utf8");//调整为中文字符
    recv_t *recv_buf=(recv_t *)arg;
    int recv_flag=recv_buf->type;
    switch (recv_flag)
    {
        case LOGIN :
            login(recv_buf,&mysql);
            break;
        case REGISTER :
            register_server(recv_buf,&mysql);
            break;
        case RETRIEVE:
            Retrieve_server(recv_buf,&mysql);
            break;
        case ADD_FRIENDS:
            add_friend_server(recv_buf,&mysql);
            break;
        case ADD_FRIENDS_QUERY:
            add_friend_server_already_agree(recv_buf,&mysql);
            break;
        case DEL_FRIENDS:
            del_friend_server(recv_buf,&mysql);
            break;
        case LIST_FRIENDS:
            List_friends_server(recv_buf,&mysql);
            break;
        default:
            printf("error\n");
            break;
    }
    printf("end of pthread!\n");
    struct epoll_event ev;
    ev.data.fd = recv_buf->conn_fd;
    ev.events = EPOLLIN | EPOLLONESHOT;
    //设置这个的目的是客户端在挂掉以后会发送一个信息　LT模式下没有接到包会不停的发　就会导致服务器epoll收到很多消息
    //解决方案是开始时事件类型改为那三个　然后设置EPOLLONESHOT　一个套接字只接受一次信息　在线程中在加上即可
    epoll_ctl(recv_buf->epfd, EPOLL_CTL_MOD,recv_buf->conn_fd, &ev);
    mysql_close(&mysql);
    free(recv_buf);
}