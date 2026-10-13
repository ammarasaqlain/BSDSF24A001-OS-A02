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

extern int errno;

void do_ls(const char *dir);
void do_ls_horizontal(const char *dir);
void do_ls_long(const char *dir);
void print_long_format(const char *name, struct stat *file_stat);

int main(int argc, char *argv[]) {
    int l_flag = 0;
    int x_flag = 0;
    int opt;

    // getopt now looks for both 'l' and 'x' flags
    while ((opt = getopt(argc, argv, "lx")) != -1) {
        if (opt == 'l') l_flag = 1;
        else if (opt == 'x') x_flag = 1;
    }

    if (optind == argc) {
        // Route execution based on which flag was flipped
        if (l_flag) do_ls_long(".");
        else if (x_flag) do_ls_horizontal(".");
        else do_ls(".");
    } else {
        for (int i = optind; i < argc; i++) {
            printf("Directory listing of %s :\n", argv[i]);
            if (l_flag) do_ls_long(argv[i]);
            else if (x_flag) do_ls_horizontal(argv[i]);
            else do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

// Default vertical column display (Down then across)
void do_ls(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
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
            if (index < count) printf("%-*s", col_width, files[index]);
        }
        printf("\n");
    }

    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
}

// New horizontal column display (-x flag)
void do_ls_horizontal(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
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

    struct winsize w;
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) term_width = w.ws_col;

    int col_width = max_len + 2;
    int num_cols = term_width / col_width;
    if (num_cols == 0) num_cols = 1;

    // Horizontal logic is way simpler: just print sequentially and break the line when the row is full
    int current_col = 0;
    for (int i = 0; i < count; i++) {
        printf("%-*s", col_width, files[i]);
        current_col++;
        if (current_col >= num_cols) {
            printf("\n");
            current_col = 0; // Reset horizontal position counter
        }
    }
    // Print a final newline if the last row didn't perfectly fill up
    if (current_col != 0) printf("\n");

    for (int i = 0; i < count; i++) free(files[i]);
    free(files);
}

// Long listing (-l flag)
void do_ls_long(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    
    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat file_stat;
        if (lstat(full_path, &file_stat) == -1) continue;
        
        print_long_format(entry->d_name, &file_stat);
    }
    closedir(dp);
}

// Formats and prints the raw stat data for long listing
void print_long_format(const char *name, struct stat *file_stat) {
    char perms[11];
    strcpy(perms, "----------");

    if (S_ISDIR(file_stat->st_mode)) perms[0] = 'd';
    else if (S_ISLNK(file_stat->st_mode)) perms[0] = 'l';
    else if (S_ISCHR(file_stat->st_mode)) perms[0] = 'c';
    else if (S_ISBLK(file_stat->st_mode)) perms[0] = 'b';
    else if (S_ISFIFO(file_stat->st_mode)) perms[0] = 'p';
    else if (S_ISSOCK(file_stat->st_mode)) perms[0] = 's';

    if (file_stat->st_mode & S_IRUSR) perms[1] = 'r';
    if (file_stat->st_mode & S_IWUSR) perms[2] = 'w';
    if (file_stat->st_mode & S_IXUSR) perms[3] = 'x';
    if (file_stat->st_mode & S_ISUID) perms[3] = (file_stat->st_mode & S_IXUSR) ? 's' : 'S';

    if (file_stat->st_mode & S_IRGRP) perms[4] = 'r';
    if (file_stat->st_mode & S_IWGRP) perms[5] = 'w';
    if (file_stat->st_mode & S_IXGRP) perms[6] = 'x';
    if (file_stat->st_mode & S_ISGID) perms[6] = (file_stat->st_mode & S_IXGRP) ? 's' : 'S';

    if (file_stat->st_mode & S_IROTH) perms[7] = 'r';
    if (file_stat->st_mode & S_IWOTH) perms[8] = 'w';
    if (file_stat->st_mode & S_IXOTH) perms[9] = 'x';
    if (file_stat->st_mode & S_ISVTX) perms[9] = (file_stat->st_mode & S_IXOTH) ? 't' : 'T';

    struct passwd *pwd = getpwuid(file_stat->st_uid);
    struct group *grp = getgrgid(file_stat->st_gid);
    char *user_name = pwd ? pwd->pw_name : "UNKNOWN";
    char *group_name = grp ? grp->gr_name : "UNKNOWN";

    char *time_str = ctime(&file_stat->st_mtime);
    time_str[strlen(time_str) - 1] = '\0'; 

    printf("%s %4lu %-8s %-8s %8ld %.12s %s\n", 
           perms, (unsigned long)file_stat->st_nlink, user_name, group_name, 
           (long)file_stat->st_size, time_str + 4, name);
}
