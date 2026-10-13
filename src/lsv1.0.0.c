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
void do_ls_long(const char *dir);
void print_long_format(const char *name, struct stat *file_stat);

int main(int argc, char *argv[]) {
    int l_flag = 0;
    int opt;

    // getopt loops through arguments to find the -l flag
    while ((opt = getopt(argc, argv, "l")) != -1) {
        if (opt == 'l') {
            l_flag = 1;
        }
    }

    // optind is the index of the first non-flag argument (the directories)
    if (optind == argc) {
        // No directories specified, default to current directory "."
        if (l_flag) do_ls_long(".");
        else do_ls(".");
    } else {
        // Loop through all specified directories
        for (int i = optind; i < argc; i++) {
            printf("Directory listing of %s :\n", argv[i]);
            if (l_flag) do_ls_long(argv[i]);
            else do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

// The updated default ls function with "down then across" column formatting
void do_ls(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    // Initialize dynamic array for filenames
    int capacity = 128;
    int count = 0;
    char **files = malloc(capacity * sizeof(char *));
    int max_len = 0;

    errno = 0;
    // Read all directory entries into the dynamically allocated array
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue; // skip hidden files

        int len = strlen(entry->d_name);
        if (len > max_len) max_len = len;

        // Reallocate memory if we hit the capacity limit
        if (count >= capacity) {
            capacity *= 2;
            files = realloc(files, capacity * sizeof(char *));
        }

        // Duplicate the string into our array
        files[count] = strdup(entry->d_name);
        count++;
    }

    if (errno != 0) perror("readdir failed");
    closedir(dp);

    if (count == 0) {
        free(files);
        return;
    }

    // Get the terminal width using ioctl
    struct winsize w;
    int term_width = 80; // Fallback width
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {
        term_width = w.ws_col;
    }

    // Calculate grid layout
    int col_width = max_len + 2; // Add a 2-space buffer between columns
    int num_cols = term_width / col_width;
    if (num_cols == 0) num_cols = 1; // Prevent division by zero if terminal is tiny

    // Calculate total rows needed (integer division ceiling trick)
    int num_rows = (count + num_cols - 1) / num_cols;

    // Print items "down then across"
    for (int r = 0; r < num_rows; r++) {
        for (int c = 0; c < num_cols; c++) {
            // Calculate the 1D array index based on the 2D grid position
            int index = r + (c * num_rows);
            if (index < count) {
                // %-*s dynamically pads the string with spaces based on col_width
                printf("%-*s", col_width, files[index]);
            }
        }
        printf("\n");
    }

    // Free the dynamically allocated memory
    for (int i = 0; i < count; i++) {
        free(files[i]);
    }
    free(files);
}
// The new long listing function
void do_ls_long(const char *dir) {
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL) {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue; // skip hidden files

        // Construct the full absolute path so lstat doesn't crash on nested files
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat file_stat;
        // Grab the metadata. Using lstat instead of stat to handle symlinks correctly.
        if (lstat(full_path, &file_stat) == -1) {
            perror("lstat failed");
            continue;
        }

        print_long_format(entry->d_name, &file_stat);
    }
    if (errno != 0) perror("readdir failed");
    closedir(dp);
}

// Formats and prints the raw stat data
void print_long_format(const char *name, struct stat *file_stat) {
    char perms[11];
    strcpy(perms, "----------");

    // 1. File Type
    if (S_ISDIR(file_stat->st_mode)) perms[0] = 'd';
    else if (S_ISLNK(file_stat->st_mode)) perms[0] = 'l';
    else if (S_ISCHR(file_stat->st_mode)) perms[0] = 'c';
    else if (S_ISBLK(file_stat->st_mode)) perms[0] = 'b';
    else if (S_ISFIFO(file_stat->st_mode)) perms[0] = 'p';
    else if (S_ISSOCK(file_stat->st_mode)) perms[0] = 's';

    // 2. User permissions with SetUID edge case
    if (file_stat->st_mode & S_IRUSR) perms[1] = 'r';
    if (file_stat->st_mode & S_IWUSR) perms[2] = 'w';
    if (file_stat->st_mode & S_IXUSR) perms[3] = 'x';
    if (file_stat->st_mode & S_ISUID) perms[3] = (file_stat->st_mode & S_IXUSR) ? 's' : 'S';

    // 3. Group permissions with SetGID edge case
    if (file_stat->st_mode & S_IRGRP) perms[4] = 'r';
    if (file_stat->st_mode & S_IWGRP) perms[5] = 'w';
    if (file_stat->st_mode & S_IXGRP) perms[6] = 'x';
    if (file_stat->st_mode & S_ISGID) perms[6] = (file_stat->st_mode & S_IXGRP) ? 's' : 'S';

    // 4. Other permissions with Sticky bit edge case
    if (file_stat->st_mode & S_IROTH) perms[7] = 'r';
    if (file_stat->st_mode & S_IWOTH) perms[8] = 'w';
    if (file_stat->st_mode & S_IXOTH) perms[9] = 'x';
    if (file_stat->st_mode & S_ISVTX) perms[9] = (file_stat->st_mode & S_IXOTH) ? 't' : 'T';

    // 5. Resolve IDs to readable names
    struct passwd *pwd = getpwuid(file_stat->st_uid);
    struct group *grp = getgrgid(file_stat->st_gid);
    char *user_name = pwd ? pwd->pw_name : "UNKNOWN";
    char *group_name = grp ? grp->gr_name : "UNKNOWN";

    // 6. Format time and strip the trailing newline character ctime adds
    char *time_str = ctime(&file_stat->st_mtime);
    time_str[strlen(time_str) - 1] = '\0';

    // 7. Print perfectly aligned output
    // %4lu = 4 spaces unsigned long, %-8s = 8 spaces left-aligned string, %8ld = 8 spaces long int
    printf("%s %4lu %-8s %-8s %8ld %.12s %s\n",
           perms,
           (unsigned long)file_stat->st_nlink,
           user_name,
           group_name,
           (long)file_stat->st_size,
           time_str + 4, // +4 skips the day of the week for a cleaner look
           name);
}
