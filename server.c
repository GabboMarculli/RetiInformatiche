#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <malloc.h>
#include <unistd.h>
#include <dirent.h>

#define MAXLEN 64

const char CODICE_HANGING = '1',
           CODICE_SHOW = '2',
           CODICE_CHAT = '3',
           CODICE_SHARE = '4',
           CODICE_OUT = '5',
           CODICE_CHATGRUPPO = '8';

int sd ,
    variabilePerComodita = 0,
    fdmax,
    server_port;

// sockaddr è una struttura per memorizzare l'indirizzo di un socket
struct sockaddr_in my_addr,     // il mio indirizzo
                   cl_addr;     // indirizzo del client
 
const char SUCCESSO = '0',
           ERRORE = '1',
           OCCUPATO = '2';

fd_set master;  // qua ci sono tutti i descrittori (lista di socket descriptor)
fd_set read_fds; // qua passano (colabrodo) i socket pronti, da leggere

// ***** funzione SIGNUP
FILE * fd;

// la FINDUSERNAME cerca un utente nel file degli utenti registrati, ovvero che hanno fatto signup. Essa può ritornare:
// - torna -2 se non è possibile creare un utente, per problemi col file "utenti.txt" 
// - torna 1 se: l'utente esiste, "password" passato come parametro è 1 e la password dell'utente registrata è uguale a quella passata come parametro
// - torna -1 se: l'utente esiste, "password" passato come parametro è 1 e la password dell'utente registrata è diversa da quella passata come parametro
// - torna 0 se l'utente non esiste

// ESEMPIO di uso: nella signup voglio sapere se un utente esiste già, altrimenti lo registro, passo findUsername(username, NULL, 0,0,0), torna la porta se esiste già, 0 altrimenti
int findUsername(char* username, char*pass, int password){
    int ret;
    char user[32], passwd[32];
    
    // apro il file con gli utenti registrati
    fd = fopen("utenti.txt", "a+");
    if(fd == NULL){
        printf("%s","Impossibile creare utente\n");
        return -2;
    }    
   
    while( 1 ){
        // leggo il primo utente
        ret = fscanf(fd, "%s %s \n",user, passwd);
        
        // se è finito il file, esco dal ciclo
        if(ret == EOF)
            break;
       
        //printf("%s %s", user,passwd);

        // se l'utente nel file ha lo stesso nome dell'utente che vorrebbe registrarsi
        if(strcmp(username,user)==0){
            printf("%s\n","Utente esistente");

            if(password == 1){
                return (strcmp(pass,passwd) == 0) ? 1 : -1;
            }

            return 1;
        }
    }

    return 0;
}

// ******************* funzione SIGNUP
int signup(char * username, char * password){
    // se i controlli sono passati
    if(findUsername(username, NULL, 0 )==0)
        fprintf(fd, "%s %s \n", username, password);  // salvo l'utente
    else
        return -1;

    printf("%s\n","avvenuto con successo");
    fclose(fd);

    return 0;
}

// modifica il timestamp in formato più leggibile per l'utente
void printMyTimestamp(int timestamp, char * app){
    time_t     now = timestamp;
    struct tm  ts;
    char       buf[80];

    // Get current time
    time(&now);

    // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "[%a %Y-%m-%d %H:%M:%S]", &ts);
    strcpy(app, buf);
}

// *********** funzione IN

// mi salvo gli utenti connessi in memoria
struct utentiConnessi{
    char user_dest[MAXLEN];
    int port;
    int timestamp_login;
    int timestamp_logout;
    int sd;
    struct utentiConnessi* next;
};

// inizializzo la lista
struct utentiConnessi* Uc = NULL;

// funzione di utilità per inserire nella lista
void inserisciCodaUtentiConn(char*username, int porta, int sd) {
    // salvo nel log non volatile
    FILE * fd;
    fd = fopen("log.txt", "a+");
    if(fd == NULL){
        printf("%s","Impossibile aggiungere record\n");
        exit(-1);
    }   
    fprintf(fd, "%s %d %d %d %d\n", username, porta, sd, (int)time(NULL), 0);
	fclose(fd);

    struct utentiConnessi *p, *s;
    for(s = Uc; s; s= s->next){
        if(strcmp(username, s->user_dest) == 0){
            s->port = porta;
            s->sd = sd;
            s->timestamp_login = (int)time(NULL);
            s->timestamp_logout = 0;
            return;
        }

        p = s;
    }

    s = malloc(sizeof(*Uc));
    strcpy(s->user_dest, username);
    s->sd = sd;
    s->port= porta;
    s->timestamp_login = (int)time(NULL);
    s->timestamp_logout = 0;
    s->next = NULL;

    if(Uc == NULL){
        Uc = s;
    } else {
       p->next = s;
    } 

    return;
}

// torna la porta di un utente
int getPort(char * username){
    struct utentiConnessi *aux = Uc;

    while(aux){
        if(strcmp(aux->user_dest,username) == 0){
            return aux->port;
        }

        aux = aux->next;
    }

    return -1;
}

int getUsername(int sd, char* buf){
    struct utentiConnessi *aux = Uc;

    while(aux){
        if(aux->sd == sd){
            strcpy(buf, aux->user_dest);
            return 0;
        }

        aux = aux->next;
    }

    return -1;
}

// ******************** funzione LIST

void list(){
    char timestamp[MAXLEN];
    struct utentiConnessi *aux = Uc;

    if(!aux){
        printf("Non ci sono utenti online\n");
        return;
    }

    while(aux){
        if(aux->timestamp_logout == 0){
            printMyTimestamp(aux->timestamp_login, timestamp);
            printf("%s*%s*%d\n", aux->user_dest, timestamp , aux->port);
        }

        aux = aux->next;
    }
}

// *********************** funzione OUT per i client
int out(char * username){
    struct utentiConnessi *t= Uc;

    FILE * fd;
    fd = fopen("log.txt", "a+");
    if(fd == NULL){
        printf("%s","Impossibile aggiungere record\n");
        return -1;
    }   

	// vado avanti fino alla fine della lista 
    while(t){
        if(strcmp(t->user_dest,username) == 0){
            t->timestamp_logout = (int)time(NULL);
            
            fprintf(fd, "%s %d %d %d %d\n", username, t->port, t->sd, t->timestamp_login, (int)time(NULL));
            fclose(fd);

            close(t->sd);
            FD_CLR(t->sd, &master);

            return 0;
        }

        t=t->next;
    }

    printf("%s","utente non presente\n");
    fclose(fd);
    return -1;
}

// ******************* funzioni di utilità

// la funzione isOnline:
// torna -1 se l'utente è offline
// torna -2 se l'utente non è nella lista, ovvero non ha fatto accessi da quando il server è attivo
// torna il socketDescriptor di 'username', nel caso in cui questi sia online
// nel caso in cui 'username' sia online, modifica anche 'variabilePerComodita', assegnandole la porta su cui 'username' ascolta

int isOnline(char * username){
    struct utentiConnessi * aux = (Uc);
    //list();
    while(aux){
        if(strcmp(aux->user_dest, username)==0){
            if(aux-> timestamp_logout != 0)
                return -1;
            else{
                variabilePerComodita = aux->port;
                //printf("%s %d\n", "aux->sd: ", aux->sd);
                return aux->sd;
            }
        }

        aux = aux->next;          
    }

    printf("%s","Utente non presente online\n");
    return -2;
}

// stampo il menù
void cmdlist(){
    system("clear");
    printf("*********server started******************\n");
    printf("1) help -> mostra i dettagli dei comandi\n");
    printf("2) list -> mostra l'elenco degli utenti connessi\n");
    printf("3) esc -> chiude il server \n");
}

// ************* uso le due funzioni successive quando devo mandare lunghezza messaggio + messaggio
void sendData(int sd, const void * buf,size_t length){
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

// ***************** funzioni per la CHAT
// Creo una directory per ogni receiver. In ogni cartella, un file per ogni sender.
// Quindi per ogni utente sarà presente una cartella, contenente un file per ogni utente che li 
// ha mandato un messaggio (ed è stato bufferizzato dal server)

// funzione per inserire un messaggio pendente
int inserisciMessaggioPendente(char* sender, char*receiver, char* message, int timestamp){
    FILE *fd, *fp;
    char *stringa = "messPendenti/",
         nomeDirectory[MAXLEN],
         nomeFile[MAXLEN],
         buffer[MAXLEN],
         app[MAXLEN];

    int num, ret;

    struct stat st = {0}, sp = {0}, sq = {0};

    // controllo se esiste già la cartella, nel caso la creo
    if(stat(stringa, &st) == -1){
        mkdir(stringa, 0700);
    }

    // nome della directory
    sprintf(nomeDirectory, "%s%s", stringa, receiver);

    // controllo se esiste già la cartella, nel caso la creo
    if(stat(nomeDirectory, &sp) == -1){
        mkdir(nomeDirectory, 0700);
    }

    sprintf(nomeFile, "%s/%s%s", nomeDirectory, sender, ".txt");
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("Impossibile inserire messaggio\n");
        return -1;
    }  

    // inserisco il messaggio pendente
    printMyTimestamp(timestamp, buffer);
    fprintf(fd, "%s %s\n", buffer, message);
    fclose(fd);

    // adesso aggiorno il file della hanging
    sprintf(nomeFile, "%s/%s", nomeDirectory, "hanging.txt");
    if(stat(nomeFile, &sq) == -1){
        fd = fopen(nomeFile, "w");
    } else
        fd = fopen(nomeFile, "r");

    if(fd == NULL){
        printf("Impossibile inserire hanging\n");
        return -1;
    } 

    fp = fopen(nomeFile, "r+");
    if(fd == NULL){
        printf("Impossibile inserire hanging\n");
        return -1;
    } 

    // cerco il record relativo a quell'utente
    while( 1 ){
        // leggo il primo utente
        ret = fscanf(fd, "%d %s %s\n", &num , buffer, app);
        
        if(ret == EOF){
            // se sono qua, è il primo messaggio appeso di quel sender
            printMyTimestamp(timestamp, app);
            fprintf(fp, "%d %s %s\n", 1 , sender, app); 
            break;
        }

        // se l'utente nel file ha lo stesso nome dell'utente che vorrebbe registrarsi
        if(strcmp(buffer,sender)==0){
           num++; 
           printMyTimestamp(timestamp, app); 
           fprintf(fp, "%d %s %s\n", num, sender, app); 
           break;  
        }

        fscanf(fp,"%d %s %s\n", &num , buffer, app); 
    }

    fclose(fd);
    fclose(fp);

    return 0;
}

// In questa struttura salvo tutti gli utenti che sono in una chat.
// Dunque, se A vuole chattare con B e appuro che B è offline, qua dentro salvo sender = A, receiver = B e descriptor associato ad A
// Finchè non chiuderà la chat, ogni messaggio arrivato da A non sarà un comando (signup, in, ecc), ma un messaggio da bufferizzare come pendente
struct utentiInChat{
    char sender[MAXLEN];
    char receiver[MAXLEN];
    int sockDescr;          // descriptor associato al sender, chi manda i al receiver, il quale è offline
    struct utentiInChat * next;
};

struct utentiInChat* UiC = NULL;

// aggiungo un elemento a utentiInChat
void newChat( char*sender,char * receiver, int descr) {	
    struct utentiInChat *p = malloc(sizeof(*UiC));
    strcpy(p->sender,sender);
    strcpy(p->receiver, receiver);
    p->sockDescr=descr;
    p->next = UiC;
    UiC = p;
}

// cerco un descrittore in utentiInChat
int findDescr(int descr, char * sender, char * receiver){
    struct utentiInChat * aux = UiC;
    while(aux){
        if(aux->sockDescr == descr){
            strcpy(sender,aux->sender);
            strcpy(receiver, aux->receiver);
            return 0;
        }
        aux= aux->next;
    }

    return -1;
}

int findChat(char * sender, char* receiver){
    struct utentiInChat * aux = UiC;
    while(aux){
        if( (strcmp(aux->sender, sender)== 0 && strcmp(aux->receiver, receiver) == 0) )
            return 1;
        
        aux= aux->next;
    }

    return 0;
}

// rimuovo un elemento da utentiInChat
void removeChat( int i, char * sender, char * receiver){
    // caso lista vuota
    if(UiC == NULL)
        return;

    // caso lista con n elementi
    struct utentiInChat * aux, *app;
    if(UiC->next){
        aux = UiC;

        // se è subito il primo, e la lista ha più di un elemento
        if(aux->sockDescr == i && strcmp(aux->sender, sender) == 0 && strcmp(aux->receiver, receiver) == 0){
            app = UiC;
            UiC = UiC->next;
            free(app);
            return;
        }

        // se è nel mezzo alla lista
        while(aux->next != NULL){
            if(aux->sockDescr == i && strcmp(aux->sender, sender) == 0 && strcmp(aux->receiver, receiver) == 0){            
                app = aux->next;
                aux->next = aux->next->next;
                free(app);
                return;
            } else
                aux = aux->next;
        }
    } else{
        // se è il primo elemento
        if(UiC->sockDescr == i && strcmp(UiC->sender, sender) == 0 && strcmp(UiC->receiver, receiver) == 0){
            app = UiC;
             UiC = NULL;
            free(app);
            return;
        }
    }

    // eliminazione primo elemento
     if(aux->sockDescr == i && strcmp(aux->sender, sender) == 0 && strcmp(aux->receiver, receiver) == 0){
        app = UiC;
        UiC = UiC->next;
        free(app);
    }
}

// funzione per quando ricevo il comando "chat"
void chat(char* username, char*sender, int i){
    int ret;
    char buffer[MAXLEN];

    if(findUsername(username, NULL, 0) == 0){
        variabilePerComodita = -1;
    }else
        sd = isOnline(username);

    // variabilePerComodità sarà = 0 se l'utente è offline, uguale alla porta dove contattare l'utente
    // se invece è online, -1 se non esiste
    ret = send(i, (void*)&variabilePerComodita, sizeof(int), 0);
    if(ret < 0){
        perror("failed send length\n");
        exit(-1);
    }

    //riazzero la variabile di appoggio
    if(variabilePerComodita == -1){ // l'utente non esiste
        variabilePerComodita = 0;
        return;
    }
    
     variabilePerComodita = 0;    

    // caso utente desiderato online
    if(sd > 0){ 
        // comunico a quell' utente che qualcuno vuole chattarci
        ret = send(sd, (void*)&CODICE_CHAT, 1 , 0);
        if(ret<0){
            printf("failed send length");
            exit(-1);
        }

        sprintf(buffer, "%s %d", sender, getPort(sender));
        // mando al receiver il nome di chi vuole chattarci 
        sendMessage(sd, buffer);
    
        printf("comunicazione peer to peer\n");
    } else {   
         // altrimenti mi segno che quell'utente è in chat e d'ora in poi lo accodo quello che manda nei messaggi pendenti
          newChat(sender, username, i); 
    }
}

// ************************* funzioni per la CHAT DI GRUPPO

// quando un utente in chat digita \u, si manda la lista degli utenti online
// passo come parametri il socket descriptor del richiedente
void sendUserOnline(int i){
    struct utentiConnessi *aux = Uc;
    uint32_t lenmsg;
    int ret;

    while(aux){
        if(aux->timestamp_logout == 0){
                sendMessage(i, aux->user_dest);
                printf("%s %s %d\n","mandato", aux->user_dest, i);
        }

        aux = aux->next;
    }

    // invio 0 per comunicare che sono finiti
    lenmsg = 0;
    ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
    if(ret<0){
        printf("failed send length");
        exit(-1);
    }
}

// quando un utente digita "\a username", si manda la porta di quello username, se online almeno può connettercisi
void sendUserPort(int i, char* username, char* richiedente){
    struct utentiConnessi *aux = Uc;
    int ret;
    uint32_t lmsg;
    char buffer[MAXLEN];

    while(aux){
        printf("dentro while sendport\n");
        if(strcmp(aux->user_dest,username) == 0 && aux->timestamp_logout == 0){
                // comunico, a chi vuole iniziare il gruppo, la porta dell'utente desiderato
                lmsg = htons(aux->port);
                ret = send(i, (void*)&lmsg , sizeof(uint32_t),0);
                if(ret<0){
                    printf("failed send length");
                    exit(-1);
                }
                printf("%s %d\n","mandato", aux->port);

                // mando all'utente richiesto una notifica, dicendo che qualcuno vuole iniziare con lui una chat di gruppo
                ret = send(aux->sd, (void*)&CODICE_CHATGRUPPO, 1 , 0);
                if(ret<0){
                    printf("failed send length");
                    exit(-1);
                }
                printf("%s %c\n","mandato", CODICE_CHATGRUPPO);

                 sprintf(buffer, "%s %d", richiedente, getPort(richiedente));

                // mando al receiver il nome di chi vuole chattarci 
                sendMessage(aux->sd, buffer);
                printf("%s %s\n","mandato", buffer);

                return;
            }

        aux = aux->next;
    }

    // comunico che c'è stato un qualche tipo di errore
    printf("utente offline o non esistente\n");
    lmsg = 0;
    ret = send(i, (void*)&lmsg , sizeof(uint32_t),0);
    if(ret<0){
        printf("failed send length");
        exit(-1);
    }
}

// ************************** funzione HANGING
struct nameFile{
    char nome[MAXLEN];
    struct nameFile * next;
};

struct nameFile * nf = NULL;

void inserisciDentroNameFile(char*username) {	
    struct nameFile *p = malloc(sizeof(*nf));
    strcpy(p->nome,username);
    p->next = nf;
    nf=p;
}

void deallocateNameFile(){
    struct nameFile * app;
    while(nf){
        app = nf;
        nf = nf->next;
        free(app);
    }
    nf = NULL;
}

// riempie la lista "nf" con tutti i file presenti nella cartella "directory"
void getListOfFileInDirectory(char * directory){
    char *stringa = "messPendenti/";
    char target[MAXLEN];

    sprintf(target, "%s%s/", stringa, directory);

    DIR * dirp = NULL;

    // definita in dirent.h
    struct dirent *dp;

    // apro la cartella
    dirp = opendir(target);

    while(dirp){
        //errno = 0;
        if( (dp = readdir(dirp)) != NULL){
            // filtro i . e .., 
            if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){

            } else
                inserisciDentroNameFile(dp->d_name);
        }else{
            closedir(dirp);
            dirp = NULL;
        }
    }
}

// elimino '.txt' dallla fine della stringa
void purificate(char * string, char * string2){
    int len = strlen(string), i;

    for(i= 0; i<len-4;i++)
        string2[i] = string[i];  
    
    string2[len-4] = '\0';
}

// ************* funzione HANGING
void hanging(char* username, int i){ 
    char buffer[MAXLEN],
         *stringa = "messPendenti/";

    int ret;
    uint32_t lenmsg;
    FILE *fd;

    // recupero il file di hanging dell'utente
    sprintf(buffer, "%s%s/%s",stringa, username, "hanging.txt");
    fd = fopen(buffer, "r");

    // non ci sono messaggi pendenti
    if(fd == NULL){
        lenmsg = 0;
        ret = send(i, (void*)&lenmsg, sizeof(uint32_t), 0);
        if(ret < 0){
            printf("failed send length");
            exit(-1);
        }
        return;
    }    
 
    // se ci sono messaggi pendenti, glieli mando
     while(fgets(buffer, MAXLEN, fd)!=NULL){
        sendMessage(i, buffer);
        bzero(buffer, MAXLEN);  // libero il buffer
    }

    // comunico che ho terminato l'invio
    lenmsg = 0;
    ret = send(i, (void*)&lenmsg, sizeof(uint32_t), 0);
    if(ret < 0){
        printf("failed send length");
        exit(-1);
    }
} 

// ******************* funzione SHOW
int show(char* usernameSender, char* usernameReceiver,int i){
    char nomeFile[MAXLEN],
         target[MAXLEN],
         buffer[MAXLEN], b[MAXLEN], c[MAXLEN],
         mess[MAXLEN],
         *stringa = "messPendenti/";

    int ret, num;
    uint32_t lenmsg;
    FILE *fd;
  
    // utente non esistente
    if(findUsername(usernameSender, (char*)0 , 0) == 0){  
        lenmsg = -2;
        ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
        if(ret<0){
            printf("failed send length");
            exit(-1);
        }
        return 0;
    }
   
    // recupero la cartella dei messaggi pendenti destinati a usernameReceiver, e ne recupero i file
    getListOfFileInDirectory(usernameReceiver);
    if(!nf){
            // non ci sono messaggi pendenti
            printf("non ci sono messaggi pendenti\n");
            lenmsg = -1;
            ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
            if(ret<0){
                printf("failed send length");
                exit(-1);
            }
            return 0;
    }

    sprintf(buffer, "%s%s/",stringa, usernameReceiver);  
    
    // recupero il nome del file
    struct nameFile* aux = nf;
    while(aux){     
        purificate(aux->nome, target);
        printf("%s\n", aux->nome);
        // trovo il file desiderato dei messaggi pendenti
        if(strcmp( target ,usernameSender) == 0){
            sprintf(nomeFile, "%s%s",buffer, aux->nome);
            printf("%s\n", nomeFile);

            fd = fopen(nomeFile, "a+");
            if(fd == NULL){
               printf("%s","Impossibile creare utente\n");
               return -2;
            } 
            break;
        }

        if(!aux->next){
            // non ci sono messaggi pendenti
            printf("non ci sono messaggi pendenti\n");
            lenmsg = -1;
            ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
            if(ret<0){
                printf("failed send length");
                exit(-1);
            }
            return 0;
        }

        aux = aux->next;
    } 

    // mando i messaggi pendenti
     while( fgets(mess, MAXLEN, fd)!=0 ){
         sendMessage(i,mess);
    }
 
    // comunico che ho finito di leggere
    lenmsg = 0;
    ret = send(i, (void*)&lenmsg , sizeof(uint32_t),0);
    if(ret<0){
        printf("failed send length");
        exit(-1);
    }

    fclose(fd);
    remove(nomeFile);
    deallocateNameFile();

    // aggiorno il file di hanging
    sprintf(nomeFile, "%s%s",buffer, "hanging.txt");
    fd = fopen(nomeFile, "r");
    if(fd == NULL){
        printf("%s","Impossibile aprire file\n");
        return -2;
    } 
    printf("%s %s \n", "aperto", nomeFile);

    // mi salvo tutte le righe tranne quella di cui ho fatto show, usando nameFile come lista di appoggio
    while(1){
        ret = fscanf(fd, "%d %s %s %s %s\n", &num , target, buffer, b ,c);
        
        if(ret == EOF)
            break;

        if(strcmp(target, usernameSender)!= 0){
            sprintf(mess, "%d %s %s %s %s",num, target, buffer, b ,c);
            inserisciDentroNameFile(mess);
        }
    }

    fclose(fd);
    remove(nomeFile);  

    // adesso riscrivo tutti i record aggiornati
    fd = fopen(nomeFile, "a+");
    if(fd == NULL){
        printf("%s","Impossibile aprire file2\n");
        return -2;
    }   

    struct nameFile* app = nf ;
    while(app){
        fprintf(fd, "%s\n", app->nome); 
        printf("%s %s \n", "riscrivo", app->nome);
        app = app->next;
    }
    
    deallocateNameFile();
    fclose(fd);

    return 0;
}

// ******************** funzione ESC del server
void esc(){
    struct utentiConnessi *aux = Uc;

    while(aux){
        close(aux->sd);
        FD_CLR(aux->sd, &master);

        aux = aux->next;
    }
}

void createDirectory(){
       struct stat st = {0};

     if(stat("messPendenti/", &st) == -1){
        mkdir("messPendenti/", 0700);
    }

}

int main(int argc, char* argv[]){
    int ret, 	/* la uso per le risposte*/
		sd,		/* ci metto il socket */ 
        len,
        parametro4, // la uso nei comandi
        new_sd;

    int i;
 
    char parametro1[MAXLEN], parametro2[MAXLEN], parametro3[MAXLEN]; 
    char azione[MAXLEN], input[MAXLEN], sender[MAXLEN], receiver[MAXLEN];

    uint32_t lmsg;
    socklen_t lenAccept;
    char buffer[MAXLEN];

    if(argc == 2)
        server_port = atoi(argv[1]);

    if(server_port != 4242){
        printf("il server vuole la porta 4242\n");
        return -1;
    }

    // azzero i set, equivalente di memset
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

	/* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM,0);

	/* Creazione indirizzo */
	memset(&my_addr, 0, sizeof(my_addr)); 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

	/* Aggancio del socket all'indirizzo */
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr) );

	/* Inizio dell'ascolto, coda da 10 connessioni. Il socket è passivo, usato per ricevere richieste di connessione */
    ret = listen(sd, 10);
    if(ret < 0){
        perror("Errore in fase di bind: \n");
		exit(-1);
    }

    // aggiungo il listener alla lista dei socket monitorati
    FD_SET(sd, &master);
    FD_SET(STDIN_FILENO, &master);
    fdmax = sd;     // tengo d'occhio il socket di indice massimo

    cmdlist();
    createDirectory();

    lenAccept = sizeof(cl_addr); 

	while(1){
        read_fds = master; 

        select(fdmax+1, &read_fds, NULL , NULL, NULL);

        // scorro  i descrittori
        for(i=0; i<=fdmax; i++){
            if(FD_ISSET(i, &read_fds)){ 
                if(i==sd){  // se c'è da leggere dal socket listener, quello da cui accetto le connessioni
                     // Accetto nuove connessioni 
                    new_sd = accept(sd, (struct	sockaddr*)&cl_addr, &lenAccept);

                    // monitoro il nuovo socket
                    FD_SET(new_sd, &master);

                    // se un nuovo socket descriptor ha un listener > dell'attuale, lo aggiorno come max almeno lo scorro nel for
                    if(new_sd > fdmax)
                        fdmax = new_sd;
                } else if(i==0){
                    // input da tastiera
                    read(STDIN_FILENO, input, MAXLEN);
                    sscanf(input, "%s\n", azione);

                    if(strcmp(azione, "list")==0){
                        list();
                    } else if(strcmp(azione, "esc")==0){
                        esc();
                        return 0;
                    } else if(strcmp(azione, "help") == 0){
                        cmdlist();
                    } else {
                        printf("Comando errato\n");
                        continue;
                    }
                }  else {
                    // controllo se l'utente è in una chat, nel caso bufferizzo il messaggio mandato
                    if(findDescr(i,sender,receiver)== 0){
                        ret = recv(i, (void*)&lmsg, sizeof(uint32_t), 0);             
                        if(ret < 0){
                            perror("Errore in fase di ricezione della lunghezza: \n");
                            exit(-1);
                        } else if(ret == 0){
                            getUsername(i, azione);
                            out(azione);
                            printf("Utente %s disconnesso\n", azione);
                            close(i); 
                            FD_CLR(i, &master);
                            break;
                        }
                                    
                        len = ntohs(lmsg);

                        ret = recv(i, (void*)buffer, len, 0);         
                        if(ret < 0){
                            perror("Errore in fase di ricezione: \n");
                            exit(-1);
                        }

                        printf("%s\n", buffer);

                        // se è finita la chat
                        if(strcmp(buffer, "\\q") == 0){
                            removeChat(i, sender, receiver);
                            break;
                        }

                        if(strcmp(buffer, "*") == 0 || strcmp(buffer, "**")==0){
                            break;
                        }
                        
                        // inserisco il messaggio pendente
                        inserisciMessaggioPendente(sender,receiver,buffer, (int)time(NULL));
                        break;
                    }

                    // ricevo il comando dell'utente
                     ret = recv(i, (void*)&lmsg, sizeof(uint32_t), 0);     

                     if(ret == 0){
                        getUsername(i, azione);
                        out(azione);
                        printf("Utente %s disconnesso\n", azione);
                        close(i); 
                        FD_CLR(i, &master);
                        break;
                     }  
                
                    if(ret < 0){
                        perror("Errore in fase di ricezione della lunghezza: \n");
                        exit(-1);
                    }
                            
                    len = ntohs(lmsg);   
                    ret = recv(i, (void*)buffer, len, 0);
                        
                    if(ret < 0){
                        perror("Errore in fase di ricezione: \n");
                        exit(-1);
                    }
                            
                    buffer[len] = '\0';

                    sscanf(buffer, "%s %s %s %d", parametro1, parametro2, parametro3, &parametro4);
                    printf("%s %s %s %d\n", parametro1, parametro2, parametro3, parametro4);
                
                    // controllo quale comando mi è stato inviato
                    if(strcmp(parametro1, "signup")==0){
                        if(signup(parametro2, parametro3) !=0){
                            printf("signup errata\n");
                            send(i, (void*)&ERRORE,sizeof(char),0);       // comunico 1 se fallisce
                            continue;
                        }

                        send(i, (void*)&SUCCESSO , sizeof(char),0);       // comunico 0 se ha successo
                        break;
                    } else if(strcmp(parametro1, "in")==0){ 
                            // se l'utente è già online, rifiuto la in
                            if(isOnline(parametro2) > 0){
                                variabilePerComodita = 0;
                                send(i, (void*)&OCCUPATO, sizeof(char), 0);
                                break;
                            }else{
                                ret = findUsername(parametro2, parametro3, 1);
                        
                                if(ret == 1){   // se l'utente è già registrato e la password è corretta
                                    inserisciCodaUtentiConn( parametro2 ,parametro4, i);  // la find username, se l'utente è già registrato, ritorna la porta sulla quale ascolta   	
                                    send(i, (void*)&SUCCESSO , sizeof(char),0);       // comunico 0 se ha successo
                                 } else
                                    send(i, (void*)&ERRORE,sizeof(char),0);   // comunico 1 se fallisce
                                break;
                            }
                        }            

                switch (parametro4){
                    case 1:
                        hanging(parametro1, i); 
                        break;

                    case 2:
                        show(parametro1,parametro2, i);
                        break; 

                    case 3:
                        chat(parametro1, parametro2,i);
                        break;

                    case 4:
                        //share_file();
                        break;

                    case 5:
                        out(parametro2);
                        break;

                    case 6: 
                        sendUserOnline(i);
                        break;

                    case 7:
                        sendUserPort(i, parametro1, parametro2);
                        break;

                    default:
                        break;
                  }

                }
            } 
        } 
    }
}