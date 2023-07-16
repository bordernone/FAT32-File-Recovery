#ifndef NYUFILE_CORE_H
#define NYUFILE_CORE_H

void print_file_system_info(const char *disk);
void list_root_directory(const char *diskPath);
void recover_contiguous_file(const char *diskPath, const char *filename, const char *sha1);
void recover_non_contiguous_file(const char *diskPath, const char *filename, char *sha1);

#endif
