
#ifndef FILE_H
#define FILE_H

/*
  file functions: signature, same, copy

*/


#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/md5.h>

#include <istream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <list>
using namespace std;

#include "types.h"
#include "utils.h"

// block size used for our file diff routine
const static uint BLOCKSIZE = 4096;
const static uint SIGNATURE_CHARS = 4096; // this is version specific, changing it will change generated signatures

/* 
  Convenience functions for open
   -we never use open() directly
*/
   int open_read(const string &s){
     RETURN open(s.c_str() ,O_RDONLY);
   }
   int open_read(const stringstream &ss){
     RETURN open_read(ss.str());
   }

   int open_write(const string &s1){ 
     RETURN open(s1.c_str(), O_CREAT | O_RDWR ,S_IRUSR | S_IWUSR);
   }
   int open_write(const stringstream &ss){ 
     RETURN open_write(ss.str());
   }



/*--------------------------------------------------------------------------------
  This uses the MD5 sum of the first 4096 bytes of a file as a signature. The 'same' function we are currently
  using already checks file length.  Using only the first block (or as many bytes as the file has if it less
  that of a block) is for performance reasons.  Typically if the signatures match, we then run 'same' to 
  make sure it is not due to aliasing.
*/
  class signature{
  public:
    void print(ostream &os) const {
      const uchar *pt = data + MD5_DIGEST_LENGTH - 1;
      const uchar *end = data -1;
      while(pt != end){
        os << hex << setfill('0') << setw(2) << (uint)*pt;
      pt--;
      };
    }

    bool sign( int file_descript ){// file_descript must be value, i.e. not less than zero
      unsigned long file_size;
      char* file_buffer;

      uchar buff[SIGNATURE_CHARS];
      size_t buffer_size = read(file_descript ,buff ,SIGNATURE_CHARS);
      MD5(buff, buffer_size, data);
      RETURN true;
    }

    bool sign( char *filepath ){
      int file_descript;
      unsigned long file_size;
      char* file_buffer;

      file_descript = open_read(filepath);
      if(file_descript < 0) RETURN false;

      bool mksign = sign( file_descript);

      close(file_descript);

      RETURN mksign;
    }


    bool fromhex(uint ch ,uint &result){
      if(ch >= '0' && ch <= '9'){
        result = ch - '0';
        RETURN true;
      } else if( ch >= 'a' && ch <= 'f' ){
        result = ch - 'a' + 10;
        RETURN true;
      } else if( ch >= 'A' && ch <= 'F' ){
        result = ch - 'A' + 10;
        RETURN true;
      }
      RETURN false;
    }

    bool parse( string &signature_string ){
      string::reverse_iterator rit = signature_string.rbegin();
      uint ch0;
      uint ch1;
      uchar *pt = data;
      uchar *pt_end = data + MD5_DIGEST_LENGTH;
      // cout << "converting from: " << signature_string << endl;
      while( rit != signature_string.rend() && pt != pt_end){
        if( !fromhex(*rit ,ch0) ) RETURN false;
        rit++;
        if( rit == signature_string.rend() || !fromhex(*rit ,ch1) ) RETURN false;
        *pt = (ch1 << 4) + ch0;
        // cout << ":" << hex << (uint)*pt;
      rit++;
      pt++;
      }
      // cout << endl;
      RETURN true;
    }

    bool operator == (const signature &other) const{
      const uchar *pt0 = data;
      const uchar *pt0_end = data + MD5_DIGEST_LENGTH;
      const uchar *pt1 = other.data;
      while( pt0 != pt0_end && *pt0 == *pt1){ pt0++; pt1++;}
      RETURN pt0 == pt0_end;
    }

    signature &operator = (const signature &other){
      uchar *pt0 = data;
      const uchar *pt0_end = data + MD5_DIGEST_LENGTH;
      const uchar *pt1 = other.data;
      while( pt0 != pt0_end){ *pt0 = *pt1; pt0++; pt1++;}
      RETURN *this;
    }

    uchar data[MD5_DIGEST_LENGTH];
  };



/*--------------------------------------------------------------------------------
  Returns true if the two file images are identical
*/
  bool same(int fdi, int fdj){ 
    off_t offi ,offj;
    offi = lseek(fdi ,0 ,SEEK_END);
    offj = lseek(fdj ,0 ,SEEK_END);

    if(offi != offj) RETURN false;

    lseek(fdi ,0 ,SEEK_SET);
    lseek(fdj ,0 ,SEEK_SET);

    size_t sizi ,sizj;
    char buffi[BLOCKSIZE] ,buffj[BLOCKSIZE];

    do{
      sizi = read(fdi ,buffi ,BLOCKSIZE);
      sizj = read(fdj ,buffj ,BLOCKSIZE);
      if(sizi != sizj) RETURN false;
    }while( sizi != 0 && memcmp(buffi ,buffj ,sizi) == 0);

    RETURN sizi == 0;
  }

/*--------------------------------------------------------------------------------
  mkdir that creates intermediate directories along path if they do not already exist
*/
  bool blaze_path(const string &the_path_string){
    stringstream the_path_stream(the_path_string);
    list<string> path_list;
    string token;
    while( getline( the_path_stream ,token ,'/') ){
      if( !the_path_stream.eof() ){
        path_list.push_back(token);
      }
    }
    string path;
    list<string>::iterator it = path_list.begin();
    int err;
    if( it != path_list.end() ){
      path = *it;
      err = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if( err == -1 && errno != EEXIST ){
        RETURN false;
      }
    it++;
    }
    while( it != path_list.end() ){
      path += "/" + *it;
      err = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      if( err == -1 && errno != EEXIST ){
        RETURN false;
      }
    it++;
    }
    RETURN true;
  }


/*--------------------------------------------------------------------------------
  copies file fdi to fdj
*/
  bool copy(int fdi, int fdj){ 
    off_t offi ,offj;

    lseek(fdi ,0 ,SEEK_SET);
    lseek(fdj ,0 ,SEEK_SET);

    size_t sizi ,sizj;
    char buff[BLOCKSIZE];

    do{
      sizi =  read(fdi ,buff ,BLOCKSIZE);
      if(sizi == 0) BREAK;
      sizj = write(fdj ,buff ,sizi);
      if(sizi != sizj) RETURN false;
    }while( sizi != 0 );

    RETURN true;
  }

   bool copy(const string &s0,  const string &s1){
     int fd0 = open_read(s0);
     int fd1 = open_write(s1);
     if( fd0 == -1 || fd1 == -1 ) RETURN false;
     if( copy(fd0, fd1) ){
       close(fd0);
       close(fd1);
       RETURN true;
     }
     RETURN false;
   };

   bool copy(const stringstream &s0,  const stringstream &s1){
     RETURN copy(s0.str(), s1.str());
   };

   bool touch(const string &s1){ 
     int fd1;
     if( (fd1=open_write(s1)) == -1) RETURN false;
     close(fd1);
     RETURN true;
   }

   bool touch(const stringstream &s1){ 
     RETURN touch(s1.str());
   }

  bool exists(const string &pathname){ 
    int fd = open_read(pathname);
    if( fd == -1 ) RETURN false;  // no version 0 found, our work is done
    close(fd);
    RETURN true;
  }
  bool exists(const stringstream &pathname){ 
    RETURN exists(pathname.str());
  }



/*--------------------------------------------------------------------------------
  increment version numbers on a file
   a version number is a trailing semicolon followed by an integer
*/



/*
  a versioned file name ends in a semicolon followed by a number

  the active version is ';0'  i.e. version 0.  Older versions have consequtively bigger numbers.

  the 'unversioned_pathname'  is the pathname to the file without the semicolon or number, there
  should be no actual file at this path (because the version number is missing)

  this subroutine is given an unversioned_pathname it then increments all the version 
  numbers.  After this routine is done, there will be no version 0 of the file, rather
  number will start at 1.
*/
  uint rev(const string &not_versioned_pathname){

    // find the max version
    //
      uint suffix=0;
      uint maxsuffix=0;
      int fd;
      list<string> pathnames;
      stringstream versioned_pathname;

      versioned_pathname << not_versioned_pathname << ";" << suffix;
      fd = open_read(versioned_pathname);
      if( fd == -1 ) RETURN 0;  // no version 0 found, our work is done
      close(fd);
      versioned_pathname.clear();
      versioned_pathname.str("");

      suffix++; // suffix is now 1
      versioned_pathname << not_versioned_pathname << ";" << suffix;
      fd = open_read(versioned_pathname);
      while( fd != -1 ){
        close(fd);
        versioned_pathname.clear();
        versioned_pathname.str("");

        suffix++;
        versioned_pathname << not_versioned_pathname << ";" << suffix;
        fd = open_read(versioned_pathname);
      }
      versioned_pathname.clear();
      versioned_pathname.str("");

    // rename them all
    //   at this point suffix is at least one
    //
      maxsuffix = suffix;
      string new_pathname;
      do{
          versioned_pathname << not_versioned_pathname << ";" << suffix; 
          new_pathname = versioned_pathname.str();
          versioned_pathname.clear();
          versioned_pathname.str("");
          suffix--;
          versioned_pathname << not_versioned_pathname << ";" << suffix; 
          rename( versioned_pathname.str().c_str() ,new_pathname.c_str()); // should be checking for errors !
      } while(suffix != 0 );
        
  RETURN maxsuffix;
  }

/*
A class for a file

*/
  #if 0
  // attributes
  typedef uint F_ATT;
  const F_ATT F_NULL = 0;
  const F_ATT F_HAS_PATHNAME = 1;
  const F_ATT F_BOUND = 2;  // means the class is buffering a file on disk


  class file{
  public:
    file(){ attributes = F_NULL;}
    file(const file &other){

    }
    ~file(){  /* deletes file?  closes file? */ }

    void give_pathname(const stringstream &a_pathname){ give_pathname(a_pathname.str());}
    void give_pathname(const string &a_pathname){ 
      pathname = a_pathname;
      attributes |= F_HAS_PATHNAME;
    }

    bool exists(){
      if( attributes & F_HAS_PATHNAME ) RETURN exists(pathname);
      RETURN false; // if the file has not been given a name yet, then we say it doesn't exist
    }

    void up_version(){


    }

    void bind(){;}
    void unbind(){;}

  protected:
      string pathname;
      F_ATT attributes;


  };
  #endif


#endif
