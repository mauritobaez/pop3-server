#ifndef DIRECTORIES_H
#define DIRECTORIES_H

#include <stdio.h>
#include "pop3_utils.h"

#define PATH_SEPARATOR '/'

// obtiene un array de emails y modifica email_count con la cantidad de emails
email_metadata_t *get_emails_at_directory(const char *directory, size_t *email_count);

// une dos paths
char *join_path(const char *dir1, const char *dir2);

// devuelve el file stream de un archivo
FILE *open_email_file(pop3_client *client, char *filename);
bool path_is_directory(char *path);

#endif
