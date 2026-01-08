#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libaio.h>

#define BUF_SIZE 4096

int main() {
    const char *filename = "example.txt";
    const char *data = "Hello, world!";
    char buffer[BUF_SIZE];
    off_t offset = 0;
    size_t size = strlen(data);
    int fd;

    // Open file for writing
    fd = open(filename, O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Initialize io_context
    io_context_t ctx;
    if (io_setup(1, &ctx) == -1) {
        perror("io_setup");
        exit(EXIT_FAILURE);
    }

    // Prepare asynchronous write operation
    struct iocb iocb_write;
    memset(&iocb_write, 0, sizeof(struct iocb));
    io_prep_pwrite(&iocb_write, fd, (void *)data, size, offset);

    // Submit asynchronous write operation
    struct iocb *iocbs_write[1] = {&iocb_write};
    if (io_submit(ctx, 1, iocbs_write) != 1) {
        perror("io_submit write");
        io_destroy(ctx);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Wait for write operation to complete
    struct io_event events_write[1];
    if (io_getevents(ctx, 1, 1, events_write, NULL) != 1) {
        perror("io_getevents write");
        io_destroy(ctx);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close file after writing
    close(fd);

    // Open file for reading
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Initialize io_context for reading
    io_context_t ctx_read;
    if (io_setup(1, &ctx_read) == -1) {
        perror("io_setup read");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Prepare asynchronous read operation
    struct iocb iocb_read;
    memset(&iocb_read, 0, sizeof(struct iocb));
    io_prep_pread(&iocb_read, fd, buffer, size, offset);

    // Submit asynchronous read operation
    struct iocb *iocbs_read[1] = {&iocb_read};
    if (io_submit(ctx_read, 1, iocbs_read) != 1) {
        perror("io_submit read");
        io_destroy(ctx_read);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Wait for read operation to complete
    struct io_event events_read[1];
    if (io_getevents(ctx_read, 1, 1, events_read, NULL) != 1) {
        perror("io_getevents read");
        io_destroy(ctx_read);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close file after reading
    close(fd);

    // Print data read from file
    printf("Data read from file: %s\n", buffer);

    return 0;
}
