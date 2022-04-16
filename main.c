#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include <linux/limits.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define D_STR 80    // изменение выделения памяти под строку
#define D_ARG 5     // изменение выделения памяти под указатели

int id;

// проверка выделения памяти
void check_mem(void * ptr){
    if (ptr == NULL) {
        perror("Memory problem");
        exit(0);
    }
}

void signal_child(int sig){
    printf("\n");
    kill(id, sig); // посылание какого-то сигнала в дочерний процесс
    (void) signal(SIGINT, SIG_IGN);
}

int main(){
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    while (true){
        printf("%s$ ", cwd);
        char** argv = NULL;
        int cur_arg_num;
        char cur_str[BUFSIZ];
        // цикл для получения с клавиатуры комманды/программы и её параметров
        //scanf("%s", cur_str);
        if(!fgets(cur_str, sizeof(cur_str), stdin)){
            exit(EXIT_SUCCESS);
        }
        cur_str[strlen(cur_str) - 1] = 0;
        char delim[] = " ";
        size_t i = 0;
        argv = (char**)malloc(D_ARG * sizeof(char *));
        check_mem((void*)argv);
        cur_arg_num = D_ARG;
        strlen(cur_str);
        argv[i] = strtok(cur_str, delim);
        while (argv[i] != NULL){
            ++i;
            if (i == cur_arg_num){
                cur_arg_num += D_ARG;
                argv = (char**)realloc(argv, cur_arg_num * sizeof(char *));
                check_mem((void*)argv);
            }
            argv[i] = strtok(NULL, delim);
        }
        cur_arg_num = i;
        if(!argv[0]) continue;
        else if (strcmp(argv[0], "exit") == 0){
            if (cur_arg_num > 1) {
                printf("Too many arguments!\n");
            }
            else{
                free(argv);
                break;
            }
        }
        else if (strcmp(argv[0], "cd") == 0){
            if (cur_arg_num != 2){
                printf("Wrong number of arguments!\n");
            }
            else {
                if (chdir(argv[1])){
                    printf("Can't go to this directory: %s\n", strerror(errno));
                }
                if (!getcwd(cwd, PATH_MAX)){
                    perror("getcwd");
                }
            }
        }
        else{ // разделение терминала на две программы
            id = fork();
            switch (id){
            case -1:
                printf("Critical error!!!\n");
                return 0;
            case 0: // дочерний процесс
                if(execvp(*argv, argv) == -1) exit(EXIT_FAILURE);
            default: // процесс-родитель
                { // field of view (like pov)
                signal(SIGINT, signal_child);
                char buf[BUFSIZ] = {0};
                int st;
                wait(&st);
                if (!WIFEXITED(st) /*&& strcmp(strerror(WEXITSTATUS(st)), "Success")*/) printf("%s\n", strerror(WEXITSTATUS(st)));
                }
            }
        }
        // очистка памяти
        free(argv);
    }
    return 0;
}