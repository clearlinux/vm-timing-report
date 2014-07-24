/*
 * This file is part of vm-timing-report.
 *
 * Copyright (C) 2014 Intel Corporation
 * Author: Ikey Doherty <michael.i.doherty@intel.com>
 *
 * vm-timing-eport is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <linux/un.h>
#include <pthread.h>
#include <getopt.h>

#define VMPREFIX "VMREPORTID"

/**
 * Each entry in the boot table
 */
struct BootEntry {
        char *cmd_buffer;
        char *analyze_string;
        struct timespec time_start;
        struct timespec time_end;
};

/**
 * Configuration
 */
struct Config {
        const char *path;
        const char *prefix;
        const char *suffix;
        const char *sock_path;
        uint32_t memory;
        const char *kernel;
        const char *initrd;
        uint32_t top;
        struct timespec time_start;
        struct timespec time_end;
        uint32_t ok;
        int sock_fd;
};

struct BootEntry **entries = NULL;

/**
 * Firstly initialise all data entries and check paths, etc.
 *
 * @param config The current config
 * @return a boolean value, indicating success
 */
bool init_vms(struct Config *config)
{
        struct stat st = {0};
        struct BootEntry *entry = NULL;
        char *img_file = NULL;

        /* For systems with qemu-system-x86_64 */
        const char *command = "qemu-system-x86_64 -device virtio-serial-pci -chardev socket,id=ch0,path=%s "
"-device virtserialport,chardev=ch0,name=serial0 -enable-kvm -m %dm -drive file=%s,if=virtio -usb -device usb-kbd "
" -append \"root=/dev/vda quiet "VMPREFIX":%u\" -kernel %s -initrd %s -vnc :%u &";

        if (!entries) {
                entries = calloc(sizeof(struct BootEntry), config->top);
        }

        /* First iteration, organise the boot entries - in case of memory issues */
        for (uint32_t i = 0; i < config->top; i++) {
                entries[i] = calloc(sizeof(struct BootEntry), 1);
                entry = entries[i];

                /* Image location */
                if (!asprintf(&img_file, "%s/%s-%u%s", config->path, config->prefix, i, config->suffix)) {
                        fprintf(stderr, "Unable to allocate memory\n");
                        abort();
                }
                /* Check image exists.. */
                if (stat(img_file, &st) != 0) {
                        fprintf(stderr, "Unable to locate image %s: %s\n", img_file, strerror(errno));
                        free(img_file);
                        return false;
                }
                /* Full command */
                if (!asprintf(&entry->cmd_buffer, command, config->sock_path, config->memory, img_file, i, config->kernel, config->initrd, i)) {
                        fprintf(stderr, "Unable to allocate memory\n");
                        abort();
                }
                free(img_file);
                img_file = NULL;
                config->ok += 1;
        }
        return true;
}
/**
 * Launch the specified number of machines
 *
 * @param data The current config pointer
 */
void* launch_machines(void *data)
{
        struct BootEntry *entry = NULL;
        bool first_booted = false;
        __attribute__ ((unused)) int ret;

        struct Config *config = (struct Config*)data;

        /* Now iterate and launch */
        for (uint32_t i = 0; i < config->top; i++) {
                entry = entries[i];
                /* Execute VM here */
                ret = system(entry->cmd_buffer);
                clock_gettime(CLOCK_MONOTONIC, &entry->time_start);
                /* Make sure VM start times go from here */
                if (!first_booted) {
                        config->time_start = entry->time_start;
                        first_booted = true;
                }
                /* Just in case there are issues - command is visible */
                printf("%s\n", entry->cmd_buffer);
        }
        printf("All machines booted\n\n");
                
        return NULL;
}

/**
 * Free the used BootEntrys
 * @param top The number of entries to free
 */
void free_entries(uint32_t top)
{
        struct BootEntry *entry = NULL;

        if (!entries) {
                return;
        }

        for (uint32_t i = 0; i < top; i++) {
                entry = entries[i];
                if (entry->cmd_buffer) {
                        free(entry->cmd_buffer);
                }
                if (entry->analyze_string) {
                        free(entry->analyze_string);
                }
                free(entry);
        }
        free(entries);
}

/**
 * Setup the unix socket
 *
 * @param config Current configuration
 * @return a boolean value, indicating success
 */
bool init_socket(struct Config *config)
{
        struct sockaddr_un address = {0};
        int sock_fd;

        sock_fd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (sock_fd < 0) {
                fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
                return false;
        }

        address.sun_family = AF_UNIX;
        memcpy(address.sun_path, config->sock_path, (sizeof(char)*UNIX_PATH_MAX));

        /* Attempt to bind the unix socket */
        if (bind(sock_fd, (struct sockaddr*)&address, sizeof(struct sockaddr_un)) != 0) {
                fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
                return false;
        }

        config->sock_fd = sock_fd;

        return true;
}

/**
 * Ensure our required directories exist
 *
 * @param config Current configuration
 * @return a boolean value, determining whether we should continue
 */
bool init(struct Config *config)
{
        struct stat st = { 0 };
        if (stat(config->path, &st) == -1) {
                fprintf(stderr, "Error locating %s: %s\n", config->path, strerror(errno));
                return false;
        }
        if (stat(config->kernel, &st) == -1) {
                fprintf(stderr, "Error locating %s: %s\n", config->kernel, strerror(errno));
                return false;
        }
        if (stat(config->initrd, &st) == -1) {
                fprintf(stderr, "Error locating %s: %s\n", config->initrd, strerror(errno));
                return false;
        }
        return init_socket(config);
}

/**
 * Read all of our pipes
 * @param config Current configuration
 */
void handle_requests(struct Config *config)
{
        int sock;
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        ssize_t len;
        fd_set fds;
        int ret;
        int *sockets = NULL;
        int connection_count = 0;
        int handled_count = 0;
        struct BootEntry *entry = NULL;

        if (listen(config->sock_fd, 1) != 0) {
                fprintf(stderr, "Unable to listen - aborting: %s\n", strerror(errno));
                return;
        }

        sockets = calloc(config->top, sizeof(int));

        while (true) {
                FD_ZERO(&fds);
                /* Always relisten on server */
                FD_SET(config->sock_fd, &fds);
                char buffer[512];

                for (int i = 0; i < connection_count; i++) {
                        if (sockets[i] != -1) {
                                FD_SET(sockets[i], &fds);
                        }
                }

                ret = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
                if (ret < 0) {
                        fprintf(stderr, "Unable to select(): %s\n", strerror(errno));
                        return;
                }
                if (FD_ISSET(config->sock_fd, &fds)) {
                        /* We have a connection ! */
                        sock = accept(config->sock_fd, (struct sockaddr*)&ss, &slen);
                        if (sock < 0) {
                                fprintf(stderr, "Unable to accept(): %s\n", strerror(errno));
                        } else {
                                sockets[connection_count] = sock;
                                connection_count += 1;
                                printf("Got a connection: %d\n", sock);
                        }
                }
                for (int j = 0; j < connection_count; j++) {
                        if (sockets[j] != -1 && FD_ISSET(sockets[j], &fds)) {
                                len = read(sockets[j], &buffer, 500);
                                char *copy = NULL;
                                if (len < 0) {
                                        fprintf(stderr, "No data on socket %d\n", sockets[j]);
                                        goto close;
                                }
                                /* Ensure we null terminate this */
                                if (buffer[len-1] != '\0') {
                                        buffer[len-1] = '\0';
                                }
                                copy = strdup(buffer);
                                char *left = strtok(buffer, "|||");
                                char *right = strtok(NULL, "|||");
                                if (right == NULL) {
                                        fprintf(stderr, "UNKNOWN REQUEST: %s\n", copy);
                                        goto close;
                                }
                                /* Obtain the ID */
                                left = strtok(left, VMPREFIX":");
                                uint32_t index = (uint32_t)atoi(left);
                                printf("VM %u reported as booted\n", index);
                                entry = entries[index];
                                entry->analyze_string = strdup(right);
                                if (!entry->analyze_string) {
                                        fprintf(stderr, "Failed to allocate memory!\n");
                                        abort();
                                }
                                clock_gettime(CLOCK_MONOTONIC, &entry->time_end);
close:
                                if (copy) {
                                        free(copy);
                                }
                                close(sockets[j]);
                                FD_CLR(sockets[j], &fds);
                                sockets[j] = -1;
                                handled_count += 1;
                        }
                }
                if (handled_count == config->top) {
                        clock_gettime(CLOCK_MONOTONIC, &config->time_end);
                        break;
                }
        }
        free(sockets);
}

/**
 * Finally, print out a report if the previous operations all went well.
 */
void print_report(struct Config *config)
{
        struct BootEntry *entry = NULL;
        uint64_t total = 0;

        for (int i = 0; i < config->top; i++) {
                entry = entries[i];
                // Might need this precision in future
                unsigned long long elapsed = (unsigned long long)((entry->time_end.tv_nsec - entry->time_start.tv_nsec) + ((entry->time_end.tv_sec - entry->time_start.tv_sec) * 1000000000));
                total += elapsed;
                double seconds = (double)elapsed / 1000000000.0;
                printf("VM %u booted in: %.4gs: %s\n", i, seconds, entry->analyze_string);
        }
        double avg = ((total/1000000000.0)/config->top);
        printf("Average boot: %.4gs\n", avg);

        /* Total boot time */
        unsigned long long total_elapsed =  (unsigned long long)((config->time_end.tv_nsec - config->time_start.tv_nsec) + ((config->time_end.tv_sec - config->time_start.tv_sec) * 1000000000));
        double total_seconds = (double)total_elapsed / 1000000000.0;
        printf("It took %.4gs to boot all VMs\n", total_seconds);
}

/**
 * Cleanup directories and such
 * @param config Current configuration
 */
void cleanup(struct Config *config)
{
        struct stat st;

        close(config->sock_fd);
        /* Remove socket if it exists */
        if (stat(config->sock_path, &st) == 0) {
                unlink(config->sock_path);
        }
}

/**
 * As simple as it looks. Prints help.
 */
void print_help(const char *progname)
{
        const char *help_msg = "%s - help options\n"
"       -m      memory  How much memory (MB) to allocate each VM\n"
"       -p      prefix  Prefix of the image file names\n"
"       -s      suffix  Suffix of the image file names\n"
"       -v      vmdir   Path to directory containing VM images\n"
"       -k      kernel  Path to the kernel\n"
"       -i      initrd  Path to the initrd\n"
"       -n      number  Number of VMS to boot\n"
"       -h      help    Print this help message\n";

        printf(help_msg, progname);
}
int main(int argc, char **argv)
{
        int exit_code = EXIT_FAILURE;
        /* Initialise our configuration */
        struct Config config = {0};
        config.ok = 0;
        config.sock_path = "./vmsocket";
        int c;
        int optindex;

        /* Getopt initialisation of config struct */
        static struct option options[] = {
                { "memory", required_argument, 0, 'm' },
                { "prefix", required_argument, 0, 'p' },
                { "suffix", required_argument, 0, 's' },
                { "vmdir" , required_argument, 0, 'v' },
                { "kernel", required_argument, 0, 'k' },
                { "initrd", required_argument, 0, 'i' },
                { "number", required_argument, 0, 'n' },
                { "help", no_argument, 0, 'h' },
                { 0, 0, 0, 0 }
        };

        while ((c = getopt_long(argc, argv, "m:p:v:s:k:i:n:h", options, &optindex)) != -1) {
                switch (c) {
                        case 'm':
                                config.memory = (uint32_t)atoi(optarg);
                                break;
                        case 'p':
                                config.prefix = optarg;
                                break;
                        case 's':
                                config.suffix = optarg;
                                break;
                        case 'v':
                                config.path = optarg;
                                break;
                        case 'k':
                                config.kernel = optarg;
                                break;
                        case 'i':
                                config.initrd = optarg;
                                break;
                        case 'n':
                                config.top = (uint32_t)atoi(optarg);
                                break;
                        case 'h':
                                print_help(argv[0]);
                                goto end;
                        default:
                                abort();
                }
        }
        /* Sanity */
        if (config.prefix == NULL) {
                fprintf(stderr, "No prefix set\n");
                goto end;
        }
        /* Non fatal */
        if (config.suffix == NULL) {
                config.suffix = "";
        }
        if (config.path == NULL) {
                fprintf(stderr, "No vmdir set\n");
                goto end;
        }
        if (config.kernel== NULL) {
                fprintf(stderr, "No kernel set\n");
                goto end;
        }
        if (config.initrd == NULL) {
                fprintf(stderr, "No initrd set\n");
                goto end;
        }

        /* Initialise directories and socket */
        if (!init(&config)) {
                goto end;
        }

        if (!init_vms(&config)) {
                goto clean_end;
        }

        /* Attempt to launch these machines in a thread so we can select first */
        pthread_t thread;
        if (pthread_create(&thread, NULL, launch_machines, (void*)&config) != 0) {
                fprintf(stderr, "Failed to launch machines. Aborting\n");
                goto clean_end;
        }

        handle_requests(&config);
        print_report(&config);

        pthread_join(thread, NULL);
clean_end:
        cleanup(&config);

end:
        /* Ensure we only clean what was actually allocated in our lifetime */
        free_entries(config.ok);
        return exit_code;
}
