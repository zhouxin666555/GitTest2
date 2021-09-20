#include <iostream>
#include "Client.h"

using namespace std;
//构造函数
Client::Client()
{
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

	sock = 0;
	pid = 0;
	isClientWork = true;
	epfd = 0;
}

//完成与客户端的连接
void Client::Connect()
{
	cout<<"Connect Server: "<<SERVER_IP<<" : "<<SERVER_PORT<<endl;

	sock = socket(PF_INET,SOCK_STREAM,0);
	if(sock < 0)
	{
		perror("sock error");
		exit(-1);
	}

	//与服务器进行连接
	if( connect(sock,(struct sockaddr*)&serverAddr,sizeof(serverAddr)) < 0 )
	{
		perror("connect error");
		exit(-1);
	}

	//创建管道
	if(pipe(pipe_fd) < 0)
	{
		perror("pipe error");
		exit(-1);
	}

	//创建监听红黑树
	epfd = epoll_create(EPOLL_SIZE);
	if(epfd < 0)
	{
		perror("epfd error");
		exit(-1);
	}


	//将套接字sock与管道读端添加到监听树上
	addfd(epfd,sock,true);
	addfd(epfd,pipe_fd[0],true);
}

//关闭相应套接字以及管道读写端
void Client::Close()
{
	if(pid)
	{
		close(pipe_fd[0]);
		close(sock);
	}
	else
	{
		close(pipe_fd[1]);
	}
}

//客户端运作函数
void Client::Start()
{

	//定义结构体数组存取事件(服务器+管道读端)
	static struct epoll_event events[2];

	//与服务器进行连接
	Connect();

	//创建子进程
	pid = fork();

	if(pid < 0)
	{
		perror("fork error");
		close(sock);
		exit(-1);
	}

	//子进程（负责得到用户的输入内容）
	else if(pid == 0)
	{
		//关闭读端
		close(pipe_fd[0]);

		cout<<"Please input 'exit' to exit the chat room"<<endl;
		cout<<"\\ + ClientId to private chat"<<endl;//私聊提示

		//客户端有事件发送给服务器
		while(isClientWork)
		{
			//从终端输入消息
			memset(msg.content,0,sizeof(msg.content));
			fgets(msg.content,BUF_SIZE,stdin);

			//如果输入的是exit则代表退出
			if(strncasecmp(msg.content,EXIT,strlen(EXIT)) == 0)
			{
				isClientWork = false;
			}
			//若输入的是消息内容
			else
			{
				memset(send_buf,0,BUF_SIZE);
				memcpy(send_buf,&msg,sizeof(msg));
				//将信息写到管道写端
				if( write(pipe_fd[1],send_buf,sizeof(send_buf)) < 0 )
				{
					perror("fork error");
					exit(-1);
				}
			}
		}
	}
	//父进程（负责与服务器完成通信）
	else
	{
		close(pipe_fd[1]);
		while(isClientWork)
		{
			//获取监听总个数（-1：阻塞方式）
			int epoll_events_count = epoll_wait(epfd,events,2,-1);

			for(int i=0; i < epoll_events_count; i++)
			{
				memset(recv_buf,0,sizeof(recv_buf));
				//如果是与服务器连接的事件，接收服务器信息将其显示
				if(events[i].data.fd == sock)
				{
					int ret = recv(sock,recv_buf,BUF_SIZE,0);
					memset(&msg,0,sizeof(msg));
					memcpy(&msg,recv_buf,sizeof(msg));

					if(ret == 0)
					{
						cout<<"Server closed connection: "<<sock<<endl;
						close(sock);
						isClientWork = false;

					}
					else
					{
						cout<<msg.content<<endl;
					}
				}
				//如果是子进程管道事件，将其发送到服务器
				else
				{
					int ret = read(events[i].data.fd,send_buf,BUF_SIZE);
					if(ret == 0)
					{
						isClientWork = false;
					}
					else
					{
						send(sock,send_buf,sizeof(send_buf),0);
					}
				}
			}
		}
	}
	Close();
}



