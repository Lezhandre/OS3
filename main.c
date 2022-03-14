#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include <linux/limits.h>
#include <ncurses.h>
#include <unistd.h>
#include <malloc.h>

#define D_STR 80    // изменение выделения памяти под строку
#define D_ARG 5     // изменение выделения памяти под указатели

WINDOW * wnd;

// функция для считывания строки без пробелов и переносов
// при этом перенос строки сохраняется
int mygetstr(char * str, int n){
    int c;
    char f = 0;
    for (int i = 0; i < n; ++i) {
        c = getch();
        switch (c)
        {
        case '\n':
            if (ungetch('\n') != OK) return -1;
            str[i] = 0;
            return ++i;
        case ' ':
            if (f) {
                str[i] = 0;
                return ++i;
            }
            else break;
        case -1:
            return -1;
        default:
            str[i] = c;
            f = 1;
            break;
        }
    }
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

int main()
{
    wnd = initscr();
    scrollok(wnd, true);
    init_pair(1, COLOR_BLACK, COLOR_GREEN); // скорее всего не работает (видимо цвет меняется только в настройках окна)
    keypad(wnd, true);  // нужно для включения особых  клавиш (надо самостоятельно запрограммировать поведение)
    char cwd[PATH_MAX];
    int y = 0, x = 0;
    while (true) {
        move(y, 0);
        attrset(A_BOLD | COLOR_PAIR(1));
        printw(getcwd(cwd, PATH_MAX));
        printw("$ ");
        attroff(A_BOLD);
        ++y;
        refresh();
        char ** argv = NULL;
        int i = 0, j = 1;
        int n = 0, len;
        char * cur_str;
        // цикл для получения с клавиатуры
        // комманды (программы) и её параметров
        do {
            if (i == n) {
                n += D_ARG;
                argv = (char **)realloc(argv, n);
                check_mem((void *)argv);
            }
            cur_str = NULL;
            len = 1;
            int check;
            do {
                ++j;
                len += D_STR;
                cur_str = (char *)realloc(cur_str, len);
                check_mem((void *)cur_str);
                if ((check = mygetstr(cur_str + len - D_STR - 1, D_STR)) == -1) {
                    putp("Scan-error\n");
                    endwin();
                    return 0;
                }
            } while (check > D_STR);
            argv[i] = cur_str;
            ++i;
        } while (getch() != '\n');
        if (i == n) {
            ++n;
            argv = (char **)realloc(argv, n);
            check_mem((void *)argv);
        }
        argv[i] = NULL;
        // разделение терминала
        // на две программы
        int id = fork(), err;
        switch (id)
        {
        case -1:
            putp("Critical error!!!\n");
            return 0;
        case 0: // запуск программы/комманды с аттрибутами
                // (проблемный код, в нём есть ошибки)
            err = execvp(*argv, argv + 1);
            wrefresh(wnd);
            if (err == -1) {
                move(y, 0);
                wprintw(wnd, "Something is going wrong!!!\r");
                refresh();
            }
            wrefresh(wnd);
            return 0;
        default:
            usleep(500); // костыль для приостановки основной программы, чтобы потомок успел вывести сообение об ошибке
            break;
        }
    }
    endwin();
    return 0;
}