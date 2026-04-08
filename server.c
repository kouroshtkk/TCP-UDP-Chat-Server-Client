#include "protocol.h"

Client clients[MAX_CLIENTS];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int handle_request(int client_index);
void send_udp(int,int ,int type,char*);
char* remove_stream(int source_index);
void flood_recursive(int sender,int current,char *mess,bool *visited);
void flood_msg(int client_index,char *mess);
int main(int argc,char *argv[])
{
  if(argc<2)
    {
      printf("usage: ./server port\n");
      return -1;
    }
  const char *PORT = argv[1];  
  if(strlen(PORT)>PORT_LEN)
    {
      printf("Port can not be higher than 9999\n");
      return -1;
    }
  init_clients();
  int server_socket = server_init(PORT);
  struct sockaddr_in client_addr;
  socklen_t client_len=sizeof(client_addr); 

  while(1)
    {
      int new_sock = accept(server_socket,(struct sockaddr *)&client_addr,&client_len);
      if(new_sock<0)
	{
	  perror("accept");
	  continue;
	}
      int *sock = malloc(sizeof(int));
      if(!sock)
	{
	  perror("can not malloc for sock");
	  close(new_sock);
	  continue;
	}
      *sock = new_sock;
      pthread_t thread;
      pthread_create(&thread,NULL,handle_clients,sock);
      pthread_detach(thread);
    }
  
  
}
void init_clients()
{
  for(int i = 0 ; i < MAX_CLIENTS;++i)
    {
      clients[i].cfd = -1;
      clients[i].logged_in = false;
      clients[i].unread_count = 0;
      memset(clients[i].id,0,sizeof(Id));
      memset(clients[i].friends,0,sizeof(Friends));
      clients[i].head=NULL;
      clients[i].tail=NULL;
    }
}
int server_init(const char *PORT)
{
  int sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd<0)
    {
      perror("socket");
      exit(1);
    }
  int yes = 1;
  if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))<0)
    {
      perror("setsockopt");
      exit(1);
    }
  struct sockaddr_in server_addr;
  socklen_t len=sizeof(struct sockaddr_in);
  server_addr.sin_family=AF_INET;
  server_addr.sin_port=htons(atoi(PORT));
  server_addr.sin_addr.s_addr=INADDR_ANY;
  if(bind(sockfd,(struct sockaddr *)&server_addr,len)<0)
    {
      perror("bind");
      close(sockfd);
      exit(1);
    }
  if(listen(sockfd,0)<0)
    {
      perror("listen");
      close(sockfd);
      exit(1);
    }
  return sockfd;
}

void *handle_clients(void*sock)
{
  int socket = *(int *)sock;
  free(sock);
  int client_index = register_user(socket);
  if(client_index < 0 )
    {
      printf("failed to get client index\n");
      return NULL;
    }
  while(1)
    {
      int r=handle_request(client_index);
      if(r<0)
	{
	  printf("user disconnected or error handling request\n");
	  pthread_mutex_lock(&lock);
	  clients[client_index].cfd = -2; // user disconnected "-2"
	  clients[client_index].logged_in = false;
	  pthread_mutex_unlock(&lock);
	  break;
	}
    }
  //logout user
  pthread_mutex_lock(&lock);
  clients[client_index].logged_in = false;
  clients[client_index].cfd = -2; // -2 so the slot does not get filled by new user
  pthread_mutex_unlock(&lock);
  return NULL;
}

int handle_request(int client_index)
{
  int socket = clients[client_index].cfd;
  TCP_PACK buf;
  int r = read_by_char(socket,buf);
  if(r<=0)
    {
      return -1;
    }
  printf("client %d sent: %s\n",socket,buf);
  if(strncmp(buf,"LIST?",5)==0)
    {
      pthread_mutex_lock(&lock);
      TCP_PACK send;
      int count = 0;
      for(int i = 0; i<MAX_CLIENTS;++i)
	{
	  if(clients[i].logged_in)
	    count++;
	}
      sprintf(send,"RLIST %03d+++",count);
      write(socket,send,strlen(send));
      for(int i = 0 ;i<MAX_CLIENTS;++i)
	{
	  if(clients[i].logged_in)
	    {
	      memset(send,0,sizeof(TCP_PACK));
	      sprintf(send,"LINUM %s+++",clients[i].id);
	      write(socket,send,strlen(send));
	    }
	}
      pthread_mutex_unlock(&lock);
      return 0;
    }
  else if(strncmp(buf,"IQUIT",5)==0)
    {
      TCP_PACK send;
      sprintf(send,"GOBYE+++");
      write(socket,send,strlen(send));
      return -1;
    }
  else if(strncmp(buf,"FRIE?",5)==0)
    {
      char *ptr=buf+COM_LEN+1;
      Id id;
      strncpy(id,ptr,ID_LEN);
      id[ID_LEN]='\0';
      int index = -1;
      pthread_mutex_lock(&lock);
      for(int i=0;i<MAX_CLIENTS;++i)
	{
	      if(strcmp(clients[i].id,id)==0)
		{
		  index=i;
		  break;
		}
	}
      pthread_mutex_unlock(&lock);
      if(index==-1)
	{
	  TCP_PACK send;
	  sprintf(send,"FRIE<+++");
	  write(socket,send,strlen(send));
	  return 0;
	}
      TCP_PACK send;
      sprintf(send,"FRIE>+++");
      write(socket,send,strlen(send));
      pthread_mutex_lock(&lock);
      send_udp(client_index,index,0,NULL);
      pthread_mutex_unlock(&lock);
    }
  else if(strncmp(buf,"FLOO?",COM_LEN)==0)
    {
      bool has_friends;
      pthread_mutex_lock(&lock);
      for(int i=0;i<MAX_CLIENTS;++i)
	{
	  if (clients[client_index].friends[i])
	    {
		      has_friends=true;
		      break;
	    }
	}
      pthread_mutex_unlock(&lock);
      if(!has_friends)
	{
	  TCP_PACK send;
	  sprintf(send,"GOBYE+++");
	  write(socket,send,strlen(send));
	  return -1;
	}
      char *ptr=buf+COM_LEN+1;
      char temp_buf[204];
      strcpy(temp_buf,ptr);
      temp_buf[strcspn(temp_buf,"+++")]='\0';
      char* mess=malloc(MSG_LEN+1);
      strcpy(mess,temp_buf);
      TCP_PACK send;
      sprintf(send,"FLOO>+++");
      write(socket,send,strlen(send));
      pthread_mutex_lock(&lock);
      flood_msg(client_index,mess);
      pthread_mutex_unlock(&lock);
      free(mess);
      return 0;
    }
  else if(strncmp(buf,"MESS?",COM_LEN)==0)
    {
      char *ptr=buf+COM_LEN+1;
      Id id;
      strncpy(id,ptr,ID_LEN);
      id[ID_LEN]='\0';
      int index = -1;
      pthread_mutex_lock(&lock);
      for(int i=0;i<MAX_CLIENTS;++i)
	{

	      if(strcmp(clients[i].id,id)==0)
		{
		  index=i;
		  break;
		}
	}
      pthread_mutex_unlock(&lock);
      if(index==-1)
	{
	  TCP_PACK send;
	  sprintf(send,"GOBYE+++");
	  write(socket,send,strlen(send));
	  return -1;
	}
      pthread_mutex_lock(&lock);
      if(clients[client_index].friends[index])
	{
	  char *msg_ptr=buf+COM_LEN+1+ID_LEN+1;
	  msg_ptr[strcspn(msg_ptr,"+++")]='\0';
	  send_udp(client_index,index,3,msg_ptr);
	  pthread_mutex_unlock(&lock);
	  TCP_PACK send;
	  sprintf(send,"MESS>+++");
	  write(socket,send,strlen(send));
	  return 0;
	}
      else
	{
	  pthread_mutex_unlock(&lock);
	  TCP_PACK send;
	  sprintf(send,"MESS<+++");
	  write(socket,send,strlen(send));
	  return 0;
	}
    }
  else if(strncmp(buf,"CONSU",COM_LEN)==0) //consu
    {
      pthread_mutex_lock(&lock);
      char *stream=remove_stream(client_index);
      if(stream==NULL)
	{
	  printf("stream problem\n");
	  return -1;
	}
      pthread_mutex_unlock(&lock);
      write(socket,stream,strlen(stream));
      if(strncmp(stream,"EIRF>",COM_LEN)==0)
	{
	  TCP_PACK client_resp;
	  int n = read_by_char(socket,client_resp);
	  if(n<0)
	    {
	      free(stream);
	      printf("read error\n");
	      TCP_PACK send;
	      sprintf(send,"GOBYE+++");
	      write(socket,send,strlen(send));
	      return -1;
	    }
	  if(strncmp(client_resp,"OKIRF",COM_LEN)==0)
	    {
	      TCP_PACK send;
	      sprintf(send,"ACKRF+++");
	      write(socket,send,strlen(send));
	      char *ptr=stream+COM_LEN+1;
	      Id id;
	      strncpy(id,ptr,ID_LEN);
	      id[ID_LEN]='\0';
	      int index = -1;
	      pthread_mutex_lock(&lock);
	      for(int i=0;i<MAX_CLIENTS;++i)
		{
		  if(clients[i].logged_in)
		    {
		      if(strcmp(clients[i].id,id)==0)
			{
			  index=i;
			  break;
			}
		    }
		}
	      if(index==-1)
		{
		  free(stream);
		  printf("user does not exist\n");
		  TCP_PACK send;
		  sprintf(send,"GOBYE+++");
		  write(socket,send,strlen(send));
		  pthread_mutex_unlock(&lock);
		  return -1;
		}
	      clients[index].friends[client_index]=true;
	      clients[client_index].friends[index]=true;
	      send_udp(client_index,index,1,NULL);
	      pthread_mutex_unlock(&lock);
	    }
	  else if(strncmp(client_resp,"NOKRF",COM_LEN)==0)
	    {
	      TCP_PACK send;
	      sprintf(send,"ACKRF+++");
	      write(socket,send,strlen(send));
	      char *ptr=stream+COM_LEN+1;
	      Id id;
	      strncpy(id,ptr,ID_LEN);
	      id[ID_LEN]='\0';
	      int index = -1;
	      pthread_mutex_lock(&lock);
	      for(int i=0;i<MAX_CLIENTS;++i)
		{
		  if(clients[i].logged_in)
		    {
		      if(strcmp(clients[i].id,id)==0)
			{
			  index=i;
			  break;
			}
		    }
		}
	      pthread_mutex_unlock(&lock);
	      if(index==-1)
		{
		  free(stream);
		  printf("user does not exist\n");
		  TCP_PACK send;
		  sprintf(send,"GOBYE+++");
		  write(socket,send,strlen(send));
		  return -1;
		}
	      pthread_mutex_lock(&lock);
	      send_udp(client_index,index,2,NULL);
	      pthread_mutex_unlock(&lock);
	    }
	  
	}
      free(stream);
      return 0;
    } //consu end
  return 0;
}
void flood_msg(int client_index,char *mess)
{
  bool visited[MAX_CLIENTS];
  for(int i=0;i<MAX_CLIENTS;++i)
    {
      visited[i]=false;
    }
  visited[client_index]=true;
  flood_recursive(client_index,client_index,mess,visited);
}
void flood_recursive(int sender,int current,char *mess,bool *visited)
{
  for(int i = 0;i<MAX_CLIENTS;++i)
    {
      if(clients[current].friends[i] && !visited[i])
	{
	  visited[i]=true;
	  send_udp(sender,i,4,mess);
	  flood_recursive(sender,i,mess,visited);
	}
    }
  
}
void send_udp(int source_index,int dest_index,int type,char *message)
{
  if(type==0||type==1||type==2)
    {
      Flow *new=malloc(sizeof(Flow));
      new->type=type+'0';
      strcpy(new->sender_id,clients[source_index].id);
      new->next=clients[dest_index].head;
      clients[dest_index].head=new;
      clients[dest_index].unread_count++;
      int sockfd = socket(AF_INET,SOCK_DGRAM,0);
      char mess[4];
      char hex_temp[3];
      snprintf(hex_temp,3,"%02X",clients[dest_index].unread_count);
      mess[0]=new->type;
      mess[1]=hex_temp[1];
      mess[2]=hex_temp[0];
      mess[3]='\0';
      ssize_t n = sendto(sockfd,mess,3,0,(struct sockaddr *)&clients[dest_index].udp_addr,sizeof(clients[dest_index].udp_addr));
      if(n<0)
	{
	  perror("udp send failed\n");
	}
      close(sockfd);
     }
  if(type==3||type==4)
    {
      Flow *new=malloc(sizeof(Flow));
      new->type=type+'0';
      strcpy(new->sender_id,clients[source_index].id);
      new->next=clients[dest_index].head;
      strcpy(new->message,message);
      clients[dest_index].head=new;
      clients[dest_index].unread_count++;
      int sockfd = socket(AF_INET,SOCK_DGRAM,0);
      char mess[4];
      char hex_temp[3];
      snprintf(hex_temp,3,"%02X",clients[dest_index].unread_count);
      mess[0]=new->type;
      mess[1]=hex_temp[1];
      mess[2]=hex_temp[0];
      mess[3]='\0';
      ssize_t n = sendto(sockfd,mess,3,0,(struct sockaddr *)&clients[dest_index].udp_addr,sizeof(clients[dest_index].udp_addr));
      if(n<0)
	{
	  perror("udp send failed\n");
	}
      close(sockfd);
    }    
  
}
char* remove_stream(int source_index)
{
  if(clients[source_index].unread_count==0)
    {
      char *resp = malloc(sizeof(TCP_PACK));
      sprintf(resp,"NOCON+++");
      return resp;
    }
  else if(clients[source_index].unread_count>0)
    {
      if(clients[source_index].head==NULL)
	{
	  return NULL;
	}
      if(clients[source_index].head->type=='0')
	{
	  char *resp = malloc(sizeof(TCP_PACK));
	  sprintf(resp,"EIRF> %s+++",clients[source_index].head->sender_id);
	  Flow *current = clients[source_index].head;
	  clients[source_index].head=clients[source_index].head->next;
	  free(current);
	  clients[source_index].unread_count--;
	  return resp;
	}
      else if(clients[source_index].head->type=='1')
	{
	  char *resp = malloc(sizeof(TCP_PACK));
	  sprintf(resp,"FRIEN %s+++",clients[source_index].head->sender_id);
	  Flow *current = clients[source_index].head;
	  clients[source_index].head=clients[source_index].head->next;
	  free(current);
	  clients[source_index].unread_count--;
	  return resp;
	  
	}
      else if(clients[source_index].head->type=='2')
	{
	  char *resp = malloc(sizeof(TCP_PACK));
	  sprintf(resp,"NOFRI %s+++",clients[source_index].head->sender_id);
	  Flow *current = clients[source_index].head;
	  clients[source_index].head=clients[source_index].head->next;
	  free(current);
	  clients[source_index].unread_count--;
	  return resp;
	}
      else if(clients[source_index].head->type=='3')
	{
	  char *resp = malloc(sizeof(TCP_PACK));
	  sprintf(resp,"SSEM> %s %s+++",clients[source_index].head->sender_id,clients[source_index].head->message);
	  Flow *current = clients[source_index].head;
	  clients[source_index].head=clients[source_index].head->next;
	  free(current);
	  clients[source_index].unread_count--;
	  return resp;
	  
	}
      else if(clients[source_index].head->type=='4')
	{
	  char *resp = malloc(sizeof(TCP_PACK));
	  sprintf(resp,"OOLF> %s %s+++",clients[source_index].head->sender_id,clients[source_index].head->message);
	  Flow *current = clients[source_index].head;
	  clients[source_index].head=clients[source_index].head->next;
	  free(current);
	  clients[source_index].unread_count--;
	  return resp;
	  
	}
    }
  return NULL;
}

int register_user(int socket) //todo
{
  TCP_PACK buf;
  int r = read_by_char(socket,buf);
  if(r<0)
    {
      printf("regis error\n");
      return -1;
    }
  printf("%s\n",buf);
  if(strncmp(buf,"REGIS ",COM_LEN+1)==0)
    {
      char *ptr = buf + COM_LEN + 1;
      Id id;
      strncpy(id,ptr,ID_LEN);
      id[ID_LEN]='\0';
      printf("%s\n",id);
      Port port;
      ptr += ID_LEN + 1 ;
      strncpy(port,ptr,PORT_LEN);
      port[PORT_LEN]='\0';
      int udp_port = atoi(port);
      ptr+=PORT_LEN + 1;
      printf("%d\n",udp_port);
      Password pass;
      memcpy(&pass,ptr,2);
      pthread_mutex_lock(&lock);

      int index = -1;
      for(int i = 0; i < MAX_CLIENTS;++i)
	{
	  if(clients[i].cfd==-1)
	    {
	      index = i;
	      break;
	    }
	}
      if(index!=-1)
	{
	  clients[index].cfd=socket;
	  strcpy(clients[index].id,id);
	  clients[index].password=pass;
	  clients[index].logged_in=true;
	  struct sockaddr_in addr;
	  socklen_t len = sizeof(addr);
	  getpeername(socket,(struct sockaddr *)&addr,&len);
	  clients[index].udp_addr.sin_addr.s_addr = addr.sin_addr.s_addr;
	  clients[index].udp_addr.sin_port=htons(udp_port);
	  pthread_mutex_unlock(&lock);
	  char *msg = "WELCO+++";
	  write(socket,msg,strlen(msg));
	  return index;
	}
      else
	{
	  pthread_mutex_unlock(&lock);
	  char *msg = "GOBYE+++";
	  write(socket,msg,strlen(msg));
	  close(socket);
	  return -1;
	}
    }
  else if(strncmp(buf,"CONNE ",COM_LEN+1)==0)
    {
      char *ptr = buf + COM_LEN + 1;
      Id id;
      strncpy(id,ptr,ID_LEN);
      id[ID_LEN]='\0';
      ptr += ID_LEN + 1 ;
      Password pass;
      memcpy(&pass,ptr,2);
      pthread_mutex_lock(&lock);

      int index = -1;
      for(int i = 0; i < MAX_CLIENTS;++i)
	{
	  if(clients[i].id[0]!='\0')
	    {
	      if(strcmp(id,clients[i].id) == 0 && pass == clients[i].password && !clients[i].logged_in)
		{
		  index = i;
		  break;
		}
	    }
	}
      if(index!=-1)
	{
	  clients[index].cfd=socket;
	  clients[index].logged_in=true;
	  struct sockaddr_in addr;
	  socklen_t len = sizeof(addr);
	  getpeername(socket,(struct sockaddr *)&addr,&len);
	  clients[index].udp_addr.sin_addr.s_addr = addr.sin_addr.s_addr;
	  pthread_mutex_unlock(&lock);
	  char *msg = "HELLO+++";
	  write(socket,msg,strlen(msg));
	  return index;
	}
      else
	{
	  pthread_mutex_unlock(&lock);
	  char *msg = "GOBYE+++";
	  write(socket,msg,strlen(msg));
	  close(socket);
	  return -1;
	}
    }
  else
    {
      //GOBYE+++
      pthread_mutex_unlock(&lock);
      char *msg = "GOBYE+++";
      write(socket,msg,strlen(msg));
      close(socket);
      return -1;
    }
  return -1;
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
