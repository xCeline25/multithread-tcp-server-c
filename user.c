					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */

#include "user.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>//pour close 

/** accepter une connection TCP depuis la socket d'écoute sl et retourner un
 * pointeur vers un struct user, dynamiquement alloué et convenablement
 * initialisé */
struct user *user_accept(int sl)
{
	struct user *u=malloc (sizeof(struct user));
	if (u==NULL) return NULL;
	 u->addr_len= sizeof( struct sockaddr_in);
	 u->sock= accept(sl,(struct sockaddr*)&u->address,&u->addr_len);
		if (u->sock<0){
			perror("accept");
           return NULL;
		}
	
	return u;
}

/** libérer toute la mémoire associée à user */
void user_free(struct user *user)
{
	if(user ==NULL) return;
	close(user->sock);
	free(user);
	
}
