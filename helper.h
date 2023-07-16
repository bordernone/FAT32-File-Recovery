#ifndef NYUFILE_HELPER_H
#define NYUFILE_HELPER_H
#include "fat32_struct.h"
#include <stdbool.h>

struct Disk {
    char *start;
    int size;
};

struct FAT {
    int numFats;
    int fatLength;
    int *fatsStart;
};

struct AllEntries {
    int numEntries;
    DirEntry *entries;
};

struct FileContents {
    unsigned long long length;
    unsigned char *contents;
};

struct FileToRecover {
    DirEntry *entry;
    char *startAddress;
};

struct Disk readDisk(const char *disk); // read the disk into memory
struct FAT readFAT(struct Disk disk, const struct BootEntry *boot); // read the FAT into memory
char *firstClusterStart(struct Disk disk, const struct BootEntry *boot); // get the first cluster of a file
unsigned int bytesPerCluster(const struct BootEntry *boot); // get the size of a single cluster in bytes
char *getFilename(const DirEntry *entry); // get the filename of a directory entry
void printFilename(const DirEntry *entry); // print the filename of a directory entry
int clusterChainLength(unsigned int cluster, const struct FAT *fat); // get the length of a cluster chain
struct AllEntries getEntries(struct Disk disk, const struct BootEntry *boot, unsigned int cluster); // get the entries of a cluster
struct FileContents fileContents(struct Disk disk, const struct BootEntry *boot, const DirEntry *entry, const int *fat); // get the contents of a file
struct FileContents fileContentsContiguous(struct Disk disk, const struct BootEntry *boot, const DirEntry *entry); // get the contents of a file that is stored contiguously
bool sha1Matches(const char *sha1, const struct FileContents contents); // check if the sha1 matches the contents
char *getDirEntryAddress(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, unsigned int startCluster, unsigned int entryIndex); // get the address of a directory entry
struct FileToRecover getRecoveryFileEntryContiguous(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, const char *filename, const char *sha1); // get the directory entry of a file to recover
bool isCorrectFAT(struct Disk disk, const struct BootEntry *boot, const struct DirEntry *entry, const int *fat, const char *sha1); // check if a file is the one we are looking for
struct FileToRecover getRecoveryFileEntryNonContiguous(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, const char *filename, char *sha1); // get the directory entry of a file to recover that is not stored contiguously

#endif
