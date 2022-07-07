//服务端程序,采用UDP通信(无连接)
#include <sys/socket.h> //包含网络编程的全部接口
#include <netinet/in.h> //包含宏INET_ADDRSTRLEN 大小为16，正好盛下IPv4的地址
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h> //包含IP地址的结构体与字符串之间的转换函数
#include <unistd.h>
#include <string.h>
#include<sys/wait.h>
#include<pthread.h> 

#define USER_COUNT 20  //最大用户数量
#define MAXSIZE 300	   //字符数组最大长度
#define SERV_PORT 6666 //服务器端口号
#define INS_LEN 10	   //指令最大长度
#define LOGIN 1		   //登陆标识
#define OPERATION 2	   //操作标识
#define INITIAL 0	   //标志的初始值0
#define INS_COUNT 3	   //指令数量

typedef struct                   //用户结构体
{
	char *username;				 //用户名
	char *password;				 //密码
	int mark;					 //仅仅是用来判断用户是否存在，用于判断当前用户的数量
	struct sockaddr_in useraddr; //用户IP/端口地址,用户一般使用sockaddr_in来设置套接字地址，sockaddr一般是操作系统使用。
	int mode;					 //记录用户状态 0~下线,1~上线
} User;
typedef struct
{
	int sock;
	char* mesg;
	struct sockaddr_in socaddr;
}pthreadType; 
User UserInfos[USER_COUNT] = {{"寂寞的忧伤", "123", 1}, {"暴龙战士", "12345", 1}, {"阿龙", "qwer", 1}, {"仙女小樱", "yyy", 1},{"xiaozhang","123",1}}; //用户列表,最多20个用户
char *Instructions[INS_COUNT] = {"to", "logout", "toall"};																		//该客户端共有三条指令
int user_current_count = 0;      //当前的用户数量

int decompose_string(char *str, char (*words)[MAXSIZE]);												  //分解字符串
void process_instructions(int sockfd, char (*instructions)[MAXSIZE], const struct sockaddr_in *recvaddr); //指令处理函数
void sendmessage_toall(int sockfd,int i,const char* mesg);//群发消息, i~标记，表示不给UserInfos[i]用户发消息，一般将i赋值为当前用户的i值。如果将i置为-1，则将消息发送给所有在线用户。
void* threadFunc(void* arg)
{
	pthreadType* pt = (pthreadType*)arg;
	char client_ins[3][MAXSIZE];          //将客户端发送的消息分解。用于之后的消息处理.
	pthread_detach(pthread_self());       //将线程置于分离态
	decompose_string(pt->mesg, client_ins);
	process_instructions(pt->sock, client_ins, &(pt->socaddr));
	pthread_exit(NULL);
}

int main()
{
	//前期处理工作

	int sockfd;							  //定义一个套接字，用于保存socket()的返回值
	struct sockaddr_in servaddr, cliaddr; //定义一个服务端地址结构体和客户端地址结构体
	//服务端地址结构用于绑定，客户端地址结构在recvfrom(int sockfd, void * buff,size_t len,int flag,struct sockaddr * arc,socklen_t * addrlen)中用于将客户端地址信息返回。其中最后一个参数为传入传出参数。
	socklen_t cliaddr_len; //传入传出参数——客户端地址长度,由于sockaddr*能接收各种结构体，所以加入socklen_t类型的参数（即结构体的长度）来区分是哪种结构体。

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);	  //以UDP方式创建套接字对象，并将套接字文件描述符返回给sockfd，末尾参数一般为0，表示使用默认协议
	bzero(&servaddr, sizeof(servaddr));			  //将服务器地址结构体置零
	servaddr.sin_family = AF_INET;				  //指定协议族
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //指定IP地址，INADDR_ANY~本机所有的IP地址。
	servaddr.sin_port = htons(SERV_PORT);		  //指定端口号

	bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)); //绑定服务器,将服务器的IP地址以及端口号固定。

	printf("Accepting connections...\n");

	//启动服务
	for (int i = 0; UserInfos[i].mark != 0; i++)
		user_current_count++; //计算当前用户数量

	char buf[MAXSIZE]; //用于接收客户端的消息

	int n;	  //用于保存recvfrom()的返回值,返回值为实际接受的字节数。
	while (1) //服务器程序一般不会终止
	{
		pthread_t mypthread;
		int ret;
		cliaddr_len = sizeof(cliaddr); //将cliaddr_len的赋值语句写在这，是因为每次循环前首先将其复原为sockaddr结构体的最大长度
		n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&cliaddr, &cliaddr_len); // flag=0表示recvfrom为阻塞方式接受数据。
		if (n == -1)
		{
			printf("接收出错，丢弃\n");
			continue;
		}
		pthreadType threadArg;
		threadArg.mesg = buf;
		threadArg.sock = sockfd;
		threadArg.socaddr = cliaddr;
		ret = pthread_create(&mypthread,NULL,threadFunc,(void*)&threadArg);
		if(ret != 0)
		{
			printf("线程创建失败！\n");
			break;
		}
		//decompose_string(buf, client_ins);
		//process_instructions(sockfd, client_ins, &cliaddr);
	}
	close(sockfd);
	return 0;
}
int decompose_string(char *str, char (*words)[MAXSIZE]) //分解字符串,words是数组指针，指向一整个数组。
{
	char *p = str;
	int i = 0, j = 0;

	while (*p != ' ' && *p != '\n') //吸收第一个单词，这个至关重要
	{								//第一个单词决定了客户端的请求是登陆请求还是发送请求
		words[i][j] = *p;
		p++;
		j++;
	}
	words[i][j] = '\0'; //是字符数组转化为字符串
	i++;				//将words指向下一个单词
	j = 0;				//为吸收下一个单词做好准备
	if (*p == '\n')
		return i; //返回单词个数

	while (*p == ' ')
		p++; //跳过空格，为吸收下一个字符串做好准备

	if (strcmp(words[0], "to") == 0) //如果首单词为"to"
	{
		while (*p != ' ')
		{
			words[i][j] = *p;
			p++;
			j++;
		}
		words[i][j] = '\0';
		while (*p == ' ')
			p++; //跳过空格，为吸收下一个字符串做好准备
		i++;
		j = 0; //为吸收下一个单词做好准备
	}
	while (*p != '\n') //吸收发向所有用户的消息
	{
		words[i][j] = *p;
		p++;
		j++;
	}
	words[i][j] = '\0';
	i++;
	return i; //返回单词个数
}

void process_instructions(int sockfd, char (*instructions)[MAXSIZE], const struct sockaddr_in *recvaddr) //指令处理函数
{
	char(*pp)[MAXSIZE] = instructions;

	int flag = INITIAL; //标志指令的属性
	int i = 0, j = 0;

	for (i = 0; i < user_current_count; i++) //比对首单词是不是用户名
	{
		if (strcmp(UserInfos[i].username, pp[0]) == 0) //匹配登陆用户的信息
		{
			flag = LOGIN; //将标志设为登陆
			goto x;
		}
	}

	for (j = 0; j < INS_COUNT; j++)
	{
		if (strcmp(Instructions[j], pp[0]) == 0) //匹配指令
		{
			flag = OPERATION; //将标志设为操作。
			goto x;
		}
	}

x:
	switch (flag)
	{
	case LOGIN: //处理登陆操作
		if (strcmp(UserInfos[i].password, pp[1]) != 0 || UserInfos[i].mode == 1)
		{
			printf("用户%s登陆异常\n", UserInfos[i].username);
			sendto(sockfd, "登陆失败，密码错误或该用户已登陆\n", sizeof("登陆失败，密码错误或该用户已登陆\n"), 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr));
			break;
		}
		char str_ip[INET_ADDRSTRLEN]; //长度正好是IPv4的字符串长度正好16.
		//将用户的地址存入结构体中
		UserInfos[i].useraddr = *recvaddr; //将用户的地址存入结构体中,再输出地址之前，必须先初始化UserInfos[i].useraddr.否则第一次输出的ip地址和端口号都是0
		UserInfos[i].mode = 1;			   //标志用户已上线
		printf("用户:%s已登陆,IP地址:%s,端口号:%d\n", UserInfos[i].username,
			   inet_ntop(AF_INET, &UserInfos[i].useraddr.sin_addr, str_ip, sizeof(str_ip)), //导出客户端的IP地址
			   ntohs(UserInfos[i].useraddr.sin_port));										//导出客户端的端口号
		sendto(sockfd, "登陆成功\n", sizeof("登陆成功\n"), 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr));

		//向其他在线用户发送该用户以上线
		char online_reminder[MAXSIZE];
		sprintf(online_reminder, "%s已上线,快和TA聊聊吧\n", UserInfos[i].username);
		sendmessage_toall(sockfd,i,online_reminder);
		/*for (int k = 0; k < user_current_count; k++)
		{
			if (k != i && UserInfos[k].mode == 1)
			{
				sendto(sockfd, online_reminder, strlen(online_reminder) + 1, 0, (struct sockaddr *)&UserInfos[k].useraddr, sizeof(UserInfos[k].useraddr)); //发送消息到在线用户
			}
		}*/
		break;
	case OPERATION: //处理发送消息和下线的指令需求
	{
		//用源用户的IP地址+端口号求出发送者的用户名
		char username[20];			  //源用户名，即发送者的名字。
		char sec_ip[INET_ADDRSTRLEN]; //存放源用户信息中的ip地址
		char det_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &recvaddr->sin_addr, sec_ip, sizeof(sec_ip)); //将ip地址转化为字符串。
		int sec_port = ntohs(recvaddr->sin_port);						 //导出源用户信息中的端口号
		int i;
		for (i = 0; i < user_current_count; i++)
			if (ntohs(UserInfos[i].useraddr.sin_port) == sec_port && strcmp(inet_ntop(AF_INET, &UserInfos[i].useraddr.sin_addr, det_ip, sizeof(det_ip)), sec_ip) == 0)
			{
				strcpy(username, UserInfos[i].username);
				break;
			}

		char suffix[40];
		sprintf(suffix, "  by %s\n", username); //消息末尾的署名后缀
		switch (j)
		{
		case 0: //关于指令"to",发送消息给指定用户
		{
			char *error_send[2] = {"用户不存在\n", "用户不在线\n"};
			int ii;
			for (ii = 0; ii < user_current_count; ii++) //查找目标用户，UserInfos[ii]表示的目标用户的用户信息
			{
				if (strcmp(UserInfos[ii].username, pp[1]) == 0)
					break;
			}

			if (ii == user_current_count) //发送的用户不存在
			{
				sendto(sockfd, error_send[0], strlen(error_send[0]) + 1, 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr)); //发送错误信息到客户端
				break;
			}
			else if (UserInfos[ii].mode == 0)
			{
				sendto(sockfd, error_send[1], strlen(error_send[0]) + 1, 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr)); //发送错误信息到客户端
				break;
			}
			//用户在线,发送信息
			strcat(pp[2], suffix);																								   //将后缀连接在消息后面
			sendto(sockfd, pp[2], strlen(pp[2]) + 1, 0, (struct sockaddr *)&UserInfos[ii].useraddr, sizeof(UserInfos[ii].useraddr)); //给目标用户发送信息
			break;
		}
		case 1: //关于指令"logout",退出登陆
			printf("用户：%s已下线\n", username);
			UserInfos[i].mode = 0;
			char str[30];
			sendto(sockfd, "拜拜，欢迎下次再来\n", sizeof("拜拜，欢迎下次再来\n"), 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr));
			char logout_mesg[60];
			sprintf(logout_mesg,"%s已下线,下次再找TA聊吧\n",username);
			sendmessage_toall(sockfd,i,logout_mesg);
			break;
		case 2: //关于指令"toall",发送消息给所有在线用户
			strcat(pp[1], suffix);
			sendmessage_toall(sockfd,i,pp[1]);
			// for (int k = 0; k < user_current_count; k++)
			// {
			// 	if (k != i && UserInfos[k].mode == 1)
			// 	{
			// 		sendto(sockfd, pp[1], strlen(pp[1]) + 1, 0, (struct sockaddr *)&UserInfos[k].useraddr, sizeof(UserInfos[k].useraddr));
			// 		//发送消息到在线用户
			// 	}
			// }
			break;
		}
		break;
	}
	case INITIAL:
		printf("登陆用户名错误或操作指令无法识别\n");
		sendto(sockfd, "登陆用户名错误或操作指令无法识别\n", sizeof("登陆用户名错误或操作指令无法识别\n"), 0, (struct sockaddr *)recvaddr, sizeof(*recvaddr));
		break;
	}
}
void sendmessage_toall(int sockfd,int i,const char* mesg)       
{
	for (int k = 0; k < user_current_count; k++)
			{
				if (k != i && UserInfos[k].mode == 1)
				{
					sendto(sockfd, mesg, strlen(mesg) + 1, 0, (struct sockaddr *)&UserInfos[k].useraddr, sizeof(UserInfos[k].useraddr));
					//发送消息到在线用户
				}
			}
}
