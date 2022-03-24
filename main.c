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

#define D_STR 80    // изменение выделения памяти под строку
#define D_ARG 5     // изменение выделения памяти под указатели

WINDOW * wnd;
int child_id;
int bash_hist_desc;

// функция для считывания строки без пробелов и переносов
// при этом перенос строки сохраняется
int mygetstr(char * str, int * len){
    chtype c;
    bool change;
    int i;
    char cwd[PATH_MAX] = { 0 };
    getcwd(cwd, PATH_MAX);
    for (i = *len - (D_STR + 1); i < *len; ++i) {
        c = getch();
        switch (c)
        {
        case '\n':
            if (ungetch('\n') == ERR) return -1;
            str[i] = 0;
            return i;
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
        case KEY_UP:
            {
            char minibuf[2];
            ssize_t read_num, in = 0;
            //lseek(bash_hist_desc, , SEEK_CUR);
            while ((read_num = lseek(bash_hist_desc, -2, SEEK_CUR)) >= 0) {
                read_num = read(bash_hist_desc, minibuf, 1);
                if (minibuf[0] == '\n') break;
                ++in;
            }
            if (read_num < 0)
                lseek(bash_hist_desc, 0, SEEK_SET);
            if (in > 0) { //if smth was actually read
                if (wdeleteln(stdscr) == ERR) return -1;
                str = (char *)realloc(str, in + 1);
                pread(bash_hist_desc, str, in, lseek(bash_hist_desc, 0, SEEK_CUR));
                *len = in;
                str[in] = 0;
                printw("\r%s", cwd);
                refresh();
                printw("$ %s", str);
                refresh();
                i = *len;
            }
            else --i;
            }
            break;
        case KEY_DOWN:
            {
            char minibuf[2];
            size_t in = 0, read_num;
            if (read(bash_hist_desc, minibuf, 1) > 0 && minibuf[0] != '\n') {
                ++in;
                while (read(bash_hist_desc, minibuf, 1) > 0 && minibuf[0] != '\n')
                    ++in;
                char cwd[PATH_MAX];
                if (wdeleteln(stdscr) == ERR) return -1;
                str = (char *)realloc(str, in + 1);
                pread(bash_hist_desc, str, in, lseek(bash_hist_desc, -in - 1, SEEK_CUR));
                lseek(bash_hist_desc, +in + 1, SEEK_CUR);
                *len = in;
                str[in] = 0;
                printw("\r%s", cwd);
                refresh();
                printw("$ %s", str);
                refresh();
                refresh();
                i = *len;
            }
            else --i;
            }
            break;
        default:
            str[i] = c;
            break;
        }
    }
    str[i] = 0;
    return i + 1;
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
    scrollok(wnd, true); // для ультрамегасупердуперпролистывания (чтобы вывод ещё больше не портился)
    init_pair(1, COLOR_BLACK, COLOR_GREEN); // скорее всего не работает (видимо цвет меняется только в настройках окна)
    keypad(wnd, true);  // нужно для включения особых клавиш (надо самостоятельно запрограммировать поведение)
    char cwd[PATH_MAX], minibuf[1], buf[BUFSIZ];
    int y = 0;
    bash_hist_desc = open("./my_history", O_RDWR | O_CREAT | O_APPEND); // надо исправить адрес
    while (true) {
        buf[0] = 0;
        move(y, 0);
        attrset(A_BOLD);
        printw(getcwd(cwd, PATH_MAX));
        printw("$ ");
        attroff(A_BOLD);
        ++y;
        refresh();
        char ** argv = NULL;
        int cur_arg_num;
        int len;
        char * cur_str;
        // цикл для получения с клавиатуры комманды/программы и её параметров
        len = lseek(bash_hist_desc, -1, SEEK_END);
        if (len == -1) {
            printw(strerror(errno));
            refresh();
        }
        do {
            cur_str = NULL;
            len = 1;
            int check;
            do {
                len += D_STR;
                cur_str = (char *)realloc(cur_str, len);
                check_mem((void *)cur_str);
                if ((check = mygetstr(cur_str, &len)) == -1) {
                    putp("Scan-error\n");
                    endwin();
                    return 0;
                }
            } while (check > D_STR);
        } while (getch() != '\n');
        char delim[] = " ";
        size_t i = 0;
        argv = (char**)malloc(D_ARG * sizeof(char *));
        check_mem((void *)argv);
        cur_arg_num = D_ARG;
        argv[i] = strtok(cur_str, delim);
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
        lseek(bash_hist_desc, 0, SEEK_SET);
        bool new = true, same = true;
        while (read(bash_hist_desc, minibuf, 1) > 0) {
            if (strcmp(minibuf, " ") == 0) {
                if (argv[i] == NULL) continue;
                if (strcmp(argv[i], buf) != 0) same = false;
                else ++i;
            } 
            else if (strcmp(minibuf, "\n") == 0) {
                if (same) {
                    new = false;
                    break;
                }
                buf[0] = 0;
                i = 0;
            }
            else {
                if (argv[i] == NULL) continue;
                strcat(buf, minibuf);
            }
        }
        if (new) {
            lseek(bash_hist_desc, 0, SEEK_END);
            for (size_t i = 0; argv[i]; ++i) {
                write(bash_hist_desc, argv[i], strlen(argv[i]));
                write(bash_hist_desc, " ", 1);
            }
            write(bash_hist_desc, "\n", 1);
        }
        putp("\r");
        refresh();
        putp("\n");
        refresh();
        if (strcmp(argv[0], "exit") == 0) {
            if (cur_arg_num > 1) {
                printw("Too many arguments!");
                refresh();
            }
            else {
                free(cur_str);
                free(argv);
                break;
            }
        }
        else if (strcmp(argv[0], "cd") == 0) {
            if (cur_arg_num != 2) {
                printw("Wrong number of arguments!");
                refresh();
            }
            else {
                int err = chdir(argv[1]);
                if (err == -1) {
                    printw("Can't go to this  directory: ");
                    printw(strerror(errno));
                    refresh();
                    y = getcury(wnd) + 1;
                }
            }
        } 
        else { // разделение терминала на две программы
            int file_desc[2];
            if (pipe(file_desc) == -1) {
                printw("Pipe failure\r");
                refresh();
                return 0;
            }
            child_id = fork();
            switch (child_id)
            {
            case -1:
                printw("Critical error!!!\r");
                refresh();
                return 0;
            case 0: // дочерний процесс
                close(file_desc[0]);
                dup2(file_desc[1], STDOUT_FILENO);
                dup2(file_desc[1], STDERR_FILENO);
                return execvp(*argv, argv);
            default: // процесс-родитель
                { // field of view
                close(file_desc[1]);
                (void) signal(SIGINT, kill_child);
                wait(NULL);
                char buf[BUFSIZ];
                int len;
                while ((len = read(file_desc[0], buf, BUFSIZ)) > 0) {
                    printw("%*s", len, buf);
                    refresh();
                }
                if (len < 0) {
                    printw("Reading problem!!!");
                    refresh();
                    return 0;
                }
                int tmp_y = getcury(wnd);
                if (tmp_y > y)
                    y = tmp_y;
                else if (tmp_y == y)
                    ++y;
                else
                    printw("Could not find command!");
                }
            }
        }
        // очистка памяти
        free(argv);
        free(cur_str);
    }
    close(bash_hist_desc);
    endwin();
    return 0;
}