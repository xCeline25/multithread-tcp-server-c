					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */
#include "buffer.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct buffer {
    int fd;        // fichier (socket, stdin, etc.)
    char *data;    // tableau (buffer)
    size_t size;   // taille du buffer
    size_t pos;    // position actuelle
    size_t len;    // nb d’octets valides
    int eof;       // fin de fichier 
};

buffer *buff_create(int fd, size_t buffsz)
{
	buffer *b = malloc(sizeof(buffer));
	if (b == NULL) return NULL;

	b->data = malloc(buffsz);
	if (b->data == NULL) {
		free(b);
		return NULL;
	}

	b->fd = fd;
	b->size = buffsz;
	b->pos = 0;
	b->len = 0;
	b->eof = 0;

	return b;
}

/** Retourner le prochain caractère à lire ou EOF (-1) si la fin
 * du fichier est atteinte.
 *
 * Cette fonction appellera l'appel système read si tous les octets
 * du buffer ont déjà été consommés */
int buff_getc(buffer *b)
{
	if (b == NULL) return EOF;

	if (b->pos >= b->len) {
		ssize_t n = read(b->fd, b->data, b->size); // lecture
		if (n < 0) return EOF;//erreur de lecture
		if (n == 0) { //fin fichier
			b->eof = 1;
			return EOF;
		}
		b->len = n;
		b->pos = 0;
	}

	return (unsigned char)b->data[b->pos++];
}
/** Remettre le caractère c dans le buffer.
 * Le résultat n'est garanti que pour un seul caractère.
 * retourne le caractère c. */
int buff_ungetc(buffer *b, int c)
{
    if (!b || b->pos == 0) return EOF;

    b->pos--;
    b->data[b->pos] = (char)c;
    return c;
}

void buff_free(buffer *b)
{
	if (b == NULL) return;

	free(b->data);
	free(b);
}
/** Retourner 1 si la lecture a atteint la fin du fichier, 0 sinon */
int buff_eof(const buffer *buff)
{
	if (buff == NULL) return 1; // considérer que c'est la fin du fichier

	return buff->eof;
}

/** Retourner 1 si des octets sont disponibles dans le buffer sans lecture du
 * fichier */
int buff_ready(const buffer *buff)
{
	if (buff == NULL) return 0;

	return buff->pos < buff->len;
}

/* Lire au plus size - 1 caractère dans le buffer b et les stocker dans le
 * tableau dest qui devrait pouvoir contenir au moins size octets.
 * La lecture s'arrête après une fin de fichier ou le caractère de fin de ligne
 * '\n'. Si une fin de ligne '\n' est lue, elle est stocké dans dest. Retourne
 * buf en cas de succès, NULL en cas d'erreur, ou si la fin du fichier est
 * atteinte sans que des caractères aient été lus. */
char *buff_fgets(buffer *b, char *dest, size_t size)
{
    if (!b || !dest || size == 0) return NULL;

    size_t i = 0;

    while (i < size - 1) {
        int c = buff_getc(b);

        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }

        dest[i++] = (char)c;

        if (c == '\n') break;
    }

    dest[i] = '\0';
    return dest;
}

/* Lire au plus size - 1 caractère dans le buffer b et les stocker dans le
 * tableau dest qui devrait pouvoir contenir au moins size octets.
 * La lecture s'arrête après une fin de fichier ou les deux caractères '\r'
 * suivi de '\n'. Si une fin de ligne "\r\n" est lue, elle est stocké dans dest.
 * Retourne buf en cas de succès, NULL en cas d'erreur, ou si la fin du fichier
 * est atteinte sans que des caractères aient été lus. */
char *buff_fgets_crlf(buffer *b, char *dest, size_t size)
{
    if (!b || !dest || size == 0) return NULL;

    size_t i = 0;
    int c;

    while (i < size - 1) {
        c = buff_getc(b);

        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }

        dest[i++] = (char)c;

        if (i >= 2 && dest[i-2] == '\r' && dest[i-1] == '\n') {
            break;
        }
    }

    dest[i] = '\0';
    return dest;
}
