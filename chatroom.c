#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void ensure_room_dir(const char *roompath) {
    if (mkdir(roompath, 0777) == -1) {
        if (errno != EEXIST) die("mkdir");
    }
}

static void ensure_fifo(const char *fifopath) {
    if (mkfifo(fifopath, 0666) == -1) {
        if (errno != EEXIST) die("mkfifo");
    }
}

static bool is_fifo_path(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return false;
    return S_ISFIFO(st.st_mode);
}

static void reader_loop(const char *roomname, const char *username, int read_fd) {
    (void)roomname; (void)username;

    char buf[4096];

    while (1) {
        ssize_t n = read(read_fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            // Message direcly
            fputs(buf, stdout);
            fflush(stdout);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // no data, sleep CPU
            usleep(50 * 1000); // 50ms
        } else if (n == 0) {
            // If no writer in FIFO, can turn 0r, wait
            usleep(50 * 1000);
        } else {
            // real error
            die("read");
        }
    }
}

static void send_to_one_user(const char *target_path, const char *msg) {
    // O_WRONLY: dont write
    // O_NONBLOCK:if no one read, dont lock the program
    int fd = open(target_path, O_WRONLY | O_NONBLOCK);
    if (fd == -1) {
        // ENXIO: no reader on other side, pass quietly
        if (errno == ENXIO) exit(0);
        // another error
        exit(0);
    }

    // write message
    (void)write(fd, msg, strlen(msg));
    close(fd);
}

static void broadcast_message(const char *roompath, const char *roomname,
                              const char *username, const char *text) {
    DIR *d = opendir(roompath);
    if (!d) return;

    //message format [room] user: message\n
    char msg[8192];
    snprintf(msg, sizeof(msg), "[%s] %s: %s\n", roomname, username, text);

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        // . and .. jump
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        if (strcmp(ent->d_name, username) == 0)
            continue;

        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", roompath, ent->d_name);

        // check if fifo
        if (!is_fifo_path(target_path))
            continue;

        pid_t p = fork();
        if (p == 0) {
            send_to_one_user(target_path, msg);
            exit(0);
        }
    }

    closedir(d);

    // pick children (no zombie)
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <roomname> <username>\n", argv[0]);
        return 1;
    }

    const char *roomname = argv[1];
    const char *username = argv[2];

    // 1) room path
    char roompath[1024];
    snprintf(roompath, sizeof(roompath), "/tmp/chatroom-%s", roomname);

    // 2) fifo path
    char fifopath[1024];
    snprintf(fifopath, sizeof(fifopath), "%s/%s", roompath, username);

    // 3) create room dir + fifo if missing
    ensure_room_dir(roompath);
    ensure_fifo(fifopath);

    // 4) open fifo for reading (non-blocking)
    int read_fd = open(fifopath, O_RDONLY | O_NONBLOCK);
    if (read_fd == -1) die("open read fifo");

    int dummy_w = open(fifopath, O_WRONLY | O_NONBLOCK);

    printf("Welcome to %s!\n", roomname);
    fflush(stdout);

    // 5) fork: child reads, parent writes
    pid_t child = fork();
    if (child == -1) die("fork");

    if (child == 0) {
        // CHILD: read forever
        reader_loop(roomname, username, read_fd);
        exit(0);
    }

    // PARENT: read user input and broadcast
    while (1) {
        printf("[%s] %s > ", roomname, username);
        fflush(stdout);

        char *line = NULL;
        size_t cap = 0;
        ssize_t n = getline(&line, &cap, stdin);

        if (n == -1) {
            // Ctrl+D / EOF
            free(line);
            break;
        }

        // clear newline
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';

        // if exit, exit
        if (strcmp(line, "exit") == 0) {
            free(line);
            break;
        }

        if (line[0] != '\0') {
            broadcast_message(roompath, roomname, username, line);
        }

        free(line);
    }

    // 6) cleanup
    // close reader child
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);

    close(read_fd);
    if (dummy_w != -1) close(dummy_w);

    unlink(fifopath);

    return 0;
}
