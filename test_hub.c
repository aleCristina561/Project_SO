#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

#define COMMAND_FILE "/tmp/command.txt"

pid_t monitor_pid = -1;
int monitor_exiting = 0;
int monitor_terminated = 0;

void sigchld_handler(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
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
}

void start_monitor() {
    if (monitor_pid > 0 && !monitor_terminated) {
        printf("Monitor already running.\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        execl("./monitor", "monitor", NULL);
        perror("execl failed");
        exit(1);
    }

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

    for(int i=0;i<30;++i){
	    if(monitor_terminated)break;
	    usleep(100000);
    }
    if(!monitor_terminated){
	    printf("warning:Monitor did not terminate in time");
    }
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
