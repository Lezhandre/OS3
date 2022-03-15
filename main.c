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
    for (int i = 0; i < n; ++i) {
        c = getch();
        switch (c)
        {
        case '\n':
            if (ungetch('\n') == ERR) return -1;
            str[i] = 0;
            return i;
        case ' ':
            if (i == 0) {
                --i;
                break;
            }
            else {
                str[i] = 0;
                return i;
            }
        case '\b':
            i -= 2;
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

// ls | grep CMake

int main()
{
    wnd = initscr();
    scrollok(wnd, true); // для пролистывания (не работает)
    init_pair(1, COLOR_BLACK, COLOR_GREEN); // скорее всего не работает (видимо цвет меняется только в настройках окна)
    keypad(wnd, true);  // нужно для включения особых  клавиш (надо самостоятельно запрограммировать поведение)
    char cwd[PATH_MAX];
    int y = 0, c = 0;
    while (true) {
        move(y, 0);
        attrset(A_BOLD);
        printw(getcwd(cwd, PATH_MAX));
        printw("$ ");
        attroff(A_BOLD);
        ++y;
        refresh();
        char ** argv = NULL;
        int curr_arg_num = 0;
        int mem_slots = 0, len;
        char * cur_str;
        c = ' ';
        // цикл для получения с клавиатуры комманды/программы и её параметров
        do {
            if (c != ' ') ungetch(c);
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
                if ((check = mygetstr(cur_str + len - D_STR - 1, D_STR)) == -1) {
                    putp("Scan-error\n");
                    endwin();
                    return 0;
                }
            } while (check > D_STR);
            argv[curr_arg_num] = cur_str;
            ++curr_arg_num;
        } while ((c = getch()) != '\n');
        if (curr_arg_num == mem_slots) {
            ++mem_slots;
            argv = (char **)realloc(argv, mem_slots);
            check_mem((void *)argv);
        }
        argv[curr_arg_num] = NULL;
        // разделение терминала на две программы
        int id = fork(), err;
        switch (id)
        {
        case -1:
            putp("Critical error!!!\n");
            return 0;
        case 0: // запуск программы/комманды с аттрибутами (проблемный код, в нём есть ошибки)
            err = execvp(*argv, argv);
            refresh();
            if (err == -1) {
                move(y, 0);
                printw("Something is going wrong!!!\r");
                refresh();
            }
            refresh();
            return 0;
        default:
            usleep(500); // костыль для приостановки основной программы, чтобы потомок успел вывести сообение об ошибке
            break;
        }
        // очистка памяти
        for (int i = 0; i < curr_arg_num; ++i) free(argv[i]);
        free(argv);
    }
    endwin();
    return 0;
}