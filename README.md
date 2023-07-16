### FAT32 File Recovery
This program is a file recovery tool for FAT32 file systems. 

Note: Sometimes it may not be possible to recover a file if the disk has been overwritten.

```
Usage: ./nyufile disk <options>
        -i                     Print the file system information. 
        -l                     List the root directory.
        -r filename [-s sha1]  Recover a contiguous file.
        -R filename -s sha1    Recover a possibly non-contiguous file.
```