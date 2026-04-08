#include "protocol.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int register_connect(int fd,Port *port);
int client_init(struct addrinfo **t,char *SERV,char *PORT);
int udp_init(Port*port);
void* handle_requests(void*args);
void* handle_udp(void*args);
int main(int argc,char *argv[])
{
  if(argc<3)
    {
      printf("usage: ./client IP PORT\n");
      return -1;
    }
  char ip[32];
  char port[16];
  strcpy(ip,argv[1]);
  strcpy(port,argv[2]);
  struct addrinfo *p;
  int tcp_sock = client_init(&p,ip,port);
  printf("%d\n",tcp_sock);
  Port *port_udp= malloc(sizeof(Port));
  register_connect(tcp_sock,port_udp);
  int udp_sock=udp_init(port_udp);
  free(port_udp);
  if(udp_sock<0)
    {
      printf("port already in use or udp_init failed\n");
      return -1;
    }
  pthread_t udp_thread,req_thread;
  if(pthread_create(&req_thread,NULL,handle_requests,&tcp_sock)!=0)
    {
      printf("thread creating failed\n");
      return -1;
    }
  if(pthread_create(&udp_thread,NULL,handle_udp,&udp_sock)!=0)
    {
      printf("thread creating failed\n");
      return -1;
    }
  pthread_join(req_thread,NULL);
  pthread_cancel(udp_thread);
  pthread_join(udp_thread,NULL);
  close(tcp_sock);
  close(udp_sock);
  freeaddrinfo(p);
  printf("Disconnected!\n");
}

void* handle_requests(void*arg)
{
  int sock = *(int*)arg;
  while(1)
    {
      printf("Write [LIST?] to see online users\n");
      printf("Write [IQUIT] to disconnect\n");
      printf("Write [FRIE?] to add a friend\n");
      printf("Write [CONSU] to read a stream\n");
      printf("Write [MESS?] to write a message to a friend\n");
      printf("Write [FLOO?] to write a flood message\n");
      char input_line[7];
      if(fgets(input_line,sizeof(input_line),stdin)==NULL)
	{
	  printf("Error reading command\n");
	  continue;
	}
      input_line[strcspn(input_line,"\n")]='\0';
      if(strncmp(input_line,"LIST?",COM_LEN)==0)
	{
	  TCP_PACK buf;
	  sprintf(buf,"LIST?+++");
	  write(sock,buf,strlen(buf));
	  char read_buf[COM_LEN+8];
	  int n = read(sock,read_buf,COM_LEN+7);
	  read_buf[n]='\0';
	  char count[4];
	  count[3]='\0';
	  strncpy(count, read_buf+6, 3);
	  int num_users = atoi(count);
	  printf("Online users are: \n");
	  for (int i = 0; i<num_users;++i)
	    {
	      char linum_buf[COM_LEN+ID_LEN+5];
	      read(sock,linum_buf,COM_LEN+ID_LEN+4);
	      linum_buf[COM_LEN+ID_LEN+4]='\0';

	      char user_id[9];
	      strncpy(user_id,linum_buf+COM_LEN+1,ID_LEN);
	      user_id[ID_LEN]='\0';
	      printf("%d: %s\n",i,user_id);
	    }
	}
      else if(strncmp(input_line,"IQUIT",COM_LEN)==0)
	{
	  TCP_PACK buf;
	  sprintf(buf,"IQUIT+++");
	  write(sock,buf,strlen(buf));
	  char read_buf[COM_LEN+4];
	  int n = read(sock,read_buf,COM_LEN+3);
	  read_buf[n]='\0';
	  printf("%s\n",read_buf);
	  return NULL;
	}
      else if(strncmp(input_line,"FRIE?",COM_LEN)==0)
	{
	  char buf[10];
	  printf("Enter Id 8 exactly 8 chars\n");
	  if(fgets(buf,sizeof(buf),stdin)==NULL)
	    {
	      printf("Error reading Id\n");
	      continue;
	    }
	  buf[strcspn(buf,"\n")]='\0';
	  Id id;
	  strcpy(id,buf);
	  TCP_PACK send_pack;
	  memset(send_pack, 0, MAX_TCP_LEN + 1);
	  snprintf(send_pack,sizeof(send_pack),"FRIE? %s+++",id);
	  write(sock,send_pack,strlen(send_pack));
	  char readbuf[COM_LEN+4];
	  read(sock,readbuf,COM_LEN+3);
	  readbuf[COM_LEN+3]='\0';
	  printf("%s\n",readbuf);
	}
      else if(strncmp(input_line,"MESS?",COM_LEN)==0)
	{
	  char buf[10];
	  printf("Enter destination Id 8 exactly 8 chars\n");
	  if(fgets(buf,sizeof(buf),stdin)==NULL)
	    {
	      printf("Error reading Id\n");
	      continue;
	    }
	  buf[strcspn(buf,"\n")]='\0';
	  Id id;
	  strcpy(id,buf);
	  char msg_buf[MSG_LEN+2];
	  Mess mess;
	  printf("enter message maximum 200 chars\n");
	  if(fgets(msg_buf,sizeof(msg_buf),stdin)==NULL)
	    {
	      printf("Error reading message\n");
	      continue;
	    }
	  msg_buf[strcspn(msg_buf,"\n")]='\0';
	  strncpy(mess,msg_buf,sizeof(Mess));
	  mess[MSG_LEN]='\0';
	  TCP_PACK pack;
	  sprintf(pack,"MESS? %s %s+++",id,mess);
	  write(sock,pack,strlen(pack));
	  TCP_PACK recv;
	  read(sock,recv,COM_LEN+3);
	  recv[COM_LEN+3]='\0';
	  printf("%s\n",recv);
	  
	}
      else if(strncmp(input_line,"FLOO?",COM_LEN)==0)
	{
	  char msg_buf[MSG_LEN+2];
	  Mess mess;
	  printf("enter message maximum 200 chars\n");
	  if(fgets(msg_buf,sizeof(msg_buf),stdin)==NULL)
	    {
	      printf("Error reading message\n");
	      continue;
	    }
	  msg_buf[strcspn(msg_buf,"\n")]='\0';
	  strncpy(mess,msg_buf,sizeof(Mess));
	  mess[MSG_LEN]='\0';
	  TCP_PACK pack;
	  sprintf(pack,"FLOO? %s+++",mess);
	  write(sock,pack,strlen(pack));
	  TCP_PACK recv;
	  read(sock,recv,COM_LEN+3);
	  recv[COM_LEN+3]='\0';
	  printf("%s\n",recv);
	  
	}
      else if(strncmp(input_line,"CONSU",COM_LEN)==0)
	{
	  TCP_PACK buf;
	  sprintf(buf,"CONSU+++");
	  write(sock,buf,strlen(buf));
	  TCP_PACK rec;
	  int n = read_by_char(sock,rec);
	  if(n<0)
	    {
	      printf("error reading from server");
	      continue;
	    }
	  if(strncmp(rec,"SSEM>",COM_LEN)==0)
	    {
	      Id id;
	      strncpy(id,rec+COM_LEN+1,ID_LEN);
	      id[ID_LEN]='\0';
	      Mess mess;
	      strcpy(mess,rec+COM_LEN+1+ID_LEN+1);
	      mess[strcspn(mess,"+++")]='\0';
	      printf("%s: %s\n",id,mess);
	      
	    }
	  if(strncmp(rec,"OOLF>",COM_LEN)==0)
	    {
	      Id id;
	      strncpy(id,rec+COM_LEN+1,ID_LEN);
	      id[ID_LEN]='\0';
	      Mess mess;
	      strcpy(mess,rec+COM_LEN+1+ID_LEN+1);
	      mess[strcspn(mess,"+++")]='\0';
	      printf("[FLOOD] %s: %s\n",id,mess);
	      
	    }
	  if(strncmp(rec,"EIRF>",COM_LEN)==0)
	    {
	      while(1)
		{
		  Id id;
		  strncpy(id,rec+COM_LEN+1,ID_LEN);
		  printf("%s wants to be your friend\n for accepting type [OKIRF] if not [NOKRF]\n",id);
		  TCP_PACK friend_resp;
		  char buf[7];
		  if(fgets(buf,sizeof(buf),stdin)==NULL)
		    {
		      printf("Error reading command\n");
		      continue;
		    }
		  buf[strcspn(buf,"\n")]='\0';
		  sprintf(friend_resp,"%s+++",buf);
		  write(sock,friend_resp,strlen(friend_resp));
		  TCP_PACK server_resp;
		  int n = read(sock,server_resp,COM_LEN+3);
		  if(n>=0)
		    {
		      server_resp[n]='\0';
		    }
		  printf("%s\n",server_resp);
		  break;
		}
	    }
	  else if(strncmp(rec,"FRIEN",COM_LEN)==0)
	    {
	      Id id;
	      memset(id,0,sizeof(Id));
	      strncpy(id,rec+COM_LEN+1,ID_LEN);
	      id[ID_LEN]='\0';
	      printf("%s has accepted your friend request!\n",id);
	    }
	  else if(strncmp(rec,"NOFRI",COM_LEN)==0)
	    {
	      Id id;
	      memset(id,0,sizeof(Id));
	      strncpy(id,rec+COM_LEN+1,ID_LEN);
	      id[ID_LEN]='\0';
	      printf("%s has not accepted your friend request!\n",id);
	    }
	  else if(strncmp(rec,"NOCON",COM_LEN)==0)
	    {
	      printf("no stream to handle\n");
	    }
	  // add other consu
	}
    }
  return NULL;
}
void* handle_udp(void*arg)
{
  int sock = *(int*)arg;
  char UDP_PACK[MAX_UDP_LEN+1];
  while(1)
    {
      memset(UDP_PACK,0,MAX_UDP_LEN+1);
      int rec=recv(sock,UDP_PACK,MAX_UDP_LEN,0);
      if(rec<0)
	{
	  printf("udp recieve error");
	  continue;
	}
      if(rec<3)
	{
	  continue;
	}
      char hex_code[3];
      hex_code[2]='\0';
      char msg_type = UDP_PACK[0];
      hex_code[0]=UDP_PACK[2];
      hex_code[1]=UDP_PACK[1];
      unsigned long msg_count=strtoul(hex_code,NULL,16);
      printf("\n[NOTIFICATION] : you have msg type %c, unread streams: %lu\n",msg_type,msg_count);
      fflush(stdout);
    }
  return NULL;
}
int register_connect(int fd,Port *port)
{
  while(true)
    {
      printf("write REGIS to register, CONNE to login to server\n");
      printf("(ID must be exactly 8) (PORT 0-9999) (PASS 0-65535)\n");
      char input_line[7];
      if(fgets(input_line,sizeof(input_line),stdin)==NULL)
	{
	  printf("Error reading regis or conne\n");
	  continue;
	}
      input_line[strcspn(input_line,"\n")]='\0';
      if(strncmp(input_line,"REGIS",COM_LEN)==0)
	{
	  char buf[10];
	  printf("Enter Id 8 exactly 8 chars\n");
	  if(fgets(buf,sizeof(buf),stdin)==NULL)
	    {
	      printf("Error reading Id\n");
	      continue;
	    }
	  buf[strcspn(buf,"\n")]='\0';
	  Id id;
	  strcpy(id,buf);
	  id[ID_LEN]='\0';
	  char buf_p[6];
	  printf("Enter port\n");
	  if(fgets(buf_p,sizeof(buf_p),stdin)==NULL)
	    {
	      printf("Error reading port\n");
	      continue;
	    }
	  buf_p[strcspn(buf_p,"\n")]='\0';
	  snprintf((char *)port,5,"%04d",atoi(buf_p));

	  char buf_pass[7];
	  printf("Enter Password\n");
	  if(fgets(buf_pass,sizeof(buf_pass),stdin)==NULL)
	    {
	      printf("Error reading password\n");
	      continue;
	    }
	  buf_pass[strcspn(buf_pass,"\n")]='\0';
	  Password pass =(uint16_t)atoi(buf_pass);
	  
          TCP_PACK send_buf;
          memset(send_buf, 0, MAX_TCP_LEN + 1);
          memcpy(send_buf, "REGIS ", 6);
          strncpy(send_buf + 6, id, 8); 
          send_buf[14] = ' ';
          memcpy(send_buf + 15, port, 4); 
          send_buf[19] = ' ';
          memcpy(send_buf + 20, &pass, 2);
          memcpy(send_buf + 22, "+++", 3);
	  send_buf[25]='\0';
          write(fd, send_buf, strlen(send_buf));
	  char readbuf[COM_LEN+4];
	  read(fd,readbuf,COM_LEN+3);
	  readbuf[COM_LEN+3]='\0';
	  printf("%s\n",readbuf);
	  break;
	}
      else if(strncmp(input_line,"CONNE",COM_LEN)==0)
	{
	  char buf[10];
	  printf("Enter Id 8 exactly 8 chars\n");
	  if(fgets(buf,sizeof(buf),stdin)==NULL)
	    {
	      printf("Error reading Id\n");
	      continue;
	    }
	  buf[strcspn(buf,"\n")]='\0';
	  Id id;
	  strcpy(id,buf);
	  id[ID_LEN]='\0';
	  char buf_pass[7];
	  printf("Enter Password\n");
	  if(fgets(buf_pass,sizeof(buf_pass),stdin)==NULL)
	    {
	      printf("Error reading password\n");
	      continue;
	    }
	  buf_pass[strcspn(buf_pass,"\n")]='\0';
	  Password pass =(uint16_t)atoi(buf_pass);

	  char buf_p[6];
	  printf("Enter port (exactly the port you registered with)\n");
	  if(fgets(buf_p,sizeof(buf_p),stdin)==NULL)
	    {
	      printf("Error reading port\n");
	      continue;
	    }
	  buf_p[strcspn(buf_p,"\n")]='\0';
	  snprintf((char *)port,5,"%04d",atoi(buf_p));

          TCP_PACK send_buf;
          memset(send_buf, 0, MAX_TCP_LEN + 1);
          memcpy(send_buf, "CONNE ", 6);
          strncpy(send_buf + 6, id, 8); 
          send_buf[14] = ' ';
          memcpy(send_buf + 15, &pass, 2);
          memcpy(send_buf + 17, "+++", 3);
	  send_buf[20]='\0';
          write(fd, send_buf, strlen(send_buf));
	  char readbuf[COM_LEN+4];
	  read(fd,readbuf,COM_LEN+3);
	  readbuf[COM_LEN+3]='\0';
	  printf("%s\n",readbuf);
	  break;
	  
	}
      else
	{
	  printf("Invalid input, usage : REGIS or CONNE\n");
	  continue;
	}
      
    }
  
  return 0;  
}

int client_init(struct addrinfo **t,char *SERV,char *PORT)
{
  struct addrinfo *p;
  int sockfd;
  struct addrinfo hints, *servinfo;
  memset(&hints,0,sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if(getaddrinfo(SERV,PORT,&hints,&servinfo)!=0)
    {
      printf("getaddrinfo error\n");
      return -1;
    }
  for(p = servinfo;p!=NULL;p=p->ai_next)
    {
      if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)
	{
	  perror("socket");
	  continue;
	}
      if(connect(sockfd,p->ai_addr,p->ai_addrlen) == -1)
	{
	  close(sockfd);
	  perror("connect");
	  continue;
	}
      break;
    }
  if(p==NULL)
    {
      printf("failed to connect\n");
      freeaddrinfo(servinfo);
      return -1;
    }
  *t=p;
  return sockfd;
}

int udp_init(Port *port)
{
  int sock = socket(AF_INET,SOCK_DGRAM,0);
  struct sockaddr_in address_sock;
  address_sock.sin_family=AF_INET;
  address_sock.sin_port=htons(atoi((char*)port));
  address_sock.sin_addr.s_addr=htonl(INADDR_ANY);
  int r = bind(sock,(struct sockaddr*)&address_sock,sizeof(struct sockaddr_in));
  if(r!=0)
    {
      printf("udp bind failed\n");
      return -1;
    }
  return sock;
}
int read_by_char(int fd,TCP_PACK ptr)
{
  char c;
  ssize_t rc,n;
  int i = 0;
  ssize_t plus_c = 0;
  for(n = 1 ; n<=MAX_TCP_LEN;++n)
    {
      rc = read(fd,&c,1);
      if(rc==1)
	{
	  ptr[i]=c;
	  i++;
	  if(c=='+')
	    {
	      plus_c++;
	    }
	  else
	    {
	      plus_c=0;
	    }
	  if(plus_c>=3)
	    {
	      break;
	    }
	}
      else if(rc == 0)
	{
	  if(n==1)
	    {
	      return 0;
	    }
	  else
	    {
	      break;
	    }
	}
      else
	{
	  return -1;
	}
    }
  ptr[i]='\0';
  return n;
}
