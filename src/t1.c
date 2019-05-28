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
#define MSG_MAX 500

char user[USER_MAX];

void create_queue(char *filename) {
    char queue[QUEUE_PREFIX + USER_MAX];
    struct mq_attr attr;

    strcpy(queue, "/chat-");
    strcat(queue, user);

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * MSGLEN; // tamanho de cada mensagem
    attr.mq_flags = 0;

    mqd_t open_queue = mq_open(queue, O_RDWR|O_CREAT, 0666, &attr);

    if(open_queue < 0) {
        perror("mq_open\n");
        exit(1);
    }

    mq_close(open_queue);

    // char mode[] = "226";
    // int m = strtol(mode, 0, 8);

    // if(chmod(filename, m) < 0) {
    //     fprintf(stderr, "\nerror in chmod\n");
    // }

}

void user_register() {
    FILE *is_user;
    char filename[FILE_PREFIX + USER_MAX];

    printf("\nInsira um nome de usuario: ");
    scanf("%s", user);

    strcpy(filename, "/dev/mqueue/chat-");
    strcat(filename, user);

    is_user = fopen(filename, "r");

    if(is_user == NULL) {
        create_queue(filename);
    }
    // else {
    //     printf("\n\nNome de usuario indisponivel!\n\n");
    //     exit(0);
    // }

}

void split(char *message, char *remet, char *dest, char *msg) {
    int i = 0, j = 0, flag = 0;

    while(message[i] != '\0') {
        if(message[i] != ':' && flag == 0){
            remet[j] = message[i];
            j++;
        }
        else if(message[i] != ':' && flag == 1) {
            dest[j] = message[i];
            j++;
        }
        else if(message[i] != ':' && flag == 2) {
            msg[j] = message[i];
            j++;
        }
        else if(message[i] == ':') {

            if(flag == 0)
                remet[j] = '\0';
            else if(flag == 1)
                dest[j] = '\0';

            flag++;
            j = 0;
        }

        i++;
    }

    msg[j] = '\0';
}

void send_message_in_queue(char *dest, char *msg) {
    char queue[QUEUE_PREFIX + USER_MAX];
    struct mq_attr attr;

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    if(open_queue < 0) {
        perror("send_msg: mq_open\n");
        exit(1);
    }

    printf("\n%s\n", msg);

    if((mq_send(open_queue, (void *) msg, sizeof(msg), 0)) < 0) {
        perror("\nsend_msg: mq_send");
        exit(1);
    }

    mq_close(open_queue);
}

void send_message(char *message) {
    char remet[USER_MAX], dest[USER_MAX], msg[MSG_MAX];
    split(message, remet, dest, msg);
    send_message_in_queue(dest, message);
}

void chat() {
    char msg[MSGLEN];

    while(1) {
        fprintf(stdout, ">> ");
        setbuf(stdin, NULL);
        scanf("%[^\n]s", msg);
        send_message(msg);
    }
}

void listen() {
    char queue[QUEUE_PREFIX + USER_MAX];
    strcpy(queue, "/chat-");
    strcat(queue, user);

    mqd_t open_queue = mq_open(queue, O_RDWR);
 
    char message[MSGLEN];

    if((mq_receive(open_queue, (void *) message, sizeof(message), 0)) < 0) {
        perror("\nmq_receive");
        exit(1);
    }

    fprintf(stdout, "\n%s\n", message);

    mq_close(open_queue);
}

int main() {

    user_register();
    // chat();
    listen();

    return 0;
}