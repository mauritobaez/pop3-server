
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>

#include "logger.h"
#include "pop3_utils.h"
#include "directories.h"
#include "server.h"

size_t count_files_in_dir(DIR *dirp);

/*
typedef struct email_file_info {
    char* filename;
    size_t octets;
} email_file_info;
*/

// receives: directory absolute path without '/' at the end and an int*
// ->
// returns: filename and size of files in said directory and the amount of emails
email_metadata_t *get_emails_at_directory(const char *directory, size_t *email_count)
{
    // Abro el directorio en cuestión
    DIR *dirp = opendir(directory); // debería quedar maildir/directory/ o con ./ al principio no me acuerdo
    email_metadata_t *files = NULL;
    log(DEBUG, "DIRP is NULL %d", dirp == NULL);
    *email_count = 0;
    if (dirp != NULL)
    {
        size_t total_files = count_files_in_dir(dirp);
        files = malloc(sizeof(email_metadata_t) * total_files);
        int index = 0;
        struct dirent *curr; /*!= NULL*/
        struct stat sb;

        int directory_length = strlen(directory);
        char path[256];
        strncpy(path, directory, directory_length);
        path[directory_length] = '/';
        directory_length += 1;

        while ((curr = readdir(dirp)) != NULL)
        {
            if (!(strcmp(curr->d_name, "..") == 0 || strcmp(curr->d_name, ".") == 0))
            {
                strcpy(path + directory_length, curr->d_name);
                stat(path, &sb);
                if (S_ISREG(sb.st_mode))
                {
                    files[index].octets = sb.st_size;
                    files[index].filename = malloc(directory_length + strlen(curr->d_name) + 2);
                    files[index].deleted = false;
                    strcpy(files[index].filename, path);
                    index++;
                    *email_count += 1;
                }
            }
        }

        closedir(dirp);
    }
    return files;
}

FILE *open_email_file(pop3_client *client, char *filename)
{
    char command_string[1024];
    strcpy(command_string, "cat ");
    strcat(command_string, filename);
    if (global_config.transform_program != NULL)
    {
        strcat(command_string, "| ");
        strcat(command_string, global_config.transform_program);
        strcat(command_string, " 2> /dev/null");
    }
    FILE *stream = popen(command_string, "r");
    return stream;
}

size_t count_files_in_dir(DIR *dirp)
{
    size_t total = 0;
    struct dirent *curr;
    while ((curr = readdir(dirp)) != NULL)
    {
        if (!(strcmp(curr->d_name, "..") == 0 || strcmp(curr->d_name, ".") == 0))
        {
            total += 1;
        }
    }
    // go back to the beginning of the file list
    rewinddir(dirp);
    return total;
}

char *join_path(const char *dir1, const char *dir2)
{
    int dir1_length = strlen(dir1);
    int dir2_length = strlen(dir2);

    char *total = malloc(dir1_length + dir2_length + 2);
    strcpy(total, dir1);
    total[dir1_length] = PATH_SEPARATOR;
    strcpy(total + dir1_length + 1, dir2);
    return total;
}

bool path_is_directory(char *path)
{
    DIR *dir = opendir(path);
    if (dir != NULL)
    {
        closedir(dir);
        return 1;
    }
    return 0;
}
