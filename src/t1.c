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
#define CONFIRM_MESSAGE 30
#define CHANNEL_MAX 9
#define CHANNEL_MSGLEN 511

char user[USER_MAX];
char user_queue[QUEUE_PREFIX + USER_MAX];

typedef struct channels {
    char *channel_name;
    struct channels *next;
} CHANNELS;

typedef struct users {
    char *user_name;
    struct users *next;
} USERS;

typedef struct r_message {
    char *dest;
    char *msg_sent;
    int broadcast;
} R_MESSAGE;

typedef struct confirm_message {
    char *remet;
    char *dest;
} C_MESSAGE;

CHANNELS *channels_list = NULL;
CHANNELS *my_channels = NULL;

void get_out();

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

void destroy_channel(char *channel_name) {
    char *channel_queue = malloc(sizeof(char) * 17);

    strcpy(channel_queue, "/canal-");
    strcat(channel_queue, channel_name);

    char *channel_users = malloc(sizeof(char) * 26);

    strcpy(channel_users, "canal-users-");
    strcat(channel_users, channel_name);
    strcat(channel_users, ".txt");

    remove(channel_users);
    mq_unlink(channel_queue);

    free(channel_queue);
    free(channel_users);
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

    new->user_name = malloc(sizeof(char) * USER_MAX);

    strcpy(new->user_name, user_name);
    new->next = l;

    return new;
}

USERS *get_users_list(){
    USERS *users_list = NULL;
    DIR *directory = opendir("/dev/mqueue"); 
    struct dirent *files;
    char *user_name;

    char test[5];

    if(directory == NULL)
        fprintf(stderr, "\nERRO:Ocorreu um erro ao carregar usuários online.\n\n");
    else{
        while((files = readdir(directory)) != NULL){
            strncpy(test, files->d_name, 5);
            if(strcmp(test, "chat-") == 0) {
                user_name = split_file_name(files->d_name);
                if(user_name != NULL){
                    users_list = insert_in_users_list(users_list, user_name);
                }
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
    char msg[MSGLEN];
    R_MESSAGE *r_msg = (R_MESSAGE *) m;

    if(r_msg->broadcast == 1) {
        format_msg("all", r_msg->msg_sent, msg);
    }
    else {
        format_msg(r_msg->dest, r_msg->msg_sent, msg);
    }

    strcpy(queue, "/chat-");
    strcat(queue, r_msg->dest);

    mqd_t open_queue = mq_open(queue, O_WRONLY);
    pthread_t confirm;

    if(open_queue < 0)
        fprintf(stderr, "\nUNKNOWNUSER %s\n\n", r_msg->dest);
    else {
        int i;
        for(i = 0; i < 3; i++) {
            sleep(0.5);
            if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) >= 0) {
                break;
            }            
        }

        if(i == 3)
            fprintf(stderr, "\nERRO %s:%s:%s\n\n", user, r_msg->dest, r_msg->msg_sent);
    
        mq_close(open_queue);
    }

    int r = 0;
    pthread_exit((void *)&r);
}

void send_message_in_queue(char *dest, char *msg_sent, int broadcast) {
    char queue[QUEUE_PREFIX + USER_MAX];
    char msg[MSGLEN];

    if(broadcast == 1) {
        format_msg("all", msg_sent, msg);
    }
    else {
        format_msg(dest, msg_sent, msg);
    }

    strcpy(queue, "/chat-");
    strcat(queue, dest);

    mqd_t open_queue = mq_open(queue, O_WRONLY);
    
    pthread_t retry;
    R_MESSAGE *r_msg = (R_MESSAGE*) malloc(sizeof(R_MESSAGE));

    r_msg->dest = dest;
    r_msg->msg_sent = msg_sent;
    r_msg->broadcast = broadcast;

    pthread_t confirm;

    if(open_queue < 0) {
        fprintf(stderr, "\nUNKNOWNUSER %s\n\n", dest);
        perror("mq_open: send");
    }
    else {
        if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0)
            pthread_create(&retry, NULL, retry_send_message, (void *) r_msg);

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
            if(strcmp(aux->user_name, user) != 0) {
                send_message_in_queue(aux->user_name, msg_sent, 1);
            }
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

CHANNELS *remove_my_channels_list(CHANNELS *l, char *channel_name) {
    CHANNELS *aux = l;
    CHANNELS *ant;

    while(aux != NULL){
        if(strcmp(aux->channel_name, channel_name) == 0)
            break;
        ant = aux;
        aux = aux->next;
    }

    if(aux == l){
        l = l->next;
    }
    else{
        ant->next = aux->next;
    }

    return (l);
}

void *listen() {
    char queue[QUEUE_PREFIX + USER_MAX];
    strcpy(queue, "/chat-");
    strcat(queue, user);

    mqd_t open_queue = mq_open(queue, O_RDWR);

    char *message = malloc(sizeof(char) * MSGLEN);

    char *remet = malloc(sizeof(char) * USER_MAX);
    char *dest = malloc(sizeof(char) * USER_MAX);
    char *msg = malloc(sizeof(char) * MSG_MAX);

    C_MESSAGE *confirm_msg = (C_MESSAGE*) malloc(sizeof(C_MESSAGE));
    pthread_t confirm;
    
    char *channel_name = malloc(sizeof(char) * CHANNEL_MAX);

    while(1) {
        if((mq_receive(open_queue, (void *) message, MSGLEN, 0)) < 0) {
            perror("\nmq_receive");
            get_out();
            exit(1);
        }

        split_message_received(message, remet, dest, msg);

        confirm_msg->remet = remet;
        confirm_msg->dest = dest;

        if(strcmp(dest, "all") == 0)
            fprintf(stdout, ">> Broadcast de %s: %s\n\n", remet, msg);
        else if(remet[0] == '#') {
            if(strcmp(msg, "DESTROYED") == 0) {
                strncpy(channel_name, channel + 1, CHANNEL_MAX);
                fprintf(stdout, "Canal %s destruído!\n\n", channel_name);
                my_channels = remove_my_channels_list(my_channels, channel_name);
            }
            else {
                fprintf(stdout, ">> %s: %s\n\n", remet, msg);
            }
        }
        else {
            fprintf(stdout, ">> %s: %s\n\n", remet, msg);
        }

        memset(message, 0, strlen(message));
    }

    mq_close(open_queue);
    
    int r = 0;
    pthread_exit((void *)&r);
}

void handler_interruption() {
    fprintf(stdout, "\nPara sair, digite 'sair'\n\n");
}

void users_list() {
    USERS *users_list = get_users_list();
    USERS *aux = users_list;

    if(users_list) {
        fprintf(stdout, "\nLista de Usuários Online:\n\n");
        while(aux != NULL) {
            fprintf(stdout, "%s\n", aux->user_name);
            aux = aux->next;
        }

        destroy_users_list(users_list);
    }
}

void split_channel_message(char *message, char *remet, char *msg) {
    int i = 0, j = 0, flag = 0; 

    while(message[i] != '\0') {
        if(message[i] != ':' && flag == 0){
            remet[j] = message[i];
            j++;
        }
        else if(message[i] != ':' && flag == 1){
            msg[j] = message[i];
            j++;
        }
        else {
            remet[j] = '\0';
            flag++;
            j = 0;
        }

        i++;
    }

    msg[j] = '\0';
}

void join_in_channel(char *channel_name, char *user_join) {
    char filename[26];

    strcpy(filename, "canal-users-");
    strcat(filename, channel_name);
    strcat(filename, ".txt");

    FILE *fp = fopen(filename, "a+");
    char aux[USER_MAX];

    if(fp == NULL) {
        return;
    }
    else {
        while(!feof(fp)) {
            fscanf(fp, "%s", aux);

            if(strcmp(aux, user_join) == 0) {
                return;
            }
        }

        fprintf(fp, "\n%s", user_join);
        fclose(fp);
    }
}

void leave_channel(char *channel_name, char *user_leave) {
    char filename[26];
    strcpy(filename, "canal-users-");
    strcat(filename, channel_name);
    strcat(filename, ".txt");

    char new_filename[18];
    strcpy(new_filename, "new-");
    strcat(new_filename, channel_name);
    strcat(new_filename, ".txt");

    FILE *fp = fopen(filename, "r");
    FILE *new = fopen(new_filename, "w+");

    char aux[USER_MAX];

    while(!feof(fp)) {
        fscanf(fp, "%s", aux);

        if(strcmp(aux, user_leave) != 0) {
            fprintf(new, "\n%s", aux);
        }
    }

    fclose(fp);
    fclose(new);

    remove(filename);
    rename(new_filename, filename);
}

int user_in_channel(char *channel_users, char *channel_user) {
    FILE *fp = fopen(channel_users, "r");

    if(fp == NULL)
        return 0;

    char *aux = malloc(sizeof(char) * USER_MAX);

    while(!feof(fp)) {
        fscanf(fp, "%s", aux);
        if(strcmp(aux, channel_user) == 0) {
            free(aux);
            fclose(fp);
            return 1;
        }
    }

    free(aux);
    fclose(fp);

    return 0;
}

void format_not_a_member_message(char *message, char *channel_name, char *dest) {
    strcpy(message, "#");
    strcat(message, channel_name);
    strcat(message, ":");
    strcat(message, dest);
    strcat(message, ":");
    strcat(message, "NOT A MEMBER");
}

void send_not_a_member_message(char *channel_name, char *dest) {
    char *queue_name = malloc(sizeof(char) * (USER_MAX + 6));
    strcpy(queue_name, "/chat-");
    strcat(queue_name, dest);

    mqd_t open_queue = mq_open(queue_name, O_WRONLY);

    if(open_queue < 0) {
        perror("mq_open");
        return;
    }
    
    char *message = malloc(sizeof(char) * 34);

    format_not_a_member_message(message, channel_name, dest);

    if((mq_send(open_queue, (void *) message, strlen(message), 0)) < 0) {
        perror("mq_send");
        mq_close(open_queue);
        return;
    }

    mq_close(open_queue);
}

void format_channel_user_message(char *message, char *channel_name, char *remet, char *dest, char *msg) {
    strcpy(message, "#");
    strcat(message, channel_name);
    strcat(message, ":");
    strcat(message, dest);
    strcat(message, ":");
    strcat(message, "<");
    strcat(message, remet);
    strcat(message, ">");
    strcat(message, " ");
    strcat(message, msg);
}

void format_destroyed_message(char *message, char *channel_name, char *dest, char *msg) {
    strcpy(message, "#");
    strcat(message, channel_name);
    strcat(message, ":");
    strcat(message, dest);
    strcat(message, ":");
    strcat(message, msg);
}

void send_channel_user_message(char *channel_name, char *remet , char *dest, char *msg) {
    char *queue_name = malloc(sizeof(char) * (USER_MAX + 6));
    strcpy(queue_name, "/chat-");
    strcat(queue_name, dest);

    mqd_t open_queue = mq_open(queue_name, O_WRONLY);

    if(open_queue < 0) {
        perror("mq_open");
        return;
    }
    
    char *message = malloc(sizeof(char) * MSGLEN);

    if(strcmp(msg, "DESTROYED") == 0) {
        format_destroyed_message(message, channel_name, dest, msg);
    }
    else {
        format_channel_user_message(message, channel_name, remet, dest, msg);
    } 

    if((mq_send(open_queue, (void *) message, strlen(message), 0)) < 0) {
        perror("mq_open");
        mq_close(open_queue);
        return;
    }

    mq_close(open_queue);
    
}

void send_message_to_channel(char *channel_name, char *remet, char *msg) {
    char *channel_users = malloc(sizeof(char) * 26);

    strcpy(channel_users, "canal-users-");
    strcat(channel_users, channel_name);
    strcat(channel_users, ".txt");

    if(user_in_channel(channel_users, remet) == 0) {
        send_not_a_member_message(channel_name, remet);
        return;
    }

    FILE *fp = fopen(channel_users, "r");

    if(fp == NULL)
        return;

    char *aux = malloc(sizeof(char) * USER_MAX);

    while(!feof(fp)) {
        fscanf(fp, "%s", aux);
        if(strcmp(remet, aux) != 0) {
            send_channel_user_message(channel_name, remet, aux, msg);
        }
    }

    free(aux);

}

void *listen_channel(void *cn) {
    char *channel_name = (char *) cn;
    char channel_queue[CHANNEL_MAX + 7];

    strcpy(channel_queue, "/canal-");
    strcat(channel_queue, channel_name);

    mqd_t open_queue = mq_open(channel_queue, O_RDONLY);

    if(open_queue < 0) {
        perror("mq_open (channel)");
        get_out();
        exit(1);
    }

    char *message = malloc(sizeof(char) * CHANNEL_MSGLEN);
    char *remet = malloc(sizeof(char) * USER_MAX);
    char *msg = malloc(sizeof(char) * MSG_MAX);

    while(1) {
        if((mq_receive(open_queue, (void *) message, CHANNEL_MSGLEN, 0)) < 0) {
            perror("\nmq_receive (channel)");
            get_out();
            exit(1);
        }

        split_channel_message(message, remet, msg);

        if(strcmp(msg, "JOIN") == 0) {
            join_in_channel(channel_name, remet);
        }
        else if(strcmp(msg, "LEAVE") == 0) {
            leave_channel(channel_name, remet);
        }
        else {
            send_message_to_channel(channel_name, remet, msg);
        }

        memset(message, 0, strlen(message));
        memset(remet, 0, strlen(remet));
        memset(msg, 0, strlen(msg));
    }
}

CHANNELS *insert_in_channels_list(char *channel_name, CHANNELS *list) {
    CHANNELS *new = (CHANNELS*) malloc(sizeof(CHANNELS));

    new->channel_name = malloc(sizeof(char) * CHANNEL_MAX);

    strcpy(new->channel_name, channel_name);
    new->next = list;

    return new;
}

void create_channel_queue(char *channel_name) {
    struct mq_attr attr;
    mode_t oldMask, newMask;

    char channel_queue[CHANNEL_MAX + 7];

    strcpy(channel_queue, "/canal-");
    strcat(channel_queue, channel_name);

    attr.mq_maxmsg = 10; // capacidade para 10 mensagens
    attr.mq_msgsize = sizeof(char) * CHANNEL_MSGLEN; // tamanho de cada mensagem
    attr.mq_flags = 0;

    oldMask = umask((mode_t)0); //Pega umask antigo
    umask(0155); //Proíbe execução para o dono, leitura e execução para os demais 

    mqd_t open_queue = mq_open(channel_queue, O_RDWR|O_CREAT, 0666, &attr);
    pthread_t channel;

    if(open_queue < 0) {
        fprintf(stderr, "\nERRO: Nao foi possivel criar o canal.\n\n");
    }
    else {
        fprintf(stdout, "\nCanal %s criado com sucesso!\n\n", channel_name);
        channels_list = insert_in_channels_list(channel_name, channels_list);
        pthread_create(&channel, NULL, listen_channel, (void*) channel_name);
        mq_close(open_queue);
    }

    umask(oldMask); //Seta umask novamente para o que estava
}

int channel_exists(char *channel_name) {
    DIR *directory = opendir("/dev/mqueue"); 
    struct dirent *files;
    char *user_name;

    char filename[CHANNEL_MAX + 6];
    strcpy(filename, "canal-");
    strcat(filename, channel_name);

    if(directory == NULL)
        fprintf(stderr, "\nERRO: Ocorreu um erro ao criar canal.\n\n");
    else{
        while((files = readdir(directory)) != NULL){
            if(strcmp(files->d_name, filename) == 0)
                return 1;
        }
    }
    closedir(directory);

    return 0;
}

void create_channel(char *message) {
    int len = strlen(message);

    if(len > (6+CHANNEL_MAX)){
        fprintf(stderr, "\nERRO: O nome do canal não pode ter mais que 9 caracteres!\n\n");
        return;
    }

    char *channel_name = malloc(sizeof(char) * CHANNEL_MAX);
    strncpy(channel_name, message + 6, CHANNEL_MAX);

    if(channel_exists(channel_name) == 1) {
        fprintf(stderr, "\nERRO: Canal já existe!\n\n");
    }
    else {
        create_channel_queue(channel_name);
        join_in_channel(channel_name, user);
    }
}

void format_channel_message(char *msg, char *message) {
    strcpy(msg, user);
    strcat(msg, ":");
    strcat(msg, message);
}

mqd_t get_channel_queue(char *channel_name) {
    char channel_queue[CHANNEL_MAX + 7];

    strcpy(channel_queue, "/canal-");
    strcat(channel_queue, channel_name);

    mqd_t open_queue = mq_open(channel_queue, O_WRONLY);

    return open_queue;
}

void send_join_message(char *channel_name) {
    mqd_t open_queue = get_channel_queue(channel_name);

    if(!open_queue) {
        fprintf(stderr, "\nERRO: Não foi possível entrar no canal %s!\n\n", channel_name);
    }    

    char *msg = malloc(sizeof(char) * 15);
    format_channel_message(msg, "JOIN");

    if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0){
        fprintf(stderr, "\nERRO: Não foi possível entrar no canal.\n\n");
        return;
    }

    mq_close(open_queue);
}

void send_leave_message(char *channel_name) {
    mqd_t open_queue = get_channel_queue(channel_name);

    if(!open_queue) {
        fprintf(stderr, "\nERRO: Não foi possível sair do canal %s!\n", channel_name);
    }    

    char *msg = malloc(sizeof(char) * 16);
    format_channel_message(msg, "LEAVE");

    if((mq_send(open_queue, (void *) msg, strlen(msg), 0)) < 0){
        fprintf(stderr, "\nERRO: Não foi possível sair do canal.\n");
        return;
    }

    mq_close(open_queue);
}

void join(char *message) {
    int len = strlen(message);

    if(len > (5 + CHANNEL_MAX)) {
        fprintf(stderr, "\nERRO: O canal não existe!\n");
        return;
    }

    char channel_name[CHANNEL_MAX];
    strncpy(channel_name, message + 5, CHANNEL_MAX);

    if(channel_exists(channel_name) == 1) {
        send_join_message(channel_name);
        my_channels = insert_in_channels_list(channel_name, my_channels);
    }
    else {
        fprintf(stderr, "\nERRO: O canal não existe!\n\n");
    }

}

void leave(char *message) {
    int len = strlen(message);

    if(len > (6 + CHANNEL_MAX)) {
        fprintf(stderr, "\nERRO: O canal não existe!\n\n");
        return;
    }

    char channel_name[CHANNEL_MAX];
    strncpy(channel_name, message + 6, CHANNEL_MAX);

    if(channel_exists(channel_name) == 1) {
        send_leave_message(channel_name);
        my_channels = remove_my_channels_list(my_channels, channel_name);
    }
    else {
        fprintf(stderr, "\nERRO: O canal não existe!\n\n");
    }
}

void split_channel_message_queue(char *message, char *channel_name, char *msg) {
    int j = 0, flag = 0;

    for(int i = 1; i < strlen(message); i++) {
        if(message[i] != ' ' && flag == 0) {
            channel_name[j] = message[i];
            j++;
        }
        else if(message[i] == ' ' && flag == 0) {
            channel_name[j] = '\0';
            flag++;
            j = 0;
        }
        else {
            msg[j] = message[i];
            j++;
        }
    }

    msg[j] = '\0';
}

void send_message_in_channel_queue(char *message) {
    char *channel_name = malloc(sizeof(char) * CHANNEL_MAX);
    char *msg = malloc(sizeof(char) * MSG_MAX);

    split_channel_message_queue(message, channel_name, msg);

    char *queue_name = malloc(sizeof(char) * (CHANNEL_MAX + 7));
    strcpy(queue_name, "/canal-");
    strcat(queue_name, channel_name);

    mqd_t open_queue = mq_open(queue_name, O_WRONLY);

    if(open_queue < 0) {
        fprintf(stderr, "\nERRO: Não foi possível enviar a mensagem ao canal.\n\n");
        return;
    }

    char *msg_queue = malloc(sizeof(char) * CHANNEL_MSGLEN);

    format_channel_message(msg_queue, msg);

    if((mq_send(open_queue, (void *) msg_queue, strlen(msg_queue), 0)) < 0){
        fprintf(stderr, "\nERRO: Não foi possível enviar a mensagem ao canal.\n\n");
        return;
    }

    mq_close(open_queue);
}

void send_destroy_msg(char *channel_name) {
    char *message = malloc(sizeof(char) * 21);

    strcpy(message, "#");
    strcat(message, channel_name);
    strcat(message, " ");
    strcat(message, "DESTROYED");

    send_message_in_channel_queue(message);
}

void get_out() {
    CHANNELS *aux = channels_list;
    CHANNELS *temp;

    while(aux != NULL) {
        send_destroy_msg(aux->channel_name);
        fprintf(stdout, "\nSaindo...\n\n");
        sleep(3);
        destroy_channel(aux->channel_name);
        temp = aux;
        aux = aux->next;
        free(temp);
    }

    aux = my_channels;

    while(aux != NULL) {
        send_leave_message(aux->channel_name);
        temp = aux;
        aux = aux->next;
        free(temp);
    }

    mq_unlink(user_queue);
}

int main() {
    user_register();
    
    pthread_t send, receive;

    pthread_create(&receive, NULL, listen, NULL);

    char message[MESSAGE];

    printf("\n======================== VOCE ENTROU NO CHAT ========================\n");

    signal(SIGINT, handler_interruption);

    char channel_test[6];
    char join_test[5];
    char leave_test[6];

    while(1) {
        printf("\n");
        setbuf(stdin, NULL);
        scanf("%[^\n]s", message);

        strncpy(channel_test, message, 5);
        channel_test[5] = '\0';
        
        if(strcmp(channel_test, "canal") == 0) {
            create_channel(message);
            continue;
        }

        strncpy(join_test, message, 4);
        join_test[4] = '\0';

        if(strcmp(join_test, "join") == 0) {
            join(message);
            continue;
        }

        strncpy(leave_test, message, 5);
        leave_test[5] = '\0';
        
        if(strcmp(leave_test, "leave") == 0) {
            leave(message);
            continue;
        }

        if(strcmp(message, "sair") == 0){
            get_out();
            exit(0);
        }
        else if(strcmp(message, "list") == 0){            
            users_list();
            continue;
        }
        else if (message[0] == '#') {
            send_message_in_channel_queue(message);
            continue;
        }
        else if(message[0] != '@') {
            fprintf(stderr, "\nUNKNOWNCOMMAND %s\n\n", message);
            continue;
        }

        pthread_create(&send, NULL, send_message, (void*) message);
        pthread_join(send, NULL);
    }

    return 0;
}
