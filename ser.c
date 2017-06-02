#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h> // strtol()
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#define MAX_SIZE 2048
#define MAX_LINE 256
#define PORT 8888
#define MAX_CONNECTION 5
#define OFFSET 100


typedef struct {
  int index;
  int sockfd;
  char name[20];
  char file_list[MAX_SIZE];
  struct sockaddr_in addr;
} USER;

typedef struct {
  int index;
  int sockfd;
} WATCH;

typedef struct {
  int sockfd;
  int segment;
  int all_segment;
  char filename[MAX_SIZE];
} Transfer;

USER user[20];
WATCH watch[20];
char *buffer [21];
int buffer_size[21];

void connection_handler(void *);
void file_change_handler(void *);
void login_handler(char [], int, int);
void download_handler(char [], int, int);
void upload_handler(char[], int, int);
void sending_segment_handler(int);
void receiving_segment_handler(void *Arg);
int check_server_file(char []);

int main(int argc, char **argv)
{
  int listen_fd, connection_fd;
  char buf[MAX_SIZE];
  struct sockaddr_in svr_addr, cli_addr;
  socklen_t addr_len;

	pthread_t id;
	int i, ret;
  int flag = 1;

  memset(buf, '\0', MAX_SIZE);

  bzero((void *)&svr_addr, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  svr_addr.sin_port = htons(atoi(argv[1]));

  if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) <0){
    perror("Create Socket failed");
    exit(1);
  }

  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

  if(bind(listen_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0){
    perror("Bind TCP socket failed\n");
    exit(1);
  }
  //initialize

  for(i = 0; i < 20 ;i++){
    user[i].index = -1;
    memset(user[i].name, '\0', 20);
    memset(user[i].file_list, '\0', MAX_SIZE);
  }

  listen(listen_fd, MAX_CONNECTION);

  mkdir("./serverStorage", S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

  printf("Listen on port %s:%s\n", inet_ntoa(svr_addr.sin_addr), argv[1]);
  printf("Welcome to P2P server\n");
  printf("Waiting for users...\n");

  while(1){

    addr_len = sizeof(cli_addr);
    connection_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &addr_len);
    //printf("connection is from %s(IP):%d(port), %d(cli_fd)\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), connection_fd);

    //setting user parameter

    for(i = 0;i < 20; i++){
      if(user[i].index == -1) {
        user[i].index = i;
        break;
      }
    }

    if(i==20) {
      sprintf(buf, "Server connection has reach limited! Please connect later\n");

      write(connection_fd, buf, strlen(buf));

    }
    else {
      user[i].addr = cli_addr;
      user[i].sockfd = connection_fd;

      printf("User is from %s:%d\n", inet_ntoa(user[i].addr.sin_addr), ntohs(user[i].addr.sin_port));

      write(connection_fd, buf, strlen(buf));

      ret = pthread_create(&id, NULL, (void *) connection_handler, (void *)&user[i]);
      if(ret != 0) {
        perror("Creating Thread Error!\n");
        exit(1);
      }
    }





  }
	return (0);
}


void connection_handler(void *arg)
{
    USER *client = (USER *) arg;
    char buf[MAX_SIZE];
    char * cmd;
    char instruct[MAX_SIZE];
    char op[MAX_SIZE];
    char file_list[MAX_SIZE];

    memset(file_list, '\0', MAX_SIZE);
    memset(buf, '\0', MAX_SIZE);
    memset(op, '\0', MAX_SIZE);
    memset(instruct, '\0', MAX_SIZE);

    struct sockaddr_in addr = client->addr;
    int sockfd = client->sockfd;
    int index = client->index;
    strcpy(file_list, client->file_list);



    while(1){
      if(read(sockfd, instruct, MAX_SIZE) == 0){
        printf("Client(%d) is disconnected \n", sockfd);
        user[index].index = -1;
        return ;
      }

      cmd = strtok(instruct, " \n");
      strcpy(op, cmd);

      if(strcmp(op, "login")==0){
        cmd = strtok(NULL, " \n");
        strcpy(op, cmd);
        login_handler(op, sockfd, index);
      }  else if(strcmp(op, "ls")==0){
        //file_listing_handler(sockfd);
      }else if(strcmp(op, "dl")==0){
        cmd = strtok(NULL, " \n");
        strcpy(op, cmd);
        download_handler(op, sockfd, index);
      } else if(strcmp(op, "up")==0){
        cmd = strtok(NULL, " \n");
        strcpy(op, cmd);
        upload_handler(op, sockfd, index);
      } else {
        continue;
      }
  }


}

void login_handler(char id[], int sockfd, int idx){
    int i;
    char *cmd;
    char op[MAX_SIZE];
    char buf[MAX_SIZE];
    struct sockaddr_in cli_listen_addr;
    int cli_connect_fd;

    cli_connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cli_connect_fd < 0){
      perror("Create Client Connect Socket Failed\n");
      exit(1);
    }

    pthread_t pid;

    memset(buf, '\0', MAX_SIZE);
    memset(op, '\0', MAX_SIZE);

    strcpy(user[idx].name, id);

    bzero((void *)&cli_listen_addr, sizeof(cli_listen_addr));
    cli_listen_addr.sin_family = AF_INET;
    cli_listen_addr.sin_addr.s_addr = user[idx].addr.sin_addr.s_addr;
    cli_listen_addr.sin_port = htons(ntohs(user[idx].addr.sin_port) + OFFSET);

    printf("Client's Port is on %d\n", ntohs(user[idx].addr.sin_port));
    printf("Client is listen on Port %d\n", ntohs(cli_listen_addr.sin_port));

    if (connect(cli_connect_fd, (struct sockaddr *)&cli_listen_addr, sizeof(cli_listen_addr)) < 0) {
      perror("Connect to Client Listen Port failed");
      exit(1);
    }

    sprintf(buf, "server");                   //wrtie info to client listen port
    write(cli_connect_fd, buf, strlen(buf));
    sleep(1);

    watch[i].index = idx;
    watch[i].sockfd = cli_connect_fd;

    pthread_create(&pid, NULL, (void *)file_change_handler, (void *)&watch[i]);



    sprintf(buf, "[Server Info] Login success!\n");
    write(sockfd, buf, strlen(buf));
    sleep(1);

    printf("USER:%s has login!\n", id);

    return;


}


void file_change_handler(void *arg){
  WATCH *watch = (WATCH *) arg;
  char buf[MAX_SIZE];
  memset(buf, '\0', MAX_SIZE);

  if(read(watch->sockfd, buf, MAX_SIZE)>0){
    printf("%s\n", buf);
    strcpy(user[watch->index].file_list, buf);
  }

  while(1){
    if(read(watch->sockfd, buf, MAX_SIZE) > 0){
      strcpy(user[watch->index].file_list, buf);
      printf("%s\n", buf);
    }
    if(read(watch->sockfd, buf, MAX_SIZE) == 0){
      return;
    }
  }

}

void download_handler(char filename[], int sockfd, int idx){
  int user_exist_file[20];
  int i, exist_num = 0;
  char *file;
  char buf[MAX_SIZE];
  char buf2[MAX_SIZE];


  for(i = 0;i < 20; i++){                      // check all clients if exist
    if(user[i].index == -1) continue;
    else {
      if(user[i].index == idx) continue;
      file = strtok(user[i].file_list, "\n");
      strcpy(buf, file);

      if(strcmp(buf, filename)==0){

        user_exist_file [exist_num++] = user[i].index;

      } else {
        while((file = strtok(NULL, "\n")) != NULL){
          strcpy(buf, file);
          if(strcmp(buf, filename)==0){
            user_exist_file [exist_num++] = user[i].index;
          }
        }
      }
    }
  }

  memset(buf, '\0', MAX_SIZE);
  memset(buf2, '\0', MAX_SIZE);


  for(i = 0; i < exist_num; i++){

    sprintf(buf2, "%s %d\n", inet_ntoa(user[user_exist_file[i]].addr.sin_addr), ntohs(user[user_exist_file[i]].addr.sin_port) + OFFSET);

    strcat(buf, buf2);

    printf("Client file in %s", buf2);

  }


  if(check_server_file(filename)){
    exist_num++;
    printf("Exist files: %d\n", exist_num);
    strcat(buf, "server\n");
  }

  if(exist_num > 0) {
    sprintf(buf2, "%d\n", exist_num);
    strcat(buf2, buf);
    write(sockfd, buf2, strlen(buf2));
    if(check_server_file(filename)){  //send server file
      sending_segment_handler(sockfd);
    }

  } else {
    sprintf(buf, "No such file in server and all of the clients!\n");
    write(sockfd, buf, strlen(buf));
  }
  return;
}

void upload_handler(char filename [], int sockfd, int idx){
  int user_exist_file[20];
  int i, exist_num = 1;
  char *file;
  char buf[MAX_SIZE];
  char path[MAX_SIZE];
  int connect_fd;
  struct sockaddr_in connect_addr;
  FILE *fp;

  memset(path, '\0', MAX_SIZE);

  user_exist_file[0] = idx;

  sprintf(path, "./serverStorage/%s", filename);

  for(i = 0;i < 20; i++){                      // check all clients if exist
    if(user[i].index == -1) continue;
    else {
      if(user[i].index == idx) continue;
      file = strtok(user[i].file_list, "\n");
      strcpy(buf, file);

      if(strcmp(buf, filename)==0){

        user_exist_file [exist_num++] = user[i].index;

      } else {
        while((file = strtok(NULL, "\n")) != NULL){
          strcpy(buf, file);
          if(strcmp(buf, filename)==0){
            user_exist_file [exist_num++] = user[i].index;
          }
        }
      }
    }
  }

  printf("Exist files: %d\n", exist_num);

  pthread_t tid[exist_num];

  for(i = 0; i< exist_num; i++){

    connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_addr.s_addr = user[user_exist_file[i]].addr.sin_addr.s_addr;
    connect_addr.sin_port = htons(ntohs(user[user_exist_file[i]].addr.sin_port) + OFFSET);

    if(connect(connect_fd, (struct sockaddr*) &connect_addr, sizeof(connect_addr)) < 0){
      perror("Create Connection to other clients error!\n");
      exit(1);
    }

    Transfer *tfr = malloc(sizeof(Transfer));

    tfr->all_segment = exist_num;
    tfr->segment = i;
    tfr->sockfd = connect_fd;
    memset(tfr->filename, '\0', MAX_SIZE);
    strcpy(tfr->filename, filename);

    pthread_create(&tid[i], NULL, (void *) receiving_segment_handler, (void *)tfr);
  }


  for(i = 0;i < exist_num;i++){
    pthread_join(tid[i], NULL);
  }

  fp = fopen(path, "wb");

  for( i = 0 ; i < exist_num ; i++) {
      fwrite(buffer[i], sizeof(char), buffer_size[i], fp);
  }

  fclose(fp);
  for(i = 0; i < exist_num; i++){
    free(buffer[i]);
  }

  return;
}

int check_server_file(char filename []){
  DIR *dirPtr;
  struct dirent *direntPtr = NULL;
  char buff[MAX_SIZE];
  int exist = 0;


  if((dirPtr = opendir("./serverStorage")) == NULL) {
      perror("Failed to open directory\n");
  } else {
      memset(buff, '\0', MAX_SIZE);

      while((direntPtr = readdir(dirPtr)) != NULL) {
          if(strcmp(direntPtr->d_name, ".") == 0 || strcmp(direntPtr->d_name, "..") == 0) continue;

          if(strcmp(filename, direntPtr->d_name)==0){
            exist = 1;
          }
      }

  }

  closedir(dirPtr);

  return exist;

}

void sending_segment_handler(int sockfd){
  char buf[MAX_SIZE];
  char msg[MAX_SIZE];
  char filename[MAX_SIZE];
  char *file_info;
  char path[MAX_SIZE];
  int all_seg, cur_seg;
  int file_size, seg_size;
  int file_fd;
  off_t offset;
  FILE *fp;

  memset(path, '\0', MAX_SIZE);
  memset(buf, '\0', MAX_SIZE);
  memset(msg, '\0', MAX_SIZE);
  memset(filename, '\0', MAX_SIZE);

  read(sockfd, msg, MAX_SIZE); //read information form reciever

  file_info = strtok(msg, "\n");
  strcpy(buf, file_info);
  all_seg = atoi(buf);

  file_info = strtok(NULL, "\n");
  strcpy(buf, file_info);
  cur_seg = atoi(buf);

  file_info = strtok(NULL, "\n");
  strcpy(filename, file_info);

  sprintf(path, "./serverStorage/%s", filename);

  fp = fopen(path, "rb");
  if(fp!=NULL){
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fclose(fp);
    sprintf(buf, "%d", file_size);
    printf("File Size: %d\n", file_size);

    printf("SOCK FD: %d\n", sockfd);

    write(sockfd, buf, strlen(buf));  // send file size
    sleep(1);

    seg_size = file_size / all_seg;
    offset = seg_size * cur_seg;
    if(cur_seg + 1 == all_seg) {
        if(file_size % all_seg) seg_size += file_size % all_seg;
      }

    printf("SEG SIZE:%d\n", seg_size);

    file_fd = open(path, O_RDONLY);
    sendfile(sockfd, file_fd, &offset, seg_size);

  } else{
    printf("[ERROR] Can not open the file!\n");

  }

  printf("%d Segment Sending Complete!\n", cur_seg);

  return;

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

  memset(buf, '\0', MAX_SIZE);

  read(sockfd, buf, MAX_SIZE);    //read file_size
  printf("Befor atoi:%s\n", buf);

  file_size = atoi(buf);
  seg_size = file_size/all_seg;

  printf("file size: %d \n", file_size);
  printf("Start uploading %d segment\n", cur_seg);
  printf("----------------------------\n");

  if((cur_seg+1) == all_seg){
    last_size = file_size % all_seg;
    seg_size += last_size;
    buffer_size[cur_seg] = seg_size;

    buffer[cur_seg] = malloc(sizeof(char)*seg_size);

    bzero(buffer[cur_seg], seg_size);
    read(sockfd, buffer[cur_seg], seg_size);



  } else {
    buffer_size[cur_seg] = seg_size;
    buffer[cur_seg] = malloc(sizeof(char)*seg_size);

    bzero(buffer[cur_seg], seg_size);
    read(sockfd, buffer[cur_seg], seg_size);

  }
  memset(buf, '\0', MAX_SIZE);


   // print finish message
  printf("Upload finished!\n");
  printf("----------------------------\n");



  close(sockfd);

  return;

}
