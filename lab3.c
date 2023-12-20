#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char* src;
    char* dest;
} Data;

void data_destructor(Data* data) {
	free(data->src);
	free(data->dest);
	free(data);
}

void* copy_file(void* arg) {
    Data* data = (Data*) arg;
    printf("\ntid[%d] start copy %s to %s\n", gettid(), data->src, data->dest);

    int src_fd = open(data->src, O_RDONLY);
    if (src_fd < 0) {
        printf("Failed open src file\n");
        data_destructor(data);
        return NULL;
    }

    int dest_fd = open(data->dest, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (dest_fd < 0) {
        printf("Failed open dest file\n");
        close(src_fd);
        data_destructor(data);
        return NULL;
    }
    
	char buffer[1024];
	ssize_t bytes_read;
	
    while ((bytes_read = read(src_fd, buffer, 1024)) > 0) {
        if (bytes_read != write(dest_fd, buffer, bytes_read)) {
            printf("Failed write to dest file\n");
            
            close(src_fd);
            close(dest_fd);
            data_destructor(data);
           
            return NULL;
        }
    }

    close(src_fd);
    close(dest_fd);
    data_destructor(data);
    return NULL;
}

Data* create_new_data(char* src, char* dest) {
	Data* new_data = (Data*) malloc(sizeof(Data));
    new_data->src = strdup(src);
    new_data->dest = strdup(dest);
	return new_data;
}

void* copy_dir(void* arg) {
    Data* data = (Data*) arg;
	
	DIR* dir = opendir(data->src);
    if (!dir) {
        printf("Failed open src dir: %s\n", data->src);
        data_destructor(data);
        return NULL;
    }

    if (mkdir(data->dest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        printf("Failed creat dest dir: %s\n", data->dest);
        data_destructor(data);
        return NULL;
    }

    pthread_t threadArr[1024]; //Ограниченное количество файлов в директории 
    int countThread = 0; // сдвиг в массиве потоков, для создания

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char new_src[PATH_MAX], new_dest[PATH_MAX];
        snprintf(new_src, PATH_MAX, "%s/%s", data->src, entry->d_name);
        snprintf(new_dest, PATH_MAX, "%s/%s", data->dest, entry->d_name);

        struct stat src_stat;
        if (stat(new_src, &src_stat) == -1) {
            printf("Failed getting file status: %s\n", new_src);
            continue;
        }

        if (S_ISDIR(src_stat.st_mode)) {
            Data* new_data = create_new_data(new_src, new_dest);
            pthread_create(threadArr + countThread, NULL, copy_dir, new_data);
            ++countThread;
            
        } else if (S_ISREG(src_stat.st_mode)) {
          	Data* new_data = create_new_data(new_src, new_dest);
            pthread_create(threadArr + countThread, NULL, copy_file, new_data);
            ++countThread;
        }
    }

    closedir(dir);
    for (int threadP = 0; threadP < countThread; ++threadP) {
        pthread_join(threadArr[threadP], NULL);
    }
    
    data_destructor(data);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Bad arguments %s\n", argv[0]);
        return 1;
    }
    
	pthread_t thread;
    Data* data = (Data*) malloc(sizeof(Data)); //очистим в функции поточной
	data->src = strdup(argv[1]);			/// выделяется на куче и присваивается
    data->dest = strdup(argv[2]);

    pthread_create(&thread, NULL, copy_dir, data);
    pthread_join(thread, NULL);
    
    return 0;
}

