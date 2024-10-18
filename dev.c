#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>

#define MAXLEN 128
#define MAXCREDENTIALSLENGTH 20

const int CODICE_HANGING = 1,
          CODICE_SHOW = 2,
          CODICE_CHAT = 3,
          CODICE_SHARE = 4,
          CODICE_OUT = 5,
          CODICE_SENDUTENTIONLINE = 6,
          CODICE_SENDUSERPORT = 7;

const char SUCCESSO = '0',
           ERRORE = '1',
           CODICE_CHATGRUPPO = '8',
           *NEW_USER_GROUP = "new_user";
char       *LEAVE_GROUP = "leave_group",
           *ARRIVE_FILE = "arrive_file";         

int connesso = 0, // se connesso al server, ovvero il socket è connesso
    logged = 0,   // se registrato sul server, ovvero hai fatto signup
    online = 0,   // se online, ovvero hai fatto in
    sdserver,     // socket descriptor del server
    myPort;       // porta su cui mi connetto

char utenteOnline[MAXLEN], utenteinchat[MAXLEN];

struct sockaddr_in server;              
int listener, fdmax;

fd_set master;  // qua ci sono tutti i descrittori (lista di socket descriptor)
fd_set read_fds; // qua passano (colabrodo) i socket pronti, da leggere

 // ***************** salvo tutti i socket descriptor in una lista
struct socketDescriptor{
    int i;
    int porta;
    int chat;   // 0 se qualcuno ha iniziato la chat con me, 1 se l'ho iniziata io
    char username[MAXLEN];
    struct socketDescriptor* next;
};

// lista di socket descriptor
struct socketDescriptor* Sd = NULL;

// inserisce all'interno della struttura "socketDescriptor"
void inserisciTestaSockDesc(char*username, int i, int chat, int porta) {	
    struct socketDescriptor *p = malloc(sizeof(*Sd));
    strcpy(p->username,username);
    p->i = i;
    p->chat = chat;
    p->porta = porta;
    p->next = Sd;
    Sd=p;
}

// stampa la lista di socket descriptor
void stampSockDesc(){
    struct socketDescriptor * aux = Sd;
    while(aux){
        printf("%s %s %d %d %d\n", "username, porta, sd ,chat: ", aux->username, aux->porta, aux->i , aux->chat);
        aux= aux->next;
    }
}

// rimuovo un elemento da socket descriptor
void removeFromSockDesc(char * username, int i){
    // caso lista vuota
    if(Sd == NULL)
        return;

    // caso lista con n elementi
    struct socketDescriptor * aux, *app;
    if(Sd->next){
        aux = Sd;

        // se è subito il primo, e la lista ha più di un elemento
        if(strcmp(aux->username, username) == 0 || aux->i == i){
            app = Sd;
            Sd = Sd->next;
            free(app);
            return;
        }

        // se è nel mezzo alla lista
        while(aux->next != NULL){
            if( strcmp(aux->next->username, username) == 0 || aux->i == i){            
                app = aux->next;
                aux->next = aux->next->next;
                free(app);
                return;
            } else
                aux = aux->next;
        }
    } else{
        // se è il primo elemento
        if(strcmp(Sd->username, username) == 0 || Sd->i == i){
            app = Sd;
             Sd = NULL;
            free(app);
            return;
        }
    }

    // eliminazione primo elemento
    if(strcmp(aux->username, username) == 0 || aux->i == i){
        app = Sd;
        Sd = Sd->next;
        free(app);
    }
}

// torna il descriptor di 'username' , se lo ha già associato, altrimenti -1
int getSockDescr(char * username){
    struct socketDescriptor * aux = Sd;
    while(aux){
        if(strcmp(aux->username,username) == 0){
            return aux->i;
        }
        aux= aux->next;
    }

    return -1;
}

int getPort(char * username){
    struct socketDescriptor * aux = Sd;
    while(aux){
        if(strcmp(aux->username,username) == 0){
            return aux->porta;
        }
        aux= aux->next;
    }

    return -1;
}

int getUsername(int sockDescr, char * buf){
     struct socketDescriptor * aux = Sd;
    while(aux){
        if(aux->i == sockDescr){
            strcpy(buf,aux->username);
            return 0;
        }
        aux= aux->next;
    }

    return -1;
}

int getChat(char * username){
    struct socketDescriptor * aux = Sd;
    while(aux){
        if(strcmp(aux->username,username) == 0){
            return aux->chat;
        }
        aux= aux->next;
    }

    return -1;
}

// reimplemento la itoa, assente in linux
void reverse(char s[]){
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
} 

// converte intero in stringa, complementare della atoi (che invece converte stringa in intero)
void itoa(int n, char s[]){
     int i, sign;

     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
} 

// stampo il menù
void cmdlist(){
    system("clear");
    printf("**************Benvenuto*********************\n");
    printf("1) hanging \t \t ->  lista utenti che ti hanno inviato messaggi mentre eri offline\n");
    printf("2) show username \t -> ricevi i messaggi pendenti da 'username' \n");
    printf("3) chat username \t -> avvia la chat con 'username' \n");
    printf("4) share file_name \t -> invia 'file_name' agli utenti con cui si sta chattando\n");
    printf("5) out          \t -> disconnessione\n");
    printf("6) show_rubrica \t -> mostra la tua rubrica\n");
}

// stampo il menù di login
void listLogin(){
    system("clear");
    printf("1) signup\n");
    printf("2) in \n");
}

// stampo il timestamp in formato stringa
void printMyTimestamp(int timestamp){
    time_t     now = timestamp;
    struct tm  ts;
    char       buf[80];

    // Get current time
    time(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "[%a %Y-%m-%d %H:%M:%S]", &ts);
    printf("%s", buf);
}

// uso le due funzioni successive quando devo mandare lunghezza messaggio + messaggio
void sendData(int sd, char * buf,size_t length){
    ssize_t nbytes = send(sd,buf,length,0);
    
    if(nbytes < 0){
        printf("errore durante la send\n");
        close(sd);
        exit(1);
    }
}

void sendMessage(int sd, void* buf){
    int len = (strlen(buf)+1);
    uint32_t lmsg = htons(len);

    sendData(sd, (void*)&lmsg,sizeof(uint32_t));
    sendData(sd, (void*)buf, len);
}

//uso le due funzioni successive quando devo ricevere lunghezza messaggio + messaggio
int recvData(int sd, void* buf, size_t length){
    char buffer[MAXLEN];
    int nbytes = recv(sd,buf,length,0);

    if(nbytes < 0){
        perror("errore durante la recv\n");
        FD_CLR(sd, &master); 
        close(sd);
        exit(1);
    }  else if(nbytes == 0){
        if(sd != sdserver && sd != -1){
            getUsername(sd, buffer);
            printf("%s %s %s", "L'utente ", buffer,  "si è disconnesso\n");
            removeFromSockDesc("", sd);
        }
        else{
            printf("Il server si è disconnesso.\n");
            sdserver = -1;
            connesso = 0;
        }
            
        FD_CLR(sd, &master);   
        close(sd); 
        return -1;
    }

    return 0;
}

int recvMessage(int sd, void * buf){
    int len, ret;
    uint32_t lmsg;

    ret = recvData(sd, (void*)&lmsg,sizeof(uint32_t));
    if(ret == -1)
        return -1;

    if(lmsg == 0)
        return 0;

    len = ntohs(lmsg);

    ret = recvData(sd, (void*)buf, len);
    if(ret == -1) 
         return -1;
       
    return 1;
}

// funzione per fare connect con la porta "porta" e salvare il sock desc in "sd"
int attachSocket(int* sd, struct sockaddr_in addr,char * address, int porta){
    int ret;

    if(porta == myPort){
        perror("Errore nella connect, stai usando la tua porta\n");
        return -1;
    }

    *sd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(porta);

    inet_pton(AF_INET, address, &addr.sin_addr);

    ret = connect(*sd , (struct sockaddr*) &addr, sizeof(addr));

    if(ret<0){
        perror("Errore nella connect\n");
        return -1;
    } else{
        printf ("Connessione riuscita\n");
    }

    connesso = 1;             
    return 0;
}

// controlli per la funzione "sendCredenziali"
int checksendCredenziali(char * input){
    if(strcmp(input, "signup")==0)
            return -1;
    else if(strcmp(input, "in") == 0 )
            return -2;
    else if(strcmp(input, "out")==0)
            return -3;
    
    return -4;
}

// Quando devo mandare "in", "out" oppure "signup" al server, uso questa funzione
int sendCredenziali(char *input, char * username, char * password, int porta){
    if(sdserver == -1){
        printf("Server offline...\n");
        return -1;
    }

    char string[MAXLEN];

    sprintf(string, "%s %s %s %d", input,username,password, porta);
    sendMessage(sdserver, string); 

    return 0;
}

// controlli per la funzione "login"
int checkLogin(char * buf1, char * buf2){
    if(strlen(buf1) > MAXCREDENTIALSLENGTH || strlen(buf2) > MAXCREDENTIALSLENGTH){
        printf("Username o password troppo lunghe...\n");
        sleep(1);
        return -4;
    }

     if(strcmp(buf1, NEW_USER_GROUP) == 0 || strcmp(buf1,LEAVE_GROUP) == 0 || strcmp(buf1,ARRIVE_FILE) == 0){
         printf("Username o password non disponibili...\n");
         sleep(1);
        return -5;
    }

    return 0;
}

// funzione per effettuare il login: sifare signup oppure in
int login(int porta){
    char  azione[7], 
          parametro1[MAXLEN], 
          parametro2[MAXLEN], 
          parametro3[MAXLEN], 
          input[MAXLEN];
    int ris;

    while(1){
        // ricevo un input da tastiera
        read(STDIN_FILENO, input, MAXLEN);
        sscanf(input, "%s %s %s %s\n", azione, parametro1, parametro2, parametro3);

        // controllo la validità dell'input
        ris = checkLogin(parametro1, parametro2); 
        if(ris != 0)
            return ris;

        // controllo quale comando è stato dato

        // caso SIGNUP
        if(strcmp(azione, "signup") == 0){
            if(!connesso)
                attachSocket(&sdserver, server, "127.0.0.1", 4242);

            ris = sendCredenziali(azione, parametro1, parametro2, porta);

            if( ris== -1 ){
                printf("signup failed\n");
                return checksendCredenziali(azione);
            }else if(ris==0){
                return 1;
            }   

        // caso IN
        }else if(strcmp(azione, "in")==0){
            if(atoi(parametro1)!= 4242){
                printf("La porta del server è 4242\n");
                sleep(1);
                return -5;
            }
            if(!connesso)
                attachSocket(&sdserver, server, "127.0.0.1", atoi(parametro1));
            
            if(sendCredenziali(azione,parametro2,parametro3, porta) == -2){
                printf("login failed\n");
                return checksendCredenziali(azione);
            }else{
                strcpy(utenteOnline, parametro2);
                return 2;
            }
        } else {
            printf("comando non valido \n");
            sleep(1);
        }
    }

    return 0;
}

// creo il listener
void createListener(int porta, struct sockaddr_in my_addr){
    int ret;
    listener =  socket(AF_INET, SOCK_STREAM,0);

    // Creazione indirizzo 
	memset(&my_addr, 0, sizeof(my_addr)); 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(porta);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

	// Aggancio del socket all'indirizzo 
    ret = bind(listener, (struct sockaddr*)&my_addr, sizeof(my_addr) );

	// Inizio dell'ascolto
    ret = listen(listener, 10);

    if(ret < 0){
        perror("Errore in fase di bind: \n");
		exit(-1);
    }
}

// comunico al server di voler eseguire una certa operazione, i cui parametri sono i primi due array, mentre il codice dell'operazione è in "codice"
// se volessi iniziare una chat fra A e B, farei sendCommand(A,B,CODICE_CHAT);
int sendCommand(char* username, char* sender, int codice){
    if(sdserver == -1){
        printf("server offline ...\n");
        return -1;
    }

    char mess[MAXLEN];

    sprintf(mess, "%s %s %s %d", username, sender,"NULL", codice);
    sendMessage(sdserver, mess);

    return 0;
}

// ******************** funzioni per la CHAT ***************************

// stampa il log di una chat, passata come parametro
void acquireLog(FILE* fd){
    char mess[MAXLEN];

    system("clear");
    fseek(fd,0,SEEK_SET);

     while( fgets(mess, MAXLEN, fd) ){
        printf("%s", mess);
    }

    //stampo uno spazio finale almeno si stacca il log da quello che l'utente scriverà
    printf("\n");
}

// funzioni e strutture per la CHAT DI GRUPPO
int gruppo = 0; // questa variabile vale 1 quando è in corso una chat di gruppo, 0 altrimenti

// struttura dati in cui sono presenti gli username e i descriptor dei partecipanti al gruppo
struct gruppo{
    char * username;
    int sd;
    struct gruppo * next;
};

// lista dei partecipanti al gruppo
struct gruppo * G = NULL;

 // inserisce in coda alla lista G
int inserisciCodaGruppo(char * username, int sd){   
    struct gruppo *p, *s;
    for(s = G; s; s= s->next){
        if(strcmp(username, s->username) == 0){
            printf("utente già nel gruppo\n");
            return -1;
        }

        p = s;
    }

    s = malloc(sizeof(*G));
    s->username = malloc(strlen(username));
    strcpy(s->username, username);
    s->sd = sd;
    s->next = NULL;

    if(G == NULL){
        G = s;
    } else {
       p->next = s;
    } 

    return 0;
}

// stampa gli username dei partecipanti al gruppo, con formato:
// CASO LISTA VUOTA                 []
// CASO LISTA CON 1 PARTECIPANTE    [1]
// CASO LISTA CON N PARTECIPANTI    [1,2,  ...  ,N]
void stampGroup(){
    if(!G){
        printf("[]\n");
        return;
    }

    struct gruppo * aux = G;
    printf("[");
    while(aux){
        if(!aux->next)
            printf("%s]",aux->username);
        else
            printf("%s,", aux->username);

        aux = aux->next;
    }
    printf("\n");
}

// torna 1 se "username" è nel gruppo, 0 altrimenti
int findMember(char * username){
    struct gruppo * aux = G;
    while(aux){
        if(strcmp(aux->username, username) == 0){
            return 1;
        }
        aux = aux->next;
    }

    return 0;
}

// ************************ SHARE FILE
int share_file(char * nomeFile, int i, int gruppo){
    FILE* fd;
    char data[MAXLEN] = {0}, buffer[MAXLEN];
    struct gruppo * aux = G;

    uint32_t lmsg;

    sprintf(buffer, "%s %s ", ARRIVE_FILE, nomeFile);
    if(gruppo == 0)
        sendMessage(i, buffer);
    else{
        while(aux){
            sendMessage(aux->sd, buffer);
            aux = aux->next;
        }
    }

    fd = fopen(nomeFile, "rb");
    if(fd == NULL){
        printf("Impossibile fare share");
        return -1;
    }  

    while(fgets(data, MAXLEN, fd)!=NULL){
         aux = G;
        if(gruppo == 0)
            sendMessage(i, data);
        else{
             while(aux){
                sendMessage(aux->sd, data);
                aux = aux->next;
            }
        }
            
        //printf("mandato\n");
        bzero(data, MAXLEN);  // libero il buffer
    }

    lmsg = 0;

    if(gruppo == 0)
        send(i, (void*)&lmsg, sizeof(uint32_t), 0);
    else {
        aux = G;
        while(aux){
            send(aux->sd, (void*)&lmsg, sizeof(uint32_t), 0);
            aux = aux->next;
        }
    }

    return 0;
}

int recv_file(int i, char* nomeFile){
    FILE *fd;
    char *path = "";
    char buffer[MAXLEN], app[MAXLEN];

     struct stat st = {0};
    sprintf(buffer, "%s%s/", path, utenteOnline);

     if(stat(buffer, &st) == -1){
        mkdir(buffer, 0700);
    }

    sprintf(app, "%s%s ", buffer, nomeFile);

    fd = fopen(app, "a+");
    while(recvMessage(i, buffer)!=0) {
        fprintf(fd, "%s", buffer);
        bzero(buffer, MAXLEN);
    }
    fclose(fd);
    return 0;
}

// receive online user from server
void recvOnlineUser(){
    int ret = 1, ris;
    char buffer[MAXLEN];

    ris = sendCommand(NULL, NULL, CODICE_SENDUTENTIONLINE);

    if(ris == -1)
        return;

    while(ret!=0){
        ret = recvMessage(sdserver, buffer);
        if(ret!=0)
            printf("%s\n", buffer);
    } 
}

// manda ad "i" i partecipanti al gruppo
void sendGroupPartecipant(int i){
    struct gruppo * aux = G;
    char buffer[MAXLEN], app[MAXLEN];
    int ret;
    uint32_t lenmsg;
    
    while(aux){    
        sprintf(buffer, "%s %d", aux->username, getPort(aux->username));
        getUsername(i, app);
        sendMessage(i, buffer);

        aux = aux->next;
    }

    //comunico che ho finito
    lenmsg = 0;
    ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
    if(ret<0){
        printf("failed send length");
        exit(-1);
    }
}

// ricevi i partecipanti di un gruppo
void recvGroupPartecipant(int i){
    char buffer[MAXLEN], app[MAXLEN];
    int ret, sock;

    struct sockaddr_in peer;

    while(1){
        if(recvMessage(i, buffer) == 0)
            break;

        sscanf(buffer, "%s %d", app, &ret);

        printf("%s %s %s\n", "ricevuto ", buffer,"dentro recvGroupPartec\n");

        sock = getSockDescr(app);
        if( sock == -1){
            attachSocket(&sock, peer, "127.0.0.1", ret);
            inserisciTestaSockDesc(app, sock, 0, ret);

            FD_SET(sock, &master);

            if(sock > fdmax)
                fdmax = sock;
        }
        
        inserisciCodaGruppo(app, sock);
    }
}

// fai connettere tutti i partecipanti del gruppo
void connectOtherPartecipant(int porta, char * username){
    struct gruppo * aux = G;
    char buffer[MAXLEN], po[MAXLEN/8];
    itoa(porta, po); 

    while(aux){ 
        sprintf(buffer, "%s %s %s",NEW_USER_GROUP, po, username);
        sendMessage(aux->sd, buffer);

        aux = aux->next;
    }
}

// controllo se è lecita l'aggiunta di un utente al gruppo
int checkAddToGroup(char * buffer){
    if(strcmp(buffer, utenteOnline) == 0){
        printf("non puoi aggiungere te stesso\n");
        return -1;
    } else if(findMember(buffer) == 1 || strcmp(buffer, utenteinchat) == 0){
        printf("utente già presente\n");
        return -1;
    }

    return 0;
}

// aggiunge "username" al gruppo
int addToGroup(char* username){
    uint32_t lmsg;
    int len , sock, ret;
    char c;
    struct sockaddr_in peer;

     // mi faccio mandare dal server la porta dell'utente
    ret = sendCommand(username, utenteOnline, CODICE_SENDUSERPORT);
    if(ret == -1)
        return -1;

    ret = recv(sdserver, (void*)&lmsg, sizeof(uint32_t) , 0);               
    if(ret < 0){
        perror("Errore in fase di ricezione della lunghezza: \n");
        exit(-1);
    }
    // len conterrà la porta
    len = ntohs(lmsg);

    if(len == 0){
        printf("l'utente non è al momento online o non esiste\n");
        return -1;
    }

    // aggancio l'utente
    sock = getSockDescr(username);

    if(sock == -1){ printf("dentro\n");        
        attachSocket(&sock, peer, "127.0.0.1", len);
        inserisciTestaSockDesc(username, sock, 0, len);

        FD_SET(sock, &master);

        if(sock > fdmax)
            fdmax = sock;
    }

    // ricevo un ack dall'utente, per capire se entra o meno nel gruppo
    ret = recv(sock, (void*)&c, 1 , 0);                
    if(ret < 0){
        perror("Errore in fase di ricezione della lunghezza: \n");
        exit(-1);
    }

    if(c != '0')
        return -1;

    // caso in cui sono in una chat ed entro nel gruppo
    if(strlen(utenteinchat) > 0){
        inserisciCodaGruppo(utenteinchat, getSockDescr(utenteinchat) );
        memset(&utenteinchat, 0, sizeof(utenteinchat));
    }

    sendGroupPartecipant(sock); // mando al nuovo aggiunto i partecipanti del gruppo    
    connectOtherPartecipant(len, username); // mando a tutti gli altri membri il nuovo aggiunto
    inserisciCodaGruppo(username, sock);
    
    return 0;
}

// manda un messaggio a tutti i partecipanti del gruppo
void sendMessageToGroup(char * buffer){
    struct gruppo * aux = G;

    char mess[MAXLEN];
    sprintf(mess, "%s -> %s", utenteOnline, buffer);

    while(aux){
        sendMessage(aux->sd, mess);

        aux = aux->next;
    }
}

// elimino la lista del gruppo
void deallocateGruppo(){
    struct gruppo * app;
    while(G){
        app = G;
        G = G->next;
        free(app);
    }
    G = NULL;
}

// esco dal gruppo
void leaveGroup(){
    sendMessageToGroup(LEAVE_GROUP);
    deallocateGruppo();
    gruppo = 0;
}

// rimuovo un elemento da G
void removeFromGroup(char * username){
    // caso lista vuota
    if(G == NULL)
        return;

    // caso lista con n elementi
    struct gruppo * aux, *app;
    if(G->next){
        aux = G;

        // se è subito il primo, e la lista ha più di un elemento
        if(strcmp(aux->username, username) == 0){
            app = G;
            G = G->next;
            free(app);
            return;
        }

        // se è nel mezzo alla lista
        while(aux->next != NULL){
            if( strcmp(aux->next->username, username) == 0 ){            
                app = aux->next;
                aux->next = aux->next->next;
                free(app);
                return;
            } else
                aux = aux->next;
        }
    } else{
        // se è il primo elemento
        if(strcmp(G->username, username) == 0){
            app = G;
             G = NULL;
            free(app);
            return;
        }
    }

    // eliminazione primo elemento
    if(strcmp(aux->username, username) == 0){
        app = G;
        G = G->next;
        free(app);
    }
}

void chatDiGruppo(){
    system("clear");
    printf("Benvenuto nel gruppo. Rimani in contatto coi tuoi amici!\n");
    stampGroup();
    
    int ret, i, new_sd,
        premuto = 0;    // a 0 finchè non digito "\u", almeno blocco la possibilità di fare "\a" senza primva aver chiesto gl utenti online 
    char c, buffer[MAXLEN], app[MAXLEN], input[MAXLEN], mess[MAXLEN];
    struct sockaddr_in peer_addr;

    socklen_t lenAccept;
    lenAccept = sizeof(peer_addr);

    gruppo=1;

     while(1){
        read_fds = master;   

        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        // scorro i descrittori
        for(i=0; i<=fdmax; i++){
            if(FD_ISSET(i, &read_fds)){ 
                if(i==sdserver){               
                    ret = recv(sdserver, (void*)&c, 1 , 0);            
                    // c conterrà il codice di cosa vuole fare, tipo la chat    
                    if(ret < 0){
                        perror("Errore in fase di ricezione della lunghezza: \n");
                        exit(-1);
                    } else if(ret== 0){
                       FD_CLR(sdserver, &master);
                       close(sdserver);
                       continue;
                    }

                    recvMessage(sdserver, buffer);
                    sscanf(buffer, "%s %d", app, &ret);

                    // se il codice "c" mandato dal server è "3", qualcuno vuole chattare con noi
                    // se il codice "c" mandato dal server è "8", qualcuno vuole aggiungerci ad un gruppo
                    if(c == '3'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole chattare con te! Porta: ", ret);
                    } else if(c == '8'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole aggiungerti ad un gruppo! Porta: ", ret);
                    }

                    // guardo se sono già connesso a quell'utente, in caso contrario mi ci connetto
                    new_sd = getSockDescr(app);
                    if(new_sd == -1){
                        // Accetto nuove connessioni
                        new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                        inserisciTestaSockDesc(app, new_sd, 0, ret);

                        // monitoro il nuovo socket
                        FD_SET(new_sd, &master);

                        // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                        if(new_sd > fdmax)
                            fdmax = new_sd;
                    }

                    if(c == '8'){
                        // non si entra in un gruppo se siamo già in un altro
                        ret = send(new_sd,(void*)&ERRORE, 1 , 0);
                        if(ret<0){
                            printf("failed send length");
                            exit(-1);
                        }
                    }

                    break;
                            
            }else if(i==0){
                    read(STDIN_FILENO, input, MAXLEN);
                    memset(&mess, 0, sizeof(mess));
                    sscanf(input, "%99[^\n]", mess);

                    // caso messaggio vuoto
                    if(strlen(mess) == 0){
                        break;
                    }

                    // caso in cui vogliamo sapere gli utenti online
                    if(strcmp(mess, "\\u") == 0){
                        recvOnlineUser();
                        premuto = 1;
                        break;
                    }

                    memset(&buffer, 0, sizeof(buffer));
                    sscanf(input, "%s %s\n", app, buffer);

                    // caso in cui aggiungiamo un utente al gruppo
                    if(strcmp(app, "\\a") == 0){
                        // può aggiungerlo se ha già premuto \u
                        if(premuto == 1){
                            if(strlen(buffer) == 0) {
                                printf("specifica chi vuoi aggiungere\n");
                                break;
                            }

                            if(checkAddToGroup(buffer) == -1)
                                break;

                            ret = addToGroup(buffer);

                            if(ret == -1)
                                break;
                            else if(ret == 0)
                                gruppo = 1;

                            stampGroup();
                            break;
                        } else {
                            printf("Devi prima chiedere 'u' \n");
                            break;
                        }
                    }

                    // se vogliamo mandare un file
                    if(strcmp(app, "share") == 0){
                        share_file(buffer, 0 , 1);
                        break;
                    }

                    if(strcmp(mess, "\\q")==0){    // chiudo la chat, ma mantengo la connessione
                        cmdlist();
                        gruppo = 0;
                        premuto = 0;
                        leaveGroup();

                        return;
                    }

                    // altrimenti, mando il messaggio
                    sendMessageToGroup(mess);
                    break;
            } else {
                    ret = recvMessage(i, mess);

                    // ret è uguale a -1 se qualcuno si è disconnesso
                    if(ret== -1){

                        // se si è disconnesso il server
                        if(i == sdserver){
                            sdserver = -1; 
                            connesso = 0;
                            continue;
                        } else{ // altrimenti rimuovo il sock desc dell'utente disconnesso
                            getUsername(i, app);
                            removeFromSockDesc(app, i);
                        } 

                        // se l'utente disconnesso era dentro il gruppo
                        if(findMember(app) == 1){
                            removeFromGroup(buffer);
                            getUsername(i, buffer);
                            printf("%s %s %s","l'utente ", buffer , "è uscito dal gruppo\n");
                            stampGroup();
                        }

                        break;
                    }
                    
                    sscanf(mess, "%s %s %s", buffer, input, app);
                    
                    // se entra un nuovo utente nel gruppo
                    if(strcmp(buffer, NEW_USER_GROUP) == 0){
                        new_sd = getSockDescr(app) ;

                        if(new_sd== -1){
                            new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                            inserisciTestaSockDesc(app, new_sd, 0, atoi(input));

                            // monitoro il nuovo socket
                            FD_SET(new_sd, &master);

                            // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                            if(new_sd > fdmax)
                                fdmax = new_sd;
                        }     

                        printf("%s %s\n" ,"E' entrato l'utente ", app);
                        inserisciCodaGruppo(app, new_sd);
                        stampGroup();
                        break;
                    // se un utente esce dal gruppo
                    } else if(strcmp(app, LEAVE_GROUP) == 0){
                        removeFromGroup(buffer);
                        getUsername(i, buffer);
                        printf("%s %s %s","l'utente ", buffer , "è uscito dal gruppo\n");
                        stampGroup();
                        break;
                    // se qualcuno fa share file
                    } else if(strcmp(buffer, ARRIVE_FILE) == 0){
                        recv_file(i, input);
                        continue;
                    }
                        
                    if(strcmp(app, "\\q")== 0)
                        break;

                    // sennò stampo il messaggio
                    printf("%s\n", mess);
                    break;
                }
            }
         } 
        }
}

// chat singola
// serv sarà un booleano, ad 1 se chatto col server, a 0 se con un altro peer, 2 se l'altro peer era già dentro la chat con me
int chat(int port, FILE* fd, int serv, char * destinatario){
    char input[MAXLEN],
         mess[MAXLEN],
         buffer[MAXLEN],
         app[MAXLEN],
         c;

    FILE* fr;
    char *stringa = "",
        nomeDirectory[MAXLEN],
        nomeFile[MAXLEN];
    struct stat st = {0};

    int ret, sock, i, new_sd,
        modificato = 0,
        premuto = 0;    // a 0 finchè non digito "\u", almeno blocco la possibilità di fare "\a" senza primva aver chiesto gl utenti online 

    struct sockaddr_in peer, peer_addr;

    socklen_t lenAccept;

    // se l'utente con cui voglio chattare è online, mi ci connetto
    if(serv != 1){
        sock = getSockDescr(destinatario);
        // controllo se non ero già collegato con quel peer
        if(sock == -1){
            attachSocket(&sock, peer , "127.0.0.1", port);

            inserisciTestaSockDesc(destinatario, sock, 1, port);

            printf("%s %d %s %d\n", "agganciato il socket ", sock, "porta ", port);
            FD_SET(sock, &master);

            if(sock > fdmax)
                fdmax = sock;
        } 
    }

    lenAccept = sizeof(peer_addr);

    sock = (serv == 1) ? sdserver : sock;

    // stampo la chat fra i due
    acquireLog(fd);

    if(serv == 2)
        sock = getSockDescr(destinatario);

    // se sto parlando con un utente online, diventa il mio utenteInChat
    if(serv != 1)
        strcpy(utenteinchat, destinatario);

    while(1){
        // guardo se modificare il log della chat
        if(modificato == 1){
            acquireLog(fd);
            modificato = 0;
        }

        read_fds = master; 
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        // scorro i descrittori
        for(i=0; i<=fdmax; i++){
            if(FD_ISSET(i, &read_fds)){ 
                if(i==sdserver){

                    ret = recv(sdserver, (void*)&c, 1 , 0);             
                    // ret conterrà il codice di cosa vuole fare, tipo la chat    
                    if(ret < 0){
                        perror("Errore in fase di ricezione della lunghezza: \n");
                        exit(-1);
                    } else if(ret== 0){
                       FD_CLR(sdserver, &master);
                       close(sdserver);
                       sdserver = -1;
                       connesso = 0;
                       continue;
                    }

                    recvMessage(sdserver, buffer);
                    sscanf(buffer, "%s %d", app, &ret);

                    // se il codice "c" mandato dal server è "3", qualcuno vuole chattare con noi
                    // se il codice "c" mandato dal server è "8", qualcuno vuole aggiungerci ad un gruppo
                    if(c == '3'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole chattare con te! Porta: ", ret);
                    } else if(c == '8'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole aggiungerti ad un gruppo! Porta: ", ret);
                    }                    

                    new_sd = getSockDescr(app);
                    // guardo se sono già connesso con quel peer
                    if( new_sd == -1){
                        // Accetto nuove connessioni 
                        new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                        inserisciTestaSockDesc(app, new_sd, 0, ret);

                        // monitoro il nuovo socket
                        FD_SET(new_sd, &master);

                        // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                        if(new_sd > fdmax)
                            fdmax = new_sd;
                    }

                    // se sto parladno col server perchè il dest è offline, e quel dest vuole chattare con me,
                    // entro in chat con lui, smettendo di mandare messaggi al server
                    if(c == '3' && strcmp(app, destinatario) == 0){
                        sendMessage(sdserver, "\\q");
                        chat(ret, fd, 0, app);
                        return 0;
                    }

                    if(c == '8'){
                        // se stavo parlando col server, il quale mi bufferizzava  messaggi, smetto
                        if(serv == 1)
                            sendMessage(sdserver, "\\q");
                        
                        // comunico all'utente che mi va bene entrare nel gruppo
                        ret = send(new_sd,(void*)&SUCCESSO, 1 , 0);
                        if(ret<0){
                            printf("failed send length");
                            exit(-1);
                        }

                        inserisciCodaGruppo(app, new_sd);
                        recvGroupPartecipant(new_sd);
                        chatDiGruppo();
                        return 0;
                    }

                    break;
                            
            }else if(i==0){
                    read(STDIN_FILENO, input, MAXLEN);
                     memset(&mess, 0, sizeof(mess));
                    sscanf(input, "%99[^\n]", mess);

                    // caso messaggio vuoto
                    if( strlen(mess) == 0){
                        break;
                    }

                    if(strcmp(mess, "\\u") == 0){
                        if(serv==1){
                            printf("non puoi creare gruppi se l'utente dest è offline\n");
                            break;
                        }
                        recvOnlineUser();
                        premuto = 1;
                        break;
                    }

                    memset(&buffer, 0, sizeof(buffer));
                    sscanf(input, "%s %s\n", app, buffer);

                    if(strcmp(app, "share") == 0){
                        if(serv == 1){
                            printf("Non puoi condividere file se l'altro utente è offline\n");
                            break;
                        }

                        share_file(buffer, getSockDescr(utenteinchat), 0);
                        break;
                    }

                    // caso aggiunta di un utente al gruppo
                    if(strcmp(app, "\\a") == 0){
                        // per premere \a bisogna prima premere \u
                        if(premuto == 1){
                            if(strlen(buffer) == 0){
                                printf ("specifica chi vuoi aggiungere \n");
                                break;
                            }

                            if(checkAddToGroup(buffer) == -1)
                                break; 

                            ret = addToGroup(buffer);

                            if(ret == -1)
                                break;
                            else if(ret == 0)
                                gruppo = 1;

                            chatDiGruppo();

                            return 0; 
                        } else {
                            printf("Devi prima chiedere 'u' \n");
                            break;
                        }
                    }

                    if(strcmp(mess, "\\q")==0){    // chiudo la chat, ma mantengo la connessione
                        cmdlist();
                        memset(&utenteinchat, 0, sizeof(utenteinchat));

                        if(serv == 1)
                             sendMessage(sock, mess);

                        return 0;
                    }

                    // altrimenti, mando il messaggio
                    sendMessage(sock, mess);

                    // se il server sta bufferizzando, un solo asterisco, sennò 2
                    if( serv == 1){
                        fprintf(fd, "%c\n%s\n", '*', mess);
                    } else 
                        fprintf(fd, "%s\n%s\n", "**", mess);

                    modificato = 1;
            } else {
                    // recupero lo username di chi mi ha mandato qualcosa
                    getUsername(i, app); 

                    ret = recvMessage(i, mess);
                   
                    // l'utente si è disconnesso, ret == -1 quando qualcuno esce
                    if(ret== -1){
                        if(i == sdserver){
                            sdserver = 0; 
                            connesso = 0;
                            continue;
                        }
                       
                       // se è uscito l'utente con cui sto chattando, esco anch'io
                        if(strcmp(app, utenteinchat) == 0){
                            cmdlist();
                            memset(&utenteinchat, 0, sizeof(utenteinchat));

                            return 0;
                        } else {
                            break;
                        }
                    }
                    
                    sscanf(mess, "%s %s %s", buffer, app, input);

                    // se si vuole aggiungere un utente
                    if(strcmp(buffer, NEW_USER_GROUP) == 0){      

                        new_sd = getSockDescr(input);
                        if(new_sd == -1){
                            new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                            inserisciTestaSockDesc(input, new_sd, 0, atoi(app));

                            // monitoro il nuovo socket
                            FD_SET(new_sd, &master);

                            // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                            if(new_sd > fdmax)
                                fdmax = new_sd;
                        }
                         
                        inserisciCodaGruppo(input, new_sd);

                        getUsername(i, buffer);
                        if(strcmp(buffer, utenteinchat) == 0){
                            inserisciCodaGruppo(utenteinchat, getSockDescr(utenteinchat) );
                            memset(&utenteinchat, 0, sizeof(utenteinchat));
                        }
                        chatDiGruppo();
                        return 0;
                    // qualcuno sta condividendo un file
                    } else if(strcmp(buffer, ARRIVE_FILE) == 0){
                        recv_file(i, app);
                        break;
                    }
                                        
                    if(strcmp(mess, "\\q")== 0)
                        break;

                    getUsername(i, input);
                    if(strcmp(destinatario, input) != 0 && serv != 2)
                        printf("%s %s -> %s\n", "dall'utente", input, mess);

                    // mi salvo il messaggio 

                    // nome della directory
                    sprintf(nomeDirectory, "%s%s/",stringa, utenteOnline);

                    // controllo se esiste già la cartella, nel caso la creo
                    if(stat(nomeDirectory, &st) == -1){
                        mkdir(nomeDirectory, 0700);
                    }

                    sprintf(nomeFile, "%s%s%s", nomeDirectory, input, ".txt");
                    fr = fopen(nomeFile, "a+");
                    if(fr == NULL){
                        printf("Impossibile aprire log");
                        exit(-1);
                    }  

                    fprintf(fr, "%s", "ricevuto->\n");
                    fprintf(fr, "%s\n", mess);
                    fclose(fr);
                    modificato=1;
                }
            }
         } 
        }
        
     return 0;
}

// fa iniziare una chat fra sender (parte attiva) e username (parte passiva)
int startChat(char * username, char * sender){
    FILE *fd;
    char *stringa = "",
        nomeDirectory[MAXLEN],
        nomeFile[MAXLEN];
     
    int len, ret; 
    struct stat st = {0};
    
    // se non sono già connesso con quell'utente
    if(getSockDescr(username) == -1){
        ret = sendCommand(username,sender,CODICE_CHAT);
        if(ret == -1)
            return -1;

        // aspetto di sapere se 'username' è online
        ret = recv(sdserver, (void*)&len, sizeof(int), 0 );
        if(ret<0){
            perror("failed ack received\n");
            exit(-1);
        }

        if(len == -1){
            printf("l'utente che si vuole contattare non esiste\n");
            return -1;
        }
    }

    // creo il file del log della chat

    // nome della directory
    sprintf(nomeDirectory, "%s%s/",stringa, sender);

    // controllo se esiste già la cartella, nel caso la creo
    if(stat(nomeDirectory, &st) == -1){
        mkdir(nomeDirectory, 0700);
    }

    sprintf(nomeFile, "%s%s%s", nomeDirectory, username, ".txt");
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile iniziare chat");
        exit(-1);
    }  

    // stampo la chat, vuoto se non erano già presenti messaggi
    system("clear");
    acquireLog(fd);
    
    // aggiungo l'utente alla mia rubrica
    addToRubrica(utenteOnline, username);

    // adesso posso: 
    // - chattare direttamente con 'username', se è online (parametro serv = 0)
    // - chattare col server, se 'username' è offline (parametro serv = 1)
    // - chattare direttamente con 'username', il quale a sua volta chatta con me (parametro serv = 2)
    ret = getChat(username);
    if(ret == 0 ){ 
        // la chat esiste già
        chat(0, fd, 2, username);
    } else if(len > 0){
        // chat con utente con porta len
        printf("L'utente è online. "); 
        chat(len, fd, 0, username);
    } else{
        printf("L'utente è offline, vedrà i messaggi solo quando tornerà attivo.\n ");
        chat(len, fd, 1, username);
    }

    fclose(fd);    
    return 0;
}

// ************************ funzione OUT
int out(){
    int ris = sendCredenziali("out", utenteOnline , "null", 5);

    if( ris== -3 )
        printf("Out failed\n");
    else if(ris==0){
        // posso uscire
        online=0;
        memset(&utenteOnline, 0, sizeof(utenteOnline));

        // chiudo tutti i descriptor
        struct socketDescriptor * aux = Sd;
        while(aux){
            close(aux->i);
            FD_CLR(aux->i, &master); 
            aux= aux->next;
        }
    }  

    return ris;
} 

// ************************ funzione HANGING
int hanging(){
    char buffer[MAXLEN], username[MAXLEN];
    int numMessaggi, timestamp, ret, pos =0;

    // comunico le mie intenzioni al server
    ret = sendCommand(utenteOnline,"NULL",CODICE_HANGING);
    if(ret == -1)
        return -1;

    // il server mi manderà tutti i record relativi al suo file di hanging
    while(1){
       ret = recvMessage(sdserver,buffer);
       
       // se ret è 0 ci sono due opzioni: sono finiti i record, oppure non ce ne sono proprio
       if(ret == 0){
           // per gestire la seconda eventualità, utilizzo pos, variabile che vale 1 se almeno un record è stato inviato
           if(pos == 0){
               printf("non ci sono messaggi pendenti\n");
               return 0;
           }
           break;
       }

       // stampo il record 
       sscanf(buffer, "%d %s %d", &numMessaggi, username, &timestamp);
       printf("%d %s ", numMessaggi, username);
       printMyTimestamp(timestamp);
       printf("\n");

       // variabile ad uno se è arrivato almeno un record
       pos = 1;
    }

    return 0;
}

// ******************* funzione putDoubleStar
// quando faccio la show, i messaggi che prima avevano un solo asterisco nel log, ne acquisiscono due
struct msg{
    char message[MAXLEN];
    struct msg * next;
};

// lista di msg
struct msg * m = NULL;

 // inserisce in coda alla lista m
void inserisciCodaMsg(char * message){   
    if(m == NULL){
        m = malloc(sizeof(*m));
        strcpy(m->message, message);
        m->next = NULL;
        return;
    }

    struct msg * t;
    t = m;

    while(t->next)
        t= t->next;

    t->next = malloc(sizeof(*m));
    t= t->next;
    strcpy(t->message, message);
    t->next = NULL;
}

// dealloco m
void deallocateMsg(){
    struct msg * app;
    while(m){
        app = m;
        m = m->next;
        free(app);
    }
    m = NULL;
}

// metto il doppio asterisco ai messaggi
void putDoubleStar(char * sender, char * receiver){
    FILE* fd;

    char mess[MAXLEN],
         *stringa = "",
         nomeDirectory[MAXLEN],
         nomeFile[MAXLEN];

    int pos = 0;

    struct stat st = {0};

    // nome della directory
    sprintf(nomeDirectory, "%s%s/",stringa, sender);

    // controllo se esiste già la cartella, nel caso la creo
    if(stat(nomeDirectory, &st) == -1){
        mkdir(nomeDirectory, 0700);
    }

    sprintf(nomeFile, "%s%s%s", nomeDirectory, receiver, ".txt");
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile mettere asterischi\n");
        exit(-1);
    }  

    // salvo tutti i messaggi
     while( fgets(mess, MAXLEN, fd) != NULL){
        // cambio tutti gli asterischi in doppi asterischi
        // il fatto di tenere pos e vedere se è pari serve perchè altrimenti, se il messaggio stesso inviato dall'utente
        // fosse un asterisco, verrebbe modificato, intaccando la consistenza del log
        if( ( (pos++) %2 ==0 ) )
            strcpy(mess, "**");

        inserisciCodaMsg(mess);
    }

    fclose(fd);
    remove(nomeFile);
    pos = 0;

    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile mettere asterischi\n");
        exit(-1);
    } 

    // aggiorno il file
    struct msg * aux = m;
    while(aux){
        fprintf(fd, "%s", aux->message);
        if( ( (pos++) %2 ==0 ) )
            fprintf(fd, "%s", "\n");
        aux= aux->next;
    }
    fclose(fd);

    // finita di usare la lista , la dealloco
    deallocateMsg();
}

// *************** funzione show
// i messaggi arrivati fuori ordine sono salvati insieme al timestamp, per discriminare la non contemporaneità dei messaggi
int show(char * username){
    FILE * fd;
    char buffer[MAXLEN], nomeFile[MAXLEN], stringa[MAXLEN];
    int len, ret;
    uint32_t lenmsg;

    struct stat st = {0};

    // apro il file del log della chat che andrò ad aggiornare
    sprintf(stringa, "%s/",utenteOnline);
    if(stat(stringa, &st) == -1){
        mkdir(stringa, 0700);
    }

    sprintf(nomeFile, "%s%s%s",stringa, username, ".txt");
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile fare show ");
        exit(-1);
    }  

    // comunico al server le mie intenzioni
    ret = sendCommand(username, utenteOnline, CODICE_SHOW);
    if(ret == -1)
        return -1;

    // ricevo i messaggi pendenti
     while(1){
        ret = recv(sdserver, (void*)&lenmsg, sizeof(uint32_t), 0);        
        if(ret < 0){
            perror("Errore in fase di ricezione della lunghezza: \n");
            exit(-1);
        }

        // il server mi comunica se sono terminati i messaggi o se ho sbagliato qualcosa
        if(lenmsg == -1){
            printf("non ci sono messaggi pendenti\n");
            return -1;
        } else if(lenmsg == -2){
            printf("utente non esistente\n");
            return -2;
        }else if(lenmsg == 0){
            break;
        }
                   
        len = ntohs(lenmsg);
                            
        ret = recv(sdserver, (void*)buffer, len, 0);
        if(ret < 0){
            perror("Errore in fase di ricezione: \n");
            exit(-1);
        }

        // stampo i messaggi arrivati
        fprintf(fd , "%s\n", "ricevuto-> ");
        fprintf(fd, "%s", buffer);
        printf("%s", buffer);
    } 

    fclose(fd);
    putDoubleStar(username, utenteOnline);
    return 0;
}

// *************** funzione per aggiungere 'username' alla rubrica dell'utente 'rubrica'
int addToRubrica(char * rubrica, char * username){
    FILE *fd;
    char nomeFile[MAXLEN],
         user[MAXLEN];
     
    int ret; 

    sprintf(nomeFile, "%s/%s%s", "rubrica", rubrica, ".txt");
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile aggiungere a rubrica\n");
        exit(-1);
    }  

    while( 1 ){
        // leggo il primo utente
        ret = fscanf(fd, "%s\n",user);
        
        // se è finito il file, esco dal ciclo
        if(ret == EOF)
            break;

        // se l'utente nel file ha lo stesso nome dell'utente che vorrebbe registrarsi
        if(strcmp(username,user)==0){
            printf("%s\n","Utente già esistente");

            return 1;
        }
    }

    fprintf(fd, "%s\n", username);
    fclose(fd);

    return 0;
}

// mostra la rubrica dell'utente 'rubrica'
void showRubrica(char * rubrica){
     FILE *fd;
    char nomeFile[MAXLEN],
         user[MAXLEN];
     
    int ret; 

    struct stat buffer = {0};

    sprintf(nomeFile, "%s/%s%s", "rubrica", rubrica, ".txt");

    // verifico se l'utente ha o meno una rubrica
    if(stat(rubrica, &buffer) == -1){
        mkdir(rubrica, 0700);
    }

    fd = fopen(nomeFile, "r");
    if(fd == NULL){
        printf("Non hai nessun contatto in rubrica. Inizia una chat per aggiungerne! \n");
        return;
    }  

    while( 1 ){
        // leggo il primo utente
        ret = fscanf(fd, "%s\n",user);
        
        // se è finito il file, esco dal ciclo
        if(ret == EOF){           
            break;
        }

        printf("%s\n", user);
    }
}

// resto qui dentro finchè l'utente non fa login
void connecting(){
    int ret, ris;
    char aux;

    while(1){
        do{
            listLogin();
            ret = login(myPort); 
        } while(ret < 0);

        // ricevo dal server un ack, per sapere se l'operazione è andata a buon fine
        ris = recv(sdserver, (void*) &aux, 1, 0);
        if(ris<0){
                perror("Errore in fase di ricezione ack: \n");
                exit(-1);
        }   

        // se ci sono stati errori
        if( aux != '0'){
            memset(&utenteOnline, 0, sizeof(utenteOnline));

            // casi di utenti già registrati, password sbagliate, utenti già online, ec
            if(aux == '2')
                printf("Utente già online\n");
            else 
                printf("Username o password non corretti, riprova\n");

            sleep(2);
            listLogin();

            continue;
        } else {
            if(ret == 1){
                logged = 1;
            } else if(ret == 2){
                online = 1; 
            }
        }
        
        // se è andato tutto bene, esco
        if(online == 1)
            break;
    }

    cmdlist();
}

// controlla se il valore inserito dall'utente, relativo alla porta, è lecito
int checkPort(int argc, char* argv[]){
    if(argc>2){
        printf("numero eccessivo di parametri, ne servono 2\n");
        return -1;
    } else if(argc<2){
         printf("Fornire una porta\n");
        return -1;
    } else if(argc == 2)
        myPort = atoi(argv[1]);

    // porte da 1024 a (2 alla 16) -1
    if(myPort<1024 || myPort > 65535 || myPort == 4242){
        printf("Numero di porta errato, è compresa fra 1024 e 65535; non va bene 4242\n");
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[]){
    char azione[MAXLEN], parametro[MAXLEN], input[MAXLEN], buffer[MAXLEN], app[MAXLEN];
    int ret, i, new_sd; 
    char c;
    FILE* fd;
    char *stringa = "",
        nomeDirectory[MAXLEN],
        nomeFile[MAXLEN];
    struct stat st = {0};
    struct sockaddr_in peer_addr, my_addr;

    socklen_t lenAccept;

    // controllo che il valore della porta inserito sia legale
    if(checkPort(argc,argv) == -1)
        return -1;  

    // azzero i set, equivalente di memset, per sicurezza
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // resto qua dentro finchè non faccio una login
    connecting();

    createListener(myPort, my_addr);

    // aggiungo il sdserver alla lista dei socket monitorati
    FD_SET(sdserver, &master);
    // lo 0 indica lo stdinput
    FD_SET(STDIN_FILENO, &master);

    fdmax = sdserver;

    lenAccept = sizeof(peer_addr);

    while(1){ 
        read_fds = master;

        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        // scorro  i descrittori
        for(i=0; i<=fdmax; i++){
            if(FD_ISSET(i, &read_fds)){ 
                if(i==sdserver){ 
                    // il server mi manda sempre come prima cosa un codice 
                    ret = recv(sdserver, (void*)&c, 1 , 0);             // ret conterrà il codice di cosa vuole fare, tipo la chat    
                    if(ret < 0){
                        perror("Errore in fase di ricezione della lunghezza: \n");
                        exit(-1);
                    } else if(ret== 0){
                        sdserver = -1;
                        connesso = 0;
                        FD_CLR(sdserver, &master);
                        close(sdserver);
                        continue;
                    }

                   recvMessage(sdserver, buffer);
                     
                   sscanf(buffer, "%s %d", app, &ret);

                    // controllo se è il codice per la chat o per il gruppo
                    if(c == '3'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole chattare con te! Porta: ", ret);
                    } else if(c == '8'){
                        printf("%s %s %s %d\n", "l'utente ", app , "vuole aggiungerti ad un gruppo! Porta: ", ret);
                    }

                    // controllo se ero già connesso a quell'utente
                    new_sd = getSockDescr(app); 
                    if(new_sd == -1){ 
                        // Accetto nuove connessioni 
                        new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                        inserisciTestaSockDesc(app, new_sd, 0, ret);

                        // monitoro il nuovo socket
                        FD_SET(new_sd, &master);

                        // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                        if(new_sd > fdmax)
                            fdmax = new_sd;
                    }
                   
                   // entro nel gruppo
                    if(c == '8'){ 
                        ret = send(new_sd,(void*)&SUCCESSO, 1 , 0);
                        if(ret<0){
                            printf("failed send length");
                            exit(-1);
                        }
                        inserisciCodaGruppo(app, new_sd);  
                        recvGroupPartecipant(new_sd); 
                        chatDiGruppo();
                    }
                    
                    break;
                            
            }else if(i==0){
                    // input da tastiera dell'utente
                    read(STDIN_FILENO, input, MAXLEN);
                    memset(&parametro, 0, sizeof(parametro));
                    sscanf(input, "%s %s\n", azione, parametro);

                    // controllo quando comando è stato digitato
                    if(strcmp(azione, "chat")==0){
                        if(strcmp(parametro,utenteOnline)==0){
                            printf("Non puoi chattare con te stessso\n");
                            continue;
                        }else if(strlen(parametro) == 0){
                            printf("Specifica uno username\n");
                            break;
                        }

                        startChat(parametro, utenteOnline);
                        break;
                    } else if(strcmp(azione, "hanging")==0){
                        hanging();
                        break;
                    } else if(strcmp(azione,"show")==0){
                        if(strlen(parametro) == 0){
                            printf("Specifica uno username\n");
                            break;
                        } else if(strcmp(parametro, utenteOnline) == 0){
                            printf("Non puoi fare show con te stesso\n");
                            break;
                        }
                        show(parametro);
                        break;
                    }  else if(strcmp(azione, "out")==0){
                        if(out()== 0)
                            return 0;
                    } else if(strcmp(azione, "share") == 0){
                        printf("Devi essere dentro una chat per condividere un file.\n");
                        break;
                    } else if(strcmp(azione, "show_rubrica")== 0){
                        showRubrica(utenteOnline);
                        break;
                    } else
                        printf("Comando errato\n");
            } else {
                    ret = recvMessage(i, buffer);

                    // l'utente si è disconnesso
                    if(ret== -1){
                        if(i == sdserver){
                            sdserver = 0; 
                            connesso = 0;
                            continue;
                        } else{
                            getUsername(i, app);
                            removeFromSockDesc(app, i);
                        }                            

                        break;
                    }

                    sscanf(buffer, "%s %s %s", parametro, app, input);
                   
                   // caso aggiunta al gruppo
                    if(strcmp(parametro, NEW_USER_GROUP) == 0){
                        // controllo se ero già connesso
                        new_sd = getSockDescr(input);
                        if(new_sd == -1){
                            new_sd = accept(listener, (struct sockaddr*) &peer_addr, &lenAccept);
                            inserisciTestaSockDesc(input, new_sd, 0, atoi(app));

                            // monitoro il nuovo socket
                            FD_SET(new_sd, &master);

                            // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                            if(new_sd > fdmax)
                                fdmax = new_sd;
                        }

                        // entro nel gruppo
                        inserisciCodaGruppo(input, new_sd);
                        getUsername(i, parametro);
                        inserisciCodaGruppo(parametro, i);
                        chatDiGruppo();
                        break;
                    } else if(strcmp(parametro, ARRIVE_FILE) == 0){
                       // caso in cui mi arriva un file
                        recv_file(i, app);
                        break;
                    }                

                    if(strcmp(buffer, "\\q")== 0)
                        break;

                    getUsername(i,input);

                    // caso in cui mi è arrivato un messaggio
                    printf("%s %s -> %s\n", "dall'utente", input, buffer);

                    // salvo il messaggio nel log

                    // nome della directory
                    sprintf(nomeDirectory, "%s%s/",stringa, utenteOnline);

                    // controllo se esiste già la cartella, nel caso la creo
                    if(stat(nomeDirectory, &st) == -1){
                        mkdir(nomeDirectory, 0700);
                    }

                    sprintf(nomeFile, "%s%s%s", nomeDirectory, input, ".txt");
                    fd = fopen(nomeFile, "a+");
                    if(fd == NULL){
                        printf("Impossibile aprire log");
                        exit(-1);
                    }  

                    fprintf(fd, "%s", "ricevuto->\n");
                    fprintf(fd, "%s\n", buffer);
                    fclose(fd);
                }
             
            } 
        }
    }
    
    return 0;
}
