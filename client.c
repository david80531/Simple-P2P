#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  /* contains a number of basic derived types */
#include <sys/socket.h> /* provide functions and data structures of socket */
#include <arpa/inet.h>  /* convert internet address and dotted-decimal notation */
#include <netinet/in.h> /* provide constants and structures needed for internet domain addresses*/
#include <unistd.h>     /* `read()` and `write()` functions */
#include <dirent.h>     /* format of directory entries */
#include <sys/stat.h>   /* stat information about files attributes */
#include <time.h>
#include <pthread.h>

#define MAX_SIZE 2048
#define MAX_LINE 256
#define OFFSET 100

int user_fd [20];
int svr_fd, listen_fd, svr_ctrl_fd;
char svr_ip_addr[MAX_SIZE];
int svr_port;

void printInfo(void);
void listen_handler(void);
void watch_file_handler(void);

int main(int argc, char **argv){
  int ret, i;
  struct sockaddr_in svr_addr, cli_listen_addr;
  socklen_t addr_len;
  char buf[MAX_SIZE];
  char op[MAX_SIZE];
  char filename[MAX_SIZE];
  char *cmd;

  memset(filename, '\0', MAX_SIZE);
  memset(buf, '\0', MAX_SIZE);
  memset(op, '\0', MAX_SIZE);
  memset(svr_ip_addr, '\0', MAX_SIZE);

  pthread_t id;

  svr_fd = socket(AF_INET, SOCK_STREAM, 0);
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  if(svr_fd < 0){
    perror("Create Socket Failed!\n");
    exit(1);
  }
  if(listen_fd < 0){
    perror("Create Listen Socket Failed\n");
    exit(1);
  }

  //initialize
  for(i = 0;i < 20;i++){
    user_fd[i] = -1;
  }



  bzero(&svr_addr, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = inet_addr(argv[1]);
  svr_addr.sin_port = htons(atoi(argv[2]));


  if(connect(svr_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0){
    perror("Connection to server failed\n");
    exit(1);
  }

  strcpy(svr_ip_addr, argv[1]);
  svr_port = htons(atoi(argv[2]));

  addr_len = sizeof(cli_listen_addr);
  getsockname(svr_fd, (struct sockaddr*) &cli_listen_addr, &addr_len);

  printf("Client's port is on %d\n", ntohs(cli_listen_addr.sin_port));

  cli_listen_addr.sin_port = htons(ntohs(cli_listen_addr.sin_port) + OFFSET);

  printf("Client is listen on port : %d\n", ntohs(cli_listen_addr.sin_port));


  if(bind(listen_fd, (struct sockaddr*) &cli_listen_addr, sizeof(cli_listen_addr)) < 0 ){
    perror("Bind listen socket failed!\n");
    exit(1);
  }

  listen(listen_fd, 21);

  ret = pthread_create(&id, NULL, (void *) listen_handler, NULL);   // listening...
  if(ret < 0){
    perror("Create thread error!\n");
    exit(1);
  }

  mkdir("./localStorage", S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

  printInfo();

  while(1){

    printf("simpleP2P Client>");

  fgets(buf, MAX_SIZE, stdin);

  if(write(svr_fd, buf, strlen(buf)) < 0){
        perror("Write Bytes Failed\b");
        exit(1);
  }
  sleep(1);

  cmd = strtok(buf, " \n");
  strcpy(op, cmd);

  if(strcmp(op, "login")==0){
    memset(buf, '\0', MAX_SIZE);
    read(svr_fd, buf, MAX_SIZE);
    printf("%s\n", buf);

  } else if(strcmp(op, "ls")==0){
        //memset(buf, '\0', MAX_SIZE);
        //read(sockfd, buf, MAX_SIZE);
        //printf("%s\n", buf);
  } else if(strcmp(op, "dl")==0){
    cmd = strtok(NULL, "\n");
    strcpy(filename, cmd);
    memset(buf, '\0', MAX_SIZE);
    read(svr_fd, buf, MAX_SIZE);
    printf("%s", buf);
  }

  }

  return 0;
}

void listen_handler(void){
  struct sockaddr_in connect_addr;
  socklen_t addr_len;
  int connect_fd;
  int i, ret;
  char buf[MAX_SIZE];
  pthread_t pid;

  memset(buf, '\0', MAX_SIZE);

  while(1){
    addr_len = sizeof(connect_addr);
    connect_fd = accept(listen_fd, (struct sockaddr*) &connect_addr, &addr_len);
    //printf("Connection is from %s:%d\n", inet_ntoa(connect_addr.sin_addr), ntohs(connect_addr.sin_port));

    read(connect_fd, buf, MAX_SIZE);

    if(strcmp(buf, "server")==0){

      printf("Server is from %s:%d\n", inet_ntoa(connect_addr.sin_addr), ntohs(connect_addr.sin_port));
      svr_ctrl_fd = connect_fd;

      pthread_create(&pid, NULL, (void *)watch_file_handler, NULL);
      if(ret < 0) {
        perror("Create watch file thread error!\n");
        exit(1);
      }

    } else{
      for(i = 0; i < 20; i++){
        if(user_fd[i] != -1){
          user_fd[i] = connect_fd;
          break;
        }
      }
      //pthread_create...
    }

  }
}

void watch_file_handler(void){
    DIR *dirPtr;
    struct dirent *direntPtr = NULL;
    char buff[MAX_SIZE];


    if((dirPtr = opendir("./localStorage")) == NULL) {
        perror("Failed to open directory\n");
    } else {
        memset(buff, '\0', MAX_SIZE);

        while((direntPtr = readdir(dirPtr)) != NULL) {
            if(strcmp(direntPtr->d_name, ".") == 0 || strcmp(direntPtr->d_name, "..") == 0) continue;

            strcat(buff, "\n");
            strcat(buff, direntPtr->d_name);
        }

        if(strlen(buff) == 0) strcpy(buff, "NF");

        if( write(svr_ctrl_fd, buff, strlen(buff)) < 0) {
            perror("Writing file list failed.\n");
            exit(1);
        }
    }

    closedir(dirPtr);

    return;
}

void printInfo(void) {
  printf("\n\n\nWelcome to P2P system!\n");
  printf("[Info] Type login + account to login\n");
  printf("[Info] Type ls to list all the files\n");
  printf("[Info] Type dl + filename to download file\n");
  printf("[Info] Type up + filename to upload file\n");
}
