#ifndef DIRECTORIES_H
#define DIRECTORIES_H

#define PATH_SEPARATOR '/'

email_metadata_t* get_file_info(const char* directory, size_t *email_count);
char *join_path(const char *dir1, const char *dir2);
int open_email_file(pop3_client* client, char *filename);

#endif