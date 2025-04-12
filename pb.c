#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h> 
#include <unistd.h> 
#include <sys/stat.h> 
#include <dirent.h> 
#include <time.h> 
#include <errno.h>

#define USERNAME_MAX 32 
#define CLUE_MAX 256 
#define DATA_FILENAME "treasures.dat" 
#define LOG_FILENAME "logged_hunt"

typedef struct { 
    int id; 
    char username[USERNAME_MAX]; 
    float latitude; float longitude; 
    char clue[CLUE_MAX]; 
    int value; } Treasure;

void log_operation(const char *hunt, const char *message) { 
    char log_path[256]; 
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt, LOG_FILENAME); 
    int log_fd = open(log_path, O_WRONLY | O_APPEND | O_CREAT, 0644); 
    if (log_fd < 0) return;

dprintf(log_fd, "[%ld] %s\n", time(NULL), message);
close(log_fd);

}

int add_treasure(const char *hunt_id) { 
    char dir_path[128]; 
    snprintf(dir_path, sizeof(dir_path), "%s", hunt_id); 
    mkdir(dir_path, 0755);

char file_path[256];
snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, DATA_FILENAME);
int fd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
if (fd < 0) {
    perror("Error opening file");
    return -1;
}

Treasure t;
printf("Enter Treasure ID: "); 
scanf("%d", &t.id);
printf("Enter Username: "); 
scanf("%s", t.username);
printf("Enter Latitude: "); 
scanf("%f", &t.latitude);
printf("Enter Longitude: "); 
scanf("%f", &t.longitude);
printf("Enter Clue: "); 
getchar(); 
fgets(t.clue, CLUE_MAX, stdin);
t.clue[strcspn(t.clue, "\n")] = 0;
printf("Enter Value: "); 
scanf("%d", &t.value);

write(fd, &t, sizeof(Treasure));
close(fd);

log_operation(hunt_id, "Added treasure");
char log_path[256];
char symlink_name[256];
snprintf(symlink_name, sizeof(symlink_name), "logged_hunt-%s", hunt_id);
symlink(log_path, symlink_name);

return 0;

}

int list_treasures(const char *hunt_id) { 
    char file_path[256]; 
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, DATA_FILENAME); 
    int fd = open(file_path, O_RDONLY); 
    if (fd < 0) { 
        perror("Error opening file"); 
        return -1; }

struct stat st;
stat(file_path, &st);
printf("Hunt: %s\n", hunt_id);
printf("Size: %ld bytes\n", st.st_size);
printf("Last modified: %s", ctime(&st.st_mtime));

Treasure t;
while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
    printf("ID: %d, User: %s, Lat: %.2f, Long: %.2f, Value: %d\n",
           t.id, t.username, t.latitude, t.longitude, t.value);
}

close(fd);
return 0;

}

int view_treasure(const char *hunt_id, int id) { 
    char file_path[256]; 
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, DATA_FILENAME); 
    int fd = open(file_path, O_RDONLY); 
    if (fd < 0) { perror("Error opening file"); 
        return -1; }

Treasure t;
while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
    if (t.id == id) {
        printf("ID: %d\nUsername: %s\nLat: %.2f\nLong: %.2f\nClue: %s\nValue: %d\n",
               t.id, t.username, t.latitude, t.longitude, t.clue, t.value);
        close(fd);
        return 0;
    }
}

printf("Treasure with ID %d not found.\n", id);
close(fd);
return 0;

}

int remove_treasure(const char *hunt_id, int id) { 
    char file_path[256], temp_path[256]; 
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_id, DATA_FILENAME); 
    snprintf(temp_path, sizeof(temp_path), "%s/temp.dat", hunt_id);

int fd = open(file_path, O_RDONLY);
int temp_fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd < 0 || temp_fd < 0) {
    perror("File error");
    return -1;
}

Treasure t;
int found = 0;
while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
    if (t.id != id) {
        write(temp_fd, &t, sizeof(Treasure));
    } else {
        found = 1;
    }
}

close(fd);
close(temp_fd);

rename(temp_path, file_path);

if (found) log_operation(hunt_id, "Removed a treasure");
else printf("Treasure not found.\n");

return 0;

}

int remove_hunt(const char *hunt_id) { 
    char dir_path[128]; 
    snprintf(dir_path, sizeof(dir_path), "%s", hunt_id);

char log_symlink[256];
snprintf(log_symlink, sizeof(log_symlink), "logged_hunt-%s", hunt_id);
unlink(log_symlink);

char data_file[256], log_file[256];
snprintf(data_file, sizeof(data_file), "%s/%s", dir_path, DATA_FILENAME);
snprintf(log_file, sizeof(log_file), "%s/%s", dir_path, LOG_FILENAME);
unlink(data_file);
unlink(log_file);
rmdir(dir_path);

return 0;

}

int main(int argc, char *argv[]) { 
    if (argc < 3) { 
        fprintf(stderr, "Usage: %s --<operation> <hunt_id> [<id>]\n", argv[0]); 
        return 1; }

if (strcmp(argv[1], "--add") == 0) {
    return add_treasure(argv[2]);
} else if (strcmp(argv[1], "--list") == 0) {
    return list_treasures(argv[2]);
} else if (strcmp(argv[1], "--view") == 0 && argc == 4) {
    return view_treasure(argv[2], atoi(argv[3]));
} else if (strcmp(argv[1], "--remove_treasure") == 0 && argc == 4) {
    return remove_treasure(argv[2], atoi(argv[3]));
} else if (strcmp(argv[1], "--remove_hunt") == 0) {
    return remove_hunt(argv[2]);
}

fprintf(stderr, "Unknown or malformed command.\n");
return 1;

}