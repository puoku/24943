#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int i;
    if (argc == 1) {        /* если аргументов нет – работаем с текущим каталогом */
        argc = 2;
        argv[1] = ".";
    }

    for (i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) != 0) {   /* не смогли получить stat – просто пишем ошибку */
            perror(argv[i]);
            continue;
        }

        /* тип файла + права доступа */
        char m[11];
        if (S_ISDIR(st.st_mode))
            m[0] = 'd';
        else if (S_ISREG(st.st_mode))
            m[0] = '-';
        else
            m[0] = '?';

        m[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        m[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        m[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        m[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        m[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        m[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        m[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
        m[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        m[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        m[10] = '\0';

        /* владелец и группа */
        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);

        /* время модификации */
        char tbuf[20];
        struct tm *tm = localtime(&st.st_mtime);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", tm);

        /* только имя файла без пути */
        char *name = argv[i];
        char *p = strrchr(name, '/');
        if (p) name = p + 1;

        printf("%s %3lu %-8s %-8s ",
               m,
               (unsigned long)st.st_nlink,
               pw ? pw->pw_name : "?",
               gr ? gr->gr_name : "?");

        if (S_ISREG(st.st_mode))
            printf("%10lld ", (long long)st.st_size);
        else
            printf("           ");   /* пустое поле размера, 11 пробелов */

        printf("%s %s\n", tbuf, name);
    }

    return 0;
}
