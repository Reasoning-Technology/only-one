#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/md5.h>

#include "types.h"
#include <iostream>
#include <iomanip>
using namespace std;

// Get the size of the file by its file descriptor
unsigned long get_size_by_fd(int fd) {
    struct stat statbuf;
    if(fstat(fd, &statbuf) < 0) exit(-1);
    return statbuf.st_size;
}

class md5sum{
public:
  void print(ostream &os){
    uchar *pt = the_sum;
    uchar *end = pt + MD5_DIGEST_LENGTH;
    while(pt != end){
      os << setfill('0') << setw(2) << hex << (uint)*pt;
    pt++;
    };
  }
  bool parse( char *filepath ){
    int file_descript;
    unsigned long file_size;
    char* file_buffer;

    file_descript = open(filepath, O_RDONLY);
    if(file_descript < 0) RETURN false;
    file_size = get_size_by_fd(file_descript);
    printf("file size:\t%lu\n", file_size);
    file_buffer = (char *)mmap(0, file_size, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer, file_size, the_sum);
    close(file_descript);
    return true;
  }

  uchar the_sum[MD5_DIGEST_LENGTH];
};


int main(int argc, char *argv[]) {

    if(argc != 2) { 
            printf("Must specify the file\n");
            exit(-1);
    }
    printf("using file:\t%s\n", argv[1]);

    md5sum result;
    result.parse(argv[1]);
    result.print(cout);
    printf("  %s\n", argv[1]);

    return 0;
}
