					/* Celine SAIDI 12409031
					Je déclare qu'il s'agit de mon propre travail.
					Ce travail a été réalisé intégralement par un 
					être humain. */
#include <string.h>

/* Changer une ligne se terminant par CRLF en une ligne se terminant par LF.
 * La ligne doit être terminée par un carcactère nul.
 * Retourne la ligne modifiée ou NULL en cas d'erreur. */
char *crlf_to_lf(char *line_with_crlf){
    if (line_with_crlf == NULL) return NULL;

    size_t len = strlen(line_with_crlf);

  
    if (len < 2) return NULL;

    if (line_with_crlf[len - 2] == '\r' && line_with_crlf[len - 1] == '\n') {
        line_with_crlf[len - 2] = '\n';
        line_with_crlf[len - 1] = '\0';
        return line_with_crlf;
    }

    return NULL; 
}


char *lf_to_crlf(char *line_with_lf)
{
    if (line_with_lf == NULL) return NULL;

    size_t len = strlen(line_with_lf);

    
    if (len < 1) return NULL;

    if (line_with_lf[len - 1] == '\n') {
        line_with_lf[len] = '\n';
        line_with_lf[len - 1] = '\r';
        line_with_lf[len + 1] = '\0';
        return line_with_lf;
    }

    return NULL; 
}
