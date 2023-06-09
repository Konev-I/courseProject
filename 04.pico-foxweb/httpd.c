#include "httpd.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CONNECTIONS 1000
#define BUF_SIZE 65535
#define QUEUE_SIZE 1000000

static int listenfd;
int *clients;
static void start_server(const char *);
static void respond(int);

static char *buf;

// Client request
char *method, // "GET" or "POST"
    *uri,     // "/index.html" things before '?'
    *qs,      // "a=1&b=2" things after  '?'
    *prot,    // "HTTP/1.1"
    *payload; // for POST

int payload_size;

struct sockaddr_in clientaddr;
void serve_forever(const char *PORT) {
  struct sockaddr_in clientaddr;
  socklen_t addrlen;

  int slot = 0;

  //printf("Server started %shttp://127.0.0.1:%s%s\n", "\033[92m", PORT,"\033[0m");

  // create shared memory for client slot array
  clients = mmap(NULL, sizeof(*clients) * MAX_CONNECTIONS,
                 PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

  // Setting all elements to -1: signifies there is no client connected
  int i;
  for (i = 0; i < MAX_CONNECTIONS; i++)
    clients[i] = -1;
  start_server(PORT);

  // Ignore SIGCHLD to avoid zombie threads
  signal(SIGCHLD, SIG_IGN);

  // ACCEPT connections
  while (1) {
    addrlen = sizeof(clientaddr);
    clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);

    if (clients[slot] < 0) {
      perror("accept() error");
      exit(1);
    } else {
      if (fork() == 0) {
        close(listenfd);
        respond(slot);
        close(clients[slot]);
        clients[slot] = -1;
        exit(0);
      } else {
        close(clients[slot]);
      }
    }

    while (clients[slot] != -1)
      slot = (slot + 1) % MAX_CONNECTIONS;
  }
}

// start server
void start_server(const char *port) {
  struct addrinfo hints, *res, *p;

  // getaddrinfo for host
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, port, &hints, &res) != 0) {
    perror("getaddrinfo() error");
    exit(1);
  }
  // socket and bind
  for (p = res; p != NULL; p = p->ai_next) {
    int option = 1;
    listenfd = socket(p->ai_family, p->ai_socktype, 0);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (listenfd == -1)
      continue;
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
      break;
  }
  if (p == NULL) {
    perror("socket() or bind()");
    exit(1);
  }

  freeaddrinfo(res);

  // listen for incoming connections
  if (listen(listenfd, QUEUE_SIZE) != 0) {
    perror("listen() error");
    exit(1);
  }
}

// get request header by name
char *request_header(const char *name) {
  header_t *h = reqhdr;
  while (h->name) {
    if (strcmp(h->name, name) == 0)
      return h->value;
    h++;
  }
  return NULL;
}

// get all request headers
header_t *request_headers(void) { return reqhdr; }

// Handle escape characters (%xx)
static void uri_unescape(char *uri) {
  char chr = 0;
  char *src = uri;
  char *dst = uri;

  // Skip inital non encoded character
  while (*src && !isspace((int)(*src)) && (*src != '%'))
    src++;

  // Replace encoded characters with corresponding code.
  dst = src;
  while (*src && !isspace((int)(*src))) {
    if (*src == '+')
      chr = ' ';
    else if ((*src == '%') && src[1] && src[2]) {
      src++;
      chr = ((*src & 0x0F) + 9 * (*src > '9')) * 16;
      src++;
      chr += ((*src & 0x0F) + 9 * (*src > '9'));
    } else
      chr = *src;
    *dst++ = chr;
    src++;
  }
  *dst = '\0';
}

// client connection
void respond(int slot) {
  int rcvd;

  FILE* logs = fopen("/var/log/myLogs/myLog.log", "a");

  char *info = NULL; 
  char *login = NULL;
  char *pass = NULL;

  buf = malloc(BUF_SIZE);
  rcvd = recv(clients[slot], buf, BUF_SIZE, 0);

  if (rcvd < 0) {// receive error
    //fprintf(stderr, ("recv() error\n"));
  }
  else if (rcvd == 0) {// receive socket closed
    //fprintf(stderr, "Client disconnected upexpectedly.\n");
  }
  else // message received
  {
    char* log  = malloc(1024);
    buf[rcvd] = '\0';

    method = strtok(buf, " \t\r\n");
    uri = strtok(NULL, " \t");
    prot = strtok(NULL, " \t\r\n");

    uri_unescape(uri);

    //fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);

    qs = strchr(uri, '?');

    if (qs)
      *qs++ = '\0'; // split URI
    else
      qs = uri - 1; // use an empty string

    header_t *h = reqhdr;
    char *t, *t2;
    while (h < reqhdr + 16) {
      char *key, *val;

      key = strtok(NULL, "\r\n: \t");
      if (!key)
        break;

      val = strtok(NULL, "\r\n");
      while (*val && *val == ' ')
        val++;

      h->name = key;
      h->value = val;
      h++;
      //fprintf(stderr, "[H] %s: %s\n", key, val);

      if (!strcmp(key, "Authorization")) {
        //key -  это название (нужно Authorization)
        //а в value будет "Basic " + закодированные в base64 данные

        //Оставляем только данные
        val += 6;

        //Расшифровываем из base64
        int tmpStr = strlen(val);
        info = malloc(200);
        strcpy(info, "echo -n \"");
        strcat(info, val);
        strcat(info, "\" | base64 -d");
        FILE *fp = popen(info, "r");
        fgets(info, 200, fp);

        //Получаем логин и пароль
        int tmpDelimiter;
        for(tmpDelimiter = 0; tmpDelimiter < strlen(info); tmpDelimiter++) {
          if(info[tmpDelimiter] == ':') {
            break;
          }
        }

        login = malloc(tmpDelimiter + 1);
        pass = malloc(strlen(info) - tmpDelimiter + 1);
        strncpy(login, info, tmpDelimiter);
        strcpy(pass, &info[tmpDelimiter+1]);

        if (strlen(login) == 0 || strlen(pass) == 0) {
          login = NULL;
          pass = NULL;  
        }
      }


      t = val + 1 + strlen(val);
      if (t[1] == '\r' && t[2] == '\n')
        break;
    }
    t = strtok(NULL, "\r\n");
    t2 = request_header("Content-Length"); // and the related header if there is
    payload = t;
    payload_size = t2 ? atol(t2) : (rcvd - (t - buf));

    // bind clientfd to stdout, making it easier to write
    int clientfd = clients[slot];
    dup2(clientfd, STDOUT_FILENO);
    close(clientfd);

    fclose(logs);

    // socklen_t addrlen = sizeof(clientaddr);
    // clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
    // char * ip = inet_ntoa(clientaddr.sin_addr);

    // call router
    int returnCode = route(login, pass);

    logs = fopen("/var/log/myLogs/myLog.log", "a");
    //Журнализация в собственный текстовой журнал HTTP-запросов
    fprintf(logs, "%s %s %d\n", method, uri, returnCode);
    fclose(logs);

    // tidy up
    fflush(stdout);
    shutdown(STDOUT_FILENO, SHUT_WR);
    close(STDOUT_FILENO);
  }

  free(buf);
}
