/**
 * @file test_poll.c
 * @brief Test program for poll/select support
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <sys/select.h>
 #include <sys/poll.h>
 #include <errno.h>

 #define DEVICE "/dev/mypoll"

 void test_select() {
     int fd;
     fd_set readfds, writefds;
     struct timeval timeout;
     int ret;
     char buffer[100];

     printf("=== Testing select() ===\n");

     fd = open(DEVICE, O_RDWR);
     if (fd < 0) {
         perror("Failed to open device");
         return;
     }

     // Test 1: Check if readable (should be NO - empty buffer)
     printf("\n1. Checking if device is readable (empty buffer)...\n");
     FD_ZERO(&readfds);
     FD_SET(fd, &readfds);
     timeout.tv_sec = 2;
     timeout.tv_usec = 0;

     ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
     if (ret > 0 && FD_ISSET(fd, &readfds)) {
         printf("   Device is READABLE (unexpected!)\n");
     } else if (ret == 0) {
         printf("   Timeout: Device is NOT readable (expected)\n");
     }

     // Test 2: Check if writable (should be YES - has space)
     printf("\n2. Checking if device is writable (has space)...\n");
     FD_ZERO(&writefds);
     FD_SET(fd, &writefds);
     timeout.tv_sec = 2;
     timeout.tv_usec = 0;

     ret = select(fd + 1, NULL, &writefds, NULL, &timeout);
     if (ret > 0 && FD_ISSET(fd, &writefds)) {
         printf("   Device is WRITABLE (expected)\n");
     } else {
         printf("   Device is NOT writable\n");
     }

     // Test 3: Write data, then check readable
     printf("\n3. Writing data to device...\n");
     write(fd, "Hello poll!", 11);
     printf("   Data written\n");

     printf("\n4. Checking if device is now readable...\n");
     FD_ZERO(&readfds);
     FD_SET(fd, &readfds);
     timeout.tv_sec = 2;
     timeout.tv_usec = 0;

     ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
     if (ret > 0 && FD_ISSET(fd, &readfds)) {
         printf("   Device is READABLE (expected)\n");
         read(fd, buffer, sizeof(buffer));
         printf("   Read: %s\n", buffer);
     } else {
         printf("   Device is NOT readable (unexpected!)\n");
     }

     close(fd);
     printf("\n=== select() tests complete ===\n\n");
 }

 void test_poll() {
     int fd;
     struct pollfd pfd;
     int ret;
     char buffer[100];

     printf("=== Testing poll() ===\n");

     fd = open(DEVICE, O_RDWR);
     if (fd < 0) {
         perror("Failed to open device");
         return;
     }

     pfd.fd = fd;

     // Test 1: Check POLLIN (readable) - should timeout
     printf("\n1. Polling for POLLIN (empty buffer)...\n");
     pfd.events = POLLIN;
     ret = poll(&pfd, 1, 2000);  // 2 second timeout

     if (ret > 0) {
         if (pfd.revents & POLLIN)
             printf("   POLLIN: Device is readable (unexpected!)\n");
         if (pfd.revents & POLLOUT)
             printf("   POLLOUT: Device is writable\n");
     } else if (ret == 0) {
         printf("   Timeout: No events (expected)\n");
     }

     // Test 2: Check POLLOUT (writable) - should succeed immediately
     printf("\n2. Polling for POLLOUT (has space)...\n");
     pfd.events = POLLOUT;
     ret = poll(&pfd, 1, 2000);

     if (ret > 0 && (pfd.revents & POLLOUT)) {
         printf("   POLLOUT: Device is writable (expected)\n");
     }

     // Test 3: Write data, then poll for POLLIN
     printf("\n3. Writing data...\n");
     write(fd, "Hello from poll!", 16);

     printf("\n4. Polling for POLLIN (after write)...\n");
     pfd.events = POLLIN;
     ret = poll(&pfd, 1, 2000);

     if (ret > 0 && (pfd.revents & POLLIN)) {
         printf("   POLLIN: Device is readable (expected)\n");
         read(fd, buffer, sizeof(buffer));
         printf("   Read: %s\n", buffer);
     }

     // Test 4: Poll for both read and write
     printf("\n5. Polling for POLLIN | POLLOUT...\n");
     pfd.events = POLLIN | POLLOUT;
     ret = poll(&pfd, 1, 2000);

     if (ret > 0) {
         printf("   Events returned:\n");
         if (pfd.revents & POLLIN)
             printf("     - POLLIN (readable)\n");
         if (pfd.revents & POLLOUT)
             printf("     - POLLOUT (writable)\n");
     }

     close(fd);
     printf("\n=== poll() tests complete ===\n\n");
 }

 void test_multiple_fds() {
     int fd1, fd2;
     struct pollfd pfds[2];
     int ret;

     printf("=== Testing Multiple File Descriptors ===\n");

     fd1 = open(DEVICE, O_RDWR);
     fd2 = open(DEVICE, O_RDWR);

     if (fd1 < 0 || fd2 < 0) {
         perror("Failed to open device");
         return;
     }

     // Write to fd1
     printf("\n1. Writing to first fd...\n");
     write(fd1, "Data for fd1", 12);

     // Poll both fds
     pfds[0].fd = fd1;
     pfds[0].events = POLLIN;
     pfds[1].fd = fd2;
     pfds[1].events = POLLIN;

     printf("\n2. Polling both fds for readability...\n");
     ret = poll(pfds, 2, 2000);

     if (ret > 0) {
         if (pfds[0].revents & POLLIN)
             printf("   fd1 is readable\n");
         if (pfds[1].revents & POLLIN)
             printf("   fd2 is readable (same device!)\n");
     }

     close(fd1);
     close(fd2);
     printf("\n=== Multiple FD tests complete ===\n\n");
 }

 int main() {
     printf("Character Device poll/select Test Program\n");
     printf("==========================================\n\n");

     test_select();
     sleep(1);

     test_poll();
     sleep(1);

     test_multiple_fds();

     printf("All tests complete!\n");
     return 0;
 }
