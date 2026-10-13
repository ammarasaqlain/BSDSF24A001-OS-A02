#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/ioctl.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

extern int errno;

void process_dir(const char *dir, int l_flag, int x_flag, int R_flag);
void print_vertical(const char *dir, char **files, int count, int max_len);
void print_horizontal(const char *dir, char **files, int count, int max_len);
void print_long(const char *dir, char **files, int count);
void print_long_format(const char *dir, const char *name);
void print_colored(const char *dir, const char *name, int col_width, int pad);

int compare_filenames(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

int main(int argc, char *argv[]) {
    int l_flag = 0, x_flag = 0, R_flag = 0, opt;
    
    // Add 'R' to the getopt string
    while ((opt = getopt(argc, argv, "lxR")) != -1) {
        if (opt == 'l') l_flag = 1;
        else if (opt == 'x') x_flag = 1;
        else if (opt == 'R') R_flag = 1;
    }
    
    if (optind == argc) {
        process_dir(".", l_flag, x_flag, R_flag);
    } else {
        for (int i = optind; i < argc; i++) {
            if (argc - optind > 1 || R_flag) printf("%s:\n", argv[i]);
            process_dir(argv[i], l_flag, x_flag, R_flag);
            if (i < argc - 1) puts("");
        }
    }
    return 0;
}

void process_dir(const char *dir, int l_flag, int x_flag, int R_flag) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        // Only print error if we can't open a top-level dir, suppress for unreadable subdirs
        return; 
    }

    int capacity = 128, count = 0, max_len = 0;
    char **files = malloc(capacity * sizeof(char *));
    errno = 0;
    
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        int len = strlen(entry->d_name);
        if (len > max_len) max_len = len;
        if (count >= capacity) {
            capacity *= 2;
            files = realloc(files, capacity * sizeof(char *));
        }
        files[count++] = strdup(entry->d_name);
    }
    closedir(dp);
    
    if (count == 0) {
        free(files);
        return;
    }

    qsort(files, count, sizeof(char *), compare_filenames);

    if (l_flag) print_long(dir, files, count);
    else if (x_flag) print_horizontal(dir, files, count, max_len);
    else print_vertical(dir, files, count, max_len);

    // Feature 7: The Recursive Step
    if (R_flag) {
        for (int i = 0; i < count; i++) {
            // Construct the full absolute path
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir, files[i]);
            
            struct stat st;
            // Check if the current file is actually a directory
            if (lstat(full_path, &st) != -1 && S_ISDIR(st.st_mode)) {
                printf("\n%s:\n", full_path);
                // The recursion: function calls itself with the new path
                process_dir(full_path, l_flag, x_flag, R_flag);
            }
        }
    }

    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
}

void print_colored(const char *dir, const char *name, int col_width, int pad) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);
    struct stat st;
    
    char *color = COLOR_RESET;
    if (lstat(full_path, &st) != -1) {
        if (S_ISDIR(st.st_mode)) color = COLOR_BLUE;
        else if (S_ISLNK(st.st_mode)) color = COLOR_PINK;
        else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) color = COLOR_REVERSE;
        else if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) color = COLOR_GREEN;
        else if (strstr(name, ".tar.gz") || strstr(name, ".zip") || strstr(name, ".tar")) color = COLOR_RED;
    }

    if (pad) {
        printf("%s%s%s", color, name, COLOR_RESET);
        for (int i = strlen(name); i < col_width; i++) putchar(' ');
    } else {
        printf("%s%s%s", color, name, COLOR_RESET);
    }
}

void print_vertical(const char *dir, char **files, int count, int max_len) {
    struct winsize w;
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) term_width = w.ws_col;
    int col_width = max_len + 2;
    int num_cols = term_width / col_width;
    if (num_cols == 0) num_cols = 1;
    int num_rows = (count + num_cols - 1) / num_cols;

    for (int r = 0; r < num_rows; r++) {
        for (int c = 0; c < num_cols; c++) {
            int index = r + (c * num_rows);
            if (index < count) print_colored(dir, files[index], col_width, 1);
        }
        printf("\n");
    }
}

void print_horizontal(const char *dir, char **files, int count, int max_len) {
    struct winsize w;
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) term_width = w.ws_col;
    int col_width = max_len + 2;
    int num_cols = term_width / col_width;
    if (num_cols == 0) num_cols = 1;

    int current_col = 0;
    for (int i = 0; i < count; i++) {
        print_colored(dir, files[i], col_width, 1);
        current_col++;
        if (current_col >= num_cols) {
            printf("\n");
            current_col = 0;
        }
    }
    if (current_col != 0) printf("\n");
}

void print_long(const char *dir, char **files, int count) {
    for (int i = 0; i < count; i++) print_long_format(dir, files[i]);
}

void print_long_format(const char *dir, const char *name) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);
    struct stat file_stat;
    if (lstat(full_path, &file_stat) == -1) return;

    char perms[11];
    strcpy(perms, "----------");
    if (S_ISDIR(file_stat.st_mode)) perms[0] = 'd';
    else if (S_ISLNK(file_stat.st_mode)) perms[0] = 'l';
    else if (S_ISCHR(file_stat.st_mode)) perms[0] = 'c';
    else if (S_ISBLK(file_stat.st_mode)) perms[0] = 'b';
    else if (S_ISFIFO(file_stat.st_mode)) perms[0] = 'p';
    else if (S_ISSOCK(file_stat.st_mode)) perms[0] = 's';

    if (file_stat.st_mode & S_IRUSR) perms[1] = 'r';
    if (file_stat.st_mode & S_IWUSR) perms[2] = 'w';
    if (file_stat.st_mode & S_IXUSR) perms[3] = 'x';
    if (file_stat.st_mode & S_ISUID) perms[3] = (file_stat.st_mode & S_IXUSR) ? 's' : 'S';
    if (file_stat.st_mode & S_IRGRP) perms[4] = 'r';
    if (file_stat.st_mode & S_IWGRP) perms[5] = 'w';
    if (file_stat.st_mode & S_IXGRP) perms[6] = 'x';
    if (file_stat.st_mode & S_ISGID) perms[6] = (file_stat.st_mode & S_IXGRP) ? 's' : 'S';
    if (file_stat.st_mode & S_IROTH) perms[7] = 'r';
    if (file_stat.st_mode & S_IWOTH) perms[8] = 'w';
    if (file_stat.st_mode & S_IXOTH) perms[9] = 'x';
    if (file_stat.st_mode & S_ISVTX) perms[9] = (file_stat.st_mode & S_IXOTH) ? 't' : 'T';

    struct passwd *pwd = getpwuid(file_stat.st_uid);
    struct group *grp = getgrgid(file_stat.st_gid);
    char *time_str = ctime(&file_stat.st_mtime);
    time_str[strlen(time_str) - 1] = '\0'; 

    printf("%s %4lu %-8s %-8s %8ld %.12s ", perms, (unsigned long)file_stat.st_nlink, 
           pwd ? pwd->pw_name : "UNKNOWN", grp ? grp->gr_name : "UNKNOWN", 
           (long)file_stat.st_size, time_str + 4);
    
    print_colored(dir, name, 0, 0);
    printf("\n");
}
