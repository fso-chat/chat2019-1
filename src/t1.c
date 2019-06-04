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
#include <dirent.h>

#define USER_MAX 10
#define FILE_PREFIX 17
#define QUEUE_PREFIX 6
#define MSGLEN 522
#define MSG_MAX 500
#define MESSAGE 512

char user[USER_MAX];
char user_queue[QUEUE_PREFIX + USER_MAX];

typedef struct users {
    char *user_name;
    struct users *next;
} USERS;

void create_queue(char *filename) {
    struct mq_attr attr;
    mode_t oldMask, newMask;

    strcpy(user_queue, "/chat-");
    strcat(user_queue, user);

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * MSGLEN; // tamanho de cada mensagem
    attr.mq_flags = 0;

    oldMask = umask((mode_t)0); //Pega umask antigo
    umask(0155); //Proíbe execução para o dono, leitura e execução para os demais 

    mqd_t open_queue = mq_open(user_queue, O_RDWR|O_CREAT, 0666, &attr);

    if(open_queue < 0) {
        perror("mq_open\n");
        exit(1);
    }

    umask(oldMask); //Seta umask novamente para o que estava 

    mq_close(open_queue);
}

void user_register() {
    FILE *is_user;
    char filename[FILE_PREFIX + USER_MAX];

    printf("\nInsira um nome de usuario: ");
    scanf("%s", user);

    if(strcmp(user,"all") == 0){
        printf("\nNome de usuário inválido!\n");
        exit(0);        
    }

    strcpy(filename, "/dev/mqueue/chat-");
    strcat(filename, user);

    is_user = fopen(filename, "r");

    if(is_user == NULL) {
        create_queue(filename);
    }
    else {
        printf("\n\nNome de usuario indisponivel!\n\n");
        exit(0);
    }

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
        else if(message[i] == ':' && flag != 2) {
            if(flag == 0)
                remet[j] = '\0';
            else if(flag == 1)
                dest[j] = '\0';

            flag++;
            j = 0;
        }
        else {
            msg[j] = message[i];
            j++;
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
        else if(message[i] == ' ' && flag == 0) {
            dest[j] = '\0';
            flag++;
            j = 0;
        }
        else if(flag == 1){
            msg[j] = message[i];
            j++;
        }

        i++;
    }

    msg[j] = '\0';
}

void format_msg(char *dest, char *msg_sent, char *msg) {
    strcpy(msg, user);
    strcat(msg, ":");
    strcat(msg, dest);
    strcat(msg, ":");
    strcat(msg, msg_sent);
}

char *split_file_name(char *name){
    char *ptr = strtok(name, "-");
    ptr = strtok(NULL, "-");
    
    return ptr;
}

USERS *insert_in_users_list(USERS *l, char *user_name){
    USERS *new = (USERS*) malloc(sizeof(USERS));

    new->user_name = user_name;
    new->next = l;

    return new;
}

USERS *get_users_list(){
    USERS *users_list = NULL;
    DIR *directory = opendir("/dev/mqueue"); 
    struct dirent *files;
    char *user_name;

    if(directory == NULL)
        fprintf(stderr, "\nOcorreu um erro ao carregar usuários online.\n");
    else{
        while((files = readdir(directory)) != NULL){
            user_name = split_file_name(files->d_name);
            if(user_name != NULL){
                users_list = insert_in_users_list(users_list, user_name);
            }
        }
    }
    closedir(directory);

    return users_list; 
}

void destroy_users_list(USERS *l) {
    USERS *aux = l;
    USERS *temp;

    while(aux != NULL) {
        temp = aux;
        aux = aux->next;
        free(temp);
    }
}

void *retry_send_message(void *m) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char dest[USER_MAX], msg_sent[MSG_MAX];
    char msg[MSGLEN];
    char *message = (char *) m;

    split_message_sent(message, dest, msg_sent);
    format_msg(dest, msg_sent, msg);

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", dest);
    else {
        int i;
        for(i = 0; i < 3; i++) {
            sleep(0.5);
            if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) >= 0)
                break;
        }

        if(i == 3)
            fprintf(stderr, "\nErro ao enviar mensagem para @%s\n", dest);
    
        mq_close(open_queue);
    }

    int r = 0;
    pthread_exit((void *)&r);
}

void send_message_in_queue(char *dest, char *msg_sent) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char msg[MSGLEN];
    
    format_msg(dest, msg_sent, msg);

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_RDWR);
    
    pthread_t retry;

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", dest);
    else {
        if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0)
            pthread_create(&retry, NULL, retry_send_message, (void *) msg);
    
        mq_close(open_queue);
    }
}

void *send_message(void *m) {
    char dest[USER_MAX], msg_sent[MSGLEN];
    char *message = (char *) m;

    USERS *users_list = NULL;
    USERS *aux;

    split_message_sent(message, dest, msg_sent);

    if(strcmp(dest, "all") == 0) {
        users_list = get_users_list();
        aux = users_list;

        while(aux != NULL) {
            if(strcmp(aux->user_name, user) != 0)
                send_message_in_queue(aux->user_name, msg_sent);
            aux = aux->next;
        }

        destroy_users_list(users_list);
    }
    else {
        send_message_in_queue(dest, msg_sent);
    }

    int r = 0;
    pthread_exit((void *)&r);
}

void *listen() {
    char queue[QUEUE_PREFIX + USER_MAX];
    strcpy(queue, "/chat-");
    strcat(queue, user);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    char message[MSGLEN], remet[USER_MAX], dest[USER_MAX], msg[MSG_MAX];

    while(1) {
        if((mq_receive(open_queue, (void *) message, MSGLEN, 0)) < 0) {
            perror("\nmq_receive");
            mq_unlink(user_queue);
            exit(1);
        }

        split_message_received(message, remet, dest, msg);
        fprintf(stderr, ">> %s: %s\n\n", remet, msg);

        memset(message, 0, strlen(message));
    }

    mq_close(open_queue);
    
    int r = 0;
    pthread_exit((void *)&r);
}

void handler_interruption() {
    fprintf(stderr, "\nPara sair, digite 'sair'\n");
}

void users_list() {
    USERS *users_list = get_users_list();
    USERS *aux = users_list;

    if(users_list) {
        fprintf(stderr, "\nLista de Usuários Online:\n\n");
        while(aux != NULL) {
            fprintf(stderr, "%s\n", aux->user_name);
            aux = aux->next;
        }

        destroy_users_list(users_list);
    }
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
        else if(strcmp(message, "list") == 0){            
            users_list();
            continue;
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