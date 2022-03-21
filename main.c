#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include <linux/limits.h>
#include <ncurses.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/wait.h>

#define D_STR 80    // изменение выделения памяти под строку
#define D_ARG 5     // изменение выделения памяти под указатели

WINDOW * wnd;
int child_id;

// функция для считывания строки без пробелов и переносов
// при этом перенос строки сохраняется
int mygetstr(char * str, int k, int n){
    int c;
    for (int i = k; i < n; ++i) {
        c = getch();
        switch (c)
        {
        case '\r':
            if (ungetch('\r') == ERR) return -1;
            str[i] = 0;
            return i;
        case ' ':
            if (i == 0) {
                --i;
                break;
            }
            else {
                str[i] = 0;
                if (ungetch(' ') == ERR) return -1;
                return i;
            }
        case KEY_BACKSPACE:
            if (i > 0) {
                if (delch() == TRUE) return -1;
                i -= 2;
            }
            else {
                int x, y;
                getyx(wnd, y, x);
                move(y, x + 1);
                --i;
            }
            break;
        case -1:
            return -1;
        default:
            str[i] = c;
            break;
        }
    }
    str[n] = 0;
    return n + 1;
}

// проверка выделения памяти
void check_mem(void * ptr){
    if (ptr == NULL) {
        perror("Memory problem");
        endwin();
        exit(0);
    }
}

void kill_child(int sig){
    int err = kill(child_id, sig); // посылание какого-то сигнала в дочерний процесс
    if (err > 0)
        putp("Child process died\r");
    (void) signal(SIGINT, SIG_IGN);
}

int main(){
    wnd = initscr();
    nonl();
    scrollok(wnd, true); // для пролистывания (не работает)
    init_pair(1, COLOR_BLACK, COLOR_GREEN); // скорее всего не работает (видимо цвет меняется только в настройках окна)
    keypad(wnd, true);  // нужно для включения особых клавиш (надо самостоятельно запрограммировать поведение)
    char cwd[PATH_MAX];
    int y = 0;
    while (true) {
        //move(y, 0);
        attrset(A_BOLD);
        //printw("\r");
        printw(getcwd(cwd, PATH_MAX));
        printw("$ ");
        attroff(A_BOLD);
        ++y;
        refresh();
        char ** argv = NULL;
        int curr_arg_num = 0;
        int mem_slots = 0, len;
        char * cur_str;
        // цикл для получения с клавиатуры комманды/программы и её параметров
        do {
            if (curr_arg_num == mem_slots) {
                mem_slots += D_ARG;
                argv = (char **)realloc(argv, mem_slots);
                check_mem((void *)argv);
            }
            cur_str = NULL;
            len = 1;
            int check;
            do {
                len += D_STR;
                cur_str = (char *)realloc(cur_str, len);
                check_mem((void *)cur_str);
                if ((check = mygetstr(cur_str, len - D_STR - 1, D_STR)) == -1) {
                    putp("Scan-error\n");
                    endwin();
                    return 0;
                }
            } while (check > D_STR);
            argv[curr_arg_num] = cur_str;
            ++curr_arg_num;
        } while (getch() != '\r');
        putp("\r");
        refresh();
        putp("\n");
        refresh();
        if (curr_arg_num == mem_slots) {
            ++mem_slots;
            argv = (char **)realloc(argv, mem_slots);
            check_mem((void *)argv);
        }
        argv[curr_arg_num] = NULL;
        if (curr_arg_num == 1 && strcmp(argv[0], "exit") == 0) {
            for (int i = 0; i < curr_arg_num; ++i) free(argv[i]);
            free(argv);
            break;
        }
        else if (curr_arg_num == 2 && strcmp(argv[0], "cd") == 0) {
            int err = chdir(argv[1]);
            if (err == -1) {
                //move(y, 0);
                refresh();
                perror("Can't go to this directory: ");
                printw("\r\n");
                refresh();
            }
        }
        else { // разделение терминала на две программы
            child_id = fork();
            int err;
            switch (child_id)
            {
            case -1:
                printw("Critical error!!!\r");
                return 0;
            case 0: // дочерний процесс
                //move(y, 0);
                refresh();
                err = execvp(*argv, argv);
                if (err == -1) {
                    printw("Something is going wrong!!!\r");
                    printw("\n");
                    ++y;
                    refresh();
                }
                return 0;
            default: // процесс-родитель
                { // field of view
                int* stat_loc = (int*)malloc(sizeof(int));
                (void) signal(SIGINT, kill_child);
                wait(stat_loc);
                free(stat_loc);
                break;
                }
            }
            //move(y, 0);
            //refresh();
        }
        // очистка памяти
        for (int i = 0; i < curr_arg_num; ++i) free(argv[i]);
        free(argv);
    }
    endwin();
    return 0;
}