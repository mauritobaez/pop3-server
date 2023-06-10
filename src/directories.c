#define _BSD_SOURCE //Siempre hacerlo antes del include

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <malloc.h>

#include "logger.h"
#include "./pop3_utils.h"
#include "directories.h"

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
email_file_info* get_file_info(const char* directory, size_t *email_count) {
    printf("Directory: %s\n", directory);
    //Abro el directorio en cuestión
    DIR * dirp = opendir(directory); //debería quedar maildir/directory/ o con ./ al principio no me acuerdo
    email_file_info * files = NULL;
    log(INFO, "DIRP is NULL %d", dirp == NULL);
    if(dirp != NULL) {
        size_t total_files = count_files_in_dir(dirp);
        files = malloc(sizeof(email_file_info) * total_files);
        int index = 0;
        struct dirent * curr; /*!= NULL*/
        struct stat sb;

        int directory_length = strlen(directory);
        char path[256];
        strncpy(path, directory, directory_length);
        path[directory_length] = '/';
        directory_length += 1;
        
        while((curr = readdir(dirp)) != NULL) {
            if(!(strcmp(curr->d_name,"..") == 0 || strcmp(curr->d_name, ".") == 0)){
                strcpy(path + directory_length, curr->d_name);
                stat(path, &sb);
                if (S_ISREG(sb.st_mode)) {
                    files[index].octets = sb.st_size;
                    files[index].filename = malloc(strlen(curr->d_name) + 1);
                    strcpy(files[index].filename, curr->d_name);
                    index++;
                    *email_count += 1;
                }
            }
        }
        
        closedir(dirp);
    }
    return files;
}

size_t count_files_in_dir(DIR *dirp) {
    size_t total = 0;
    struct dirent *curr;
    while ((curr = readdir(dirp)) != NULL) {
        if (!(strcmp(curr->d_name, "..") == 0 || strcmp(curr->d_name, ".") == 0)) {
            total += 1;
        }
    }
    // go back to the beginning of the file list
    rewinddir(dirp);
    return total;
}


char *join_path(const char *dir1, const char *dir2) {
    int dir1_length = strlen(dir1);
    int dir2_length = strlen(dir2);

    char *total = malloc(dir1_length + dir2_length + 2);
    strcpy(total, dir1);
    total[dir1_length] = PATH_SEPARATOR;
    strcpy(total + dir1_length + 1, dir2);
    return total;
}