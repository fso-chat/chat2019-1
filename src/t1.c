#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <mqueue.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

#define USER_MAX 10
#define FILE_PREFIX 17
#define QUEUE_PREFIX 6
#define MSGLEN 528
#define MSG_MAX 500
#define MESSAGE 512
#define CODE_LEN 5
#define CONFIRM_MESSAGE 30

char user[USER_MAX];
char user_queue[QUEUE_PREFIX + USER_MAX];

typedef struct users {
    char *user_name;
    struct users *next;
} USERS;

typedef struct r_message {
    char *dest;
    char *msg_sent;
    char *code;
    int broadcast;
} R_MESSAGE;

typedef struct confirm_message {
    char *remet;
    char *dest;
    char *code;
} C_MESSAGE;

void create_queue() {
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

    if(strcmp(user, "all") == 0){
        printf("\nNome de usuário inválido!\n");
        exit(0);        
    }

    strcpy(filename, "/dev/mqueue/chat-");
    strcat(filename, user);

    is_user = fopen(filename, "r");

    if(is_user == NULL) {
        create_queue();
        // create_confirm_queue();
    }
    else {
        printf("\n\nNome de usuario indisponível!\n\n");
        exit(0);
    }

}

void split_message_received(char *message, char *remet, char *dest, char *msg, char *code) {
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
        else if(message[i] == ':' && flag != 3) {
            if(flag == 0)
                remet[j] = '\0';
            else if(flag == 1)
                dest[j] = '\0';
            else if(flag == 2)
                msg[j] = '\0';

            flag++;
            j = 0;
        }
        else {
            code[j] = message[i];
            j++;
        }

        i++;
    }

    code[j] = '\0';
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

void format_msg(char *dest, char *msg_sent, char *code, char *msg) {
    strcpy(msg, user);
    strcat(msg, ":");
    strcat(msg, dest);
    strcat(msg, ":");
    strcat(msg, msg_sent);
    strcat(msg, ":");
    strcat(msg, code);
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

void split_confirm_message(char *message, char *remet, char *dest, char *code, char *type) {
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
            code[j] = message[i];
            j++;
        }
        else if(message[i] == ':' && flag != 3) {
            if(flag == 0)
                remet[j] = '\0';
            else if(flag == 1)
                dest[j] = '\0';
            else if(flag == 2)
                code[j] = '\0';

            flag++;
            j = 0;
        }
        else {
            type[j] = message[i];
            j++;
        }

        i++;
    }

    type[j] = '\0';
}

void format_confirm_message(char *msg, char *remet, char *dest, char *code, char *type) {
    strcpy(msg, remet);
    strcat(msg, ":");
    strcat(msg, dest);
    strcat(msg, ":");
    strcat(msg, code);
    strcat(msg, ":");
    strcat(msg, type);
}

void *create_code_queue(void *c) {
    char *code = (char *) c;

    fprintf(stderr, "\ncodigo da msg: %s\n", code);

    char queue_name[CODE_LEN+1];

    strcpy(queue_name, "/");
    strcat(queue_name, code);

    fprintf(stderr, "\nnome da fila: %s\n", queue_name);        

    struct mq_attr attr;

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * CONFIRM_MESSAGE; // tamanho de cada mensagem
    attr.mq_flags = 0;

    mqd_t open_queue = mq_open(queue_name, O_RDWR|O_CREAT, 0666, &attr);

    if(open_queue < 0) {
        perror("mq_open\n");
        exit(1);
    }

    char confirm_message[CONFIRM_MESSAGE];
    char remet[USER_MAX], dest[USER_MAX], code_r[CODE_LEN], type[2];
    char msg[CONFIRM_MESSAGE];
    
    if((mq_receive(open_queue, (void *)confirm_message, CONFIRM_MESSAGE, 0)) < 0) {
        perror("\nmq_receive");
        mq_unlink(queue_name);
        exit(1);
    }
    else {
        fprintf(stderr, "\nMsg recebida: %s", confirm_message);
        split_confirm_message(confirm_message, remet, dest, code_r, type);

        if(strcmp(dest, user) == 0 && strcmp(type, "?") == 0 && strcmp(code_r, code) == 0) {
            format_confirm_message(msg, dest, remet, code_r, "y");
        }
        else {
            format_confirm_message(msg, dest, remet, code_r, "n");
        }

        if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0){
            perror("mq_send\n");
            mq_unlink(queue_name);
            exit(1);
        }
    }

    mq_close(open_queue);
    int r = 0;
    pthread_exit((void *)&r);

}

void *retry_send_message(void *m) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char msg[MSGLEN];
    R_MESSAGE *r_msg = (R_MESSAGE *) m;

    if(r_msg->broadcast == 1) {
        format_msg("all", r_msg->msg_sent, r_msg->code, msg);
    }
    else {
        format_msg(r_msg->dest, r_msg->msg_sent, r_msg->code, msg);
    }

    strcpy(queue, "/chat-");
    strcat(queue, r_msg->dest);

    mqd_t open_queue = mq_open(queue, O_WRONLY);
    pthread_t confirm;

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", r_msg->dest);
    else {
        int i;
        for(i = 0; i < 3; i++) {
            sleep(0.5);
            if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) >= 0) {
                pthread_create(&confirm, NULL, create_code_queue, (void *) r_msg->code);
                break;
            }            
        }

        if(i == 3)
            fprintf(stderr, "\nERRO %s:%s:%s\n", user, r_msg->dest, r_msg->msg_sent);
    
        mq_close(open_queue);
    }

    int r = 0;
    pthread_exit((void *)&r);
}

char *get_message_code() {
    char *code = malloc(sizeof(char) * CODE_LEN);
    struct timeval time;
    
    gettimeofday(&time, NULL);
    srand((time.tv_sec * 1000) + (time.tv_usec/1000));
    int n = rand() % (100000 + 1);

    sprintf(code, "%05d", n);

    return code;
}

void send_message_in_queue(char *dest, char *msg_sent, int broadcast) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char msg[MSGLEN];

    char *code = get_message_code();
    fprintf(stderr, "\ncodigo gerado: %s\n", code);

    if(broadcast == 1) {
        format_msg("all", msg_sent, code, msg);
    }
    else {
        format_msg(dest, msg_sent, code, msg);
    }

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_WRONLY);
    
    pthread_t retry;
    R_MESSAGE *r_msg = (R_MESSAGE*) malloc(sizeof(R_MESSAGE));

    r_msg->dest = dest;
    r_msg->msg_sent = msg_sent;
    r_msg->code = code;
    r_msg->broadcast = broadcast;

    pthread_t confirm;

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n", dest);
    else {
        if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0)
            pthread_create(&retry, NULL, retry_send_message, (void *) r_msg);
        else
            pthread_create(&confirm, NULL, create_code_queue, (void *) code);
    
        free(r_msg);
        mq_close(open_queue);
    }
}

void *send_message(void *m) {
    char dest[USER_MAX], msg_sent[MSG_MAX];
    char *message = (char *) m;

    USERS *users_list = NULL;
    USERS *aux;

    split_message_sent(message, dest, msg_sent);

    if(strcmp(dest, "all") == 0) {
        users_list = get_users_list();
        aux = users_list;

        while(aux != NULL) {
            if(strcmp(aux->user_name, user) != 0)
                send_message_in_queue(aux->user_name, msg_sent, 1);
            aux = aux->next;
        }

        destroy_users_list(users_list);
    }
    else {
        send_message_in_queue(dest, msg_sent, 0);
    }

    memset(message, 0, strlen(message));
    int r = 0;
    pthread_exit((void *)&r);
}

void *confirm_message_received(void *c) {
    C_MESSAGE *c_msg = (C_MESSAGE *) c;

    char queue_name[CODE_LEN + 12];
    strcpy(queue_name, "/dev/mqueue/");
    strcat(queue_name, c_msg->code);

    fprintf(stderr, "\ncode: %s\n", c_msg->code);
    fprintf(stderr, "\nfila: %s\n", queue_name);

    char msg[CONFIRM_MESSAGE];
    char confirm_message[CONFIRM_MESSAGE];

    format_confirm_message(msg, c_msg->dest, c_msg->remet, c_msg->code, "?");

    mqd_t open_queue = mq_open(queue_name, O_RDWR);

    if(open_queue < 0){
        perror("\nmq_open: could not confirm message received.");
    }
    else {
        if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0){
            perror("\nmq_send: could not confirm message received.");
        }
        else{
            fprintf(stderr, "tam: sizeof %lu, const %d", sizeof(confirm_message), CONFIRM_MESSAGE);
            if((mq_receive(open_queue, (void *)confirm_message, CONFIRM_MESSAGE, 0)) < 0) {
                perror("\nmq_receive: could not confirm message received");
            }

            char remet[USER_MAX], dest[USER_MAX], code_r[CODE_LEN], type[2];

            split_confirm_message(confirm_message, remet, dest, code_r, type);

            if(strcmp(type, "n") == 0)
                fprintf(stderr, "\nCould not confirm message received from %s\n", c_msg->remet);
            else
                fprintf(stderr, "\nMessage from %s confirmed", c_msg->remet);
        }

        mq_close(open_queue);
    }
    
    mq_unlink(queue_name);
    free(c_msg);
    
    int r = 0;
    pthread_exit((void *)&r);
}

void *listen() {
    char queue[QUEUE_PREFIX + USER_MAX];
    strcpy(queue, "/chat-");
    strcat(queue, user);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    char message[MSGLEN], remet[USER_MAX], dest[USER_MAX], msg[MSG_MAX], code[CODE_LEN];
    C_MESSAGE *confirm_msg = (C_MESSAGE*) malloc(sizeof(C_MESSAGE));
    pthread_t confirm;

    while(1) {
        if((mq_receive(open_queue, (void *) message, MSGLEN, 0)) < 0) {
            perror("\nmq_receive");
            mq_unlink(user_queue);
            exit(1);
        }

        split_message_received(message, remet, dest, msg, code);

        confirm_msg->remet = remet;
        confirm_msg->dest = dest;
        confirm_msg->code = code;
        
        pthread_create(&confirm, NULL, confirm_message_received, (void *) confirm_msg);

        if(strcmp(dest, "all") == 0)
            fprintf(stderr, ">> Broadcast de %s: %s\n\n", remet, msg);
        else
            fprintf(stderr, ">> %s: %s\n\n", remet, msg);

        memset(message, 0, strlen(message));
    }

    mq_close(open_queue);
    free(confirm_msg);
    
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

    char message[MESSAGE];

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