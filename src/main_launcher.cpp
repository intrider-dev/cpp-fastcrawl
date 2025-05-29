#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>

int main(int argc, char* argv[]) {
    const int maxRestarts = 9999;
    int restartCount = 0;

    while (restartCount < maxRestarts) {
        pid_t pid = fork();

        if (pid == 0) {
            // В дочернем процессе — запуск основной программы
            execvp(argv[1], argv + 1);
            std::perror("execvp failed");
            return 127;
        }

        int status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == 0) {
                std::cout << "Process exited normally\n";
                break;
            } else {
                std::cerr << "Process exited with code " << code << ", restarting...\n";
            }
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGINT || sig == SIGTERM || sig == SIGKILL) {
                std::cout << "Process terminated by user (signal " << sig << "), exiting.\n";
                break;
            } else {
                std::cerr << "Process crashed with signal " << sig << ", restarting...\n";
            }
        }

        ++restartCount;
        sleep(2); // пауза перед перезапуском
    }

    if (restartCount >= maxRestarts) {
        std::cerr << "Too many crashes, aborting.\n";
        return 1;
    }

    return 0;
}
