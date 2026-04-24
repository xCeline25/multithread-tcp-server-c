					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list/list.h"
#include "user.h"

#define PORT_FREESCORD 4321

/** Gérer toutes les communications avec le client renseigné dans
 * user, qui doit être l'adresse d'une struct user */
void *handle_client(void *user);
/** Créer et configurer une socket d'écoute sur le port donné en argument
 * retourne le descripteur de cette socket, ou -1 en cas d'erreur */
int create_listening_sock(uint16_t port);

int main(int argc, char *argv[])
{
	int listen_sock=create_listening_sock(PORT_FREESCORD);
	if (listen_sock <0) exit(1);

	while(1){
		//accepter les demandes de connexion 
		struct user *u= user_accept(listen_sock);
		if (u==NULL) continue;
		
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
	struct user *u =(struct user*) clt;
	if (u==NULL) return NULL;
	ssize_t n;
		while((n=read(u->sock,buf,sizeof(buf)))>0){
			write(u->sock,buf,n);
		}
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
