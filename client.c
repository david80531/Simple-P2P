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

#define MAX_SIZE 2048
#define MAX_LINE 256
#define OFFSET 100

int user_fd [20];

int main(int argc, char argv**){
  int svr_fd;
  struct sockaddr_in svr_addr, cli_listen_addr;
  struct socklen_t addr_len;

  svr_fd = socket(AF_INET, SOCK_STREAM, 0);

  if(svr_fd < 0){
    perror("Create Socket Failed!\n");
    exit(1);
  }

  bzero(&svr_addr, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_addr.s_addr = inet_addr(argv[1]);
  svr_addr.sin_port = htons(atoi(argv[2]));

  addr_len = sizeof(svr_addr);
  if(connect(svr_fd, (struct sockaddr *)&svr_addr, &addr_len) < 0){
    perror("Connection to server failed\n");
    exit(1);
  }

  addr_len = sizeof(cli_listen_addr);
  getsockname(svr_fd, (struct sockaddr*) &cli_listen_addr, &addr_len);

  printf("Client's port is on %d\n", ntohs(cli_listen_addr.sin_port));

  cli_listen_addr.sin_port = htons(ntohs(cli_listen_addr.sin_port) + OFFSET);

  printf("Client is listen on port : %d\n", ntohs(cli_listen_addr.sin_port));



  return 0;
}
