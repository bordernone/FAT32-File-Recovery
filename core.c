#include "core.h"
#include "helper.h"
#include "fat32_struct.h"
#include "common.h"
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

void print_file_system_info(const char *disk) {
    struct Disk d = readDisk(disk);
    BootEntry *boot = (BootEntry *)d.start;
    printf("Number of FATs = %d\n", (int) boot->BPB_NumFATs);
    printf("Number of bytes per sector = %d\n", (int) boot->BPB_BytsPerSec);
    printf("Number of sectors per cluster = %d\n", (int) boot->BPB_SecPerClus);
    printf("Number of reserved sectors = %d\n", (int) boot->BPB_RsvdSecCnt);

    // unmapping the disk
    munmap(d.start, d.size);
}

void list_root_directory(const char *diskPath) {
    struct Disk d = readDisk(diskPath);
    BootEntry *boot = (BootEntry *)d.start;

    unsigned int rootCluster = boot->BPB_RootClus;

    struct AllEntries entries = getEntries(d, boot, rootCluster);
    int validFiles = 0;
    for (int i = 0; i < entries.numEntries; i++) {
        if (entries.entries[i].DIR_Name[0] == 0xE5 || entries.entries[i].DIR_Attr == 0x0F)
            continue;
        printFilename(&entries.entries[i]);
        validFiles++;
    }

    printf("Total number of entries = %d\n", validFiles);

    free(entries.entries);
    // unmapping the disk
    munmap(d.start, d.size);
}

void recover_contiguous_file(const char *diskPath, const char *filename, const char *sha1) {
    struct Disk d = readDisk(diskPath);
    BootEntry *boot = (BootEntry *)d.start;
    struct FAT fat = readFAT(d, boot);
    struct FileToRecover fileToRecover = getRecoveryFileEntryContiguous(d, boot, fat, filename, sha1);

    // if size is 0
    if (fileToRecover.entry->DIR_FileSize == 0) {
        
    } else {
        unsigned int startingCluster = fileToRecover.entry->DIR_FstClusHI << 16 | fileToRecover.entry->DIR_FstClusLO;
        unsigned int fileSize = fileToRecover.entry->DIR_FileSize;
        unsigned int bytesInCluster = bytesPerCluster(boot);

        int numberOfClusters = fileSize / bytesInCluster + (fileSize % bytesInCluster != 0);

        // Fix the FAT table
        int (*fatsArray)[fat.fatLength] = (void *) fat.fatsStart;
        for (int i = 0; i < numberOfClusters - 1; i++) {
            for (int fatIndex = 0; fatIndex < fat.numFats; fatIndex++) {
                fatsArray[fatIndex][startingCluster + i] = startingCluster + i + 1;
            }
        }
        for (int fatIndex = 0; fatIndex < fat.numFats; fatIndex++) {
            fatsArray[fatIndex][startingCluster + numberOfClusters - 1] = EOFat;
        }
    }

    // Fix the directory entry
    char *dirEntryAddress = fileToRecover.startAddress;
    dirEntryAddress[0] = filename[0];

    // Write back to disk
    msync(d.start, d.size, MS_SYNC);

    // unmapping the disk
    munmap(d.start, d.size);

    printf("%s: successfully recovered", filename);
    if (strlen(sha1) > 0) {
        printf(" with SHA-1");
    }
    printf("\n");
}

void recover_non_contiguous_file(const char *diskPath, const char *filename, char *sha1) {
    struct Disk d = readDisk(diskPath);
    BootEntry *boot = (BootEntry *)d.start;
    struct FAT fat = readFAT(d, boot);
    struct FileToRecover *fileToRecover = malloc(sizeof(struct FileToRecover));
    if (strcmp(sha1, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0 || strlen(sha1) == 0) {
        *fileToRecover = getRecoveryFileEntryContiguous(d, boot, fat, filename, sha1);
    } else {
        *fileToRecover = getRecoveryFileEntryNonContiguous(d, boot, fat, filename, sha1);
    }

    // Fix the directory entry
    char *dirEntryAddress = fileToRecover->startAddress;
    dirEntryAddress[0] = filename[0];

    // Write back to disk
    msync(d.start, d.size, MS_SYNC);

    // unmapping the disk
    munmap(d.start, d.size);

    printf("%s: successfully recovered", filename);
    if (strlen(sha1) > 0) {
        printf(" with SHA-1");
    }
    printf("\n");
}