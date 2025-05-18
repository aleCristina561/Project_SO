#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define HUNTS_DIR "./hunts"
#define MAX_USERS 100

typedef struct {
    char name[64];
    int total_score;
} UserScore;

UserScore users[MAX_USERS];
int user_count = 0;

void add_score(const char *user, int value) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, user) == 0) {
            users[i].total_score += value;
            return;
        }
    }
    // new user
    if (user_count < MAX_USERS) {
        strncpy(users[user_count].name, user, 63);
        users[user_count].total_score = value;
        user_count++;
    }
}

void parse_treasure_file(const char *filepath) {
   FILE *fp = fopen(filepath, "r");
    if (!fp) return;

    char line[256];
    char owner[64] = "";
    int value = 0;

    while (fgets(line, sizeof(line), fp)!=NULL) {
        printf("Reading line: %s", line);
      	fflush(stdout);	// Debugging line
        if (strncmp(line, "Username:", 9) == 0) {
            sscanf(line + 9, "%s", owner);
        } else if (strncmp(line, "Value:", 6) == 0) {
            sscanf(line + 6, "%d", &value);
        }
    }

    if (strlen(owner) > 0) {
        printf("Adding score for owner: %s with value: %d\n", owner, value);  // Debugging line
        add_score(owner, value);
    }

    fclose(fp);}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt>\n", argv[0]);
        return 1;
    }

    char hunt_path[128];
    snprintf(hunt_path, sizeof(hunt_path), "%s/%s", HUNTS_DIR, argv[1]);

    DIR *dir = opendir(hunt_path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG && strcmp(entry->d_name, "treasures.dat") == 0) {
	    char filepath[256];
 	    snprintf(filepath, sizeof(filepath), "%s/%s", hunt_path, entry->d_name);
	    int fp=open(filepath,O_RDONLY);
            parse_treasure_file(filepath);
        }
    }
    closedir(dir);

    // Output scores
    for (int i = 0; i < user_count; i++) {
        printf("%s: %d\n", users[i].name, users[i].total_score);
    }

    return 0;
}

