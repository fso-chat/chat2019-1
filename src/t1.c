#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define USER_MAX 10
#define FILE_PREFIX 17
#define QUEUE_PREFIX 6
#define MSGLEN 522
#define MSG_MAX 500
#define MESSAGE 512

char user[USER_MAX];
char user_queue[QUEUE_PREFIX + USER_MAX];

void create_queue(char *filename) {
    struct mq_attr attr;

    strcpy(user_queue, "/chat-");
    strcat(user_queue, user);

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * MSGLEN; // tamanho de cada mensagem
    attr.mq_flags = 0;

    mqd_t open_queue = mq_open(user_queue, O_RDWR|O_CREAT, 0666, &attr);

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

void split_message_received(char *message, char *remet, char *dest, char *msg) {
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

void split_message_sent(char *message, char *dest, char *msg) {
    int i = 1, j = 0, flag = 0;

    while(message[i] != '\0') {
        if(message[i] != ' ' && flag == 0) {
            dest[j] = message[i];
            j++;
        }
        else if(message[i] != ' ' && flag == 1){
            msg[j] = message[i];
            j++;
        }
        else if(message[i] == ' ') {
            
            if(flag == 0)
                dest[j] = '\0';
            else if(flag == 1)
                msg[j] = '\0';

            flag++;
            j = 0;
        }

        i++;
    }
}

void format_msg(char *message, char *dest, char *msg_sent) {
    char msg[MSG_MAX];
    
    split_message_sent(message, dest, msg);

    strcpy(msg_sent, user);
    strcat(msg_sent, ":");
    strcat(msg_sent, dest);
    strcat(msg_sent, ":");
    strcat(msg_sent, msg);
}

void *retry_send_message(void *m) {
    char queue[QUEUE_PREFIX + USER_MAX];    
    char *message = (char *) m;
    char dest[USER_MAX], msg_sent[MSGLEN];

    format_msg(message, dest, msg_sent);

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", dest);
    else {
        int i;
        for(i = 0; i < 3; i++) {
            sleep(0.5);
            if((mq_send(open_queue, (void *) msg_sent, strlen(msg_sent), 0)) >= 0)
                break;
        }

        if(i == 3)
            fprintf(stderr, "\nERRO %s\n", message);
    
        mq_close(open_queue);
    }

    int r = 0;
    pthread_exit((void *)&r);
}

void send_message_in_queue(char *message) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char dest[USER_MAX], msg_sent[MSGLEN];
    
    format_msg(message, dest, msg_sent);

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_RDWR);
    pthread_t retry;

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", dest);
    else {
        if((mq_send(open_queue, (void *) msg_sent, strlen(msg_sent), 0)) < 0)
            pthread_create(&retry, NULL, retry_send_message, (void *) message);
    
        mq_close(open_queue);
    }
}

void *send_message(void *m) {
    char *message = (char *) m;
    send_message_in_queue(message);

    int r = 0;
    pthread_exit((void *)&r);
}

void *listen() {
    char queue[QUEUE_PREFIX + USER_MAX];
    strcpy(queue, "/chat-");
    strcat(queue, user);

    mqd_t open_queue = mq_open(queue, O_RDWR);
 
    char message[MSGLEN];
    char remet[USER_MAX], dest[USER_MAX], msg[MSG_MAX];

    while(1) {
        if((mq_receive(open_queue, (void *) message, MSGLEN, 0)) < 0) {
            perror("\nmq_receive");
            mq_unlink(user_queue);
            exit(1);
        }

        split_message_received(message, remet, dest, msg);
        fprintf(stderr, "\n%s: %s\n\n", remet, msg);
    }

    mq_close(open_queue);
    
    int r = 0;
    pthread_exit((void *)&r);
}

void handler_interruption() {
    fprintf(stderr, "\nPara sair, digite 'sair'\n");
}

int main() {

    user_register();
    
    pthread_t send, receive;

    pthread_create(&receive, NULL, listen, NULL);

    char message[MSGLEN];

    printf("\n======================== VOCE ENTROU NO CHAT ========================\n");

    signal(SIGINT, handler_interruption);

    while(1) {
        printf("\n");
        setbuf(stdin, NULL);
        scanf("%[^\n]s", message);

        if(strcmp(message, "sair") == 0){
            mq_unlink(user_queue);
            exit(0);
        }
        else if(message[0] != '@') {
            fprintf(stderr, "\nUNKNOWNCOMMAND %s\n", message);
            continue;
        }

        pthread_create(&send, NULL, send_message, (void*) message);
        pthread_join(send, NULL);
    }

    return 0;
}