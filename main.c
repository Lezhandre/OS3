#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

#include <linux/limits.h>
#include <ncurses.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>

#define D_STR 80    // изменение выделения памяти под строку
#define D_ARG 5     // изменение выделения памяти под указатели

int id;
int bash_hist_desc;

// проверка выделения памяти
void check_mem(void * ptr){
    if (ptr == NULL) {
        perror("Memory problem");
        endwin();
        exit(0);
    }
}

void signal_child(int sig){
    int err = kill(id, sig); // посылание какого-то сигнала в дочерний процесс
    (void) signal(SIGINT, SIG_IGN);
}

int main(){
    char cwd[PATH_MAX], minibuf[1];
    int y = 0;
    getcwd(cwd, PATH_MAX);
    bash_hist_desc = open("./my_history", O_RDWR | O_CREAT | O_APPEND); // надо исправить адрес
    if(bash_hist_desc < 0) {
        printf("Could not open history file\n");
        //return -1;
    }
    while (true) {
        printf("%s$ ", cwd);
        char ** argv = NULL;
        int cur_arg_num;
        int len;
        char cur_str[BUFSIZ];
        // цикл для получения с клавиатуры комманды/программы и её параметров
        //scanf("%s", cur_str);
        fgets(cur_str, sizeof(cur_str), stdin);
        cur_str[strlen(cur_str) - 1] = 0;
        char delim[] = " ";
        size_t i = 0;
        argv = (char**)malloc(D_ARG * sizeof(char *));
        check_mem((void *)argv);
        cur_arg_num = D_ARG;
        len = strlen(cur_str);
        argv[i] = strtok(cur_str, delim);
        struct termios oldt, newt;
        int ch;
        tcgetattr( STDIN_FILENO, &oldt );
        newt = oldt;
        newt.c_lflag &= ~( ICANON | ECHO );
        tcsetattr( STDIN_FILENO, TCSANOW, &newt );
        while (argv[i] != NULL) {
            ++i;
            if (i == cur_arg_num) {
                cur_arg_num += D_ARG;
                argv = (char **)realloc(argv, cur_arg_num * sizeof(char *));
                check_mem((void *)argv);
            }
            argv[i] = strtok(NULL, delim);
        }
        cur_arg_num = i;
        if(!argv[0]) continue;
        else if(strcmp(argv[0], "\033[A") == 0){
            char minibuf[1];
            ssize_t read_num, in = 0;
            while ((read_num = lseek(bash_hist_desc, -2, SEEK_CUR)) >= 0) {
                read_num = read(bash_hist_desc, minibuf, 1);
                if (minibuf[0] == '\n') break;
                ++in;
            }
            if (read_num < 0)
                lseek(bash_hist_desc, 0, SEEK_SET);
            if (in > 0) { // если что-то прочиталось
                pread(bash_hist_desc, cur_str, in, lseek(bash_hist_desc, 0, SEEK_CUR));
                cur_str[in] = 0;
                printf("\r%s$ %s\n", cwd, cur_str);
            }
        }
        else if(strcmp(argv[0], "\033[B") == 0){
            char minibuf[1];
            size_t in = 0, read_num;
            if (read(bash_hist_desc, minibuf, 1) > 0 && minibuf[0] != '\n') {
                ++in;
                while (read(bash_hist_desc, minibuf, 1) > 0 && minibuf[0] != '\n') ++in;
                if (wdeleteln(stdscr) == ERR) return -1;
                refresh();
                pread(bash_hist_desc, cur_str, in, lseek(bash_hist_desc, 0, SEEK_CUR) - in - 1);
                cur_str[in] = 0;
                printf("\r%s$ %s\n", cwd, cur_str);
                i = in;
            }
        }
        else if (strcmp(argv[0], "exit") == 0) {
            if (cur_arg_num > 1) {
                printf("Too many arguments!\n");
            }
            else {
                //free(cur_str);
                free(argv);
                break;
            }
        }
        else if (strcmp(argv[0], "cd") == 0) {
            if (cur_arg_num != 2) {
                printf("Wrong number of arguments!\n");
            }
            else {
                int err = chdir(argv[1]);
                getcwd(cwd, PATH_MAX);
                if (err == -1) {
                    printf("Can't go to this directory: %s\n", strerror(errno));
                }
            }
        }
        else { // разделение терминала на две программы
            id = fork();
            switch (id)
            {
            case -1:
                printf("Critical error!!!\n");
                return 0;
            case 0: // дочерний процесс
                if(execvp(*argv, argv) == -1) exit(EXIT_FAILURE);
            default: // процесс-родитель
                { // field of view (like pov)
                signal(SIGINT, signal_child);
                char buf[BUFSIZ] = {0};
                int lenin;
                int st;
                wait(&st);
                if (!WIFEXITED(st)){
                    // добавление в историю
                    if (lseek(bash_hist_desc, -2, SEEK_END) < 0)
                        lseek(bash_hist_desc, 0, SEEK_END);
                    bool new = true;
                    for (size_t i = 0; read(bash_hist_desc, minibuf, 1) > 0 && *minibuf != '\n'; ++i) {
                        lseek(bash_hist_desc, -2, SEEK_CUR);
                        if (len < 1 + i || *minibuf != cur_str[len - 1 - i] && !(!cur_str[len - 1 - i] && *minibuf == ' ')) {
                            new = false;
                            break;
                        }
                    }
                    if (new) {
                        lseek(bash_hist_desc, 0, SEEK_END);
                        for (size_t i = 0; argv[i]; ++i) {
                            write(bash_hist_desc, argv[i], strlen(argv[i]));
                            if (i + 1 < cur_arg_num) write(bash_hist_desc, " ", 1);
                        }
                        write(bash_hist_desc, "\n", 1);
                    }
                }
                else printf("%s\n", strerror(WEXITSTATUS(st)));
                }
            }
        }
        // очистка памяти
        free(argv);
        //free(cur_str);
    }
    close(bash_hist_desc);
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return 0;
}