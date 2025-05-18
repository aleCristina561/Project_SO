#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>

#define COMMAND_FILE "/tmp/command.txt"

volatile sig_atomic_t command_ready = 0;
int running = 1;

// Output to FD 3 (setup in hub)
#define OUTPUT_FD 3

void handle_signal(int sig) {
    command_ready = 1;
}

void redirect_stdout_to_pipe() {
    dup2(OUTPUT_FD, STDOUT_FILENO);
    dup2(OUTPUT_FD, STDERR_FILENO);
}

void run_test_manager(char *const argv[]) {
    if (fork() == 0) {
        redirect_stdout_to_pipe();
        execv("./new_test_manager", argv);
        perror("execv failed");
        exit(1);
    } else {
        wait(NULL);
    }
}

void process_command(const char *cmd) {
    if (strcmp(cmd, "list_hunts") == 0) {
        char *args[] = {"./new_test_manager", "--list_hunts", NULL};
        run_test_manager(args);
    } else if (strncmp(cmd, "list_treasures ", 15) == 0) {
        char *hunt = strdup(cmd + 15);
        char *args[] = {"./new_test_manager", "--list", hunt, NULL};
        run_test_manager(args);
        free(hunt);
    } else if (strncmp(cmd, "view_treasure ", 14) == 0) {
        char hunt[64];
        int id;
        sscanf(cmd + 14, "%s %d", hunt, &id);
        char id_str[16];
        snprintf(id_str, sizeof(id_str), "%d", id);
        char *args[] = {"./new_test_manager", "--view", hunt, id_str, NULL};
        run_test_manager(args);
    } else if (strcmp(cmd, "stop_monitor") == 0) {
        dprintf(OUTPUT_FD, "[Monitor] Stopping.\n");
        running = 0;
    } else {
        dprintf(OUTPUT_FD, "[Monitor] Unknown command: %s\n", cmd);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    dprintf(OUTPUT_FD, "[Monitor] Ready. PID: %d\n", getpid());

    while (running) {
        pause();

        if (command_ready) {
            command_ready = 0;
            FILE *fp = fopen(COMMAND_FILE, "r");
            if (!fp) continue;

            char command[256];
            if (fgets(command, sizeof(command), fp)) {
                command[strcspn(command, "\n")] = 0;
                process_command(command);
            }
            fclose(fp);
        }

        usleep(100000);
    }

    return 0;
}

