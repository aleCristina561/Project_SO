#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

#define COMMAND_FILE "/tmp/command.txt"
#define HUNT_DIR "./hunts"

pid_t monitor_pid = -1;
int monitor_pipe[2];
int monitor_exiting = 0;
int monitor_terminated = 0;

void sigchld_handler(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid) {
        monitor_terminated = 1;
        monitor_pid = -1;
        printf("Monitor terminated with status %d.\n", WEXITSTATUS(status));
    }
}

void write_command_to_file(const char *command) {
    FILE *fp = fopen(COMMAND_FILE, "w");
    if (!fp) {
        perror("fopen");
        return;
    }
    fprintf(fp, "%s\n", command);
    fclose(fp);
}

void send_command_to_monitor(const char *command) {
    if (monitor_pid <= 0) {
        printf("Error: Monitor not running.\n");
        return;
    }
    if (monitor_exiting && !monitor_terminated) {
        printf("Monitor is shutting down. Please wait.\n");
       
    }
    write_command_to_file(command);
    kill(monitor_pid, SIGUSR1);
    usleep(100000); // Let monitor write to pipe
    char buffer[256];
    ssize_t len;
    while ((len = read(monitor_pipe[0], buffer, sizeof(buffer)-1)) > 0) {
        buffer[len] = '\0';
        printf("%s", buffer);
        if (len < sizeof(buffer) - 1) break;
    }
}

void start_monitor() {
    if (monitor_pid > 0 && !monitor_terminated) {
        printf("Monitor already running.\n");
        return;
    }

    if (pipe(monitor_pipe) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Child: redirect write end to FD 3
        close(monitor_pipe[0]);
        dup2(monitor_pipe[1], 3);
        close(monitor_pipe[1]);
        execl("./new_monitor", "new_monitor", NULL);
        perror("execl failed");
        exit(1);
    }

    close(monitor_pipe[1]); // Parent keeps read end only
    monitor_pid = pid;
    monitor_exiting = 0;
    monitor_terminated = 0;
    printf("Monitor started with PID %d\n", monitor_pid);
}

void stop_monitor() {
    if (monitor_pid <= 0 || monitor_terminated) {
        printf("Monitor is not running.\n");
        return;
    }
    monitor_exiting = 1;
    send_command_to_monitor("stop_monitor");

    for (int i = 0; i < 30; ++i) {
        if (monitor_terminated) break;
        usleep(100000);
    }
    if (!monitor_terminated) {
        printf("Warning: Monitor did not terminate in time.\n");
    }
}

// NEW: Execute score_calculator for each hunt
void calculate_scores() {
    DIR *dir = opendir(HUNT_DIR);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_DIR || 
            strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }    
	if(entry->d_type==DT_DIR){
		int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe");
                continue;
            }

            pid_t pid = fork();
            if (pid == 0) { // Child process
                close(fd[0]);  // Close unused read end of the pipe

                // Redirect stdout to the pipe
                if (dup2(fd[1], STDOUT_FILENO) < 0) {
                    perror("dup2 failed");
                    exit(1);
                }
                close(fd[1]);

                // Execute the score_calculator program
                execl("./score_calculator", "score_calculator", entry->d_name, NULL);
                perror("execl failed");  // Only reached if execl fails
                exit(1);
            } else if (pid > 0) { // Parent process
                close(fd[1]);  // Close unused write end of the pipe
                // Read the output from the child process via the pipe
                char buffer[256];
                ssize_t len;
                printf("Scores for hunt: %s\n", entry->d_name);
                while ((len = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[len] = '\0';  // Null-terminate the string
                    printf("%s", buffer);
                }
                if (len < 0) {
                    perror("read failed");
                }

                close(fd[0]);

                // Wait for the child process to finish
                waitpid(pid, NULL, 0);
            } else {
                perror("fork failed");
            }
        }
    }

    closedir(dir);
}

void handle_input(char *input) {
    if (strcmp(input, "start_monitor") == 0) {
        start_monitor();
    } else if (strcmp(input, "list_hunts") == 0) {
        send_command_to_monitor("list_hunts");
    } else if (strcmp(input, "list_treasures") == 0) {
        char hunt[32];
        printf("Enter Hunt Name: ");
        scanf("%s", hunt);
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "list_treasures %s", hunt);
        send_command_to_monitor(cmd);
    } else if (strcmp(input, "view_treasure") == 0) {
        char hunt[32];
        int id;
        printf("Enter Hunt Name: ");
        scanf("%s", hunt);
        printf("Enter Treasure ID: ");
        scanf("%d", &id);
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "view_treasure %s %d", hunt, id);
        send_command_to_monitor(cmd);
    } else if (strcmp(input, "calculate_score") == 0) {
        calculate_scores();
    } else if (strcmp(input, "stop_monitor") == 0) {
        stop_monitor();
    } else if (strcmp(input, "exit") == 0) {
        if (monitor_pid > 0 && !monitor_terminated) {
            printf("Error: Cannot exit while monitor is still running.\n");
        } else {
            printf("Exiting.\n");
            exit(0);
        }
    } else {
        printf("Unknown command.\n");
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[256];
    while (1) {
        printf("treasure_hub> ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        handle_input(input);
    }

    return 0;
}

