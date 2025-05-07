#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#define COMMAND_FILE "/tmp/command.txt"

volatile sig_atomic_t command_ready = 0;
int running = 1;

void handle_signal(int sig) {
    command_ready = 1;
}

void process_command(const char *cmd) {
    if (strcmp(cmd, "list_hunts") == 0) {
        if (fork() == 0) {
            execl("./test_manager", "test_manager", "--list_hunts", NULL);
            perror("execl failed");
            exit(1);
        } else {
            wait(NULL);
        }
    } else if (strncmp(cmd, "list_treasures ", 15) == 0) {
       char *hunt = strdup(cmd + 15);
       if (fork() == 0) {
            execl("./test_manager", "test_manager", "--list", hunt, NULL);
            perror("execl failed");
            exit(1);
        } else {
            wait(NULL);
        }
        free(hunt);
    } else if (strncmp(cmd, "view_treasure ", 14) == 0) {
        char hunt[64];
        int id;
        sscanf(cmd + 14, "%s %d", hunt, &id);
        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", id);
        if (fork() == 0) {
            execl("./test_manager", "test_manager", "--view", hunt, id_str, NULL);
            perror("execl failed");
            exit(1);
        } else {
            wait(NULL);
        }
    } else if (strcmp(cmd, "stop_monitor") == 0) {
        printf("[Monitor] Stopping.\n");
        running = 0;
    } else {
        printf("[Monitor] Unknown command: %s\n", cmd);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    printf("[Monitor] Ready. PID: %d\n", getpid());

    while (running) {
	pause();

        if (command_ready) {
            command_ready = 0;
            FILE *fp = fopen(COMMAND_FILE, "r");
            if (!fp) {
                perror("fopen");
                continue;
            }
            char command[256];
            if (fgets(command, sizeof(command), fp)) {
                command[strcspn(command, "\n")] = 0;
                process_command(command);
            }
            fclose(fp);
        }
        usleep(100000); // avoid busy waiting
    }

    return 0;
}
