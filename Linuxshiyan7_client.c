//采用UDP通信的客户端程序
#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include<sys/types.h>
#include<signal.h>

#define MAXSIZE 300
#define SERV_PORT 6666

void handler(int signo)               //捕获信号后的处理程序
{
	switch (signo)
	{
	case SIGINT:
		printf("\n检测到Ctrl+C，无法识别，如果要退出请输入logout.\n");
		break;
	case SIGTSTP:
		printf("\n检测到Ctrl+Z，无法识别，如果要退出请输入logout.\n");
		break;
	case SIGQUIT:
		printf("\n检测到Ctrl+\\，无法识别，如果要退出请输入logout.\n");
		break;
	}
}

int main()
{
	struct sockaddr_in servaddr;
	char message_send[MAXSIZE]; //向服务端发送的消息
	char message_recv[MAXSIZE]; //从服务端接收的消息
	int sockfd;					//套接字文件描述符，类似管道。（套接字对象）
	// if (argc != 2)
	// {
	// 	printf("程序启动缺少用户名!\n");
	// 	exit(-1);
	// }
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); //创建套接字对象，打开网络通信端口
	//设置服务器地址
	bzero(&servaddr, sizeof(servaddr));					       //结构体置零
	servaddr.sin_family = AF_INET;						       //设置协议
	inet_pton(AF_INET, "127.0.0.0.1", &servaddr.sin_addr); //设置IP地址
	servaddr.sin_port = htons(SERV_PORT);				       //设置端口号

	char password[10];
	char username[30];
	char argv[30];
	printf("请输入用户名\n");
	scanf("%s",argv);
	while(getchar()!='\n');
	printf("请输入密码\n");
	int i=0;                                              //计数器
	while(1)              
	{
	fgets(password, sizeof(password), stdin);
	//登陆时，消息显示登陆标签，通过登陆标签来示意给服务端，这是一条登陆指令(消息).
	//而服务端晓得这是登陆指令后，便采取有关登陆的动作.
	//这样做的解决的问题是：1.登陆操作与无法识别操作的混淆，(之前登陆判断是通过已有的用户名，不明确，比如输入一个不存在的用户名，服务器给出的判断是'无法识别指令'，而不是'用户名不存在'.)
	//2.提高了运行效率，减少了不必要的循环。现在只需一条判断语句便可知是否是登陆命令，而不是循环。同时登陆判断独立起来，不与操作指令和未知指令一起判断，提高了判断速度。
	//3.保证了登陆的唯一性，也就是说每个用户登陆成功后，便不可再执行登陆操作。因为客户端登陆验证和之后的发送指令是分离的。
	char login_info[60] = "<LOGIN> ";                        //登陆标签
	strcat(login_info,argv);
	strcat(login_info, " "); //在用户名和密码之间插入一个间隔符号（空格）
	strcat(login_info, password);
	//登陆验证
	sendto(sockfd, login_info, strlen(login_info) + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)); //向服务端请求登陆验证

	recvfrom(sockfd, message_recv, sizeof(message_recv), 0, NULL, 0); //接受服务端发送的信息。
	printf("%s", message_recv); //服务端发送的信息中包含\n和\0.
	if(strcmp(message_recv,"该用户名不存在.\n")==0)
	{
		printf("(已退出)\n");
		exit(-1);
	}
	if(strcmp(message_recv,"登陆成功.\n")==0)       //如果登陆成功
	{
		break;
	}
	//如果密码错误
	if(i == 2)                                   //密码最多输入三次，三次错误后退出
	{
		printf("连续三次输入错误，退出程序!\n");
		exit(-1);
	}
	printf("请重新输入您的密码\n");
	i++;
	}
	//通信阶段
	//双进程，一个进程全负责接受，一个进程负责发送
	pid_t pid;
	pid = fork();
	if(pid == -1)
	{		
		printf("子进程创建失败,程序终止\n");
		exit(-1);
	}
	//接受、发送分离

	
	if(pid == 0)           //子进程入口
	{
		//在多进程程序中发送终端信号应注意的问题
		//这里需要注意一点，就是在终端输入的Ctrl+c会产生SIG_INT信号,该信号会广播发送到连接该终端的每一个进程。
		//简而言之就是，Ctrl+C发送的信号，子父进程都会收到。
		//那么就会产生另一个问题，就是收到信号后，子父进程如何反应？
		//答案就是如果该进程中有捕获函数，则执行捕获函数的内容，否则执行默认操作。
		//再一点说明就是，如果捕获函数安装在共享区域，即没在子父分区里面，则父子进程都可调用执行该捕获函数。
		//如果只在父进程中安装捕获函数，那么后果就是子进程执行默认操作，被终止。
		signal(SIGINT,SIG_IGN); 
		signal(SIGTSTP,SIG_IGN);
		signal(SIGQUIT,SIG_IGN);
		while(1)
		{
			recvfrom(sockfd, message_recv, sizeof(message_recv), 0, NULL, 0); //接受服务端发送的信息
			printf("%s", message_recv);
			if(strcmp(message_recv,"拜拜，欢迎下次再来\n")==0||strcmp(message_recv,"用户名被异地登陆，强制退出\n")==0)//程序唯一出口
				break;
		}
		kill(getppid(),SIGKILL);//子进程直接发送2号信号给父进程，父进程直接终止
	}
	if(pid > 0)            //父进程入口
	{
		signal(SIGINT,handler);        //关于signal()捕获函数为啥放在这？见上文。
		signal(SIGTSTP,handler);
		signal(SIGQUIT,handler);
		//while (fgets(message_send, sizeof(message_send), stdin) != NULL）&& strncmp(message_send, "quit", 4) != 0)
		while(1)
		{
			/* 存在一个问题，就是必须输入一个回车才能结束程序，以为父进程会阻塞在fgets（）那里
			pid_t p = waitpid(pid,NULL,WNOHANG);//以非阻塞的方式试探子进程是否终止。子进程终止，父进程给子进程收尸后，紧跟着也终止。
			//相当于向服务端发送logout后，服务端发送终止客户端的消息，客户端终止。反映到客户端程序上即服务端发送消息给客户端，子进程受到后，子进程终止。父进程探测到子进程终止后，立马给子进程收尸，随后也终止，客户端结束.
			if(p>0)          //如果子进程终止，即输入logout后，服务端发送退出消息给子进程。
			{
				break;
			}
			*/
			fgets(message_send,sizeof(message_send),stdin);
			sendto(sockfd, message_send, strlen(message_send) + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)); //将需求发送给服务端
		}
		
	}
	close(sockfd);
	return 0;
}
