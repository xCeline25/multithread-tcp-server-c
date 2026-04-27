					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list/list.h"
#include "user.h"
#include "buffer/buffer.h"
#include "utils.h"

#define PORT_FREESCORD 4321

/** Gérer toutes les communications avec le client renseigné dans
 * user, qui doit être l'adresse d'une struct user */
void *handle_client(void *user);
/** Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur */
int create_listening_sock(uint16_t port);

/* La repetition du message aux clients */
void *repeter(void *arg);
int nickname_exists(const char *nick);
void remove_user_from_list(struct user *u);

int tube[2];//0 lecture //1 ecriture
struct list *users;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;//pour proteger la liste des clients

//le buf actuel lit des block on va utiliser buffer pour les lignes

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	int listen_sock=create_listening_sock(PORT_FREESCORD);
	if (listen_sock <0) exit(1);
	signal(SIGPIPE, SIG_IGN);
	
	char message_bienvenue[] =
		"Bienvenue sur freescord\r\n"
		"Entrez votre pseudo avec: nickname <pseudo>\r\n"
		"Pseudo max 16 caracteres, sans ':'\r\n"
		"\r\n";

	if(pipe(tube)<0){
		perror("pipe");
		exit(1);
	}

	users=list_create();//les client seront ici 
	pthread_t t;
	pthread_create(&t,NULL,repeter,NULL);
	pthread_detach(t);

	while(1){
		//accepter les demandes de connexion 
		struct user *u= user_accept(listen_sock);
		if (u==NULL) continue;
		write(u->sock,message_bienvenue,strlen(message_bienvenue));
		
				

		//lire les octets envoye par le client et lui renvoyer 
		//jusqu,a ce que le client ait ferme sa socket 
		pthread_t tid;
		pthread_create(&tid,NULL,handle_client,u);
		pthread_detach(tid);

		
		
	}
	close(listen_sock);
	return 0;
}

void *handle_client(void *clt)
{ 
	char buf[1024];
	char msg[1200];
	
	struct user *u =(struct user*) clt;
	if (u==NULL) return NULL;
	buffer *b = buff_create(u->sock, 1024);
	if (b == NULL) {
		user_free(u);
		return NULL;
	}
	int valide=0;

			while(!valide){ //lire ce que le client envoie socket reseau 
				if (buff_fgets_crlf(b, buf, sizeof(buf)) == NULL) {
					buff_free(b);
					user_free(u);
					return NULL;
				}
		
				//3 si la commande ne commence pas par "nickname ".
				if (strncmp(buf, "nickname ", 9) != 0){
					write(u->sock,"3 commande invalide\r\n", strlen("3 commande invalide\r\n"));
					continue;
				}
				char *nick = buf + 9;
				size_t len = strlen(nick);
				//pour enlever \r\n a la fin du nickname
				while (len > 0 && (nick[len-1] == '\n' || nick[len-1] == '\r')) {
					nick[len-1] = '\0';
					len--;
				}
				//2 si le nickname est interdit (par exemple car il est trop long, ou contient :, ou est grossier, en
				//fonction de la politique du serveur)
				if (len == 0 || len > 16 || strchr(nick, ':') != NULL) {
					write(u->sock, "2 nickname interdit\r\n", strlen("2 nickname interdit\r\n"));
					continue;
				}

				pthread_mutex_lock(&lock);
				int exists = nickname_exists(nick);
				pthread_mutex_unlock(&lock);

				if (exists) {
					write(u->sock, "1 nickname deja pris\r\n", strlen("1 nickname deja pris\r\n"));
					continue;
				} else {
					//0 si le nickname est valide, c’est-à-dire autorisé par le serveur
					valide = 1;

					strncpy(u->nickname, nick, 17);
					u->nickname[16] = '\0'; // s'assurer que la chaîne est bien terminée

					pthread_mutex_lock(&lock);
					list_add(users, u);
					pthread_mutex_unlock(&lock);
					
					write(u->sock, "0 nickname valide\r\n", strlen("0 nickname valide\r\n"));
				}
			}
			
			while (buff_fgets_crlf(b, buf, sizeof(buf)) != NULL){
				snprintf(msg, sizeof(msg), "%s: %s", u->nickname, buf);
				write(tube[1],msg,strlen(msg));
			}

	remove_user_from_list(u);
	buff_free(b);	
	user_free(u);
	return NULL;	

}

int create_listening_sock(uint16_t port)
{

	int sock=socket(AF_INET,SOCK_STREAM,0);
	if (sock<0){
		perror("socket");
		return -1;
	}
	struct sockaddr_in sa ={.sin_family= AF_INET,
							.sin_port=htons(port),
							.sin_addr.s_addr =htonl(INADDR_ANY)
						};
	
	if (bind(sock,(struct sockaddr*)&sa,sizeof(sa))<0){
		perror("bind");
		return -1;
	}
	if (listen(sock,10)<0){
		perror("listen");
		return -1;
	}
	
	
	return sock;

}

void *repeter(void *arg)
{
  char buf[1024];
  ssize_t n;

  while ((n=read(tube[0],buf,sizeof(buf)))>0){
	pthread_mutex_lock(&lock);
	struct node *c =users->first;//list_get(users,0);marche pas
	while(c!=NULL){
		struct user *u = (struct user* )c->elt;//on recupere le client 
		if (u && u->sock >=0){
			write(u->sock,buf,n);
		}

		c=c->next;
	}
	pthread_mutex_unlock(&lock);
  }
  return NULL;
}

//pour verifier que le nickname n'est pas deja pris par un autre client
int nickname_exists(const char *nick) {
    struct node *c = users->first;

    while (c != NULL) {
        struct user *u = (struct user*) c->elt;

        if (u && strcmp(u->nickname, nick) == 0) {
            return 1; // déjà pris
        }

        c = c->next;
    }
    
    return 0; // libre
}

void remove_user_from_list(struct user *u)
{
	pthread_mutex_lock(&lock);
	list_remove_element(users, u);
	pthread_mutex_unlock(&lock);
}