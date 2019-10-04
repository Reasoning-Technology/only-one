
#ifndef DIRECTORY_H
#define DIRECTORY_H


/*
file and pathname string manipulations
defines 'file_record'
regular expression match for file names
breadth first directory tree traversal routine, 'list_files'

*/

//file functions: signature, same, copy
#include "file.h"

// for directory manipulation
#include <dirent.h>
#include <unistd.h>

// for sterror and errno
#include <errno.h>
#include <string.h>

// STL objects
#include <string>
#include <regex>

// archive objects
#include "phrase.h"


//--------------------------------------------------------------------------------
// given a path name, strips the trailing slashes
//
  void strip_trailing(string &path){
    while(path.size() != 0 && path[path.size()-1] == '/' ) path.erase(path.size() - 1);
  };


//--------------------------------------------------------------------------------
//  a file record describes a file:
//  type = F_file or F_symbolic_link
//  pathname - is the path to the file with the file name appended 
//  mtime - modification time
//
  #define F_file 1
  #define F_symbol_link 2
  class file_record{
  public:
    file_record(){;}
    file_record(string &pathname ,time_t mtime): pathname(pathname),mtime(mtime),type(F_file){;}
    file_record(string &pathname ,time_t mtime, string &target): pathname(pathname),mtime(mtime),type(F_symbol_link),target(target){;}

    unsigned int type;
    string pathname;  // the full path to the file
    time_t mtime;  // the modification time of the file
    string target; // used for symoblic links, target file pathname

    // prints the file record into a phrase 
    // # <filename> # <mtime> # <signature>
    void print(phrase &ph) const{
      ph.push_back(pathname);
      stringstream ss;
      ss << mtime;
      ph.push_back(ss.str());
    }

    // # <filename> # <mtime> 
    ParseStatus parse(phrase &ph){
      if(ph.size() < 2) RETURN ParseStatus::Malformed;

      pathname = *ph.first();
      ph.pop_front();

      stringstream ss;
      ss.str(*ph.first());
      if( !(ss >> mtime) ) RETURN ParseStatus::Malformed;
      ph.pop_front();

      RETURN ParseStatus::Found;
    }

    bool operator < (const  file_record &fr) const {
      RETURN mtime < fr.mtime || (mtime == fr.mtime && pathname < fr.pathname);
    }
  };


//--------------------------------------------------------------------------------
// a set of file records
//
  class file_record_list : public set<file_record>{
  public:
    // for tax files, head will typically be "source"
    void print(ostream &os ,string &head) const{
      phrase a_phrase;
      iterator it = begin();
      while(it != end()){
        a_phrase.clear();
        a_phrase.push_front(head);
        it->print(a_phrase); // puts file record poited to by it into 'a_phrase' (more of an a_phrase init than a print)
        a_phrase.print(os);
      it++;
      }
    }
  };


//--------------------------------------------------------------------------------
// name matches any regex in a list - helper function
//
  bool match(const string &name ,const list<regex> &regex_list){
    list<regex>::const_iterator i = regex_list.begin();
    while( i != regex_list.end() ){
      if( regex_match(name ,*i) ) return true;
      ++i;
    }
    return false;
  }


//--------------------------------------------------------------------------------
// breadth first traversal of directory to create a file_record_list
//   directory path given in:  source_root_dir
//   returns a list of file records
// 
  void list_files(const string &source_root_dirname ,file_record_list &files, file_record_list &links, const list<regex> &excludes){

    struct dirent *dep;

    string filepath;
    string filename;
    list<string> directories;
    directories.push_back(source_root_dirname);

    DIR *source_dirp;
    string source_dirname;
    struct stat file_attributes;

    list<string>::iterator dit;
    dit = directories.begin();
    while(dit != directories.end() ){

        source_dirname = *dit;
        source_dirp = opendir( source_dirname.c_str() );
        if( source_dirp == NULL){
          cerr << "could not open source directory, skipping: " << source_dirname << endl;
          goto _next_dir; 
        }
        while( dep = readdir(source_dirp) ){

          filename = dep->d_name;
          if(filename == "." || filename == ".." || match(filename ,excludes) ) CONTINUE;
          filepath = source_dirname + "/" + filename;

          if( stat( filepath.c_str() ,&file_attributes) == -1){
            cerr << filepath << " " << strerror(errno) << endl;
            CONTINUE;
          }

          if( dep->d_type == DT_DIR ) { 
            //cout << "adding to directory list: " << filepath << endl;
            directories.push_back(filepath);
            CONTINUE;
          }

          if( dep->d_type == DT_LNK ) { 
            size_t buflen = 16;
            size_t buflen_read;
            char *buf=0;
            do{
              delete buf;
              buflen = buflen << 1;
              buf = new char[buflen];
              buflen_read = readlink(filepath.c_str(), buf ,buflen);
            }while( buflen == buflen_read || buflen >= 127 * 1024 * 1024);
            if( buflen >= 127 * 1024 * 1024){
               cerr << "extreme length link target for link ignored: " << filepath << endl;
               CONTINUE;
            }
            buf[buflen_read] = 0;
            string sbuf(buf);
            links.insert( file_record(filepath, file_attributes.st_mtime, sbuf) );
            //cout << "found link: " << buf << endl;
            delete buf;
            CONTINUE;
          }

          if( dep->d_type != DT_REG ) {
            cerr << "skipping, not link nor regular file: " << filepath << endl;
            CONTINUE;
          }

          files.insert( file_record(filepath, file_attributes.st_mtime) );
        }
        
    _next_dir:
      closedir(source_dirp);
      dit++;
      directories.pop_front();
    }

  };



#endif
