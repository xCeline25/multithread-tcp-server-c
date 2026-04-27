					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include "buffer/buffer.h"
#include "utils.h"
#include <string.h>


#define PORT_FREESCORD 4321

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(const char *adresse, uint16_t port);

void usage(char* nom_prog);

int main(int argc, char *argv[])
{
	

	if (argc !=2){
		usage(argv[0]);
		return 1;
	}
	int sock = connect_serveur_tcp(argv[1],PORT_FREESCORD);
	if (sock<0){
		perror("connect");
		return -1;
	}
	
	buffer *b = buff_create(sock, 1024); //  mémoire interne TCP pour reconstruire lignes

	char buf[1024];// input utilisateur (écriture vers serveur)
	char line[1024];// input serveur (lecture depuis socket)

	struct pollfd fds[2]={
						 {.fd=0, .events=POLLIN},//terminal de lecture
					     {.fd=sock, .events=POLLIN}//socket de lecture
						};

	while(1){
		ssize_t n;
		// on traite le buffer (socket ) avant poll pour eviter de faire un poll inutile
		// si le buffer a deja des octets a lire
		//on vide le buffer  avant d’attendre poll 
		if (buff_ready(b)) { 
			//lire une ligne du buffer et l’écrire sur le terminal
			if (buff_fgets_crlf(b, line, sizeof(line)) != NULL) {
				//changer les CRLF en LF 
				  crlf_to_lf(line);
				  write(1, line, strlen(line)); //on ecrit sur le terminal donc on consomme le buffer avant de faire un poll
				   continue;
				 }
		 
		}
		if (poll(fds,2,-1)<0){
			perror("poll");
			break; 
		}
		
		//terminal pret oui /non
		if(fds[0].revents & POLLIN){
			//lire sur le terminal 
			n=read(0,buf,sizeof(buf));
			 if (n < 0) {
				perror("read");
				break;
			}
			if(n==0) break;//non
			//envoi avec write prcq on est sur TCP et la connection est deja etablie 
			buf[n]='\0';//pour avoir une chaine de caractere et utiliser strlen 
			lf_to_crlf(buf);
			
			write(sock,buf,strlen(buf)); //strlen pas n prcq ca change la taille CRLF ajoute le  caractère \r
		}

		//socket pret oui/non
		if(fds[1].revents & (POLLIN|POLLHUP)){
			//lire la reponse avec read
			if (buff_fgets_crlf(b, line, sizeof(line)) == NULL) {
				//non
				break;
			}
			//ecrire sur terminal discripteur 1
			crlf_to_lf(line); 
			write (1,line,strlen(line));
		}
		
		

	}
	 buff_free(b);
	close(sock);
	return 0;
}

int connect_serveur_tcp(const char *adresse, uint16_t port)
{
	char service[6];
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *ai = NULL;
	int sock = -1;

	snprintf(service, sizeof(service), "%u", (unsigned int)port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int gai_ret = getaddrinfo(adresse, service, &hints, &res);
	if (gai_ret != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_ret));
		return -1;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			continue;
		}
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}
		close(sock);
		sock = -1;
	}

	freeaddrinfo(res);
	return sock;
}

void usage(char *nom_prog){
	fprintf(stderr,"Usage:%s hote\n"
			"client pour\n"
			"Exemple:%s 127.0.0.1\n"
			"Exemple:%s ::1\n"
			"Exemple:%s localhost\n",nom_prog,nom_prog,nom_prog,nom_prog);
}