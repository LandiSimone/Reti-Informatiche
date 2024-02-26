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
};

/* contiene le inforazioni relative al registro */
struct registro{
	char user_dest[DIM_USERNAME];
	int port;
	char timestamp_login[50];
	char timestamp_logout[50];
};

/* contiene le informazioni relative ai messaggi pendenti */
struct msg_pendenti{
	char mittente[DIM_USERNAME];
	int num_msg;
	char timestamp[50];
};

/* struttura per ottenere l'ora attuale */
time_t rawtime;
struct tm* timeinfo;

int tcp_listener, tcp_connect, accept_socket; /* socket */
struct sockaddr_in self_addr; /* struttura server */
struct sockaddr_in connect_addr, bind_addr; /* strutture per connessioni TCP */
struct sockaddr_in appoggio;
socklen_t addrlen;
int portno; /* porta del peer */
int len; /* contiene la lunghezza dei messaggi */
int ret; /* variabile utile al salvataggio di vaalori di ritorno di funzioni */
int porta_temp; /* variabile temporanea per passare il numero di porta ai device */
uint16_t messageLen; /* variabile che contiene la lunghezza del messaggio in ricezione */
char buffer[DIM_BUFFER]; /* contenitore per la ricezione di messaggi */
char buffertemp[DIM_BUFFER]; /* contenitore di appoggio per l'invio di messaggi */
struct user user; /* variabile che contiene le informazioni dell'utente loggato */
struct registro registro; /* variabile che contiene le informazioni degli utenti nel registro */
struct msg_pendenti msg_pendenti; /* variabile che contiene le informazioni relative ai messaggi pendenti */
char username[DIM_USERNAME]; /* contiene l'username dell'utente che vuole loggarsi */
char password[DIM_PASSWORD]; /* contiene la password dell'utente che vuole loggarsi */
char username_temp[DIM_USERNAME]; /* contiene uno username di appoggio */
int srv_port; /* contiene il valore della porta specificato dal device */
int dev_port; /* contiene il valore della porta del device */ 
char nomeFile[50]; /* nome del file che si vuole aprire */
char comando[50]; /* stringa contenente la funzione da eseguire */
char risposta[DIM_BUFFER]; /* stringa contenente il messaggio di risposta da recapitare al client */
FILE *file, *file2, *file3, *file4; /* puntatore a file */
char ingresso[DIM_BUFFER]; /* variabile utile a contenere temporaneamente una stringa in ingresso */
int presente; /* variabile per sapere se un utente è presente o meno all'interno di un file */
char timestamp_temp[50]; /* variabile temporanea per il timestamp */

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
	printf(" SERVER STARTED ");
	for (i = 0; i < 20; i++)
		printf("*");
	printf("\n");

	printf("Digita un comando: \n\n");
	printf("1) help --> mostra i dettagli dei comandi\n");
	printf("2) list --> mostra un elenco degli utenti connessi\n");
	printf("3) esc --> chiude il server\n");
	printf("_\n\n");
}

/*funzione che stampa a video la sezione d'aiuto*/
void print_help(){
	int i;
	for (i = 0; i < 20; i++)
		printf("*");
	printf(" HELP ");
	for (i = 0; i < 20; i++)
		printf("*");
	printf("\n");

	printf("list --> mostra l'elenco degli utenti connessi alla rete, indicando username*timestamp*porta\n");
	printf("esc --> termina il server. La terminazione non impedisce alle chat in corso di proseguire\n");
	printf("_\n\n");
}

int main(int argc, char* argv[]){

	/* se la porta non è specificata uso 4242 */
	if (argc != 2)
        portno = 4242;
    else
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

    /* messaggio di inizio */
	print_start_message();

	while(1){

		/* inizializzo il set read_fds, manipolato dalla select() */
		read_fds = master;
		
		/* mi blocco  in attesa di descrittori pronti in lettura */
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

		/* controllo se il socket per lo stdin è pronto */
        if(FD_ISSET(0, &read_fds)){ /* stdin */
    		
    		/* comando in input */
			fgets(ingresso, DIM_BUFFER, stdin);
			sscanf(ingresso, "%s", comando);
			
			if(strcmp(comando, "help") == 0){
				/* mostra una breve descrizione dei comandi */
				print_help();
			}
			else if(strcmp(comando, "list") == 0){
				
				/* mostra elenco utenti connessi */
				file = fopen("registro.bin", "rb");
				while(1){
					
					/* inizializzo struttura registro per usarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono alla fine del file */
						printf("Fine utenti connessi\n");
						break;				
					}
					if(strcmp(registro.timestamp_login, "") != 0 && strcmp(registro.timestamp_logout, "") == 0){
						/* utente online */
						/* stampo il timestamp senza il carattere '\n' */
						len = strlen(registro.timestamp_login);
						strcpy(timestamp_temp, registro.timestamp_login);
						timestamp_temp[--len] = '\0';
						
						printf("%s*%s*%i\n", registro.user_dest, timestamp_temp, registro.port);								
					}
				}
				fclose(file);
			}
			else if(strcmp(comando, "esc") == 0){

				/* chiude il server */
				printf("Chiusura del server\n");
				exit(0);
			}
			else{
				printf("Inserire un comando valido\n");
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

            printf("Richiesta ricevuta dal server\n");

            if(strcmp(comando, "signup") == 0){
            	
            	printf("Richiesto comando signup\n");
                
                /* registrazione utente */
                sscanf(buffer, "%s %s %s", comando, username, password);
                
                /* scrivo nel file relativo alle informazioni degli utenti registrati 
                controllando che non sia già presente */
                file = fopen("registrati.bin", "rb");
                while(1){
                    
                    /* inizializzo struttura user */
                    memset(&user, 0, sizeof(struct user));
                    ret = fread(&user, sizeof(struct user), 1, file);
                    if(ret == 0){
                        /* sono arrivato alla fine del file */
                        fclose(file);
                        break;                      
                    }
                    if(strcmp(user.username, username) == 0 && strcmp(user.password, password) == 0){
                        /* dati già presenti */
                        presente = 1;
                    }
                }
                if(presente == 0){
                
                    file = fopen("registrati.bin", "ab");
                    /* inserisco un nuovo record */
                    memset(&user, 0, sizeof(struct user));
                    strcpy(user.username, username);
                    strcpy(user.password, password);
                    fwrite(&user, sizeof(struct user), 1, file);
                    fclose(file);
                    
                    /* invio risposta all'utente */
                    memset(risposta, 0, sizeof(risposta));
                    strcpy(risposta, "Registrazione effettuata con successo\n");
                    len = strlen(risposta) + 1;
                    messageLen = htons(len);
                    ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                    ret = send(accept_socket, (void*) risposta, len, 0);
                    if(ret < 0){
                        perror("Errore in fase di invio: \n");
                        exit(1);                    
                    }

                    /* inserisco il valore nel registro */                      
                    file = fopen("registro.bin", "ab");
                    memset(&registro, 0, sizeof(struct registro));
                    strcpy(registro.user_dest, username);
                    registro.port = 0; 
                    strcpy(registro.timestamp_login, "");
                    strcpy(registro.timestamp_logout, "");
                    fwrite(&registro, sizeof(struct registro), 1, file);
                    fclose(file);
                
                    printf("Utente registrato\n");
                }
                else{   
                	
                	/* valore già registrato */
                    presente = 0;
                    memset(risposta, 0, sizeof(risposta));
                    strcpy(risposta, "Registrazione già effettuata\n");
                    len = strlen(risposta) + 1;
                    messageLen = htons(len);
                    ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                    ret = send(accept_socket, (void*) risposta, len, 0);
                    if(ret < 0){
                        perror("Errore in fase di invio: \n");
                        exit(-1);                   
                    }
                
                    printf("Utente già registrato\n");
                }
            }
			else if(strcmp(comando, "in") == 0){
				
				printf("Richiesto comando in\n");
			
				/* ottengo i parametri passati dall' utente */
				sscanf(buffer, "%s %i %s %s", comando, &srv_port, username, password);		

				/* ricevo il numero di porta dell'utente */
				ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
				ret = recv(accept_socket, (void*)buffer, len, 0);
				if(ret < 0){
					perror("Errore in fase di ricezione: \n");
					exit(1);				
				}
				sscanf(buffer, "%i", &dev_port);	
						
				/* controllo se l'utente che ha effettuato la richiesta è registrato */
				file = fopen("registrati.bin", "rb");
				while(1){
					
					/* inizializzo struttura user */
					memset(&user, 0, sizeof(struct user));						
					ret = fread(&user, sizeof(struct user), 1, file);
					if(ret == 0){
					
						/* sono arrivato alla fine del file, l'utente non è registrato */
						
						memset(risposta, 0, sizeof(risposta));							
						strcpy(risposta, "Utente non registrato\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(accept_socket, (void*) risposta, len, 0);
						if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);							
						}
						
						printf("Utente non registrato, impossibile login\n");
						break;
													
					}
					if(strcmp(user.username, username) == 0 && strcmp(user.password, password) == 0){
					
						/* scrivo nel registro del server */
						/* controllo se gia presente altrimenti inserisco il valore */						
						file2 = fopen("registro.bin", "rb");
						file3 = fopen("tempx.bin", "ab");
						while(1){
							memset(&registro, 0, sizeof(struct registro));
							ret = fread(&registro, sizeof(struct registro), 1, file2);
							if(ret == 0){
					
								/* sono alla fine del file */
								if (presente == 0){
									/* devo aggiungere un record */
									memset(&registro, 0, sizeof(struct registro));
									strcpy(registro.user_dest, username);
									registro.port = dev_port; 
									/* recupero l'ora corrente */
									time(&rawtime);
									/* converto l'ora */
									timeinfo = localtime(&rawtime);
									strcpy(registro.timestamp_login, asctime(timeinfo));
									strcpy(registro.timestamp_logout, "");
									fwrite(&registro, sizeof(struct registro), 1, file3);
								}
								presente = 0;
								break;
							}
							else if(strcmp(registro.user_dest, username) == 0){
						
								/* aggiorno il valore */
								presente = 1;
								memset(&registro, 0, sizeof(struct registro));
								strcpy(registro.user_dest, username);
								registro.port = dev_port; 
								/* recupero l'ora corrente */
								time(&rawtime);
								/* converto l'ora */
								timeinfo = localtime(&rawtime);
								strcpy(registro.timestamp_login, asctime(timeinfo));
								strcpy(registro.timestamp_logout, "");
								fwrite(&registro, sizeof(struct registro), 1, file3);
							}
							else{
						
								/* copio nel nuovo file */
								fwrite(&registro, sizeof(struct registro), 1, file3);
							}
						}
						/* sostituisco con il nuovo file */
						remove("registro.bin");
						rename("tempx.bin", "registro.bin");
						fclose(file2);
						fclose(file3);
						
						memset(risposta, 0, sizeof(risposta));		
						strcpy(risposta, "Login effettuato\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(accept_socket, (void*) risposta, len, 0);
						if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);						
						}
						printf("Login dell'utente effettuato con successo\n");
						break;
					}
				}
				fclose(file);


				if(strcmp(risposta, "Utente non registrato\n") == 0)
					continue;
				
				/* verifico se notifica di disconnessione */
				ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
				ret = recv(accept_socket, (void*)buffer, len, 0);
				if(ret < 0){
					perror("Errore in fase di ricezione: \n");
					exit(1);				
				}
				
				sscanf(buffer, "%s", comando);

				if(strcmp(comando, "DISCONNESSIONE") == 0){

					/* devo aggiornare il file registro */
					sscanf(buffer, "%s %s %s", comando, username_temp, timestamp_temp);
					
					file = fopen("registro.bin", "rb");
					file2 = fopen("tempx.bin", "ab");
					while(1){
				
						/* inizializzo struttura registro */
						memset(&registro, 0, sizeof(struct registro));						
						ret = fread(&registro, sizeof(struct registro), 1, file);
					
						if(ret == 0){
							/* fine del file */
							break;							
						}
				
						if(strcmp(registro.user_dest, username_temp) == 0){
							/* aggiorno valori, sovrascrivendo i dati risulterebbe offline*/
							//strcpy(registro.timestamp_login, "");
							//strcpy(registro.timestamp_logout, timestamp_temp);					
						}
						fwrite(&registro, sizeof(struct registro), 1, file2);
					}
					/* sostituisco con il file aggiornato */
					remove("registro.bin");
					rename("tempx.bin", "registro.bin");
					fclose(file);
					fclose(file2);		
					printf("Aggiornato file del registro con il timestamp perso\n");
				}	
			}
			else if(strcmp(comando, "hanging") == 0){
				
				/* invio all' utente il  contenuto del file msgpendenti con le informazioni */

				sscanf(buffer, "%s %s", comando, username);
				memset(nomeFile, 0, sizeof(nomeFile));
				strcpy(nomeFile, username);
				strcat(nomeFile, "_msgpendenti.bin");
				file = fopen(nomeFile, "rb");
				if(file == NULL){
					
					/* apro in scrittura per crearlo */
					file = fopen(nomeFile, "wb");
					fclose(file);
					file = fopen(nomeFile, "rb");
				}
				/* leggo i valori presenti nel file */						
				while(1){
				
					/* inizializzo la struttura msg_pendenti per utilizzarla come appoggio */
					memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
					ret = fread(&msg_pendenti, sizeof(struct msg_pendenti), 1, file);
					if(ret == 0){
				
						/* sono arrivato alla fine del file */
						break;
					}
					
					memset(risposta, 0, sizeof(risposta));
					strcpy(risposta, msg_pendenti.mittente);
					strcat(risposta, "	");
					sprintf(buffertemp, "%i", msg_pendenti.num_msg);
					strcat(risposta, buffertemp);
					strcat(risposta, "	");
					strcat(risposta, msg_pendenti.timestamp);
					strcat(risposta, "\n");
					len = strlen(risposta) + 1;
					messageLen = htons(len);
					ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
					ret = send(accept_socket, (void*) risposta, len, 0);
					if(ret < 0){
						perror("Errore in fase di invio: \n");
						exit(1);							
					}
				}
				fclose(file);
				
				/* segnalo fine delle informazioni relative ai messaggi pendenti */
				memset(risposta, 0, sizeof(risposta));
				strcpy(risposta, "Fine messaggi pendenti\n");
				len = strlen(risposta) + 1;
				messageLen = htons(len);
				ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
				ret = send(accept_socket, (void*) risposta, len, 0);
				if(ret < 0){
					perror("Errore in fase di invio: \n");
					exit(1);							
				}
				
				printf("Inviate all'utente le informazioni relative ai messaggi pendenti\n");
			}
            else if(strcmp(comando, "show") == 0){
            	
            	/* ottengo i parametri passati dall' utente */
                sscanf(buffer, "%s %s %s", comando, username, username_temp);

                /* cerco in username_msgpendenti.bin se ho messaggi pendenti relativi all'username richiesto e nel caso aggiorno il file */
                memset(nomeFile, 0, sizeof(nomeFile));
                strcpy(nomeFile, username);
                strcat(nomeFile, "_msgpendenti.bin");
                file = fopen(nomeFile, "rb");
                if(file == NULL){
                    
                    /* apro in scrittura per crearlo */
                    file = fopen(nomeFile, "wb");
                    fclose(file);
                    file = fopen(nomeFile, "rb");
                }
                file2 = fopen("tempx.bin", "ab");

                /* leggo i valori presenti nel file */                      
                while(1){
                
                    /* inizializzo la struttura msg_pendenti per utilizzarla come appoggio */
                    memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
                    ret = fread(&msg_pendenti, sizeof(struct msg_pendenti), 1, file);
                    if(ret == 0){
                        /* fine del file */
                        break;                          
                    }
                
                    if(strcmp(msg_pendenti.mittente, username_temp) == 0){
                        /* ho messaggi pendenti da parte dello username richiesto */
                        presente = 1;
                    }
                    else{
                        /* scrivo i valori nel file di appoggio che diventerà il nuovo file */
                        fwrite(&msg_pendenti, sizeof(struct msg_pendenti), 1, file2);
                    }
                            
                }
                if (presente == 1){
                
                    /* aggiorno il file con i dati relativi ai messaggi pendenti */
                    remove(nomeFile);
                    rename("tempx.bin", nomeFile);
                    fclose(file);
                    fclose(file2);
                
                    presente = 0;
                
                    printf("Sono presenti messaggi pendenti\n");
                    
                    memset(nomeFile, 0, sizeof(nomeFile));
                    strcpy(nomeFile, username_temp);
                    strcat(nomeFile, username);                 
                    strcat(nomeFile, "_msgpendenti.bin");
                    file = fopen(nomeFile, "rb");
                    if(file == NULL){
                    
                        /* apro in scrittura per crearlo */
                        file = fopen(nomeFile, "wb");
                        fclose(file);
                        file = fopen(nomeFile, "rb");
                    }
                    file2 = fopen("tempx.bin", "ab");
                    while(1){
                
                        /* inizializzo il buffer che userò come appoggio */
                        memset(buffer, 0, sizeof(buffer));
                        
                        ret = fread(buffer, sizeof(buffer), 1, file);
                        if(ret == 0){
                
                            /* fine del file, effettuo la sostituzione */
                            remove(nomeFile);
                            rename("tempx.bin", nomeFile);
                            fclose(file);
                            fclose(file2);
                            
                            memset(risposta, 0, sizeof(risposta));
                            strcpy(risposta, "Fine dei messaggi pendenti\n");
                            len = strlen(risposta) + 1;
                            messageLen = htons(len);
                            ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                            ret = send(accept_socket, (void*) risposta, len, 0);
                            if(ret < 0){
                                perror("Errore in fase di invio: \n");
                                exit(1);                        
                            }
                            
                            /* se user.username è online notifico l'avvenuta ricezione dei messaggi altrimenti verrà notificato durante il prossimo accesso */
                            /* verifico se online dal file registro.bin */
                            file3 = fopen("registro.bin", "rb");
                            file4 = fopen("tempz.bin", "ab");
                            while(1){                                   
                            
                                /* inizializzo la struttura registro per utilizzarla come appoggio */
                                memset(&registro, 0, sizeof(struct registro));
                                ret = fread(&registro, sizeof(struct registro), 1, file3);
                                if(ret == 0){
                                	break;
                                }
                                if(strcmp(registro.user_dest, username_temp) == 0 && strcmp(registro.timestamp_logout, "") == 0){

                                	/* creo socket per comunicare con l'utente */
                                    tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                                    if (tcp_connect < 0){
                                        perror("Impossibile creare il socket TCP: \n");
                                        exit(1);
                                    }

                                    memset(&bind_addr, 0, sizeof(bind_addr));
                                    bind_addr.sin_port = htons(portno+1);
                                    bind_addr.sin_family = AF_INET;
                                    inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                                    ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                                    if(ret < 0){
                                        perror("Errore in fase di bind: \n");
                                        exit(1);
                                    }

                                    memset(&connect_addr, 0, sizeof(connect_addr));
                                    connect_addr.sin_port = htons(registro.port);
                                    connect_addr.sin_family = AF_INET;
                                    inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                                    ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                                    if(ret < 0){
                                        
                                        /* utente non online, aggiungo messaggio al file username_notifiche.bin */
                                        memset(buffer, 0, sizeof(buffer));
                                        strcpy(buffer, username);
                                        memset(nomeFile, 0, sizeof(nomeFile));
                                        strcpy(nomeFile, username_temp);
                                        strcat(nomeFile, "_notifiche.bin");
                                        file = fopen(nomeFile, "ab");
                                    
                                        fwrite(buffer, sizeof(buffer), 1, file);
                                    
                                        fclose(file);

                                        /* aggiorno il valore di logout */
                                        strcpy(registro.timestamp_login, "");
                                        /* recupero l'ora corrente */
                                        time(&rawtime);
                                        /* converto l'ora */
                                        timeinfo = localtime(&rawtime);
                                        strcpy(registro.timestamp_logout, asctime(timeinfo));
                                        
                                        /* scrivo nel file aggiornato */
                                        fwrite(&registro, sizeof(struct registro), 1, file4);               
                                    
                                        printf("Utente %s non connesso, aggiornato l'istante di uscita\n", username_temp);
                                    
                                        continue;
                                    }
                                    else{
                                        fwrite(&registro, sizeof(struct registro), 1, file4);
                                    }

                                    /* utente online, invio la notifica */
                                    memset(risposta, 0, sizeof(risposta));
                                    strcpy(risposta, "RIC\n");
                                    len = strlen(risposta) + 1;
                                    messageLen = htons(len);
                                    ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
                                    ret = send(tcp_connect, (void*) risposta, len, 0);
                                    if(ret < 0){
                                        perror("Errore in fase di invio: \n");
                                        exit(1);                        
                                    }
                                    
                                    /* invio lo username di chi ha ricevuto il messaggio */
                                    memset(risposta, 0, sizeof(risposta));
                                    strcpy(risposta, username);
                                    len = strlen(risposta) + 1;
                                    messageLen = htons(len);
                                    ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
                                    ret = send(tcp_connect, (void*) risposta, len, 0);
                                    if(ret < 0){
                                        perror("Errore in fase di invio: \n");
                                        exit(1);                        
                                    }

                                    close(tcp_connect);
                                }
                                else if(strcmp(registro.user_dest, username_temp) == 0 && strcmp(registro.timestamp_logout, "") != 0){
                                
                                    /* utente non online, aggiungo messaggio al file username_notifiche.bin */
                                    memset(buffer, 0, sizeof(buffer));
                                    strcpy(buffer, username);
                                    memset(nomeFile, 0, sizeof(nomeFile));
                                    strcpy(nomeFile, username_temp);
                                    strcat(nomeFile, "_notifiche.bin");
                                    file = fopen(nomeFile, "ab");
                                    
                                    fwrite(buffer, sizeof(buffer), 1, file);
                                    
                                    fclose(file);
                                    
                                    fwrite(&registro, sizeof(struct registro), 1, file4);                                     
                                }
                                else{
                                    fwrite(&registro, sizeof(struct registro), 1, file4);
                                }
                            }
                            /* sostituisco con il file aggiornato */
                            remove("registro.bin");
                            rename("tempz.bin", "registro.bin");
                            fclose(file3);
                            fclose(file4);
                            break;                          
                        }

                        /* invio il messaggio pendente */
                        memset(risposta, 0, sizeof(risposta));
                        strcpy(risposta, buffer);
                        len = strlen(risposta) + 1;
                        messageLen = htons(len);
                        ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                        ret = send(accept_socket, (void*) risposta, len, 0);
                        if(ret < 0){
                            perror("Errore in fase di invio: \n");
                            exit(1);                        
                        }
                    }           
                }
                else{
                    /* non sono presenti messaggi pendenti dall'utente richiesto */
                    /* non necessario aggiornare il file */
                    remove("tempx.bin");
                    fclose(file);
                    fclose(file2);
                
                    printf("Non ci sono messaggi pendenti per l'utente\n");
                    
                    memset(risposta, 0, sizeof(risposta));
                    strcpy(risposta, "Non ci sono messaggi pendenti dall'utente richiesto\n");
                    len = strlen(risposta) + 1;
                    messageLen = htons(len);
                    ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
                    ret = send(accept_socket, (void*) risposta, len, 0);
                    if(ret < 0){
                        perror("Errore in fase di invio: \n");
                        exit(1);                        
                    }
                }
            }
			else if(strcmp(comando, "chat") == 0){
				
				/* copio valore username del destinatario */
				
				/* ottengo lo username di chi ha richiesto il comando */
				ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
				ret = recv(accept_socket, (void*)buffer, len, 0);
				if(ret < 0){
					printf("Errore in fase di ricezione: \n");
					exit(1);
				}
				sscanf(buffer, "%s", username);

				/* ottengo lo username del destinatario */
				ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
				ret = recv(accept_socket, (void*)buffer, len, 0);
				if(ret < 0){
					printf("Errore in fase di ricezione: \n");
					exit(1);
				}
				sscanf(buffer, "%s", username_temp);

				/* controllo se username ha notifiche di lettura messaggi */
				memset(nomeFile, 0, sizeof(nomeFile));
				strcpy(nomeFile, username);
				strcat(nomeFile, "_notifiche.bin");
				file = fopen(nomeFile, "rb");
				if(file == NULL){
					
					/* apro in scrittura per crearlo */
					file = fopen(nomeFile, "wb");
					fclose(file);
					file = fopen(nomeFile, "rb");
				}
				file2 = fopen("tempx.bin", "ab");
				while(1){
				
					/* leggo dal file tutte le notifiche presenti */
					memset(buffer, 0, sizeof(buffer));
					ret = fread(buffer, sizeof(buffer), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
				
					if(strcmp(buffer, username_temp) == 0){
						
						/* questo vuol dire che ha letto i messaggi */
						/* invio all'utente ACK di lettura */
						memset(risposta, 0, sizeof(risposta));
						strcpy(risposta, "RIC\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(accept_socket, (void*) risposta, len, 0);
						if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);							
						}
					
						presente = 1;
					}
					else{
						
						/* copio gli altri valori nel file aggiornato */
						fwrite(buffer, sizeof(buffer), 1, file2);
					}
				}
				/* sostituisco i file */
				remove(nomeFile);
				rename("tempx.bin", nomeFile);
				fclose(file);
				fclose(file2);

				if(presente == 0){
					
					memset(risposta, 0, sizeof(risposta));
					strcpy(risposta, "Nessuna notifica\n");
					len = strlen(risposta) + 1;
					messageLen = htons(len);
					ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
					ret = send(accept_socket, (void*) risposta, len, 0);
					if(ret < 0){
						perror("Errore in fase di invio: \n");
						exit(1);							
					}
				}
				else{
					presente = 0;
				}
							
				/* inizio chat */
				printf("Richiesto comando chat\n");
			}
			else if(strcmp(comando, "/u") == 0){
				
				sscanf(buffer, "%s %s", comando, username);
                		
           		/* chat di gruppo */
				/* mostro una lista degli utenti online, file registro.bin*/
				file = fopen("registro.bin", "rb");
				while(1){
					
					/* inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
					
					if(strcmp(registro.timestamp_login, "") != 0 && strcmp(registro.timestamp_logout, "") == 0 && strcmp(registro.user_dest, username) != 0){
					
						/* utente online */	
						memset(risposta, 0, sizeof(risposta));							
						strcpy(risposta, registro.user_dest);
						strcat(risposta, "\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(accept_socket, (void*) risposta, len, 0);
						if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);						
						}
					}
				}
				fclose(file);
						
				/* notifico all'utente che la lista è finita */
				memset(risposta, 0, sizeof(risposta));
				strcpy(risposta, "Fine lista\n");
				len = strlen(risposta) + 1;
				messageLen = htons(len);
				ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
				ret = send(accept_socket, (void*) risposta, len, 0);
				if(ret < 0){
					perror("Errore in fase di invio: \n");
					exit(1);						
				}
			}
			else if(strcmp(comando, "/a") == 0){
				
				sscanf(buffer, "%s %s %s", comando, username, username_temp);
						
				/* controllo se username valido */
				file = fopen("registro.bin", "rb");
				while(1){
					
					/* scorro tutto il file per vedere se lo username selezionato è online */
					/* inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
					
					if(strcmp(registro.user_dest, username_temp) == 0 && strcmp(registro.timestamp_login, "") != 0 && strcmp(registro.timestamp_logout, "") == 0){
						/* scelta valida */									
						presente = 1;
						break;						
					}
				}
				fclose(file);
						
				if(presente == 0){
						
					/* utente richiesto non è online */
					printf("Richiesto un utente non valido\n");
					
					memset(risposta, 0, sizeof(risposta));	
					strcpy(risposta, "Utente non valido\n");
					len = strlen(risposta) + 1;
					messageLen = htons(len);
					ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
					ret = send(accept_socket, (void*) risposta, len, 0);
					if(ret < 0){
						perror("Errore in fase di invio: \n");
						exit(1);						
					}	
					continue;						
				}
					
				presente = 0;
				
				printf("Richiesto un utente valido\n");
				
				memset(risposta, 0, sizeof(risposta));		
				strcpy(risposta, "Utente valido\n");
				len = strlen(risposta) + 1;
				messageLen = htons(len);
				ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
				ret = send(accept_socket, (void*) risposta, len, 0);
				if(ret < 0){
					perror("Errore in fase di invio: \n");
					exit(1);						
				}	
						
				/* invio il numero di porta dell'utente richiesto */
				file = fopen("registro.bin", "rb");
				while(1){
						
					/* scorro tutto il file per la porta dello username selezionato */
					/* inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
						
					if(strcmp(registro.user_dest, username_temp) == 0){
						/* porta dell'utente selezionato */
						sprintf(risposta, "%i", registro.port);								
						break;						
					}
				}
				fclose(file);
				
				/* invio la risposta */
				strcat(risposta, "\n");
				len = strlen(risposta) + 1;
				messageLen = htons(len);
				ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
				ret = send(accept_socket, (void*) risposta, len, 0);
				if(ret < 0){
					perror("Errore in fase di invio: \n");
					exit(1);						
				}
			}
			else if(strcmp(comando, "out") == 0){
				
				sscanf(buffer, "%s %s", comando, username);

				/* memorizzo nel registro il logout dell'utente */			
				file = fopen("registro.bin", "rb");
				file2 = fopen("tempx.bin", "ab");
				while(1){
				
					/*inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
				
					if(strcmp(registro.user_dest, username) == 0){
				
						/* aggiorno timestamp */
						strcpy(registro.timestamp_login, "");
						/* recupero l'ora corrente */
						time(&rawtime);
						/* converto l'ora */
						timeinfo = localtime(&rawtime);
						strcpy(registro.timestamp_logout, asctime(timeinfo));
					}	
				
					/* scrivo nel file aggiornato */
					fwrite(&registro, sizeof(struct registro), 1, file2);				
				}
				/* sostituisco i file */
				remove("registro.bin");
				rename("tempx.bin", "registro.bin");
				fclose(file);
				fclose(file2);
				
				printf("Aggiornato l'istante di uscita dell' utente\n");
				
				/* aggiorno gli altri utenti dell'uscita dell'utente */
				file = fopen("registro.bin", "rb");
				file2 = fopen("tempx.bin", "ab");
				while(1){
				
					/*inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
				
					if(strcmp(registro.user_dest, username) != 0 && strcmp(registro.timestamp_logout, "") == 0 && strcmp(registro.timestamp_login, "") != 0){
						
						/* utente online */
						/* creo un socket per comunicare con l'utente */
						tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                		if (tcp_connect < 0){
                    		perror("Impossibile creare il socket TCP: \n");
                    		exit(1);
                		}

                		memset(&bind_addr, 0, sizeof(bind_addr));
                		bind_addr.sin_port = htons(portno+1);
                		bind_addr.sin_family = AF_INET;
                		inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                		ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                		if(ret < 0){
                    		perror("Errore in fase di bind: \n");
                    		exit(1);
                		}

                		memset(&connect_addr, 0, sizeof(connect_addr));
                		connect_addr.sin_port = htons(registro.port);
                		connect_addr.sin_family = AF_INET;
                		inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                		ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                		if(ret < 0){
                 			
                 			/* utente che doveva essere online non è online */
							/* aggiorno il timestamp in registro.bin*/
							
							/* recupero l'ora corrente */
							time(&rawtime);
							/* converto l'ora */
							timeinfo = localtime(&rawtime);
							strcpy(registro.timestamp_logout, asctime(timeinfo));
							strcpy(registro.timestamp_login, "");

							printf("Utente %s non connesso, aggiornato l'istante di uscita\n", registro.user_dest);

							continue;
                		}

						/* invio ACK di logout */
						memset(risposta, 0, sizeof(risposta));
						strcpy(risposta, "LOGOUT\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(tcp_connect, (void*) risposta, len, 0);
						if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);						
						}

						/* invio lo username dell'utente che si è disconnesso */
						memset(risposta, 0, sizeof(risposta));
						strcpy(risposta, username);
						strcat(risposta, "\n");
						len = strlen(risposta) + 1;
						messageLen = htons(len);
						ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
						ret = send(tcp_connect, (void*) risposta, len, 0);
							if(ret < 0){
							perror("Errore in fase di invio: \n");
							exit(1);
						}

						/* aspetto ACK di ricezione della notifica di logout */
						ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
						len = ntohs(messageLen);
						ret = recv(tcp_connect, (void*)buffer, len, 0);
						if(ret < 0){
							printf("Errore in fase di ricezione: \n");
							exit(1);
						}
						
						close(tcp_connect);
					}

					/* scrivo nel file aggiornato */
					fwrite(&registro, sizeof(struct registro), 1, file2);						
				}
				/* sostituisco i file */
				remove("registro.bin");
				rename("tempx.bin", "registro.bin");
				fclose(file);
				fclose(file2);

				/* invio ACK di ricevuto OUT all'utente */
				memset(risposta, 0, sizeof(risposta));
				strcpy(risposta, "OUTOK\n");
				len = strlen(risposta) + 1;
				messageLen = htons(len);
				ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
				ret = send(accept_socket, (void*) risposta, len, 0);
				if(ret < 0){
					perror("Errore in fase di invio: \n");
					exit(1);						
				}
			}
			else{
				
				/* richiesto invio messaggio */
				/* salvo il messaggio su un buffer di appoggio */
				memset(buffertemp, 0, sizeof(buffertemp));
				strcpy(buffertemp, buffer);

				/* ottengo mittente e destinatario */
				ret = recv(accept_socket, (void*)&messageLen, sizeof(uint16_t), 0);
				len = ntohs(messageLen);
				ret = recv(accept_socket, (void*)buffer, len, 0);
				if(ret < 0){
					perror("Errore in fase di ricezione: \n");
					exit(1);				
				}
				
				sscanf(buffer, "%s %s", username, username_temp);
				
				file = fopen("registro.bin", "rb");
				file2 = fopen("tempx.bin", "ab");
				while(1){
					
					/*inizializzo la struttura registro per utilizzarla come appoggio */
					memset(&registro, 0, sizeof(struct registro));
					ret = fread(&registro, sizeof(struct registro), 1, file);
					if(ret == 0){
						/* sono arrivato alla fine del file */
						break;
					}
					
					if(strcmp(registro.user_dest, username_temp) == 0){
						
						/* controllo se online */
						if(strcmp(registro.timestamp_login, "") != 0 && strcmp(registro.timestamp_logout, "") == 0){
							
							/* destinatario online */
							printf("Il destinatario del messaggio è online\n");
							
							/* ottengo il numero di porta del device del destinatario */
							porta_temp = registro.port;
							
							/* creo un socket per comunicare con l'utente */
							tcp_connect = socket(AF_INET, SOCK_STREAM, 0);
                			if (tcp_connect < 0){
                				perror("Impossibile creare il socket TCP: \n");
                				exit(1);
                			}

                			memset(&bind_addr, 0, sizeof(bind_addr));
                			bind_addr.sin_port = htons(portno+1);
                			bind_addr.sin_family = AF_INET;
                			inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);

                			ret = bind(tcp_connect, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
                			if(ret < 0){
                				perror("Errore in fase di bind: \n");
                				exit(1);
                			}

                			memset(&connect_addr, 0, sizeof(connect_addr));
                			connect_addr.sin_port = htons(registro.port);
                			connect_addr.sin_family = AF_INET;
                			inet_pton(AF_INET, "127.0.0.1", &connect_addr.sin_addr);

                			ret = connect(tcp_connect, (struct sockaddr*)&connect_addr, sizeof(connect_addr));
                			if(ret < 0){
                				
                				/* utente non connesso, aggiorno timestamp_logout */
								/* recupero l'ora corrente */
								time(&rawtime);
								/* converto l'ora */
								timeinfo = localtime(&rawtime);
								strcpy(registro.timestamp_logout, asctime(timeinfo));
								strcpy(registro.timestamp_login, "");
								
								printf("Utente %s non connesso, aggiornato l'istante di uscita\n", registro.user_dest);

								/* destinatario non ha ricevuto i messaggi le salvo nel server e invio ACK di salvataggio */
								memset(nomeFile, 0, sizeof(nomeFile));
								strcpy(nomeFile, username_temp);
								strcat(nomeFile, "_msgpendenti.bin");
								file3 = fopen(nomeFile, "rb");
								if(file3 == NULL){
					
									/* apro in scrittura per crearlo */
									file3 = fopen(nomeFile, "wb");
									fclose(file2);
									file3 = fopen(nomeFile, "rb");
								}
								file4 = fopen("tempy.bin", "ab");
								while(1){
									
									/*inizializzo la struttura msg_pedenti per utilizzarla come appoggio */
									memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
									ret = fread(&msg_pendenti, sizeof(struct msg_pendenti), 1, file3);
									if(ret == 0){
										/* sono arrivato alla fine del file */
										break;
									}
										
									if(strcmp(msg_pendenti.mittente, username) == 0){
										/* ci sono già messaggi pendenti da parte del mittente */
										msg_pendenti.num_msg++;
										/* recupero l'ora corrente */
										time(&rawtime);
										/* converto l'ora */
										timeinfo = localtime(&rawtime);
										strcpy(msg_pendenti.timestamp, asctime(timeinfo));
										presente = 1; 
									}
										
									fwrite(&msg_pendenti, sizeof(struct msg_pendenti), 1, file4);
								}
										
								if(presente == 0){
								
									/* aggiungo il record al file */
									memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
									strcpy(msg_pendenti.mittente,username);
									msg_pendenti.num_msg = 1;
									/* recupero l'ora corrente */
									time(&rawtime);
									/* converto l'ora */
									timeinfo = localtime(&rawtime);
									strcpy(msg_pendenti.timestamp, asctime(timeinfo));
									fwrite(&msg_pendenti, sizeof(struct msg_pendenti), 1, file4);							
								}
									
								presente = 0;
									
								/* sostituisco con il file aggiornato */
								remove(nomeFile);
								rename("tempy.bin", nomeFile);
								fclose(file3);
								fclose(file4);
								
								/* memorizzo il corpo del messaggio in mittdest_msgpendenti.bin */
								/* la show recupera il testo dei messaggi da questo file */
								memset(nomeFile, 0, sizeof(nomeFile));
								strcpy(nomeFile, username);
								strcat(nomeFile, username_temp);
								strcat(nomeFile, "_msgpendenti.bin");
								file3 = fopen(nomeFile, "ab");
								
								fwrite(buffertemp, sizeof(buffertemp), 1, file3);
								fclose(file3);
								
								/* invio ACK di memorizzazione */
								memset(risposta, 0, sizeof(risposta));
								strcpy(risposta, "MEM\n");
								len = strlen(risposta) + 1;
								messageLen = htons(len);
								ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
								ret = send(accept_socket, (void*) risposta, len, 0);
								if(ret < 0){
									perror("Errore in fase di invio: \n");
									exit(1);						
								}
									
								printf("Ho inviato correttamente l' ACK di memorizzazione al destinatario\n");

								continue;
                			}

							/* invio messaggio al destinatario: mittente msg */
							/* invio ACK */
							memset(risposta, 0, sizeof(risposta));
							strcpy(risposta, "MSG\n");
							len = strlen(risposta) + 1;
							messageLen = htons(len);
							ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
							ret = send(tcp_connect, (void*) risposta, len, 0);
							if(ret < 0){
								perror("Errore in fase di invio: \n");
								exit(1);						
							}
									
							/* invio username mittente */
							memset(risposta, 0, sizeof(risposta));
							strcpy(risposta, username);
							len = strlen(risposta) + 1;
							messageLen = htons(len);
							ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
							ret = send(tcp_connect, (void*) risposta, len, 0);
							if(ret < 0){
								perror("Errore in fase di invio: \n");
								exit(1);						
							}
									
							/* invio messaggio */
							memset(risposta, 0, sizeof(risposta));
							strcpy(risposta, buffertemp);
							len = strlen(risposta) + 1;
							messageLen = htons(len);
							ret = send(tcp_connect, (void*) &messageLen, sizeof(uint16_t), 0);
							ret = send(tcp_connect, (void*) risposta, len, 0);
							if(ret < 0){
								perror("Errore in fase di invio: \n");
								exit(1);						
							}
									
							printf("Inviato il messaggio al destinatario\n");
						
							/* attendo ACK di conferma da parte del destinatario */
							ret = recv(tcp_connect, (void*)&messageLen, sizeof(uint16_t), 0);
                			len = ntohs(messageLen);
                			ret = recv(tcp_connect, (void*)buffer, len, 0);
                			if(ret < 0){
                   				printf("Errore in fase di ricezione: \n");
                   				exit(1);        
                  			}

                    		close(tcp_connect);

							if(strcmp(buffer, "MSGOK\n") == 0){
								
								printf("Ho inviato correttamente il messaggio al destinatario\n");										
									
								/* invio ack con porta destinatario */
								memset(risposta, 0, sizeof(risposta));
								strcpy(risposta, "RIC\n");
								len = strlen(risposta) + 1;
								messageLen = htons(len);
								ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
								ret = send(accept_socket, (void*) risposta, len, 0);
								if(ret < 0){
									perror("Errore in fase di invio: \n");
									exit(1);						
								}
										
								sprintf(risposta, "%i", porta_temp);
								strcat(risposta, "\n");
								len = strlen(risposta) + 1;
								messageLen = htons(len);
								ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
								ret = send(accept_socket, (void*) risposta, len, 0);
								if(ret < 0){
									perror("Errore in fase di invio: \n");
									exit(1);						
								}
										
								printf("Ho inviato correttamente l' ACK di ricezione al destinatario\n");	
							}
						}
						else{
							
							/* aggiorno timestamp_logout */
							/* recupero l'ora corrente */
							time(&rawtime);
							/* converto l'ora */
							timeinfo = localtime(&rawtime);
							strcpy(registro.timestamp_logout, asctime(timeinfo));
							strcpy(registro.timestamp_login, "");
							
							/* destinatario offline */
							printf("Il destinatario del messaggio è offline\n");
									
							/* devo memorizzare il messaggio e inviare l'ACK al mittente */
							memset(nomeFile, 0, sizeof(nomeFile));
							strcpy(nomeFile, username_temp);
							strcat(nomeFile, "_msgpendenti.bin");
							file3 = fopen(nomeFile, "rb");
							if(file3 == NULL){
					
								/* apro in scrittura per crearlo */
								file3 = fopen(nomeFile, "wb");
								fclose(file3);
								file3 = fopen(nomeFile, "rb");
							}
							file4 = fopen("tempy.bin", "ab");
							while(1){
							
								/*inizializzo la struttura msg_pedenti per utilizzarla come appoggio */
								memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
								ret = fread(&msg_pendenti, sizeof(struct msg_pendenti), 1, file3);
								if(ret == 0){
									/* sono arrivato alla fine del file */
									break;
								}
									
								if(strcmp(msg_pendenti.mittente, username) == 0){
									
									/* ci sono già messaggi pendenti da parte del mittente */
									msg_pendenti.num_msg++;
									/* recupero l'ora corrente */
									time(&rawtime);
									/* converto l'ora */
									timeinfo = localtime(&rawtime);
									strcpy(msg_pendenti.timestamp, asctime(timeinfo));
									presente = 1; 
								}
									
								fwrite(&msg_pendenti, sizeof(struct msg_pendenti), 1, file4);
							}
									
							if(presente == 0){
							
								/* aggiungo il record al file */
								memset(&msg_pendenti, 0, sizeof(struct msg_pendenti));
								strcpy(msg_pendenti.mittente, username);
								msg_pendenti.num_msg = 1;
								/* recupero l'ora corrente */
								time(&rawtime);
								/* converto l'ora */
								timeinfo = localtime(&rawtime);
								strcpy(msg_pendenti.timestamp, asctime(timeinfo));
								fwrite(&msg_pendenti, sizeof(struct msg_pendenti), 1, file4);							
							}
									
							presente = 0;
							/* sostituisco con il file aggiornato */
							remove(nomeFile);
							rename("tempy.bin", nomeFile);
							fclose(file3);
							fclose(file4);
											
							/* memorizzo il corpo del messaggio in mittdest_msgpendenti.bin */
							/* la show recupera il test dei messaggi da questo file */
							memset(nomeFile, 0, sizeof(nomeFile));
							strcpy(nomeFile, username);
							strcat(nomeFile, username_temp);
							strcat(nomeFile, "_msgpendenti.bin");
							file3 = fopen(nomeFile, "ab");
							
							memset(buffer, 0, sizeof(buffer));
							strcpy(buffer, buffertemp);
                            strcat(buffer, "\n");		
							fwrite(buffer, sizeof(buffer), 1, file3);
							
							fclose(file3);
												
							/* invio ACK di memorizzazione */
							memset(risposta, 0, sizeof(risposta));
							strcpy(risposta, "MEM\n");
							len = strlen(risposta) + 1;
							messageLen = htons(len);
							ret = send(accept_socket, (void*) &messageLen, sizeof(uint16_t), 0);
							ret = send(accept_socket, (void*) risposta, len, 0);
							if(ret < 0){
								perror("Errore in fase di invio: \n");
								exit(1);						
							}
									
							printf("Ho inviato correttamente l' ACK di memorizzazione al destinatario\n");	

						}
					}
							
					fwrite(&registro, sizeof(struct registro), 1, file2);	
				}
				/* sostituisco con il file aggiornato */
				remove("registro.bin");
				rename("tempx.bin", "registro.bin");
				fclose(file);							
				fclose(file2);	
			}
			
			close(accept_socket);

			/* rimuovo il descrittore da quelli da monitorare */
			FD_CLR(tcp_listener, &read_fds);
    	}
	}
}

