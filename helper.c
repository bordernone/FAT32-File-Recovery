#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"
#include <string.h>
#include <openssl/sha.h>

struct Disk readDisk(const char *disk) {
    struct Disk d;
    struct stat st;
    int fd = open(disk, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Error opening disk image: %s\n", disk);
        exit(1);
    }
    stat(disk, &st);
    d.size = st.st_size;
    d.start = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (d.start == MAP_FAILED) {
        fprintf(stderr, "Error: mmap failed \n");
        exit(1);
    }
    close(fd);
    return d;
}

struct FAT readFAT(struct Disk disk, const struct BootEntry *boot) {
    struct FAT fat;

    unsigned short reservedSectors = boot->BPB_RsvdSecCnt;
    unsigned short bytesPerSector = boot->BPB_BytsPerSec;
    unsigned short numberOfFats = boot->BPB_NumFATs;

    fat.fatLength = boot->BPB_FATSz32 * bytesPerSector / 4;
    fat.fatsStart = (int *)(disk.start + reservedSectors * bytesPerSector);
    fat.numFats = numberOfFats;
    // int (*fats)[numberOfFats] = (void *) fatStart;

    return fat;
}

char *firstClusterStart(struct Disk disk, const struct BootEntry *boot) {
    unsigned short reservedSectors = boot->BPB_RsvdSecCnt;
    unsigned short bytesPerSector = boot->BPB_BytsPerSec;
    unsigned int sectorsPerFat = boot->BPB_FATSz32;
    unsigned short numberOfFats = boot->BPB_NumFATs;
    return disk.start + reservedSectors * bytesPerSector + numberOfFats * sectorsPerFat * bytesPerSector;
}

unsigned int bytesPerCluster(const struct BootEntry *boot) {
    unsigned char sectorPerCluster = boot->BPB_SecPerClus;
    unsigned short bytesPerSector = boot->BPB_BytsPerSec;
    return sectorPerCluster * bytesPerSector;
}

char *getFilename(const DirEntry *entry) {
    int size = 11;
    char *filename = malloc(size + 2);
    int newIndex = 0;
    for (int i = 0; i < size; i++) {
        if (entry->DIR_Name[i] != ' ') {
            if (i == 8 && (entry->DIR_Attr | 0x10) != entry->DIR_Attr) {
                filename[newIndex++] = '.';
            }
            filename[newIndex++] = entry->DIR_Name[i];
        }
    }
    filename[newIndex] = '\0';
    return filename;
}

void printFilename(const DirEntry *entry) {
    char *filename = getFilename(entry);
    printf("%s", filename);
    free(filename);

    if ((entry->DIR_Attr | 0x10) == entry->DIR_Attr) {
        printf("/ (starting cluster = %d)\n", entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO);
    } else {
        printf(" (size = %d", entry->DIR_FileSize);
        if (entry->DIR_FileSize != 0) {
            printf(", starting cluster = %d", entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO);
        }
        printf(")\n");
    }
}

int clusterChainLength(unsigned int cluster, const struct FAT *fat) {
    int length = 0;
    int (*fatsArray)[fat->fatLength] = (void *) fat->fatsStart;
    unsigned int currentCluster = cluster;
    while (currentCluster < EOFat) {
        length++;
        currentCluster = fatsArray[0][currentCluster];
    }
    return length;
}

struct AllEntries getEntries(struct Disk disk, const struct BootEntry *boot, unsigned int cluster) {
    struct FAT fat = readFAT(disk, boot);
    int (*fatsArray)[fat.fatLength] = (void *) fat.fatsStart;
    
    unsigned int bytesInCluster = bytesPerCluster(boot);
    unsigned int entriesInCluster = bytesInCluster / sizeof(DirEntry);

    unsigned int totalClusters = clusterChainLength(cluster, &fat);

    char *firstCluster = firstClusterStart(disk, boot);

    DirEntry *entries = malloc(totalClusters * entriesInCluster * sizeof(DirEntry));
    int entriesIndex = 0;
    unsigned int currentCluster = cluster;
    while (currentCluster < EOFat) {
        char *clusterAddress = (currentCluster - 2) * bytesInCluster + firstCluster;
        for (unsigned int i = 0; i < entriesInCluster; i++) {
            DirEntry *entry = (DirEntry *) (clusterAddress + i * sizeof(DirEntry));
            if (entry->DIR_Name[0] == 0x00) {
                break;
            }
            entries[entriesIndex++] = *entry;
        }
        currentCluster = fatsArray[0][currentCluster];
    }

    struct AllEntries allEntries;
    allEntries.entries = entries;
    allEntries.numEntries = entriesIndex;
    return allEntries;
}

struct FileContents fileContents(struct Disk disk, const struct BootEntry *boot, const DirEntry *entry, const int *fat) {
    unsigned int fileSize = entry->DIR_FileSize;
    char *firstClusterAddress = firstClusterStart(disk, boot);
    unsigned int bytesInCluster = bytesPerCluster(boot);
    unsigned char *contents = malloc(fileSize);
    if (contents == NULL) {
        fprintf(stderr, "Error: malloc failed \n");
        exit(1);
    }
    unsigned int currentCluster = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;

    unsigned int contentsIndex = 0;
    while (contentsIndex < fileSize) {
        char *clusterAddress = (currentCluster - 2) * bytesInCluster + firstClusterAddress;
        unsigned int bytesToRead = MIN(bytesInCluster, fileSize - contentsIndex);
        memcpy(contents + contentsIndex, clusterAddress, bytesToRead);
        contentsIndex += bytesToRead;
        currentCluster = fat[currentCluster];
    }

    struct FileContents fileContents;
    fileContents.contents = contents;
    fileContents.length = fileSize;
    return fileContents;
}

struct FileContents fileContentsContiguous(struct Disk disk, const struct BootEntry *boot, const DirEntry *entry) {
    unsigned int fileSize = entry->DIR_FileSize;
    char *firstClusterAddress = firstClusterStart(disk, boot);
    unsigned int bytesInCluster = bytesPerCluster(boot);
    unsigned char *contents = malloc(fileSize);
    if (contents == NULL) {
        fprintf(stderr, "Error: malloc failed \n");
        exit(1);
    }
    unsigned int currentCluster = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;

    unsigned int contentsIndex = 0;
    while (contentsIndex < fileSize) {
        char *clusterAddress = (currentCluster - 2) * bytesInCluster + firstClusterAddress;
        unsigned int bytesToRead = MIN(bytesInCluster, fileSize - contentsIndex);
        memcpy(contents + contentsIndex, clusterAddress, bytesToRead);
        contentsIndex += bytesToRead;
        currentCluster = currentCluster + 1;
    }

    struct FileContents fileContents;
    fileContents.contents = contents;
    fileContents.length = fileSize;
    return fileContents;
}

bool sha1Matches(const char *sha1, const struct FileContents filecontent) {
    unsigned char sha1FileHash[SHA_DIGEST_LENGTH];
    SHA1(filecontent.contents, filecontent.length, sha1FileHash);
    char expected_sha1[SHA_DIGEST_LENGTH * 2 + 1];
    
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&expected_sha1[i * 2], "%02x", sha1FileHash[i]);
    }
    
    return strcmp(expected_sha1, sha1) == 0;
}

char *getDirEntryAddress(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, unsigned int startCluster, unsigned int entryIndex) {
    int (*fatsArray)[fat.fatLength] = (void *) fat.fatsStart;
    unsigned int bytesInCluster = bytesPerCluster(boot);
    unsigned int entriesPerCluster = bytesInCluster / sizeof(DirEntry);
    unsigned int toSkip = entryIndex / entriesPerCluster;
    unsigned int currentCluster = startCluster;
    for (unsigned int i = 0; i < toSkip; i++) {
        currentCluster = fatsArray[0][currentCluster];
    }
    unsigned int bytesToSkip = entryIndex % entriesPerCluster * sizeof(DirEntry);
    char *firstClusterAddress = firstClusterStart(disk, boot);
    return (currentCluster - 2) * bytesInCluster + firstClusterAddress + bytesToSkip;
}

struct FileToRecover getRecoveryFileEntryContiguous(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, const char *filename, const char *sha1) {
    unsigned int rootCluster = boot->BPB_RootClus;
    struct AllEntries entries = getEntries(disk, boot, rootCluster);
    DirEntry *fileToRecover = NULL;
    int fileToRecoverIndex = -1;
    for (int i = 0; i < entries.numEntries; i++){
        DirEntry *entry = &entries.entries[i];
        if (entry->DIR_Name[0] == 0xE5 && (entry->DIR_Attr | 0x10) != entry->DIR_Attr) {
            char *name = getFilename(entry);
            if (strcmp(name + 1, filename + 1) == 0) {
                if (fileToRecover == NULL) {
                    if (strlen(sha1) == 0) {
                        fileToRecoverIndex = i;
                        fileToRecover = entry;
                    } else {
                        struct FileContents filecontent = fileContentsContiguous(disk, boot, entry);
                        if (sha1Matches(sha1, filecontent)) {
                            fileToRecoverIndex = i;
                            fileToRecover = entry;
                        }
                        free(filecontent.contents);
                    }
                } else {
                    if (strlen(sha1) != 40) {
                        fprintf(stderr, "%s: multiple candidates found\n", filename);
                        exit(1);
                    } else {
                        struct FileContents filecontent = fileContentsContiguous(disk, boot, entry);
                        if (sha1Matches(sha1, filecontent)) {
                            fileToRecoverIndex = i;
                            fileToRecover = entry;
                        }
                        free(filecontent.contents);
                    }
                }
            }
            free(name);
        }
    }
    if (!fileToRecover) {
        fprintf(stderr, "%s: file not found\n", filename);
        exit(1);
    }
    struct FileToRecover file;
    file.entry = fileToRecover;
    file.startAddress = getDirEntryAddress(disk, boot, fat, rootCluster, fileToRecoverIndex);
    return file;
}

bool isCorrectFAT(struct Disk disk, const struct BootEntry *boot, const struct DirEntry *entry, const int *fat, const char *sha1) {
    struct FileContents filecontent = fileContents(disk, boot, entry, fat);
    bool matches = sha1Matches(sha1, filecontent);
    free(filecontent.contents);
    return matches;
}

int GLOBAL_rangeStart = -1;
int GLOBAL_rangeEnd = -1;
int GLOBAL_startCluster = -1;
int *GLOBAL_fat = NULL;
int *GLOBAL_correct_fat = NULL;
int GLOBAL_fatLength = 0;
struct Disk GLOBAL_disk;
struct BootEntry GLOBAL_boot;
struct DirEntry GLOBAL_entry;
char *GLOBAL_sha1;

int recursion(int *lastArr, int length, int targetLength) {
    if (length == targetLength) {
        // Modify the FAT
        int *copyFat = malloc(GLOBAL_fatLength * sizeof(int));
        memcpy(copyFat, GLOBAL_fat, GLOBAL_fatLength * sizeof(int));
        for (int i = 0; i < length - 1; i++) {
            copyFat[lastArr[i]] = lastArr[i + 1];
        }
        copyFat[lastArr[length - 1]] = EOFat;

        // for (int i = 0; i < length; i++) {
        //     printf("%d ", lastArr[i]);
        // }
        // printf("\n");

        // Check if the FAT is correct
        if (isCorrectFAT(GLOBAL_disk, &GLOBAL_boot, &GLOBAL_entry, copyFat, GLOBAL_sha1)) {
            // printf("Found a correct FAT!\n");
            GLOBAL_correct_fat = copyFat;
            return 1;
        } else {
            free(copyFat);
            return 0;
        }
    } else {
        // recursive case
        bool flag = false;
        for (int i = GLOBAL_rangeStart; i <= GLOBAL_rangeEnd; i++) {
            flag = false;
            for (int j = 0; j < length; j++) {
                if (i == lastArr[j]) {
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                lastArr[length] = i;
                int result = recursion(lastArr, length + 1, targetLength);
                if (result == 1) {
                    return 1;
                }
            }
        }
        return 0;
    }
}

int isCorrectEntry(struct Disk disk, const struct BootEntry *boot, const struct DirEntry *entry, char *sha1) {
    if (entry->DIR_FileSize == 0) {
        return 0;
    }

    struct FAT fat = readFAT(disk, boot);
    int startCluster = entry->DIR_FstClusHI << 16 | entry->DIR_FstClusLO;
    unsigned int bytesInCluster = bytesPerCluster(boot);
    int numberOfClusters = entry->DIR_FileSize / bytesInCluster + (entry->DIR_FileSize % bytesInCluster != 0);
    // int endCluster = startCluster + 21;

    GLOBAL_boot = *boot;
    GLOBAL_disk = disk;
    GLOBAL_entry = *entry;
    GLOBAL_fat = fat.fatsStart;
    GLOBAL_fatLength = fat.fatLength;
    GLOBAL_startCluster = startCluster;
    GLOBAL_sha1 = sha1;
    GLOBAL_rangeStart = 2;
    GLOBAL_rangeEnd = 22;

    int *lastArr = malloc(numberOfClusters * sizeof(int));
    lastArr[0] = startCluster;
    int result = recursion(lastArr, 1, numberOfClusters);
    free(lastArr);
    return result;
}

struct FileToRecover getRecoveryFileEntryNonContiguous(struct Disk disk, const struct BootEntry *boot, const struct FAT fat, const char *filename, char *sha1) {
    unsigned int rootCluster = boot->BPB_RootClus;
    struct AllEntries entries = getEntries(disk, boot, rootCluster);
    DirEntry *fileToRecover = NULL;
    int fileToRecoverIndex = -1;
    for (int i = 0; i < entries.numEntries; i++){
        DirEntry *entry = &entries.entries[i];
        if (entry->DIR_Name[0] == 0xE5 && (entry->DIR_Attr | 0x10) != entry->DIR_Attr) {
            char *name = getFilename(entry);
            if (strcmp(name + 1, filename + 1) == 0) {
                if (isCorrectEntry(disk, boot, entry, sha1)) {
                    fileToRecoverIndex = i;
                    fileToRecover = entry;
                    // Modify the FAT
                    int (*fatsArray)[fat.fatLength] = (void *)fat.fatsStart;
                    for (int k = 0; k < GLOBAL_fatLength; k++) {
                        for (int x = 0; x < fat.numFats; x++) {
                            fatsArray[x][k] = GLOBAL_correct_fat[k];
                        }
                    }
                    free(GLOBAL_correct_fat);
                    break;
                }
            }
            free(name);
        }
    }
    if (!fileToRecover) {
        fprintf(stderr, "%s: file not found\n", filename);
        exit(1);
    }
    struct FileToRecover file;
    file.entry = fileToRecover;
    file.startAddress = getDirEntryAddress(disk, boot, fat, rootCluster, fileToRecoverIndex);
    return file;
}
