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
#include <fcntl.h>
#include <sys/sendfile.h>

#define MAX_SIZE 2048
#define MAX_LINE 256
#define OFFSET 100

int user_fd [20];
int svr_fd, listen_fd, svr_ctrl_fd;
char svr_ip_addr[MAX_SIZE];
int svr_port;
char *buffer [21];
int buffer_size[21];

void download_handler(char [], char []);
void printInfo(void);
void listen_handler(void);
void watch_file_handler(void);
void receiving_segment_handler(void *);
void sending_segment_handler(void *);

typedef struct {
  int sockfd;
  int segment;
  int all_segment;
  char filename[MAX_SIZE];
} Transfer;

typedef struct {
  int sockfd;
  char msg[MAX_SIZE];
} Send;



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

    if(strcmp(buf, "No such file in server and all of the clients!\n")==0){
      continue;
    } else {
     download_handler(buf, filename);
   }
 } else {
   continue;
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
  pthread_t pid, ptid;

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

      Send *sd = malloc(sizeof(Send));
      sd->sockfd = connect_fd;
      memset(sd->msg, '\0', MAX_SIZE);
      strcpy(sd->msg, buf);

      pthread_create(&ptid, NULL, (void *)sending_segment_handler, (void *)sd);
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

void download_handler(char connection_list[], char filename[]){
  char *ip_port;
  char *addr;
  char buf [MAX_SIZE];
  char list [MAX_SIZE];
  char ip[MAX_SIZE];
  char  port[MAX_SIZE];
  char path[MAX_SIZE];
  int download_fd_arr[20];
  int connect_fd;
  struct sockaddr_in connect_addr;
  int i, j;
  int all_seg, cur_seg = 0;
  FILE *fp;

  memset(list, '\0', MAX_SIZE);
  memset(buf, '\0', MAX_SIZE);
  memset(ip, '\0', MAX_SIZE);
  memset(port ,'\0', MAX_SIZE);
  memset(path, '\0', MAX_SIZE);

  printf("%s\n", connection_list);

  sprintf(path, "./localStorage/%s", filename);

  ip_port = strtok(connection_list, "\n");
  strcpy(list, ip_port);
  all_seg = atoi(list);

  pthread_t tid[all_seg];

  while((ip_port = strtok(NULL, "\n")) != NULL){
    strcpy(list, ip_port);
    printf("%s\n", list);
    if(strcmp(list, "server")==0){



    } else {

      for(i = 0 ; i < strlen(list); i++){
        if(list[i] == ' '){
          break;
        } else {
          ip[i] = list[i];
        }
      }

      int count = 0;

      for(; i < strlen(list);i++){
        port[count++] = list[i];
      }
      port[i] = '\0';

      printf("IP: %s\n", ip);
      printf("PORT: %s\n", port);

      fflush(stdout);

      connect_fd = socket(AF_INET, SOCK_STREAM, 0);
      connect_addr.sin_family = AF_INET;
      connect_addr.sin_addr.s_addr = inet_addr(ip);
      connect_addr.sin_port = htons(atoi(port));


      if(connect(connect_fd, (struct sockaddr*) &connect_addr, sizeof(connect_addr)) < 0){
        perror("Create Connection to other clients error!\n");
        exit(1);
      }

      Transfer *tfr = malloc(sizeof(Transfer));

      tfr->all_segment = all_seg;
      tfr->segment = cur_seg;
      tfr->sockfd = connect_fd;
      memset(tfr->filename, '\0', MAX_SIZE);
      strcpy(tfr->filename, filename);


      pthread_create(&tid[cur_seg], NULL, (void *) receiving_segment_handler, (void *)tfr);

      cur_seg++;

    }

    for(i = 0;i < all_seg;i++){
      pthread_join(tid[i], NULL);
    }

    fp = fopen(path, "wb");

    for( i = 0 ; i < all_seg ; i++) {
        fwrite(buffer[i], sizeof(char), buffer_size[i], fp);
    }
    fclose(fp);
    for(i = 0; i < all_seg; i++){
      free(buffer[i]);
    }

    }
  }

  void receiving_segment_handler(void *Arg){
    Transfer *tfr = (Transfer *) Arg;
    int all_seg = tfr->all_segment;
    int cur_seg = tfr->segment;
    int sockfd = tfr->sockfd;
    char filename [MAX_SIZE];
    char buf [MAX_SIZE];
    int file_size;
    int seg_size;
    int last_size;

    memset(filename, '\0', MAX_SIZE);
    memset(buf, '\0', MAX_SIZE);
    strcpy(filename, tfr->filename);

    sprintf(buf, "%d\n%d\n%s\n", all_seg, cur_seg, filename);

    write(sockfd, buf, strlen(buf));

    read(sockfd, buf, MAX_SIZE);    //read file_size

    file_size = atoi(buf);
    seg_size = file_size/all_seg;

    printf("file size: %d \n", file_size);
    printf("Start downloading %d segment\n", cur_seg);
    printf("----------------------------\n");

    if((cur_seg+1) == all_seg){
      last_size = file_size % all_seg;
      seg_size += last_size;
      buffer_size[cur_seg] = seg_size;

      buffer[cur_seg] = malloc(sizeof(char)*seg_size);

      read(sockfd, buffer[cur_seg], seg_size);

      //free(buffer[cur_seg]);

    } else {
      buffer_size[cur_seg] = seg_size;
      buffer[cur_seg] = malloc(sizeof(char)*seg_size);

      read(sockfd, buffer[cur_seg], seg_size);

      //free(buffer[cur_seg]);
    }


    read(sockfd, buf, MAX_SIZE);           // read finish message
    printf("%s\n", buf);

    close(sockfd);
    return;

  }

  void sending_segment_handler(void *Arg){
    Send *send = (Send *) Arg;
    int sockfd = send->sockfd;
    char msg[MAX_SIZE];
    char buf[MAX_SIZE];
    char *file_info;
    char filename[MAX_SIZE];
    char path[MAX_SIZE];
    int all_seg, cur_seg;
    int file_size, seg_size;
    int file_fd;
    off_t offset;
    FILE *fp;


    memset(filename, '\0', MAX_SIZE);
    memset(path, '\0', MAX_SIZE);
    memset(msg, '\0', MAX_SIZE);
    memset(buf, '\0', MAX_SIZE);

    strcpy(msg, send->msg);

    file_info = strtok(msg, "\n");
    strcpy(buf, file_info);
    all_seg = atoi(buf);

    file_info = strtok(NULL, "\n");
    strcpy(buf, file_info);
    cur_seg = atoi(buf);

    file_info = strtok(NULL, "\n");
    strcpy(filename, file_info);

    sprintf(path, "./localStorage/%s", filename);

    fp = fopen(path, "rb");
    if(fp!=NULL){
      fseek(fp, 0, SEEK_END);
      file_size = ftell(fp);
      fclose(fp);
      sprintf(buf, "%d", file_size);
      printf("File Size: %d\n", file_size);

      write(sockfd, buf, strlen(buf));  // send file size

      seg_size = file_size / all_seg;
      offset = seg_size * cur_seg;
      if(cur_seg + 1 == all_seg) {
          if(file_size % all_seg) seg_size += file_size % all_seg;
        }



      file_fd = open(path, O_RDONLY);
      sendfile(sockfd, file_fd, &offset, seg_size);
      sleep(2);
      printf("%d Segment Download Complete!\n", cur_seg);
      sprintf(buf, "%d Segment Download Complete!\n", cur_seg);
      write(sockfd, buf, sizeof(buf));

    } else{
      printf("[ERROR] Can not open the file!\n");

    }

    close(sockfd);
    return;
  }
