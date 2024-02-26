#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define DIM_BUFFER 1024 /* dimensione del buffer */
#define DIM_USERNAME 30 /* dimensione di un vettore di char contenente un username */
#define DIM_PASSWORD 30 /* dimensione di un vettore di char contenente una password */

/* strutture utili */
/* contiene le informazioni relative a un utente */
struct user{ 
	char username[DIM_USERNAME];
	char password[DIM_PASSWORD];
	int srv_port;
};

/* contiene le inforazioni relative alla rubrica */
struct rubrica{
	char user_dest[DIM_USERNAME];
	int port;
	int attiva;
};

/* contiene le informazioni relative alla disconnessione */
struct disconnessione{
	char username[DIM_USERNAME];
	char timestamp[50];
};

/* struttura per ottenere l'ora attuale */
time_t rawtime;
struct tm* timeinfo;

int tcp_listener, tcp_connect, accept_socket; /* socket */
int portno, bind_portno; /* porta del peer, porta su cui si fa la bind TCP*/
int ret, len;
socklen_t addrlen;
uint16_t messageLen;
char buffer[DIM_BUFFER];
char nomeFile[50], nomeFile2[50]; /* nome del file che si vuole aprire */
struct sockaddr_in self_addr; /* struttura self */
struct sockaddr_in connect_addr, bind_addr; /* strutture per connessioni TCP */
struct sockaddr_in appoggio;

int loggato = 0; /* se 1 peer è connesso al server, altrimenti no */
char buffer_temp[DIM_BUFFER]; /* variabile di appoggio */
char msg_temp[DIM_BUFFER]; /* variabile di appoggio per il messaggio */
char username[DIM_USERNAME]; /* variabile di appoggio per lo username */
char password[DIM_PASSWORD]; /* variabile di appoggio per la password */
char username_temp[DIM_USERNAME]; /* variabile che contiene lo username destinatario */
FILE *file, *file2, *file3, *file4; /* puntatore a file */
struct user user; /* variabile che contiene le informazioni dell'utente loggato */
struct rubrica rubrica; /* variabile che contiene le informazioni della rubrica */
struct disconnessione disconnessione; /* variabile che contiene le informazioni della disconnessione */
char comando[50]; /* contiene il comando di input */
int port_temp; /* variabile che contiene il numero di porta di appoggio */
int presente = 0; /* variabile per sapere se un utente è presente o meno all'interno di un file */  
int count = 0; /* variabile per controllare il numero di parametri passati */
int chat = 0; /* variabile per sapere se l'utente si trova nella sezione della chat */
int gruppo = 0; /* variabile per sapere se l'utente vuole creare una chat di gruppo */
/* set di descrittori */
fd_set master;
fd_set read_fds;
int fdmax;

/* funzioni utili */

/* stampa a video il messaggio d'inizio*/
void print_start_message(){
	int i;
	for (i = 0; i < 20; i++)
		printf("*");
	printf(" COMANDI DISPONIBILI ");
	for (i = 0; i < 20; i++)
		printf("*");
	printf("\n");

	printf("Digita un comando: \n\n");
	printf("1) signup <username> <password> --> creazione di un account sul server\n");
	printf("2) in <srv_port> <username> <password> --> richiesta di connessione al servizio\n");
	printf("_\n\n");
}

/*funzione che stampa a video la sezione successiva al login*/
void print_menu(){
	int i;
	for (i = 0; i < 20; i++)
		printf("*");
	printf(" MENU INIZIALE ");
	for (i = 0; i < 20; i++)
		printf("*");
	printf("\n");

	printf("1) hanging --> mostra la lista degli utenti che ti hanno inviato messaggi mentre eri offline\n");
	printf("2) show <username> --> ricevi i messaggi pendenti da parte dell'utente username\n");
	printf("3) chat <username> --> avvia una chat con l'utente username\n");
	printf("4) share <file_name> --> invia il file file_name al tuo device o agli utenti con cui stai chattando\n");
	printf("5) out --> richiedi una disconnessione dal network\n");
	printf("_\n\n");
}

/* funzione che stampa a video le istruzioni per la chat */
void print_help_chat(){
	int i;
	for (i = 0; i < 20; i++)
		printf("*");
	printf(" COMANDI CHAT ");
	for (i = 0; i < 20; i++)
		printf("*");
	printf("\n");

	printf("msg + INVIO --> invia msg all' utente richiesto\n");
	printf("/q + INVIO --> torna al menù iniziale\n");
	printf("/u + INVIO --> inizia una chat di gruppo\n");
	printf("_\n\n");
}

int main(int argc, char* argv[]){

	/* se l'utente non specifica la porta esco e notifico*/
	if (argc != 2){
		fprintf(stderr, "USO: ./dev <port> \n");
		exit(1);
	}

	/* salvo il numero di porta in portno */
    portno = atoi(argv[1]);

    /* indirizzo self */
    memset(&self_addr, 0, sizeof(self_addr));
    self_addr.sin_family = AF_INET;
    self_addr.sin_port = htons(portno);
    self_addr.sin_addr.s_addr = INADDR_ANY;

    /* creo socket tcp d'ascolto */
    tcp_listener = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listener < 0){
        perror("Impossibile creare il socket di ascolto TCP: \n");
        exit(1);
    }

    /* bind socket TCP di ascolto */
    ret = bind(tcp_listener, (struct sockaddr*)&self_addr, sizeof(self_addr));
    if(ret < 0){
        perror("Errore in fase di bind TCP di ascolto:\n");
        exit(1);
    }

	/* coda di ascolto di 10 */
    listen(tcp_listener, 10);
    
    /* pulizia fd_set */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /* inserisco socket da monitorare nel master */
    FD_SET(tcp_listener, &master);
    FD_SET(0, &master);

    /* fdmax */
    fdmax = tcp_listener;

    /* messaggio di benvenuto */
	print_start_message();	

	while(1){

		/* inizializzo il set read_fds, manipolato dalla select() */
		read_fds = master;

		/* mi blocco  in attesa di descrittori pronti in lettura */
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

		/* controllo se il socket per lo stdin è pronto */
        if(FD_ISSET(0, &read_fds)){ /* stdin */
			
			/* salvo input nel buffer */
        	fgets(buffer, DIM_BUFFER, stdin);
			sscanf(buffer, "%s", comando);	
			
            if(strcmp(comando,"/q") == 0 && chat == 1){
                /* utente vuole uscire dalla chat */

                count = sscanf(buffer, "%s", comando);
                /* se mancano i parametri notifico l'utente */
                if (count != 1){
                    printf("USO: /q\n");
                    continue;
                }
                printf("Torno al menù iniziale\n");
                chat = 0;
                gruppo = 0;

                /* visualizzo menu iniziale */
                print_menu();
            }
            else if(strcmp(comando, "/u") == 0 && chat == 1){
                /* utente vuole inziare una chat di gruppo */

                count = sscanf(buffer, "%s", comando);
                /* se mancano i parametri notifico l'utente */
                if (count != 1){
                    printf("USO: /u\n");
                    continue;
                }

                /* utente vuole iniziare una chat di gruppo */
                printf("Lista utenti online\n");

                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }

                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    
                    /* salvo il valore di logout che notificherò al server al prossimo accesso */
                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    /* salvo il valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
    
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    /* aggiorno i valori del file */
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
    
                    fclose(file);
    
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
                    
                    exit(0);
                }

                /* /u mittente */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, comando);
                strcat(buffer, " ");
                strcat(buffer, user.username);
                strcat(buffer, "\n");

                /* invio comando al server */
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }
                        
                /* ricevo dal server una lista di tutti gli utenti online,
                confronto con quelli sulla mia rubrica se stampare */                       
                while(1){
                        
                    ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(tcp_connect, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                        exit(1);        
                    }
                    sscanf(buffer, "%s", username_temp);
                            
                    /* controllo se messaggio di fine di lista */
                    if(strcmp(buffer, "Fine lista\n") == 0){
                        printf("%s", buffer);
                        break;                          
                    }
                            
                    /* verifico che sia presente in rubrica */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    while(1){
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */
                            break;                  
                        }           
                        if(strcmp(rubrica.user_dest, username_temp) == 0){
                            /* utente presente */
                            printf("%s", buffer);
                            break;                              
                        }   
                    }
                    fclose(file);   
                }

                /* stampo descrizione del comando */
                printf("Digitare: '/a username+INVIO' per aggiungere un utente alla chat\n");

                gruppo = 1;
            }
            else if(strcmp(comando, "/a") == 0 && chat == 1 && gruppo == 1){

                /* aggiunta di un membro alla chat di gruppo */

                /* copio valore username nella variabile temporanea */
                count = sscanf(buffer, "%s %s", comando, username_temp);
                /* se mancano i parametri notifico l'utente */
                if (count != 2){
                    printf("USO: /a <username>\n");
                    continue;
                }

                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }

                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    
                    /* salvo il valore di logout che notificherò al server al prossimo accesso */
                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    
                    /* salvo il valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
    
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    /* aggiorno il valore nel file*/
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
    
                    fclose(file);
            
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
    
                    exit(0);
                }

                /* /a mittente username */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, comando);
                strcat(buffer, " ");
                strcat(buffer, user.username);
                strcat(buffer, " ");
                strcat(buffer, username_temp);
                strcat(buffer, "\n");

                /* invio comando al server */
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }   

                /* se comando inserito valido aggiorno file rubrica */
                ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }
                        
                printf("%s", buffer);
                        
                if(strcmp(buffer, "Utente valido\n") == 0){
                                
                    /* ottengo il numero di porta per aggiornare la rubrica */
                    ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(tcp_connect, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                        exit(1);        
                    }           
                            
                    /* aggiorno file rubrica.bin */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("temp1.bin", "ab");
                    while(1){
                                
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */
                            break;                  
                        }           
                                
                        if(strcmp(rubrica.user_dest, username_temp) == 0){
                            /* aggiorno valore */
                            rubrica.attiva = 1;
                            sscanf(buffer, "%i", &port_temp);
                            rubrica.port = port_temp;
                        }
                                
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2); 
                    }
                    /* effettuo la sostituzione con il file aggiornato */
                    remove(nomeFile);
                    rename("temp1.bin", nomeFile);       
                    fclose(file);
                    fclose(file2);

                    close(tcp_connect);

                    /*creo una copia temporanea di rubrica*/
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("rubricatemp.bin", "ab");
                    while(1){
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */
                            break;                  
                        }
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2); 
                    }
                    fclose(file);
                    fclose(file2);

                    /* invio ai nuovi membri del gruppo i partecipanti alla chat */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("temp1.bin", "ab");
                    while(1){
                                
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */
                            break;                  
                        }           
                                
                        if(rubrica.attiva == 1){
                            /* utente fa parte della chat di gruppo */

                            /* creo socket per comunicare con l'utente */
                            tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                            if (tcp_connect < 0){
                                perror("Impossibile creare il socket TCP: \n");
                                exit(1);
                            }

                            memset(&bind_addr, 0, sizeof(bind_addr));
                            bind_addr.sin_port = htons(bind_portno);
                            bind_addr.sin_family = AF_INET;
                            inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                            ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                            if(ret < 0){
                                perror("Errore in fase di bind: \n");
                                exit(1);
                            }

                            memset(&connect_addr, 0, sizeof(connect_addr));
                            connect_addr.sin_port = htons(rubrica.port);
                            connect_addr.sin_family = AF_INET;
                            inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                            ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                            if(ret < 0){
                                
                                /* utente disconnesso, aggiorno i valori nella rubrica */
                                
                                rubrica.port = 0;
                                rubrica.attiva = 0;
                                
                                fwrite(&rubrica, sizeof(struct rubrica), 1, file2);

                                printf("Utente %s non connesso, non aggiungo al gruppo\n", username_temp);

                                continue;
                            }

                            fwrite(&rubrica, sizeof(struct rubrica), 1, file2);

                            memset(username_temp, 0, sizeof(username_temp));
                            strcpy(username_temp, rubrica.user_dest);
                                    
                            /* invio ACK al peer */
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, "CHAT\n");
                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }

                            /* invio username e porta di me stesso */
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, user.username);
                            strcat(buffer, " ");
                            sprintf(buffer_temp, "%i", portno);
                            strcat(buffer, buffer_temp);
                            strcat(buffer, "\n");
                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }

                            /* utilizzo il file della rubrica temporanea per sapere gli utenti partecipanti alla chat */
                            file3 = fopen("rubricatemp.bin", "rb");
                            while(1){
                                            
                                memset(&rubrica, 0, sizeof(struct rubrica));
                                ret = fread(&rubrica, sizeof(struct rubrica), 1, file3);
                                if(ret == 0){
                                    /* fine del file */
                                    break;                  
                                }           
                                        
                                if(rubrica.attiva == 1 && strcmp(rubrica.user_dest, username_temp) != 0){
                                    
                                    /* invio username e porta che deve aggiungere alle chat attive */
                                    memset(buffer, 0, sizeof(buffer));
                                    strcpy(buffer, rubrica.user_dest);
                                    strcat(buffer, " ");
                                    sprintf(buffer_temp, "%i", rubrica.port);
                                    strcat(buffer, buffer_temp);
                                    strcat(buffer, "\n");
                                    
                                    len = strlen(buffer) + 1;
                                    messageLen = htons(len);
                                    ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                                    ret = send(tcp_connect, (void*)buffer, len, 0);
                                    if(ret < 0){
                                        perror("Errore in fase di invio: \n");
                                        exit(1);
                                    }
                                }                                   
                            }
                            fclose(file3);

                            /* invio ACK al peer */
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, "FINECHAT\n");
                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }

                            close(tcp_connect);
                        }
                        else{
                            /* copio valore per il file da sostituire */
                            fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                        }       
                    }
                    /* sostituisco con il file aggiornato */
                    remove(nomeFile);
                    rename("temp1.bin", nomeFile);       
                    fclose(file);
                    fclose(file2);

                    /* elimino la copia temporanea */
                    remove("rubricatemp.bin");
                }
                else{
                    close(tcp_connect);
                }

                gruppo = 0;

                /* visualizzo comandi chat */
                print_help_chat();
            }
            else if(chat == 1){
                
                /* richiesto invio di messaggio */
                
                /* salvo il messaggio su un buffer di appoggio */
                memset(buffer_temp, 0, sizeof(buffer_temp));
                strcpy(buffer_temp, buffer);
                memset(msg_temp, 0, sizeof(msg_temp));
                strcpy(msg_temp, buffer);
                
                /* se ho già chat attive con il numero di porta settato vuol dire che non devo passare dal server */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, "rubrica.bin");
                file = fopen(nomeFile, "rb");
                file2 = fopen("temp1.bin", "ab");
                while(1){
                    
                    memset(&rubrica, 0, sizeof(struct rubrica));
                    ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                    if(ret == 0){
                        /* fine del file */
                        break;                  
                    }
                    if(rubrica.attiva == 1 && rubrica.port == 0){
                        
                        /* passo dal server */
                        /* creo socket per comunicare con il server */
                        tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                        if (tcp_connect < 0){
                            perror("Impossibile creare il socket TCP: \n");
                            exit(1);
                        }
                    
                        memset(&bind_addr, 0, sizeof(bind_addr));
                        bind_addr.sin_port = htons(bind_portno);
                        bind_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
                    
                        ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                        if(ret < 0){
                            perror("Errore in fase di bind: \n");
                            exit(1);
                        }
                    
                        memset(&connect_addr, 0, sizeof(connect_addr));
                        connect_addr.sin_port = htons(user.srv_port);
                        connect_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);
                    
                        ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                        if(ret < 0){
                            
                            /* passo all'iterazione successiva nel caso di una chat di gruppo conoscendo il nuemro di porta */
    
                            printf("Server non connesso, impossibile inviare il messaggio a %s\n", rubrica.user_dest);

                            rubrica.attiva = 0;

                            continue;
                        }
                    
                        /* msg  */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, buffer_temp);
                                
                        /* invio messaggio al server */
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* mittente username */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, user.username);
                        strcat(buffer, " ");
                        strcat(buffer, rubrica.user_dest);
                        strcat(buffer, "\n");

                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }
                    
                        /* a seconda dell'ACK che ricevo utente esegue azioni diverse */
                        ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        len = ntohs(messageLen);
                        ret = recv(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            printf("Errore in fase di ricezione: \n");
                            exit(1);        
                        }
                        
                        /* ACK di ricezione messaggio */
                        if(strcmp(buffer, "RIC\n") == 0){
                                    
                            /* messaggio inoltrato al destinatario */
                            printf("Messaggio inoltrato al destinatario da parte del server\n");
                                    
                            /* devo ricevere anche il numero di porta del destinatario */
                            ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            len = ntohs(messageLen);
                            ret = recv(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                printf("Errore in fase di ricezione: \n");
                                exit(1);        
                            }
                            sscanf(buffer, "%i", &port_temp);
                                    
                            /* aggiorno il numero di porta della rubrica */
                            rubrica.port = port_temp;                           
                                    
                            /* segno il messaggio nel file username_chat.bin come letto */  
                            memset(nomeFile2, 0, sizeof(nomeFile2));
                            strcpy(nomeFile2, user.username);
                            strcat(nomeFile2, rubrica.user_dest);
                            strcat(nomeFile2, "_chat.bin");                          
                            file3 = fopen(nomeFile2, "rb");
                            if(file3 == NULL){
                    
                                /* apro in scrittura per crearlo */
                                file3 = fopen(nomeFile2, "wb");
                                fclose(file3);
                                file3 = fopen(nomeFile2, "rb");
                            }
                            file4 = fopen("temp3.bin", "ab");
                            while(1){
                                        
                                /* inizializzo la strutttura per usarla come appoggio */
                                memset(buffer, 0, sizeof(buffer));
                                ret = fread(buffer, sizeof(buffer), 1, file3);
                                if(ret == 0){
                                    /* fine del file */
                                    break;                  
                                }
                                        
                                if(buffer[0] == '*' && buffer[1] != '*'){
                                    /* aggiorno messaggio come letto */
                                    memset(buffer_temp, 0, sizeof(buffer_temp));
                                    strcpy(buffer_temp, "*");
                                    strcat(buffer_temp, buffer);
                                    strcpy(buffer, buffer_temp);
                                    fwrite(buffer, sizeof(buffer), 1, file4);                         
                                }
                                else{
                                    fwrite(buffer, sizeof(buffer), 1, file4);
                                }
                            }
                            /* sostituisco il file con quello aggiornato */
                            remove(nomeFile2);
                            rename("temp3.bin", nomeFile2);                               
                            fclose(file3);
                            fclose(file4);
        
                            /* scrivo sul file l'ultimo messaggio inviato */
                            memset(nomeFile2, 0, sizeof(nomeFile2));
                            strcpy(nomeFile2, user.username);
                            strcat(nomeFile2, rubrica.user_dest);
                            strcat(nomeFile2, "_chat.bin");                          
                                
                            file3 = fopen(nomeFile2, "ab");
                            
                            memset(buffer, 0, sizeof(buffer));        
                            strcpy(buffer, "** ");
                            strcat(buffer, msg_temp);
                                
                            fwrite(buffer, sizeof(buffer), 1, file3);
                                    
                            fclose(file3);   
                        }
                    
                        /* ACK di salvataggio messaggio */
                        else if(strcmp(buffer, "MEM\n") == 0){
                            
                            /* messaggio salvato dal server */
                            /* segno il messaggio nel file username_chat.bin come non letto */
                            printf("Messaggio non recapitato\n");
                            
                            memset(nomeFile2, 0, sizeof(nomeFile2));
                            strcpy(nomeFile2, user.username);
                            strcat(nomeFile2, rubrica.user_dest);
                            strcat(nomeFile2, "_chat.bin");                          
                        
                            file3 = fopen(nomeFile2, "ab");
                        
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, "* ");
                            strcat(buffer, msg_temp);
                        
                            fwrite(buffer, sizeof(buffer), 1, file3);
                        
                            fclose(file3);                       
                        }
                    
                        close(tcp_connect);

                    }           
                    else if(rubrica.attiva == 1 && rubrica.port != 0){
                        
                        /* non passo dal server, invio il messaggio direttamente all'utente */
                        /* creo socket per comunicare con l'utente */
                        tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                        if (tcp_connect < 0){
                            perror("Impossibile creare il socket TCP: \n");
                            exit(1);
                        }

                        memset(&bind_addr, 0, sizeof(bind_addr));
                        bind_addr.sin_port = htons(bind_portno);
                        bind_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                        ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                        if(ret < 0){
                            perror("Errore in fase di bind: \n");
                            exit(1);
                        }

                        memset(&connect_addr, 0, sizeof(connect_addr));
                        connect_addr.sin_port = htons(rubrica.port);
                        connect_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                        ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                        if(ret < 0){
                            
                            /* utente non connesso, invio messaggio al server */
                            
                            printf("Utente %s non connesso, invio il messaggio al server\n", rubrica.user_dest);

                            /* creo socket per comunicare con il server */
                            tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                            if (tcp_connect < 0){
                                perror("Impossibile creare il socket TCP: \n");
                                exit(1);
                            }

                            memset(&bind_addr, 0, sizeof(bind_addr));
                            bind_addr.sin_port = htons(bind_portno);
                            bind_addr.sin_family = AF_INET;
                            inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                            ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                            if(ret < 0){
                                perror("Errore in fase di bind: \n");
                                exit(1);
                            }

                            memset(&connect_addr, 0, sizeof(connect_addr));
                            connect_addr.sin_port = htons(user.srv_port);
                            connect_addr.sin_family = AF_INET;
                            inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                            ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                            if(ret < 0){
                                
                                /* passo all'iterazione successiva nel caso di una chat di gruppo conoscendo il nuemro di porta */
    
                                printf("Server non connesso, impossibile inviare il messaggio a %s\n", rubrica.user_dest);

                                rubrica.attiva = 0;

                                presente = 1;
                            }
                            if(presente == 1){
                                presente = 0;
                                continue;
                            }
                                
                            /* msg  */
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, buffer_temp);
                            strcat(buffer, "\n");           
                            
                            /* invio messaggio al server */
                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }

                            /* mittente username */
                            memset(buffer, 0, sizeof(buffer));
                            strcpy(buffer, user.username);
                            strcat(buffer, " ");
                            strcat(buffer, rubrica.user_dest);
                            strcat(buffer, "\n");

                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }

                            /* a seconda dell'ACK che ricevo utente esegue azioni diverse */
                            ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            len = ntohs(messageLen);
                            ret = recv(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                printf("Errore in fase di ricezione: \n");
                                exit(1);        
                            }

                            /* ACK di ricezione messaggio */
                            if(strcmp(buffer, "RIC\n") == 0){
                                            
                                /* messaggio inoltrato al destinatario */
                                printf("Messaggio inoltrato al destinatario da parte del server\n");
                                            
                                /* devo ricevere anche il numero di porta del destinatario */
                                ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                                len = ntohs(messageLen);
                                ret = recv(tcp_connect, (void*)buffer, len, 0);
                                if(ret < 0){
                                    printf("Errore in fase di ricezione: \n");
                                    exit(1);        
                                }
                                sscanf(buffer, "%i", &port_temp);

                                /* inserisco il numero di porta nel file rubrica */
                                rubrica.port = port_temp;                           
                                                
                                /* segno il messaggio nel file username_chat.bin come letto */
                                memset(nomeFile2, 0, sizeof(nomeFile2));
                                strcpy(nomeFile2, user.username);    
                                strcat(nomeFile2, username_temp);
                                strcat(nomeFile2, "_chat.bin");                          
                                file3 = fopen(nomeFile2, "rb");
                                if(file3 == NULL){
                    
                                    /* apro in scrittura per crearlo */
                                    file3 = fopen(nomeFile2, "wb");
                                    fclose(file3);
                                    file3 = fopen(nomeFile2, "rb");
                                }
                                file4 = fopen("temp2.bin", "ab");
                                while(1){
                                                    
                                    /* inizializzo la strutttura per usarla come appoggio */
                                    memset(buffer, 0, sizeof(buffer));
                                    ret = fread(buffer, sizeof(buffer), 1, file3);
                                    if(ret == 0){
                                        /* fine del file */
                                        break;                  
                                    }
                                                    
                                    if(buffer[0] == '*' && buffer[1] != '*'){
                                        /* aggiorno messaggio come letto */
                                        memset(buffer_temp, 0, sizeof(buffer_temp));
                                        strcpy(buffer_temp, "*");
                                        strcat(buffer_temp, buffer);
                                        strcat(buffer, buffer_temp);
                                        
                                        fwrite(buffer_temp, sizeof(buffer_temp), 1, file4);                             
                                    }
                                    else{
                                        fwrite(buffer, sizeof(buffer), 1, file4);
                                    }
                                }
                                /* sostituisco il file con quello aggiornato */
                                remove(nomeFile2);
                                rename("temp2.bin", nomeFile2);                               
                                fclose(file3);
                                fclose(file4);
                
                                /* scrivo sul file l'ultimo messaggio inviato */
                                memset(nomeFile2, 0, sizeof(nomeFile2));
                                strcpy(nomeFile2, user.username);
                                strcat(nomeFile2, rubrica.user_dest);
                                strcat(nomeFile2, "_chat.bin");                          
                                file3 = fopen(nomeFile2, "ab");
                                
                                memset(buffer, 0, sizeof(buffer));           
                                strcpy(buffer, "** ");
                                strcat(buffer, msg_temp);
                                        
                                fwrite(buffer, sizeof(buffer), 1, file3);
                                        
                                fclose(file3);  
                            }
                            /* ACK di salvataggio messaggio */
                            else if(strcmp(buffer, "MEM\n") == 0){
                                                
                                /* messaggio salvato dal server */
                                /* segno il messaggio nel file username_chat.bin come non letto */
                                printf("Messaggio non recapitato\n");
                                
                                memset(nomeFile2, 0, sizeof(nomeFile2));            
                                strcpy(nomeFile2, user.username);
                                strcat(nomeFile2, rubrica.user_dest);
                                strcat(nomeFile2, "_chat.bin");                          
                                file3 = fopen(nomeFile2, "ab");
                                
                                memset(buffer, 0, sizeof(buffer));            
                                strcpy(buffer, "* ");
                                strcat(buffer, msg_temp);
                                            
                                fwrite(buffer, sizeof(buffer), 1, file3);
                                        
                                fclose(file3);                      
                            }
                            close(tcp_connect);

                            continue;
                        }

                        /* invio ACK al PEER */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "MSGPEER\n");
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* invio username */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, user.username);
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* invio porta */
                        sprintf(buffer, "%i", portno);
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* invio msg */ 
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, buffer_temp);
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* ottengo risposta dal peer */
                        ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        len = ntohs(messageLen);
                        ret = recv(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            printf("Errore in fase di ricezione: \n");
                            exit(1);        
                        }

                        close(tcp_connect);

                        if(strcmp(buffer, "MSGPEEROK\n") == 0){

                            /* messaggio ricevuto salvo nel file della chat */
                            memset(nomeFile2, 0, sizeof(nomeFile2));
                            strcpy(nomeFile2, user.username);
                            strcat(nomeFile2, rubrica.user_dest);
                            strcat(nomeFile2, "_chat.bin");
                            file3 = fopen(nomeFile2, "ab");
                            
                            memset(buffer, 0, sizeof(buffer));            
                            strcpy(buffer, "**");
                            strcat(buffer, buffer_temp);
                                        
                            fwrite(buffer, sizeof(buffer), 1, file3);
                                        
                            fclose(file3);
                                        
                            printf("Messaggio ricevuto dall'utente\n");
                        }                                            
                    }
                    /* scrivo nel file aggiornato */
                    fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                }
                /* sostituisco con il file aggiornato */
                remove(nomeFile);
                rename("temp1.bin", nomeFile);
                fclose(file);
                fclose(file2);
            }
			else if (strcmp(comando, "signup") == 0 && loggato  == 0){
                
                /* richiesta registrazione */
				
				count = sscanf(buffer, "%s %s %s", comando, username, password);
                /* se mancano i parametri notifico l'utente */
                if (count != 3){
                    printf("USO: signup <username> <password>\n");
                    continue;
                }
                
                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }
    			
    			memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
    			
    			ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }
				
				memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(4242);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);
    			
    			ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    perror("Errore in fase di connect: \n");
                    exit(1);
                }
				
				len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }
				
				printf("Richiesta iscrizione inviata al server\n");
				
				ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
                ret = recv(tcp_connect, (void*)buffer, len, 0);
				
				if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }
    			
    			close(tcp_connect);

                /* risultato inviato dal server */
				printf("%s", buffer);
			}
			else if (strcmp(comando, "in") == 0 && loggato  == 0){
                
                /* richiesta connessione al servizio */

                memset(&user, 0, sizeof(struct user));
				
                count = sscanf(buffer, "%s %i %s %s", comando, &user.srv_port, user.username, user.password);
                /* se mancano i parametri notifico l'utente */
                if (count != 4){
                    printf("USO: in <srv_port> <username> <password>\n");
                    continue;
                }
				
                memset(username, 0, sizeof(username));
                strcpy(username, user.username);

                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }

                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    perror("Errore in fase di connect: \n");
                    exit(1);
                }

				/* buffer contiene il comando con i parametri */
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }				
				
				printf("Richiesta connessione al servizio inviata al server\n");

                /* invio la porta dello user */
                sprintf(buffer, "%i", portno);
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }

                /* ottengo risposta dal server */
                ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }

				/* risultato inviato dal server */
				printf("%s", buffer);
						
				/* se comando in ha avuto successo setto utente loggato */
				if(strcmp(buffer, "Login effettuato\n") == 0){
					
					loggato = 1;
                    chat = 0;
                    gruppo = 0;

                    /* controllo se devo inviare al server il dato di logout */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "rb");
                    if(file == NULL){
                    	
                        /* apro in scrittura per crearlo */
						file = fopen(nomeFile, "wb");
						fclose(file);
						file = fopen(nomeFile, "rb");
						
                    }
                    
					memset(&disconnessione, 0, sizeof(struct disconnessione));
                    ret = fread(&disconnessione, sizeof(struct disconnessione), 1, file);
                    
                    if(ret != 0){
                        /* file non vuoto devo inviare il timestamp aggiornato */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "DISCONNESSIONE ");
                        strcat(buffer, disconnessione.username);
                        strcat(buffer, " ");
                        strcat(buffer, disconnessione.timestamp);
                        strcat(buffer, "\n");
                        
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }
                        printf("Inviato il valore di logout aggiornato al server\n");
                    }
                    else{
						
                        /* non devo inviare nessun timestamp */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "NODISCONNESSIONE\n");
                        
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }
                    }
                    remove(nomeFile);
                    fclose(file);

                    /* resetto le porte degli utenti della rubrica per evitare disconnessioni non segnalate */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("temp1.bin", "ab");
                    while(1){
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */                     
                            break;                   
                        }
                
                        /* aggiorno la porta */
                        rubrica.port = 0;
                        rubrica.attiva = 0;         
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                    }
                    /* sostituisco con il file aggiornato */
                    remove(nomeFile);
                    rename("temp1.bin", nomeFile);
                    fclose(file);
                    fclose(file2);

					/* user loggato, stampo i comandi successivi */
					print_menu();
				}
				/* se comando in non ha avuto successo */
				else{
					loggato = 0;				
				}

                close(tcp_connect);
			}
			else if (strcmp(comando, "hanging") == 0 && loggato == 1){
                
                /* richieste info messaggi pendenti */

                count = sscanf(buffer, "%s", comando);
                /* se mancano i parametri notifico l'utente */
                if (count != 1){
                    printf("USO: hanging\n");
                    continue;
                }

                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }

                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    
                    /* salvo l'istante di disconnessione che invierò al server al prossimo accesso */
    
                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
        
                    /* salvo valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
    
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
    
                    fclose(file);
    
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
    
                    exit(0);
                }

                /* hanging mittente */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, comando);
                strcat(buffer, " ");
                strcat(buffer, user.username);
                strcat(buffer, "\n");

                /* invio la richiesta */
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }

				printf("Richiesta info messaggi pendenti\n");

                /*ciclo fine a che non ho ricevuto tutte le informazioni */
                while(1){

				    ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(tcp_connect, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                    	exit(1);        
                    }

                    /* risultato inviato dal server */
                    printf("%s", buffer);   
							
					if(strcmp(buffer, "Fine messaggi pendenti\n") == 0){
						/* i messaggi pendenti sono finiti */
						break;
					}					
                }

                close(tcp_connect);
			}
			else if (strcmp(comando, "show") == 0 && loggato == 1){
                
                /* richiesti messaggi pendenti */

                count = sscanf(buffer, "%s %s", comando, username_temp);
                /* se mancano i parametri notifico l'utente */
                if (count != 2){
                    printf("USO: show <username>\n");
                    continue;
                }
				
                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }

                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    
                    /* salvo l'istante di disconnessione che invierò al server al prossimo accesso */
    
                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    
                    /* salvo valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
        
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
        
                    fclose(file);
    
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
    
                    exit(0);
                }

                /* show mittente username */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, comando);
                strcat(buffer, " ");
                strcat(buffer, user.username);
                strcat(buffer, " ");
                strcat(buffer, username_temp);
                strcat(buffer, "\n");

                /* buffer contiene il comando con i parametri */
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }
					
				printf("Richiesti messaggi pendenti\n");

                /*ciclo fine a che non ho ricevuto tutte le informazioni */
				while(1){
					
                    ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(tcp_connect, (void*)buffer, len, 0);
                    if(ret < 0){
                    	printf("Errore in fase di ricezione: \n");
                      	exit(1);        
                    }

                    /* risultato inviato dal server */
                    printf("%s", buffer);   
							
					if(strcmp(buffer, "Non ci sono messaggi pendenti dall'utente richiesto\n") == 0){
						/* Non ci sono messaggi pendenti\n */
						break;
					}
					else if(strcmp(buffer, "Fine dei messaggi pendenti\n") == 0){
						/* sono finiti i messaggi pendenti */
						break;					
					}
					else{
						/* sono i messaggi pendenti che devo aggiornare nel file */
                        memset(nomeFile, 0, sizeof(nomeFile));
						strcpy(nomeFile, user.username);
						strcat(nomeFile, username_temp);
						strcat(nomeFile, "_chat.bin");
						file = fopen(nomeFile, "ab");
						/* scrivo chat nel file in append */
						fwrite(buffer, sizeof(buffer), 1, file);
						fclose(file);					
					}					
                }

                close(tcp_connect);
			}
			else if (strcmp(comando, "chat") == 0 && loggato == 1){
                
                /* richiesto inizio chat */

				/* copio valore username del destinatario */
				count = sscanf(buffer, "%s %s", comando, username_temp);
                /* se mancano i parametri notifico l'utente */
                if (count != 2){
                    printf("USO: chat <username>\n");
                    continue;
                }

                /* controllo se lo username richiesto è presente nella rubrica */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, "rubrica.bin");
                file = fopen(nomeFile, "rb");
                while(1){
                    
                    /* inizializzo la strutttura per usarla come appoggio */
                    memset(&rubrica, 0, sizeof(struct rubrica));
                    ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                    if(ret == 0){
                        /* fine del file */
                        break;                  
                    }
                    if(strcmp(rubrica.user_dest, username_temp) == 0){
                        /* setto questa chat come attiva */
                        presente = 1;
                        break;
                    }
                }
                fclose(file);

                if( presente == 0){
                    /* utente non valido */
                    printf("Utente selezionato non presente in rubrica\n");
                    continue;
                }
                else{
                    /* utente valido */
                    presente = 0;
                }
                
				printf("Richiesto comando chat\n");
                
                /* creo socket per comunicare con il server */
                tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }
    
                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
    
                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);
    
                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    
                    /* salvo l'istante di disconnessione che invierò al server al prossimo accesso */
    
                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    
                    /* salvo il valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
    
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
    
                    fclose(file);
    
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
    
                    exit(0);
                }

                /* invio comando al server */
                strcpy(buffer, comando);
                strcat(buffer, "\n");
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }

                /* invio il mio username */
                strcpy(buffer, user.username);
                strcat(buffer, "\n");
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }

                /* invio lo username dell'utente con cui voglio iniziare una chat */
                strcpy(buffer, username_temp);
                strcat(buffer, "\n");
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }
                
                ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }
    
                close(tcp_connect);

				if(strcmp(buffer, "RIC\n") == 0){
                    /* messaggio ricevuto */
					/* aggiorno le spunte dei messaggi nella chat */
                    memset(nomeFile, 0, sizeof(nomeFile));
					strcpy(nomeFile, user.username);
					strcat(nomeFile, username_temp);
					strcat(nomeFile, "_chat.bin");
					file = fopen(nomeFile, "rb");
					if(file == NULL){
					
						/* apro in scrittura per crearlo */
						file = fopen(nomeFile, "wb");
						fclose(file);
						file = fopen(nomeFile, "rb");
					}
					file2 = fopen("temp1.bin", "ab");
					while(1){
						memset(buffer, 0, sizeof(buffer));
						ret = fread(buffer, sizeof(buffer), 1, file);
						if(ret == 0){
							/* fine del file */						
							break;					
						}
						if(buffer[0] == '*' && buffer[1] != '*'){
							/* aggiorno il messaggio come ricevuto */
                            memset(buffer_temp, 0, sizeof(buffer_temp));
							strcpy(buffer_temp, "*");
							strcat(buffer_temp, buffer);
                            memset(buffer, 0, sizeof(buffer));
							strcpy(buffer, buffer_temp);
						}
					    fwrite(buffer, sizeof(buffer), 1, file2);
					}
                    /* sostituisco con il file aggiornato */
					remove(nomeFile);
					rename("temp1.bin", nomeFile);
					fclose(file);
					fclose(file2);
				}
				else{
					printf("%s", buffer);
				}
                
                /* azzero tutte le chat attive e lascio attiva la chat con username specificato dal comando */
                memset(nomeFile, 0, sizeof(nomeFile));
				strcpy(nomeFile, user.username);
				strcat(nomeFile, "rubrica.bin");
				file = fopen(nomeFile, "rb");
				file2 = fopen("temp1.bin", "ab");
				while(1){
					/* inizializzo la strutttura per usarla come appoggio */
					memset(&rubrica, 0, sizeof(struct rubrica));
					ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
					if(ret == 0){
						/* fine del file */
						break;					
					}
					if(rubrica.attiva == 1){
						/* aggiorno il campo relativo alla chat */
						rubrica.attiva = 0;							
					}
					if(strcmp(rubrica.user_dest, username_temp) == 0){
						/* setto questa chat come attiva */
						rubrica.attiva = 1;
					}
					fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
				}
                /* sostituisco con il file aggiornato */
				remove(nomeFile);
				rename("temp1.bin", nomeFile);										
				fclose(file);
				fclose(file2);

				/* visualizzo il contenuto della chat con l'utente selezionato */
                memset(nomeFile, 0, sizeof(nomeFile));
				strcpy(nomeFile, user.username);
				strcat(nomeFile, username_temp);				
				strcat(nomeFile, "_chat.bin");
				file = fopen(nomeFile, "rb");
				if(file == NULL){
					
					/* apro in scrittura per crearlo */
					file = fopen(nomeFile, "wb");
					fclose(file);
					file = fopen(nomeFile, "rb");
				}
				while(1){
                    
                    /* inizializzo la strutttura per usarla come appoggio */
					memset(buffer, 0, sizeof(buffer));
					ret = fread(buffer, sizeof(buffer), 1, file);
					if(ret == 0){
						/* fine del file */
						break;					
					}
					printf("%s", buffer);
				}
				fclose(file);
				
				/* visualizzo comandi chat */
				print_help_chat();

                chat = 1;
            }
            else if (strcmp(comando, "share") == 0 && loggato == 1){
                
                /* richiesta condivisione file */

                /* ottengo il nome del file */
                count = sscanf(buffer, "%s %s", comando, nomeFile);
                /* se mancano i parametri notifico l'utente */
                if (count != 2){
                    printf("USO: share <file_name>\n");
                    continue;
                }
                /* ottengo il nome del file */
                strcat(nomeFile, ".bin");
                file = fopen(nomeFile, "rb");
                if(file == NULL){
                    printf("File non esistente\n");
                    continue;
                }
                fclose(file);
                
                /* comando di condivisione */
                printf("Richiesta condivisione file\n");

                /* invio file agli utenti con le chat attive */
                memset(nomeFile2, 0, sizeof(nomeFile2));
                strcpy(nomeFile2, user.username);
                strcat(nomeFile2, "rubrica.bin");
                file = fopen(nomeFile2, "rb");
                file4 = fopen("temp1.bin", "ab");
                while(1){
                    
                    /* inizializzo struttura per usarla come appoggio */
                    memset(&rubrica, 0, sizeof(struct rubrica));
                    ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                    if(ret == 0){
                            /* fine del file */                     
                            break;                  
                    }
                    if(rubrica.attiva == 1){
                        
                        /* creo socket per comunicare con l'utente */
                        tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                        if (tcp_connect < 0){
                            perror("Impossibile creare il socket TCP: \n");
                            exit(1);
                        }

                        memset(&bind_addr, 0, sizeof(bind_addr));
                        bind_addr.sin_port = htons(bind_portno);
                        bind_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                        ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                        if(ret < 0){
                            perror("Errore in fase di bind: \n");
                            exit(1);
                        }

                        memset(&connect_addr, 0, sizeof(connect_addr));
                        connect_addr.sin_port = htons(rubrica.port);
                        connect_addr.sin_family = AF_INET;
                        inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                        ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                        if(ret < 0){
                            /* utente non connesso aggiorno i valori nella rubrica */
                            rubrica.port = 0;
                            rubrica.attiva = 0;
                            
                            fwrite(&rubrica, sizeof(struct rubrica), 1, file4);
                            
                            printf("Utente %s non connesso, impossibile condivisione\n", username_temp);

                            continue;
                        }
                        else{
                            fwrite(&rubrica, sizeof(struct rubrica), 1, file4);
                        }

                        /* salvo il nome del destinatario */
                        memset(username_temp, 0, sizeof(username_temp));
                        strcpy(username_temp, rubrica.user_dest);

                        /* salvo la porta del destinatario */
                        port_temp = rubrica.port;

                        /* invio ACK di inizio ai peer */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "FILE\n");
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* invio il nome del file */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, nomeFile);
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        file2 = fopen(nomeFile, "rb");
                        /* invio il file */
                        while(1){
                            
                            /* inizializzo buffer per usarlo come appoggio */
                            memset(buffer, 0, sizeof(buffer));
                            ret = fread(buffer, sizeof(buffer), 1, file2);
                            if(ret == 0){
                                /* fine del file */                     
                                break;                  
                            }
                            strcat(buffer, "\n");
                            
                            /* invio al device */
                            len = strlen(buffer) + 1;
                            messageLen = htons(len);
                            ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                            ret = send(tcp_connect, (void*)buffer, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);
                            }
                        }
                        fclose(file2);

                        /* invio ACK di fine */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "FINE\n");
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);
                        }

                        /* ottengo risposta dal peer */
                        ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                        len = ntohs(messageLen);
                        ret = recv(tcp_connect, (void*)buffer, len, 0);
                        if(ret < 0){
                            printf("Errore in fase di ricezione: \n");
                            exit(1);        
                        }

                        if(strcmp(buffer, "FILEOK\n") == 0){
                            printf("File condiviso correttamente con %s\n", username_temp);
                        }

                        close(tcp_connect);
                    }
                    else{
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file4);
                    }
                }
                /* sostituisco con il file  aggiornato */
                remove(nomeFile2);
                rename("temp1.bin", nomeFile2);
                fclose(file);
                fclose(file4);
                printf("Fine condivisione del file\n");
            }
			else if (strcmp(comando, "out") == 0 && loggato == 1){	
                
                /* richiesta disconnessione */

                count = sscanf(buffer, "%s", comando);
                /* se mancano i parametri notifico l'utente */
                if (count != 1){
                    printf("USO: out\n");
                    continue;
                }

				printf("Richiesta una disconnessione dal network\n");

                /* imposto la porta delle chat a NULL così da inviare i prossimi messaggi al server */
                memset(nomeFile, 0, sizeof(nomeFile));
				strcpy(nomeFile, user.username);
				strcat(nomeFile, "rubrica.bin");
				file = fopen(nomeFile, "rb");
				file2 = fopen("temp1.bin", "ab");
				while(1){
					
					memset(&rubrica, 0, sizeof(struct rubrica));
					ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
					if(ret == 0){
						/* fine del file */						
						break;					
					}
						
					if(rubrica.port != 1){
						/* aggiorno la porta */
						rubrica.port = 0;			
					}
						
					fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
				}
                /* sostituisco con il file aggiornato */
				remove(nomeFile);
				rename("temp1.bin", nomeFile);
				fclose(file);
				fclose(file2);

                /* out username */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, comando);
                strcat(buffer, " ");
                strcat(buffer, user.username);
                strcat(buffer, "\n");

                /* creo sockket per comunicare con il server */
				tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                if (tcp_connect < 0){
                    perror("Impossibile creare il socket TCP: \n");
                    exit(1);
                }
    
                memset(&bind_addr, 0, sizeof(bind_addr));
                bind_addr.sin_port = htons(bind_portno);
                bind_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
    
                ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                if(ret < 0){
                    perror("Errore in fase di bind: \n");
                    exit(1);
                }

                memset(&connect_addr, 0, sizeof(connect_addr));
                connect_addr.sin_port = htons(user.srv_port);
                connect_addr.sin_family = AF_INET;
                inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);
    
                ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                if(ret < 0){
                    /* server non connesso salvo il valore di logout che notificherò al prossimo accesso */

                    printf("Server non connesso, salvo il valore di logout\n");
    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    
                    /* salvo valore nel file disconnessione.bin */
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "disconnessione.bin");
                    file = fopen(nomeFile, "ab");
    
                    memset(&disconnessione, 0, sizeof(struct disconnessione));
    
                    /* recupero l'ora corrente */
                    time(&rawtime);
                    /* converto l'ora */
                    timeinfo = localtime(&rawtime);
                    strcpy(disconnessione.timestamp, asctime(timeinfo));
                    strcpy(disconnessione.username, user.username);
    
                    fwrite(&disconnessione, sizeof(struct disconnessione), 1, file);
    
                    fclose(file);
    
                    loggato = 0;
                    chat = 0;
                    gruppo = 0;
    
                    exit(0);
                }
    			
                /* invio richiesta di disconnessione */
    			len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);
                }

                ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(tcp_connect, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }
    
                close(tcp_connect);
                
                /* controllo il messaggio inviato dal server */
                if(strcmp(buffer, "OUTOK\n") == 0){
                    /* richiesta ricevuta correttamente */
					printf("Disconnessione effettuata correttamente\n");
				}

				loggato = 0;
                chat = 0;
                gruppo = 0;
				exit(0);
			}
			else{ 

				/* utente ha inserito un comando non valido */
				printf("Per favore inserire un comando valido \n");
                chat = 0;
                gruppo = 0;
			}
			
			/* rimuovo il descrittore da quelli da monitorare */
			FD_CLR(0, &read_fds);
		}

		/* controllo se il socket di ascolto è pronto */
		if(FD_ISSET(tcp_listener, &read_fds)){ /* socket di ascolto TCP */
            
            addrlen = sizeof(appoggio);

            /* accetto la richiesta di connessione */
            accept_socket = accept(tcp_listener, (struct sockaddr*)&appoggio, &addrlen);
            if(accept_socket == -1){
                perror("Errore nella accept(): \n");
                exit(1);
            }

            /* ricevo i dati */
            ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
            len = ntohs(messageLen);
            ret = recv(accept_socket, (void*)buffer, len, 0);
            if(ret < 0){
                printf("Errore in fase di ricezione: \n");
                exit(1);        
            }

            /* ottengo il comando senza parametri */
            sscanf(buffer, "%s", comando);

            printf("Richiesta ricevuta dall'esterno\n");
                
            if(strcmp(comando, "MSG") == 0){ /* PRIMO MESSAGGIO */
                
                /* ottengo username mittente */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
                sscanf(buffer, "%s", username_temp);
                
                /* ottengo messaggio */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
        
                /* aggiorno il file della chat per avere la cronologia aggiornata */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, username_temp);
                strcat(nomeFile, "_chat.bin");
                file = fopen(nomeFile, "ab");
                fwrite(buffer, sizeof(buffer), 1, file);
                fclose(file);       

                /* invio ACK di chiusura */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, "MSGOK\n");
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);       
                }
                printf("Nuovo messaggio da %s\n", username_temp);
            }
            else if(strcmp(comando, "RIC") == 0){ /* RICEZIONE DI UN MESSAGGIO */
                
                /* ottengo username di chi ha letto i miei messaggi */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
                memset(username_temp, 0, sizeof(username_temp));
                strcpy(username_temp, buffer);

                printf("%s ha ricevuto i tuoi messaggi\n", username_temp);

                /* aggiorno le spunte dei messaggi nella chat */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, username_temp);
                strcat(nomeFile, "_chat.bin");
                file = fopen(nomeFile, "rb");
				if(file == NULL){
					
					/* apro in scrittura per crearlo */
					file = fopen(nomeFile, "wb");
					fclose(file);
					file = fopen(nomeFile, "rb");
				}
                file2 = fopen("tempa.bin", "ab");
                while(1){
        
                    memset(buffer, 0, sizeof(buffer));
                    ret = fread(buffer, sizeof(buffer), 1, file);
                    if(ret == 0){
                        /* fine del file */                     
                        break;                   
                    }
        
                    if(buffer[0] == '*' && buffer[1] != '*'){
                        /* aggiorno il messaggio come ricevuto */
                        memset(buffer_temp, 0, sizeof(buffer_temp));
                        strcpy(buffer_temp, "*");
                        strcat(buffer_temp, buffer);
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, buffer_temp);
                    }
                    fwrite(buffer, sizeof(buffer), 1, file2);
                }
                /* sostituisco con il file aggiornato */
                remove(nomeFile);
                rename("tempa.bin", nomeFile);
                fclose(file);
                fclose(file2);
            }
            else if(strcmp(comando, "LOGOUT") == 0){ /* NOTIFICA UTENTI DISCONNESSI */
                    
                    /* ottengo username di chi si è disconnesso */
                    ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(accept_socket, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                        exit(1);
                    }
                    sscanf(buffer, "%s", username_temp);

                    printf("%s si è disconnesso\n", username_temp);

                    /* imposto la porta dell'utente a NULL così da inviare i prossimi messaggi al server */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("tempa.bin", "ab");
                    while(1){
                            
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */                     
                             break;                   
                        }
                            
                        if(strcmp(rubrica.user_dest, username_temp) == 0){
                            /* aggiorno la porta */
                            rubrica.port = 0;
                            rubrica.attiva = 0;         
                        }
                            
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                    }
                    /* sostituisco con il file aggiornato */
                    remove(nomeFile);
                    rename("tempa.bin", nomeFile);
                    fclose(file);
                    fclose(file2);
                    
                    /* invio ACK di ricevuto logout al server */
                    memset(buffer, 0, sizeof(buffer));
                    strcpy(buffer, "LOGOUTOK\n");
                    len = strlen(buffer) + 1;
                    messageLen = htons(len);
                    ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                    ret = send(accept_socket, (void*) buffer, len, 0);
                    if(ret < 0){
                        perror("Errore in fase di invio: \n");
                        exit(1);                       
                    }
            }
            else if(strcmp(comando, "FILE") == 0){ /* RICEZIONE DI UN FILE */
                
                printf("Inizio ricezione di un file\n");

                /* ottengo il nome del file */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);        
                }
                sscanf(buffer, "%s", nomeFile);
                
                printf("File: %s\n", nomeFile);
                
                /* creo il file */
                memset(nomeFile2, 0, sizeof(nomeFile2));
                strcpy(nomeFile2, user.username);
                strcat(nomeFile2, nomeFile);
                file = fopen(nomeFile2, "ab");
                while(1){
                
                    /* ricevo le parti del file */
                    ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(accept_socket, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                        exit(1);       
                    }

                    if(strcmp(buffer, "FINE\n") == 0){
                            
                        /* fine file */
                        printf("File condiviso correttamente\n");

                        /*invio ACK di ricezione */
                        memset(buffer, 0, sizeof(buffer));
                        strcpy(buffer, "FILEOK\n");
                        len = strlen(buffer) + 1;
                        messageLen = htons(len);
                        ret = send(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                        ret = send(accept_socket, (void*)buffer, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);       
                        }

                        break;           
                    }
                    else{
                        /* scrivo nel file */
                        fwrite(buffer, sizeof(buffer), 1, file);
                    }
                }
                fclose(file);
            }
            else if(strcmp(comando, "MSGPEER") == 0){ /* NUOVO MESSAGGIO */
                
                /* ricevo il nome del mittente */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
                /* salvo lo username */
                sscanf(buffer, "%s", username_temp);

                /* ricevo la porta del mittente */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
                /* salvo la porta */
                sscanf(buffer, "%i", &port_temp);

                /* ricevo il messaggio */
                ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                len = ntohs(messageLen);
                ret = recv(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    printf("Errore in fase di ricezione: \n");
                    exit(1);
                }
                /* salvo il messaggio */
                memset(buffer_temp, 0, sizeof(buffer_temp));
                strcpy(buffer_temp, buffer);

                printf("Ricevuto messaggio da %s\n", username_temp);

                /* conosco lo username, controllo se devo aggiornare la porta */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, "rubrica.bin");
                file = fopen(nomeFile, "rb");
                file2 = fopen("tempa.bin", "ap");
                while(1){
                    
                    memset(&rubrica, 0, sizeof(struct rubrica));
                    ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                    if(ret == 0){
                        /* fine del file */                     
                        break;                   
                    }
                    if(strcmp(username_temp, rubrica.user_dest) == 0 && port_temp != rubrica.port){
                        /* agggiorno la porta */
                        rubrica.port = port_temp;
                    }
                    fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                }
                /* sostituisco con il file aggiornato */
                remove(nomeFile);
                rename("tempa.bin", nomeFile);
                fclose(file);
                fclose(file2);

                /* aggiorno il file della chat per avere la cronologia aggiornata */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, user.username);
                strcat(nomeFile, username_temp);
                strcat(nomeFile, "_chat.bin");
                file = fopen(nomeFile, "ab");

                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, buffer_temp);

                fwrite(buffer, sizeof(buffer), 1, file);
                fclose(file);       
               
                /* invio ACK di chiusura */
                memset(buffer, 0, sizeof(buffer));
                strcpy(buffer, "MSGPEEROK\n");
                len = strlen(buffer) + 1;
                messageLen = htons(len);
                ret = send(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                ret = send(accept_socket, (void*)buffer, len, 0);
                if(ret < 0){
                    perror("Errore in fase di invio: \n");
                    exit(1);       
                }
            }
            else if(strcmp(comando, "CHAT") == 0){ /* AGGIUNTA MEMBRI ALLA CHAT DI GRUPPO */
                
                printf("Aggiorno informazioni per la chat di gruppo\n");
                while(1){
                    /* ottengo username degli utenti appartenenti alla chat e nel caso li aggiungo alla rubrica */
                    ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
                    len = ntohs(messageLen);
                    ret = recv(accept_socket, (void*)buffer, len, 0);
                    if(ret < 0){
                        printf("Errore in fase di ricezione: \n");
                        exit(1);
                    }
                    if(strcmp(buffer, "FINECHAT\n") == 0){
                        break;
                    } 

                    /* salvo il valore dello username e porta passato dal peer */
                    sscanf(buffer, "%s %i", username_temp, &port_temp);
                        
                    /* scorro il file rubrica per aggiornarlo */
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, user.username);
                    strcat(nomeFile, "rubrica.bin");
                    file = fopen(nomeFile, "rb");
                    file2 = fopen("tempc.bin", "ab");
                    while(1){
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        ret = fread(&rubrica, sizeof(struct rubrica), 1, file);
                        if(ret == 0){
                            /* fine del file */
                            break;                   
                        }           
                        if(strcmp(rubrica.user_dest, username_temp) == 0){
                            rubrica.attiva = 1;
                            rubrica.port = port_temp;
                            presente = 1;                           
                        }
                        /* aggiorno il file */
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                    }
                    if(presente == 0){
                        /* devo aggiungere un nuovo record in rubrica */
                        memset(&rubrica, 0, sizeof(struct rubrica));
                        strcpy(rubrica.user_dest, username_temp);
                        rubrica.port = port_temp;
                        rubrica.attiva = 1;
                        fwrite(&rubrica, sizeof(struct rubrica), 1, file2);
                    }
                    else{
                        presente = 0;
                    }
                    /* sostituisco con il file aggiornato */
                    remove(nomeFile);
                    rename("tempc.bin", nomeFile);
                    fclose(file);
                    fclose(file2);

                    printf("%s fa parte della chat di gruppo\n", username_temp);   
                }
                printf("Aggiornata rubrica per chat di gruppo\n");
            }
            else{ /* COMANDO NON VALIDO */

                printf("Ricevuto comando non valido\n");
            }

            close(accept_socket);
            
            /* rimuovo il descrittore da quelli da monitorare */
            FD_CLR(tcp_listener, &read_fds);

        }

	}
}
