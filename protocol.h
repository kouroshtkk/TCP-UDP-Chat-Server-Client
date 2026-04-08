#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>

// const values
#define MAX_CLIENTS 100
#define ID_LEN 8
#define MSG_LEN 200
#define PORT_LEN 4
#define PASS_LEN 2 // uint16_t
#define COM_LEN 5 // [CONSU/FLOO?...]
#define TERMINATE "+++"
#define TERM_LEN 3 // +++
#define MAX_TCP_LEN 256 // [COM(5) ID(8) MESS(200) +++(3)] 5+8+200+3+3=219 
#define MAX_UDP_LEN 3 // [XYY]

//udp codes 
#define CODE_FR_REQ '0' 
#define CODE_FR_ACC '1'
#define CODE_FR_REF '2'
#define CODE_MSG '3'
#define CODE_FLOOD '4'

//typedef for encapsulation and parsing + 1 for null terminating
typedef uint16_t Password; // PASS_LEN unsigned short
typedef char Id[ID_LEN+1]; // 8 bytes
typedef char Mess[MSG_LEN+1];
typedef char Port[PORT_LEN+1];
typedef char Com[COM_LEN+1];
typedef char Term[TERM_LEN+1];
typedef bool Friends[MAX_CLIENTS];
typedef char TCP_PACK[MAX_TCP_LEN+1];
typedef struct Flow {
  char type;
  Id sender_id;
  Mess message;
  struct Flow *next;
} Flow;

typedef struct Client {
  Id id;
  Password password;
  int cfd;
  bool logged_in; 
  struct sockaddr_in udp_addr; // holds udp and ip
  Flow* head; // streams head
  Flow* tail;
  uint8_t unread_count; // [XYY] YY needs to be dealt with
  Friends friends;
} Client;

void init_clients();
void *handle_clients(void *sock);
int server_init(const char *PORT);
int register_user(int socket);
int read_by_char(int fd,TCP_PACK ptr);
#endif
