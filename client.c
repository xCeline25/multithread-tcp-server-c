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
#include <poll.h>
#include "buffer/buffer.h"
#include "utils.h"
#include <string.h>


#define PORT_FREESCORD 4321

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(char *adresse, uint16_t port);

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
	char buf[1024];

	while(1){
		//lire une ligne avec fgets on peut faire aussi read mais faut regler la gestion du \n
		if(fgets(buf,sizeof(buf),stdin)==NULL) break;
		//envoi avec write prcq on est sur TCP et la connection est deja etablie 
		write(sock,buf,strlen(buf)); 
		//lire la reponse avec read
		ssize_t n=read(sock,buf,sizeof(buf)-1);
		if (n<0) break;
		buf[n]='\0';
		//ecrire sur terminal discripteur 1 
		write (1,buf,n);

	}
	close(sock);
	return 0;
}

int connect_serveur_tcp(char *adresse, uint16_t port)
{
	int sock=socket(AF_INET,SOCK_STREAM,0);
	if (sock<0){
		perror("socket");
		return -1;
	}
	struct sockaddr_in sa ={
		.sin_family=AF_INET,
		.sin_port=htons(port)
	};

	if ((inet_pton(AF_INET,adresse,&sa.sin_addr)!=1)){
		fprintf(stderr,"adresse ipv4 non valable");
		return -1;
	}
	socklen_t sl =sizeof(struct sockaddr_in);
	if(connect(sock,(struct sockaddr *)&sa, sl)<0){
		perror("connect");
		return -1;
	}
	/* pour éviter les warnings de variable non utilisée */
	return sock; //*adresse + port; //changement ici 
}

void usage(char *nom_prog){
	fprintf(stderr,"Usage:%s addr_ipv4\n"
			"client pour\n"
			"Exemple:%s 208.97.177.124",nom_prog,nom_prog);
}