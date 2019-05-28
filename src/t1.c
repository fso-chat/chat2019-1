#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>

#define USER_MAX 10
#define FILE_PREFIX 17
#define QUEUE_PREFIX 6
#define MSGLEN 522

char user[USER_MAX];

void create_queue(char *filename) {
    char queue[QUEUE_PREFIX + USER_MAX];
    struct mq_attr attr;

    strcpy(queue, "/chat-");
    strcat(queue, user);

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * 522; // tamanho de cada mensagem
    attr.mq_flags = 0;

    if((mq_open(queue, O_RDWR|O_CREAT, 0666, &attr)) < 0) {
        perror("mq_open\n");
        exit(1);
    }

    char mode[] = "226";
    int m = strtol(mode, 0, 8);

    if(chmod(filename, m) < 0) {
        fprintf(stderr, "\nerror in chmod\n");
    }

}

void user_register() {
    FILE *is_user;
    char filename[FILE_PREFIX + USER_MAX];

    do {
        printf("\nInsira um nome de usuario: ");
        scanf("%s", user);

        strcpy(filename, "/dev/mqueue/chat-");
        strcat(filename, user);

        is_user = fopen(filename, "r");

        if(is_user == NULL) {
            create_queue(filename);
        }
        else {
            printf("\n\nNome de usuario indisponivel!\n\n");
        }
          
    } while(is_user != NULL);

}

int main() {

    user_register();

    return 0;
}