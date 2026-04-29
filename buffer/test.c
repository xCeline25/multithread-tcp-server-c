#include <stdio.h>
#include <unistd.h>
#include "buffer.h"

int main()
{
    buffer *b = buff_create(0, 1024);
    if (!b) {
        perror("buff_create");
        return 1;
    }

    char line[1024];

    printf("Tape du texte  :\n");

    while (buff_fgets(b, line, sizeof(line)) != NULL) {
        printf("Lu : %s", line);
    }

    if (buff_eof(b)) {
        printf("\n[Fin de fichier ]\n");
    }

    buff_free(b);
    return 0;
}