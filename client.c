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
#include <sys/stat.h>


#define PORT_FREESCORD 4321

/** se connecter au serveur TCP d'adresse donnée en argument sous forme de
 * chaîne de caractère et au port donné en argument
 * retourne le descripteur de fichier de la socket obtenue ou -1 en cas
 * d'erreur. */
int connect_serveur_tcp(const char *adresse, uint16_t port);
int upload_file_to_server(const char *adresse, uint16_t port, const char *filepath);
int download_file_from_server(const char *adresse, uint16_t port, const char *filepath);
void trim_crlf(char *line);
int process_server_line(const char *server_host, char *line);
const char *basename_from_path(const char *path);

void usage(char* nom_prog);

static char pending_fput_path[512];
static int pending_fput_active = 0;
static char pending_fget_name[256];
static int pending_fget_active = 0;

int main(int argc, char *argv[])
{
	

	if (argc !=2){
		usage(argv[0]);
		return 1;
	}
	const char *server_host = argv[1];
	int sock = connect_serveur_tcp(server_host,PORT_FREESCORD);
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
				if (process_server_line(server_host, line)) {
					continue;
				}
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
			if (n >= (ssize_t)sizeof(buf))
				n = sizeof(buf) - 1;
			buf[n]='\0';

			if (strncmp(buf, "fput ", 5) == 0) {
				char local_path[512];
				strncpy(local_path, buf + 5, sizeof(local_path) - 1);
				local_path[sizeof(local_path) - 1] = '\0';
				trim_crlf(local_path);

				struct stat st;
				if (stat(local_path, &st) < 0) {
					perror("stat fput");
					continue;
				}

				strncpy(pending_fput_path, local_path, sizeof(pending_fput_path) - 1);
				pending_fput_path[sizeof(pending_fput_path) - 1] = '\0';
				pending_fput_active = 1;

				const char *base = basename_from_path(local_path);
				snprintf(buf, sizeof(buf), "fput %s", base);
			} else if (strncmp(buf, "fget ", 5) == 0) {
				char name[256];
				strncpy(name, buf + 5, sizeof(name) - 1);
				name[sizeof(name) - 1] = '\0';
				trim_crlf(name);
				if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) {
					write(1, "fget: utiliser seulement le nom du fichier\n",
						strlen("fget: utiliser seulement le nom du fichier\n"));
					continue;
				}
				strncpy(pending_fget_name, name, sizeof(pending_fget_name) - 1);
				pending_fget_name[sizeof(pending_fget_name) - 1] = '\0';
				pending_fget_active = 1;
			}

			lf_to_crlf(buf);
			write(sock,buf,strlen(buf));
		}

		//socket pret oui/non
		if(fds[1].revents & (POLLIN|POLLHUP)){
			//lire la reponse avec read
			if (buff_fgets_crlf(b, line, sizeof(line)) == NULL) {
				//non
				break;
			}
			if (process_server_line(server_host, line)) {
				continue;
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

void trim_crlf(char *line)
{
	if (line == NULL)
		return;

	size_t len = strlen(line);
	while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
		line[len - 1] = '\0';
		len--;
	}
}

int upload_file_to_server(const char *adresse, uint16_t port, const char *filepath)
{
	FILE *f = fopen(filepath, "rb");
	if (f == NULL) {
		perror("fopen");
		return -1;
	}

	if (fseek(f, 0, SEEK_END) < 0) {
		perror("fseek");
		fclose(f);
		return -1;
	}

	long size = ftell(f);
	if (size < 0) {
		perror("ftell");
		fclose(f);
		return -1;
	}

	rewind(f);

	int sock = connect_serveur_tcp(adresse, port);
	if (sock < 0) {
		fclose(f);
		return -1;
	}

	char header[64];
	snprintf(header, sizeof(header), "fput_data %ld\r\n", size);
	write(sock, header, strlen(header));

	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		write(sock, buf, n);
	}

	buffer *b = buff_create(sock, 1024);
	if (b != NULL) {
		char line[256];
		if (buff_fgets_crlf(b, line, sizeof(line)) != NULL) {
			crlf_to_lf(line);
			write(1, line, strlen(line));
		}
		buff_free(b);
	}

	fclose(f);
	close(sock);
	return 0;
}

int download_file_from_server(const char *adresse, uint16_t port, const char *filepath)
{
	int sock = connect_serveur_tcp(adresse, port);
	if (sock < 0)
		return -1;

	buffer *b = buff_create(sock, 4096);
	if (b == NULL) {
		close(sock);
		return -1;
	}

	char header[128];
	if (buff_fgets_crlf(b, header, sizeof(header)) == NULL) {
		buff_free(b);
		close(sock);
		return -1;
	}

	long expected = -1;
	if (sscanf(header, "fget_data %ld", &expected) != 1 || expected < 0) {
		crlf_to_lf(header);
		write(1, header, strlen(header));
		buff_free(b);
		close(sock);
		return -1;
	}

	FILE *f = fopen(filepath, "wb");
	if (f == NULL) {
		perror("fopen");
		buff_free(b);
		close(sock);
		return -1;
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
	buff_free(b);
	close(sock);
	return remaining == 0 ? 0 : -1;
}

const char *basename_from_path(const char *path)
{
	const char *base = path;
	for (const char *p = path; p && *p; ++p) {
		if (*p == '/' || *p == '\\')
			base = p + 1;
	}
	return base;
}

int process_server_line(const char *server_host, char *line)
{
	if (line == NULL)
		return 0;

	if (pending_fput_active && strncmp(line, "fput_ready ", 11) == 0) {
		char *endptr = NULL;
		uint16_t port = (uint16_t)strtoul(line + 11, &endptr, 10);
		if (endptr == line + 11) {
			write(1, "invalid fput port\n", strlen("invalid fput port\n"));
			pending_fput_active = 0;
			return 1;
		}

		if (upload_file_to_server(server_host, port, pending_fput_path) == 0) {
			write(1, "fput done\n", strlen("fput done\n"));
		}
		pending_fput_active = 0;
		return 1;
	}

	if (pending_fget_active && strncmp(line, "fget_ready ", 11) == 0) {
		char *endptr = NULL;
		uint16_t port = (uint16_t)strtoul(line + 11, &endptr, 10);
		if (endptr == line + 11) {
			write(1, "invalid fget port\n", strlen("invalid fget port\n"));
			pending_fget_active = 0;
			return 1;
		}

		if (download_file_from_server(server_host, port, pending_fget_name) == 0) {
			write(1, "fget done\n", strlen("fget done\n"));
		} else {
			write(1, "fget failed\n", strlen("fget failed\n"));
		}
		pending_fget_active = 0;
		return 1;
	}

	return 0;
}