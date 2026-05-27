#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 4096

FILE *create_file_in_dir(const char *dirpath, const char *filename)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename);
    return fopen(path, "w");
}

void print_environ(pid_t pid, const char *dirpath)
{
    char path[256], buf[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "environ.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    fprintf(out, "--- environ ---\n");

    size_t len;
    while ((len = fread(buf, 1, BUF_SIZE, f)) > 0)
    {
        for (size_t i = 0; i < len; i++)
        {
            if (buf[i] == '\0')
                buf[i] = '\n';
        }
        fwrite(buf, 1, len, out);
    }
    fprintf(out, "\n");
    fclose(out);
    fclose(f);
}

void print_stat_and_extract(pid_t pid, const char *dirpath, uint64_t *vsize,
                            uint64_t *startstack)
{
    char path[256], buf[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "stat.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    if (fgets(buf, BUF_SIZE, f))
    {
        char *rparen = strrchr(buf, ')');
        if (rparen)
        {
            char state;
            char *p = rparen + 2;
            sscanf(p, "%c", &state);
            fprintf(out, "--- Состояние ---\n");
            fprintf(out, "Состояние процесса: %c\n", state);

            sscanf(p,
                   "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d "
                   "%*d %*u %*u "
                   "%" SCNu64 " %*u %*u %*u %*u "
                   "%" SCNu64,
                   vsize, startstack);
        }
    }
    fclose(out);
    fclose(f);
}

void print_full_stat(pid_t pid, const char *dirpath)
{
    char path[256], buf[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "stat_full.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    fprintf(out, "--- stat ---\n\n");

    if (fgets(buf, BUF_SIZE, f))
    {
        char *lp = strchr(buf, '(');
        char *rp = strrchr(buf, ')');

        if (lp && rp)
        {
            *lp = '\0';
            *rp = '\0';

            const char *fields[] = {
                "1) pid",          "2) comm",         "3) state",
                "4) ppid",         "5) pgrp",         "6) session",
                "7) tty_nr",       "8) tpgid",        "9) flags",
                "10) minflt",      "11) cminflt",     "12) majflt",
                "13) cmajflt",     "14) utime",       "15) stime",
                "16) cutime",      "17) cstime",      "18) priority",
                "19) nice",        "20) num_threads", "21) itrealvalue",
                "22) starttime",   "23) vsize",       "24) rss",
                "25) rsslim",      "26) startcode",   "27) endcode",
                "28) startstack",  "29) kstkesp",     "30) kstkeip",
                "31) signal",      "32) blocked",     "33) sigignore",
                "34) sigcatch",    "35) wchan",       "36) nswap",
                "37) cnswap",      "38) exit_signal", "39) processor",
                "40) rt_priority", "41) policy",      "42) delayacct_blkio_ticks",
                "43) guest_time",  "44) cguest_time", "45) start_data",
                "46) end_data",    "47) start_brk",   "48) arg_start",
                "49) arg_end",     "50) env_start",   "51) env_end",
                "52) exit_code"
            };

            int pid_val;
            sscanf(buf, "%d", &pid_val);
            fprintf(out, "%-25s : %d\n", fields[0], pid_val);
            fprintf(out, "%-25s : %s\n", fields[1], lp + 1);

            char *rest = rp + 2;
            char *token = strtok(rest, " \n");
            int idx = 2;

            while (token != NULL && idx < 52)
            {
                int field_num = idx + 1;

                if ((field_num >= 26 && field_num <= 30)
                    || (field_num >= 45 && field_num <= 51))
                {
                    uint64_t addr = strtoull(token, NULL, 10);
                    fprintf(out, "%-25s : 0x%" PRIx64 "\n", fields[idx], addr);
                }
                else
                {
                    fprintf(out, "%-25s : %s\n", fields[idx], token);
                }

                token = strtok(NULL, " \n");
                idx++;
            }
        }
    }

    fclose(out);
    fclose(f);
}

void print_cmdline(pid_t pid, const char *dirpath)
{
    char path[256], buf[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "cmdline.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    fprintf(out, "--- cmdline ---\n");
    printf("\n[PID %d] Командная строка (cmdline):\n", pid);
    size_t len = fread(buf, 1, BUF_SIZE - 1, f);
    if (len > 0)
    {
        for (size_t i = 0; i < len; i++)
        {
            if (buf[i] == '\0')
                buf[i] = ' ';
        }
        buf[len] = '\0';
        fprintf(out, "%s\n", buf);
        printf("%s\n", buf);
    }
    fclose(out);
    fclose(f);
}

void print_fd(pid_t pid, const char *dirpath)
{
    char path[256], target[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    DIR *d = opendir(path);
    if (!d)
        return;

    FILE *out = create_file_in_dir(dirpath, "fd.txt");
    if (!out)
    {
        closedir(d);
        return;
    }

    fprintf(out, "--- fd ---\n");
    printf("[PID %d] Файловые дескрипторы (fd):\n", pid);

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
        {
            char linkpath[512];
            snprintf(linkpath, sizeof(linkpath), "%s/%s", path, dir->d_name);
            ssize_t len = readlink(linkpath, target, sizeof(target) - 1);
            if (len != -1)
            {
                target[len] = '\0';
                fprintf(out, "fd %s -> %s\n", dir->d_name, target);
                printf("fd %s -> %s\n", dir->d_name, target);
            }
        }
    }
    fclose(out);
    closedir(d);
}

void print_symlink(pid_t pid, const char *name, const char *dirpath)
{
    char path[256], target[512], filename[64];
    snprintf(path, sizeof(path), "/proc/%d/%s", pid, name);
    snprintf(filename, sizeof(filename), "%s.txt", name);

    FILE *out = create_file_in_dir(dirpath, filename);
    if (!out)
        return;

    ssize_t len = readlink(path, target, sizeof(target) - 1);

    fprintf(out, "--- %s ---\n", name);
    if (len != -1)
    {
        target[len] = '\0';
        fprintf(out, "%s -> %s\n", name, target);
    }
    else
    {
        fprintf(out, "Нет доступа или удалено.\n");
    }
    fclose(out);
}

void print_io(pid_t pid, const char *dirpath)
{
    char path[256], buf[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "io.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    fprintf(out, "--- io ---\n");
    while (fgets(buf, sizeof(buf), f))
    {
        fprintf(out, "%s", buf);
    }
    fclose(out);
    fclose(f);
}

void print_comm(pid_t pid, const char *dirpath)
{
    char path[256], buf[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return;

    FILE *out = create_file_in_dir(dirpath, "comm.txt");
    if (!out)
    {
        fclose(f);
        return;
    }

    fprintf(out, "--- comm ---\n");
    if (fgets(buf, sizeof(buf), f))
    {
        fprintf(out, "%s", buf);
    }
    fclose(out);
    fclose(f);
}

void print_task(pid_t pid, const char *dirpath)
{
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task", pid);
    DIR *d = opendir(path);
    if (!d)
        return;

    FILE *out = create_file_in_dir(dirpath, "task.txt");
    if (!out)
    {
        closedir(d);
        return;
    }

    fprintf(out, "--- task ---\n");
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
        {
            fprintf(out, "TID: %s\n", dir->d_name);
        }
    }
    fclose(out);
    closedir(d);
}

void process_maps_and_pagemap(pid_t pid, const char *dirpath, uint64_t *calculated_vsize)
{
    char path[256], pm_path[256], line[BUF_SIZE];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    snprintf(pm_path, sizeof(pm_path), "/proc/%d/pagemap", pid);

    FILE *maps = fopen(path, "r");
    int pm_fd = open(pm_path, O_RDONLY);
    if (!maps)
        return;

    FILE *out_maps = create_file_in_dir(dirpath, "maps.txt");
    FILE *out_pagemap = create_file_in_dir(dirpath, "pagemap.txt");

    if (out_maps)
    {
        fprintf(out_maps, "--- maps ---\n");
        fprintf(out_maps, "%-48s %-12s %s\n", "Адрес (Диапазон)", "Страницы",
                "Права, смещение, dev, inode, путь");
    }

    if (out_pagemap)
    {
        if (pm_fd < 0)
        {
            fprintf(out_pagemap,
                    "Ошибка: Не удалось открыть pagemap (требуются права sudo).\n");
        }
        else
        {
            fprintf(out_pagemap, "--- pagemap ---\n");
            fprintf(out_pagemap, "Адрес            : pfn              soft-dirty "
                                 "file/shared swapped present\n");
        }
    }

    *calculated_vsize = 0;
    long page_size = sysconf(_SC_PAGE_SIZE);

    while (fgets(line, sizeof(line), maps))
    {
        uint64_t start, end;
        if (sscanf(line, "%" SCNx64 "-%" SCNx64, &start, &end) == 2)
        {
            *calculated_vsize += (end - start);

            if (out_maps)
            {
                uint64_t pages_count = (end - start) / page_size;

                char *first_space = strchr(line, ' ');
                if (first_space)
                {
                    *first_space = '\0';
                    fprintf(out_maps, "%-35s %-8" PRIu64 " %s", line, pages_count,
                            first_space + 1);
                }
                else
                {
                    fprintf(out_maps, "%s", line);
                }
            }

            if (out_pagemap && pm_fd >= 0)
            {
                for (uint64_t i = start; i < end; i += page_size)
                {
                    uint64_t offset = (i / page_size) * sizeof(uint64_t);
                    uint64_t data;

                    if (pread(pm_fd, &data, sizeof(data), offset) == sizeof(data))
                    {
                        fprintf(out_pagemap,
                                "0x%-14" PRIx64 " : %-16" PRIx64 " %-10" PRId64
                                " %-11" PRId64 " %-7" PRId64 " %-7" PRId64 "\n",
                                i, data & (((uint64_t)1 << 55) - 1), (data >> 55) & 1,
                                (data >> 61) & 1, (data >> 62) & 1, (data >> 63) & 1);
                    }
                }
            }
        }
    }

    if (out_maps)
        fclose(out_maps);
    if (out_pagemap)
        fclose(out_pagemap);
    fclose(maps);
    if (pm_fd >= 0)
        close(pm_fd);
}

void analyze_process(pid_t pid, const char *dirpath)
{
    print_environ(pid, dirpath);

    uint64_t stat_vsize = 0;
    uint64_t startstack = 0;
    print_stat_and_extract(pid, dirpath, &stat_vsize, &startstack);
    print_full_stat(pid, dirpath);

    print_cmdline(pid, dirpath);
    print_fd(pid, dirpath);
    print_symlink(pid, "cwd", dirpath);
    print_symlink(pid, "exe", dirpath);
    print_symlink(pid, "root", dirpath);

    uint64_t maps_vsize = 0;
    process_maps_and_pagemap(pid, dirpath, &maps_vsize);

    print_io(pid, dirpath);
    print_comm(pid, dirpath);
    print_task(pid, dirpath);

    FILE *out_calc = create_file_in_dir(dirpath, "calculations.txt");
    if (out_calc)
    {
        fprintf(out_calc, "--- Дополнительные вычисления ---\n");

        double stack_tb = (double)startstack / (1024.0 * 1024.0 * 1024.0 * 1024.0);
        fprintf(out_calc, "Низ стека (startstack) в десятичном виде: %" PRIu64 " байт\n",
                startstack);
        fprintf(out_calc, "Низ стека (startstack) в Терабайтах: %.6f ТБ\n\n", stack_tb);

        fprintf(out_calc, "Виртуальная память (vsize из stat): %" PRIu64 " байт\n",
                stat_vsize);
        fprintf(out_calc, "Сумма регионов (размер из maps) : %" PRIu64 " байт\n",
                maps_vsize);

        fclose(out_calc);
    }
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("Использование: %s <pid1> <pid2> <pid3>\n", argv[0]);
        return 1;
    }

    pid_t pids[3];

    for (int i = 0; i < 3; i++)
    {
        char *endptr;
        errno = 0;
        long val = strtol(argv[i + 1], &endptr, 10);

        if (endptr == argv[i + 1])
        {
            printf("Ошибка: '%s' не является числом.\n", argv[i + 1]);
            return EXIT_FAILURE;
        }
        if (*endptr != '\0')
        {
            printf("Ошибка: '%s' содержит недопустимые символы.\n", argv[i + 1]);
            return EXIT_FAILURE;
        }
        if (val <= 0)
        {
            printf("Ошибка: PID должен быть > 0.\n");
            return EXIT_FAILURE;
        }

        pids[i] = (pid_t)val;
    }

    printf("Программа начала работу. Будут созданы каталоги pid_<PID>.\n");

    for (int i = 0; i < 3; i++)
    {
        char dirpath[256];
        snprintf(dirpath, sizeof(dirpath), "pid_%d", pids[i]);

        if (mkdir(dirpath, 0755) == -1 && errno != EEXIST)
        {
            perror("Ошибка при создании каталога");
            continue;
        }

        // kill(pids[i], SIGSTOP);
        analyze_process(pids[i], dirpath);
        kill(pids[i], SIGKILL);
        waitpid(pids[i], NULL, 0);
    }

    printf("Работа завершена. Проверьте созданные папки.\n");
    return 0;
}
