#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "fat32_struct.h"
#include "helper.h"
#include "core.h"
#include "common.h"

// Usage: ./nyufile disk <options>
//   -i                     Print the file system information.
//   -l                     List the root directory.
//   -r filename [-s sha1]  Recover a contiguous file.
//   -R filename -s sha1    Recover a possibly non-contiguous file.

int main(int argc, char *argv[]) {
    int opt;
    char filename[13] = {0};
    char sha1[SHA_DIGEST_LENGTH + 1] = {0};
    bool isFileRecovery = false;
    bool isContiguous = false;
    bool printFSInfo = false;
    bool listRootDir = false;

    while ((opt = getopt(argc, argv, "ilr:R:s:")) != -1) {
        switch (opt) {
        case 'i':
            printFSInfo = true;
            break;
        case 'l':
            listRootDir = true;
            break;
        case 'r':
            isFileRecovery = true;
            isContiguous = true;
            strncpy(filename, optarg, 12);
            filename[12] = '\0';
            break;
        case 'R':
            isFileRecovery = true;
            isContiguous = false;
            strncpy(filename, optarg, 12);
            filename[12] = '\0';
            // printf("filename: %s\n", filename);
            break;
        case 's':
            strncpy(sha1, optarg, 41);
            break;
        default:
            fprintf(stderr, "Usage: %s disk <options>\n", argv[0]);
            fprintf(stderr, "  -i                     Print the file system information.\n"
                            "  -l                     List the root directory.\n"
                            "  -r filename [-s sha1]  Recover a contiguous file.\n"
                            "  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
            return 1;
        }
    }

    char *disk = argv[optind];
    if (disk == NULL) {
        fprintf(stderr, "Usage: %s disk <options>\n", argv[0]);
        fprintf(stderr, "  -i                     Print the file system information.\n"
                        "  -l                     List the root directory.\n"
                        "  -r filename [-s sha1]  Recover a contiguous file.\n"
                        "  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
        return 1;
    }

    if (printFSInfo) {
        print_file_system_info(disk);
        return 0;
    } else if (listRootDir) {
        list_root_directory(disk);
        return 0;
    } else if (isFileRecovery) {
        if (isContiguous) {
            recover_contiguous_file(disk, filename, sha1);
        } else {
            recover_non_contiguous_file(disk, filename, sha1);
        }
    } else {
        fprintf(stderr, "Usage: %s disk <options>\n", argv[0]);
        fprintf(stderr, "  -i                     Print the file system information.\n"
                        "  -l                     List the root directory.\n"
                        "  -r filename [-s sha1]  Recover a contiguous file.\n"
                        "  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
        return 1;
    }

    return 0;
}
