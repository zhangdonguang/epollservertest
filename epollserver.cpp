    #include  <stdio.h>   
    #include  <iostream>   
    #include  <unistd.h>   
    #include  <fcntl.h>   
    #include  <errno.h>  
    #include  <sys/types.h>  
    #include  <sys/socket.h>   
    #include  <sys/epoll.h>   
    #include  <netinet/in.h>   
    #include  <netinet/tcp.h>   
    #include  <netinet/ip.h>   
    #include  <arpa/inet.h> 
    #include  <string.h>
    #include  <inttypes.h>
   
    #define BUF_SIZE 1024 
    using namespace std;    
  
    class epoll_server  
    {  
        public:  
            epoll_server();  
            virtual ~epoll_server();  
            bool init(int port, int sock_count);  
            bool init(const char* ip, int port, int sock_count);  
            int epoll_server_wait(int time_out);  
            int accept_new_client();  
            int recv_data(int sock, uint8_t* recv_buf);  
            int send_data(int sock, uint8_t* send_buf, int buf_len);  
            void run(int time_out);  
      
        private:  
            int m_listen_sock;  
            int m_epoll_fd;  
            int m_max_count;
            int m_client_count;
            struct epoll_event *m_epoll_events;  
            uint8_t *m_recv_buf;// = new char[65535];  
            uint8_t *m_send_buf;// = new char[65535];  
            uint32_t m_send_count;
            uint32_t m_recv_count;
 
    };  
      
    epoll_server::epoll_server()  
    {  
        m_listen_sock = 0;  
        m_epoll_fd = 0;  
        m_max_count = 0;  
        m_client_count = 0;
        m_send_count=0;
        m_recv_count=0;
        m_epoll_events = NULL;  
        m_recv_buf = new uint8_t[65535];  
        m_send_buf = new uint8_t[65535];  
 
    }  
      
    epoll_server::~epoll_server()  
    {  
        if (m_listen_sock > 0)  
        {  
            close(m_listen_sock);  
        }  
      
        if (m_epoll_fd > 0)  
        {  
            close(m_epoll_fd);  
        } 
        m_client_count = 0;
        delete [] m_recv_buf;
        delete [] m_send_buf;
        delete [] m_epoll_events;
    }  
      
      
    bool epoll_server::init(int port , int sock_count)  
    {  
		int nRecvBuf=16*1024*1024;
        m_max_count = sock_count;     
        struct sockaddr_in server_addr;  
        memset(&server_addr, 0, sizeof(&server_addr));  
        server_addr.sin_family = AF_INET;  
        server_addr.sin_port = htons(port);  
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);          
      
        m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);  
        if(m_listen_sock == -1)  
        {  
            return (1);  
        }  
// set socket opt
        int keepAlive = 1; // 开启keepalive属性
        int keepIdle = 60; // 如该连接在60秒内没有任何数据往来,则进行探测
        int keepInterval = 5; // 探测时发包的时间间隔为5 秒
        int keepCount = 3; // 探测尝试的次数.如果第1次探测包就收到响应了,则后2次的不再发
        setsockopt(m_listen_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive));
        setsockopt(m_listen_sock, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
        setsockopt(m_listen_sock, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
        setsockopt(m_listen_sock, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));
		setsockopt(m_listen_sock, SOL_SOCKET,SO_RCVBUF,(const char * )&nRecvBuf,sizeof(int));
// set socket opt
        if(bind(m_listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)  
        {  
            return (1);  
        }  
          
        if(listen(m_listen_sock, 5) == -1)  
        {  
            return (1);  
        }  
      
        m_epoll_events = new struct epoll_event[sock_count];  
        if (m_epoll_events == NULL)  
        {  
            return (1);  
        }  
      
        m_epoll_fd = epoll_create(sock_count);  
        struct epoll_event ev;  
        ev.data.fd = m_listen_sock;  
        ev.events  = EPOLLIN|EPOLLERR;  
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_sock, &ev);  
    }  
      
    bool epoll_server::init(const char* ip, int port , int sock_count)  
    {     
       #if 0 
        m_max_count = sock_count;  
        struct sockaddr_in server_addr;  
        memset(&server_addr, 0, sizeof(&server_addr));  
        server_addr.sin_family = AF_INET;  
        server_addr.sin_port = htons(port);  
        server_addr.sin_addr.s_addr = inet_addr(ip);          
      
        m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);  
        if(m_listen_sock == -1)  
        {  
            return (1);  
        }  
          
        if(bind(m_listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)  
        {  
            return (1);  
        }  
          
        if(listen(m_listen_sock, 5) == -1)  
        {  
            return (1);  
        }  
      
        m_epoll_events = new struct epoll_event[sock_count];  
        if (m_epoll_events == NULL)  
        {  
            return(1);  
        }  
      
        m_epoll_fd = epoll_create(sock_count);  
        struct epoll_event ev;  
        ev.data.fd = m_listen_sock;  
        ev.events  = EPOLLIN;  
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_listen_sock, &ev);  
        #endif
    }  
      
    int epoll_server::accept_new_client()  
    {  
        sockaddr_in client_addr;  
        memset(&client_addr, 0, sizeof(client_addr));  
        socklen_t clilen = sizeof(struct sockaddr);   
        int new_sock = accept(m_listen_sock, (struct sockaddr*)&client_addr, &clilen); 
        if(new_sock==-1)
        {
           if( errno==EAGAIN)
           {
               cout<<"error"<<strerror(errno)<<endl; 
           }
           else if( errno==EINTR)
           {
               cout<<"error"<<strerror(errno)<<endl; 
           }
        }
        if(m_client_count<m_max_count)
        {
           m_client_count+=1;
           cout<<"client count:"<<m_client_count<<" max_count:"<<m_max_count<<endl;
        }
        else
        {
           close(new_sock);
           new_sock=-1;
           return new_sock;
        }
        struct epoll_event ev;  
        ev.data.fd = new_sock;  
        ev.events  = EPOLLIN|EPOLLERR|EPOLLRDHUP;  
        epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);  
        return new_sock;  
    }  
      
    int epoll_server::recv_data(int sock, uint8_t* recv_buf)  
    {  
        uint8_t buf[BUF_SIZE] = {0};  
        int len = 0;  
        int ret = 0;  
        while(ret >= 0)  
        {  
            ret = recv(sock, buf, BUF_SIZE, 0);  
            if(ret <= 0)  
            {  
                struct epoll_event ev;  
                ev.data.fd = sock;  
                ev.events  = EPOLLERR|EPOLLRDHUP;  
                epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, sock, &ev);  
                close(sock); 
                cout<<"client close ,normal close!"<<endl;
                if(m_client_count>0) m_client_count-=1;                           
                cout<<"client count:"<<m_client_count<<" max_count:"<<m_max_count<<endl;
                break;  
            }  
            else if(ret < BUF_SIZE)  
            {  
                memcpy(recv_buf, buf, ret);  
                len += ret;   
                struct epoll_event ev;  
                ev.data.fd = sock;  
                ev.events  = EPOLLOUT;  
                epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, sock, &ev);  
                break;  
            }  
            else  
            {  
                memcpy(recv_buf, buf, BUF_SIZE);  
                len += ret;  
            }  
        }  
        
        return ret <= 0 ? ret : len;  
    }  
      
    int epoll_server::send_data(int sock, uint8_t* send_buf, int buf_len)  
    {  
        int len = 0;  
        int ret = 0;  
        int send_len=buf_len;
        while(len < buf_len)  
        {  
            send_len=send_len-len;
            ret = send(sock, send_buf + len,send_len, 0);  
            if(ret <= 0)  
            {  
                struct epoll_event ev;  
                ev.data.fd = sock;  
                ev.events  = EPOLLERR|EPOLLRDHUP;  
                epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, sock, &ev);  
                close(sock);  
                if(m_client_count>0) m_client_count-=1;                          
                cout<<"client count:"<<m_client_count<<" max_count:"<<m_max_count<<endl; 
                break;  
            }  
            else  
            {  
                len += ret;  
            }  
      
            if(send_len<=0)  
            {  
                break;  
            }  
        }  
      
        if(ret > 0)  
        {  
            struct epoll_event ev;  
            ev.data.fd = sock;  
            ev.events  = EPOLLIN;  
            epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, sock, &ev);  
        }  
      
        return ret <= 0 ? ret : len;  
    }  
      
    int epoll_server::epoll_server_wait(int time_out)  
    {  
        int nfds = epoll_wait(m_epoll_fd, m_epoll_events, m_max_count, time_out);  
    }  
      
    void epoll_server::run(int time_out)  
    {  
        while(1)  
        {  
            int ret = epoll_server_wait(time_out);  
            if(ret == 0)  
            {  
                cout<<"time out: "<<time_out<<"ms"<<endl;  
                continue;  
            }  
            else if(ret == -1)  
            {  
                if(errno==EINTR)
                { 
                    cout<<"error"<<strerror(errno)<<endl; 
                    continue;
                }
            }  
            else  
            {  
                for(int i = 0; i < ret; i++)  
                {  
                    if(m_epoll_events[i].data.fd == m_listen_sock)  
                    { 
                        if(m_epoll_events[i].events &EPOLLERR)
                        {
                            cout<<"error"<<strerror(errno)<<endl; 
                            continue;
                        } 
                        if(m_epoll_events[i].events & EPOLLIN)  
                        {  
                            int new_sock = accept_new_client(); 
                        }
                    }  
                    else  
                    {  
                        if(m_epoll_events[i].events&EPOLLERR || m_epoll_events[i].events&EPOLLRDHUP)
                        {
                            int sock=0;
                            sock=m_epoll_events[i].data.fd;
                            struct epoll_event ev;  
                            ev.data.fd = sock;  
                            ev.events  = EPOLLERR|EPOLLRDHUP;  
                            epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, sock, &ev);  
                            close(sock);
                            if(m_epoll_events[i].events&EPOLLERR)
                               cout<<"error close socket! "<<strerror(errno); 
                            if(m_epoll_events[i].events&EPOLLRDHUP) 
                               cout<<" ,remote close socket! "<<strerror(errno)<<endl; 
                            if(m_client_count>0) 
                               m_client_count-=1; 
                            cout<<"client count:"<<m_client_count<<" max_count:"<<m_max_count<<endl;
                            continue;
                        }
                        if(m_epoll_events[i].events & EPOLLIN)  
                        {  
                            int recv_count = recv_data(m_epoll_events[i].data.fd, m_recv_buf);  
                            //cout<<m_recv_buf<<endl;  
                            m_recv_count=m_send_count=recv_count;
                            memcpy(m_send_buf, m_recv_buf,recv_count);  
                            memset(m_recv_buf, 0, m_recv_count); 
                            m_recv_count=0;
                        }  
                        else if(m_epoll_events[i].events & EPOLLOUT)  
                        {  
                            int send_count = send_data(m_epoll_events[i].data.fd, m_send_buf, m_send_count);  
                            memset(m_send_buf, 0, sizeof(m_send_buf));  
                            m_send_count=0;
                        }  
                          
                    }  
                }  
            }  
        }  

    }  
      
    int main(int argc, char *argv[])  
    {  
        epoll_server tcp_server;  
        tcp_server.init(8008, 10);  
        //tcp_server.init("127.0.0.1", 12345, 10);  
        tcp_server.run(2000);  
        return  0;  
    }
