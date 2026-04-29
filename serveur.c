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
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "list/list.h"
#include "user.h"
#include "buffer/buffer.h"
#include "utils.h"

#define PORT_FREESCORD 4321
#define FILES_DIR "files"

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
struct user *find_user_by_nickname_locked(const char *nick);
void send_user_list(struct user *dest);
int create_dynamic_transfer_sock(void);
void *handle_file_transfer_send(void *arg);
void *handle_file_transfer_receive(void *arg);

struct transfer_context {
	int transfer_sock;
	char filepath[512];
};

int tube[2];//0 lecture //1 ecriture
struct list *users;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;//pour proteger la liste des clients

//le buf actuel lit des block on va utiliser buffer pour les lignes

int main(int argc, char *argv[])
{


	int listen_sock=create_listening_sock(PORT_FREESCORD);
	if (listen_sock <0) exit(1);
	
	
	char message_bienvenue[] =
		"Bienvenue sur freescord\r\n"
		"Entrez votre pseudo avec: nickname <pseudo>\r\n"
		"Ensuite, envoyez vos messages avec: msg <texte>\r\n"
		"Commandes: list | whisper <pseudo> <texte>\r\n"
		"Fichiers: flist | fget <file> | fput <file>\r\n"
		"Pseudo max 16 caracteres, sans ':'\r\n"
		"\r\n";

	if (mkdir(FILES_DIR, 0755) < 0 && errno != EEXIST) {
		perror("mkdir");
	}

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
				size_t len = strlen(buf);
				while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
					buf[len - 1] = '\0';
					len--;
				}

				if (strcmp(buf, "list") == 0) {
					send_user_list(u);
					continue;
				}

				if (strcmp(buf, "flist") == 0) {
					DIR *dir = opendir(FILES_DIR);
					if (dir == NULL) {
						write(u->sock,
							"flist_error opendir failed\r\n",
							strlen("flist_error opendir failed\r\n"));
						continue;
					}

					write(u->sock, "flist_begin\r\n", strlen("flist_begin\r\n"));

					struct dirent *entry;
					while ((entry = readdir(dir)) != NULL) {
						char path[512];
						struct stat st;

						snprintf(path, sizeof(path), "%s/%s", FILES_DIR, entry->d_name);
						if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
							char line[300];
							snprintf(line, sizeof(line), "flist %s\r\n", entry->d_name);
							write(u->sock, line, strlen(line));
						}
					}

					write(u->sock, "flist_end\r\n", strlen("flist_end\r\n"));
					closedir(dir);
					continue;
				}

				if (strncmp(buf, "fget ", 5) == 0) {
					char *filename = buf + 5;
					if (*filename == '\0') {
						write(u->sock,
							"ERR usage: fget <filename>\r\n",
							strlen("ERR usage: fget <filename>\r\n"));
						continue;
					}

					if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
						write(u->sock,
							"ERR chemin interdit\r\n",
							strlen("ERR chemin interdit\r\n"));
						continue;
					}

					int transfer_sock = create_dynamic_transfer_sock();
					if (transfer_sock < 0) {
						write(u->sock,
							"ERR socket transfer failed\r\n",
							strlen("ERR socket transfer failed\r\n"));
						continue;
					}

					struct sockaddr_storage addr;
					socklen_t addr_len = sizeof(addr);
					if (getsockname(transfer_sock, (struct sockaddr *)&addr, &addr_len) < 0) {
						perror("getsockname");
						close(transfer_sock);
						continue;
					}

					uint16_t port = 0;
					if (addr.ss_family == AF_INET6)
						port = ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
					else if (addr.ss_family == AF_INET)
						port = ntohs(((struct sockaddr_in *)&addr)->sin_port);

					char resp[64];
					snprintf(resp, sizeof(resp), "fget_ready %u\r\n", (unsigned int)port);
					write(u->sock, resp, strlen(resp));

					struct transfer_context *ctx = malloc(sizeof(struct transfer_context));
					if (ctx == NULL) {
						close(transfer_sock);
						continue;
					}
					ctx->transfer_sock = transfer_sock;
					snprintf(ctx->filepath, sizeof(ctx->filepath), "%s/%s", FILES_DIR, filename);

					pthread_t tid;
					pthread_create(&tid, NULL, handle_file_transfer_send, (void *)ctx);
					pthread_detach(tid);
					continue;
				}

				if (strncmp(buf, "fput ", 5) == 0) {
					char *filename = buf + 5;
					if (*filename == '\0') {
						write(u->sock,
							"ERR usage: fput <filename>\r\n",
							strlen("ERR usage: fput <filename>\r\n"));
						continue;
					}

					if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
						write(u->sock,
							"ERR chemin interdit\r\n",
							strlen("ERR chemin interdit\r\n"));
						continue;
					}

					int transfer_sock = create_dynamic_transfer_sock();
					if (transfer_sock < 0) {
						write(u->sock,
							"ERR socket transfer failed\r\n",
							strlen("ERR socket transfer failed\r\n"));
						continue;
					}

					struct sockaddr_storage addr;
					socklen_t addr_len = sizeof(addr);
					if (getsockname(transfer_sock, (struct sockaddr *)&addr, &addr_len) < 0) {
						perror("getsockname");
						close(transfer_sock);
						continue;
					}

					uint16_t port = 0;
					if (addr.ss_family == AF_INET6)
						port = ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
					else if (addr.ss_family == AF_INET)
						port = ntohs(((struct sockaddr_in *)&addr)->sin_port);

					char resp[64];
					snprintf(resp, sizeof(resp), "fput_ready %u\r\n", (unsigned int)port);
					write(u->sock, resp, strlen(resp));

					struct transfer_context *ctx = malloc(sizeof(struct transfer_context));
					if (ctx == NULL) {
						close(transfer_sock);
						continue;
					}
					ctx->transfer_sock = transfer_sock;
					snprintf(ctx->filepath, sizeof(ctx->filepath), "%s/%s", FILES_DIR, filename);

					pthread_t tid;
					pthread_create(&tid, NULL, handle_file_transfer_receive, (void *)ctx);
					pthread_detach(tid);
					continue;
				}

				if (strncmp(buf, "whisper ", 8) == 0) {
					char *args = buf + 8;
					char *sep = strchr(args, ' ');
					if (sep == NULL || sep == args || *(sep + 1) == '\0') {
						write(u->sock,
							"ERR usage: whisper <pseudo> <texte>\r\n",
							strlen("ERR usage: whisper <pseudo> <texte>\r\n"));
						continue;
					}

					*sep = '\0';
					char *dest_nick = args;
					char *contenu = sep + 1;

					if (strlen(dest_nick) > 16 || strchr(dest_nick, ':') != NULL) {
						write(u->sock,
							"ERR pseudo destinataire invalide\r\n",
							strlen("ERR pseudo destinataire invalide\r\n"));
						continue;
					}

					if (*contenu == '\0') {
						write(u->sock,
							"ERR message prive vide\r\n",
							strlen("ERR message prive vide\r\n"));
						continue;
					}

					pthread_mutex_lock(&lock);
					struct user *dest = find_user_by_nickname_locked(dest_nick);
					if (dest == NULL) {
						pthread_mutex_unlock(&lock);
						write(u->sock,
							"ERR utilisateur introuvable\r\n",
							strlen("ERR utilisateur introuvable\r\n"));
						continue;
					}

					snprintf(msg, sizeof(msg), "priv %s: %s\r\n", u->nickname, contenu);
					write(dest->sock, msg, strlen(msg));

					snprintf(msg, sizeof(msg), "priv_to %s: %s\r\n", dest_nick, contenu);
					write(u->sock, msg, strlen(msg));
					pthread_mutex_unlock(&lock);
					continue;
				}

				if (strncmp(buf, "msg ", 4) != 0) {
					write(u->sock,
						"ERR commande inconnue, utilisez: msg <texte>\r\n",
						strlen("ERR commande inconnue, utilisez: msg <texte>\r\n"));
					continue;
				}

				const char *contenu = buf + 4;
				if (*contenu == '\0') {
					write(u->sock,
						"ERR message vide\r\n",
						strlen("ERR message vide\r\n"));
					continue;
				}

				snprintf(msg, sizeof(msg), "msg %s: %s\r\n", u->nickname, contenu);
				write(tube[1],msg,strlen(msg));
			}

	remove_user_from_list(u);
	buff_free(b);	
	user_free(u);
	return NULL;	

}

int create_listening_sock(uint16_t port)
{
	char service[6];
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *rp;
	int sock = -1;

	snprintf(service, sizeof(service), "%u", (unsigned int)port);

	//on essaie d'abord de créer une socket IPv6,
	// qui peut aussi accepter des connexions IPv4 
	//si le noyau le supporte
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, service, &hints, &res) == 0) {
		for (rp = res; rp != NULL; rp = rp->ai_next) {
			sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sock < 0)
				continue;

			int yes = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

			int off = 0;
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

			if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0 && listen(sock, 10) == 0)
				break;

			close(sock);
			sock = -1;
		}
		freeaddrinfo(res);
		if (sock >= 0)
			return sock;
	}

	//si la création d'une socket IPv6 a echoue 
	//on essaie de creer une socket IPv4
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, service, &hints, &res) != 0) {
		perror("getaddrinfo");
		return -1;
	}

	for (rp = res; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0)
			continue;

		int yes = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0 && listen(sock, 10) == 0)
			break;

		close(sock);
		sock = -1;
	}

	freeaddrinfo(res);

	if (sock < 0)
		perror("bind/listen");

	return sock;

}

void *repeter(void *arg)
{
  char buf[1024];
  ssize_t n;

  while ((n=read(tube[0],buf,sizeof(buf)))>0){
	pthread_mutex_lock(&lock);
	struct node *c =users->first;
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

struct user *find_user_by_nickname_locked(const char *nick)
{
	for (struct node *c = users->first; c != NULL; c = c->next) {
		struct user *u = (struct user *)c->elt;
		if (u != NULL && strcmp(u->nickname, nick) == 0)
			return u;
	}
	return NULL;
}

void send_user_list(struct user *dest)
{
	write(dest->sock, "list_begin\r\n", strlen("list_begin\r\n"));

	pthread_mutex_lock(&lock);
	for (struct node *c = users->first; c != NULL; c = c->next) {
		struct user *u = (struct user *)c->elt;
		if (u == NULL)
			continue;
		char line[64];
		snprintf(line, sizeof(line), "list %s\r\n", u->nickname);
		write(dest->sock, line, strlen(line));
	}
	pthread_mutex_unlock(&lock);

	write(dest->sock, "list_end\r\n", strlen("list_end\r\n"));
}

int create_dynamic_transfer_sock(void)
{
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *rp;
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, "0", &hints, &res) == 0) {
		for (rp = res; rp != NULL; rp = rp->ai_next) {
			sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sock < 0)
				continue;

			int yes = 1;
			setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

			int off = 0;
			setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));

			if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0 && listen(sock, 1) == 0)
				break;

			close(sock);
			sock = -1;
		}
		freeaddrinfo(res);
		if (sock >= 0)
			return sock;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, "0", &hints, &res) != 0) {
		perror("getaddrinfo port 0");
		return -1;
	}

	for (rp = res; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0)
			continue;

		int yes = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0 && listen(sock, 1) == 0)
			break;

		close(sock);
		sock = -1;
	}

	freeaddrinfo(res);
	return sock;
}

void *handle_file_transfer_send(void *arg)
{
	struct transfer_context *ctx = (struct transfer_context *)arg;
	if (ctx == NULL)
		return NULL;

	int listen_sock = ctx->transfer_sock;
	const char *filepath = ctx->filepath;

	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);

	if (client_sock < 0) {
		perror("accept transfer");
		close(listen_sock);
		free(ctx);
		return NULL;
	}

	close(listen_sock);

	FILE *f = fopen(filepath, "rb");
	if (f == NULL) {
		write(client_sock,
			"ERR file not found\r\n",
			strlen("ERR file not found\r\n"));
		close(client_sock);
		free(ctx);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char header[64];
	snprintf(header, sizeof(header), "fget_data %ld\r\n", file_size);
	write(client_sock, header, strlen(header));

	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		write(client_sock, buf, n);
	}

	fclose(f);
	close(client_sock);
	free(ctx);
	return NULL;
}

void *handle_file_transfer_receive(void *arg)
{
	struct transfer_context *ctx = (struct transfer_context *)arg;
	if (ctx == NULL)
		return NULL;

	int listen_sock = ctx->transfer_sock;
	const char *filepath = ctx->filepath;

	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
	if (client_sock < 0) {
		perror("accept transfer");
		close(listen_sock);
		free(ctx);
		return NULL;
	}

	close(listen_sock);

	buffer *b = buff_create(client_sock, 4096);
	if (b == NULL) {
		close(client_sock);
		free(ctx);
		return NULL;
	}

	char header[128];
	if (buff_fgets_crlf(b, header, sizeof(header)) == NULL) {
		buff_free(b);
		close(client_sock);
		free(ctx);
		return NULL;
	}

	long expected = -1;
	if (sscanf(header, "fput_data %ld", &expected) != 1 || expected < 0) {
		write(client_sock, "ERR bad upload header\r\n", strlen("ERR bad upload header\r\n"));
		buff_free(b);
		close(client_sock);
		free(ctx);
		return NULL;
	}

	FILE *f = fopen(filepath, "wb");
	if (f == NULL) {
		write(client_sock, "ERR cannot open output\r\n", strlen("ERR cannot open output\r\n"));
		buff_free(b);
		close(client_sock);
		free(ctx);
		return NULL;
	}

	long remaining = expected;
	while (remaining > 0) {
		int c = buff_getc(b);
		if (c == EOF)
			break;
		fputc(c, f);
		remaining--;
	}

	fclose(f);
	if (remaining == 0)
		write(client_sock, "fput_ok\r\n", strlen("fput_ok\r\n"));
	else
		write(client_sock, "ERR incomplete upload\r\n", strlen("ERR incomplete upload\r\n"));

	buff_free(b);
	close(client_sock);
	free(ctx);
	return NULL;
}