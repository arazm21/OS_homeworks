#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"



char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

// Recursive function to traverse directories and find files
void find(char *path, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // Open the directory
    if ((fd = open(path, O_RDONLY)) < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }

    // Get the directory stats
    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // If the path is not a directory, return
    if (st.type != T_DIR) {
        printf("%s is not a directory\n", path);
        close(fd);
        return;
    }

    // Copy the path into a buffer to modify
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        printf("find: path too long\n");
        close(fd);
        return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    // Read directory entries
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)  // Skip invalid entries
            continue;

        // Skip "." and ".." directories
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        // Append the directory entry name to the path
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // Get the file stats
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        //printf("here");
        // If it's a directory, recursively search it
        if (st.type == T_DIR) {
            find(buf, filename);
        } else if (st.type == T_FILE) {
            //printf("%s\n", filename);
            if (strcmp(fmtname(buf), filename) == 0) {
                printf("%s\n", buf);
            }
        }
    }

    close(fd);
}




int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: find <directory> <filename>\n");
        exit(1);
    }
    printf("looking for %s\n",argv[2]);
    find(argv[1], argv[2]);
    exit(0);
}