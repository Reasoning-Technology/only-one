/*

2012-10-04 Copyright (C) Thomas W. Lynch All Rights Reserved

  arch_insert <archive> <source>

  <archive>
     directory to hold the archived data

  <source> 
     <source> is a directory to be traversed.  During traversal 1) unique
     files that are copied into the store directory, 2) an entry giving
     the path the file is found at is placed into a map constructed in memory.
   
     uniqueness is determined by the same() function, found in file.h

     links are currently ignored.  should the file they point to be in the
     same source directory, then at least all the data will still be backed up.

     [  links are to be followed, if there is a referential loop the
     program gives up, otherwise the target file is handled just as
     though it were found during scanning, while its pathname is that of
     its physical location, i.e. the value of the final link.]

-----
2013-07-16 twl

  modifying directory structure to get the taxonomies away from the node files

  Fixed the double slash problem when the source directory is specified with a trailing slash.

----
2014-02-23 twl

  put project under hg

  objective for most recent edits today is to add an --exclude option so we can avoid the
  rdiff-backup maintenance files

  also tempted to add simultaneous update of multiple mirroed archives ..

*/

// before we start, a bit of Vogon poetry:
//
  const char *vogon_poetry = R"VOGON_POETRY(
     insert [options] <archive> <source>

    <archive> the name of the archive
     <source> a source directory tree 

    options:

      -h --help            this message
         --list_insert     prints 'insert-file <filename>' on cout for each file included in the archive
      -v --verbose         progress information, does not do --list_insert as that can get very long
      -x --exclude <regx>  ECMAscript regx for excluding files that have a matching path name

  )VOGON_POETRY";

#include "types.h"

// Program Termination Return Codes
const uint Exit_NoError   =0;
const uint Exit_BadParms  =1;
const uint Exit_NoArchive =2;
const uint Exit_NoSource  =3;
const uint Exit_InternalError =4;
const uint Exit_FileCreationError =5;

// for sterror and errno
#include <errno.h>
#include <string.h>

// STL objects used
#include <string>
#include <regex>
#include <fstream>
#include <set>
#include <list>
using namespace std;

// local objects used
#include "file.h"
#include "directory.h"
#include "taxonomy.h"



/*--------------------------------------------------------------------------------

  Given one source file, fds: scans through each node set entry in the nodes_map, hd.  If
  the node file referenced in the node set is the same() as the source file, then we
  return true and set 'npi' to point to the corresponding node set entry in the nodes_map.

          archpath - is the name of the archive directory
  source signature - is the signature of the fixed source file
               fds - is an open C file handle for the source file - used when the source signature matches to check for aliasing
               npi - is an iterator on the nodes_map, it iterates through node sets

  first we check for matching signatures between the node in the nodes_map and the
  source file.  If the signatures are the same, we then check to see if the files themselves
  are the same.

  The signature object is found in "file.h"

 */
   bool find(const string &store_path ,nodes_map &hd ,signature &source_signature ,int fds ,nodes_map::iterator &npi){
     nodes_map::iterator i = hd.begin();
     while( i != hd.end() ){
       if( source_signature == i->second.node_signature ){ // then we must check the files to be sure
         stringstream ss;
         ss << store_path << "/" << i->first;
         int fdn;
         fdn = open_read(ss);
         if( fdn == -1 ){ // pretty serious error as these are the archive node files
           cerr << "could not open archive node file for reading, skipping: " << ss.str() << endl;
           cerr << strerror(errno) << endl;
         } else {
           if(same(fds ,fdn)){
             npi = i; // npi points to the record found
             close(fdn);
             RETURN true;
           }
           close(fdn);
         }
       }
     i++;  
     }
     RETURN false;
   }


/*--------------------------------------------------------------------------------
  if the source file is not already in the archive, we add it to the archive
  if the source file is already in the archive, but its filepath is not, we add its filepath to the node set in the tax file
  if the source file modification time is older than that of the corresponding node, we update the mtime in the node set

  this routine calls 'find()' to see if the proposed file is already in the archive

*/
  const uint Insert_NotInserted =0;
  const uint Insert_Inserted = 1;
  const uint Insert_NameAllocationFailure  =2;
  const uint Insert_StorageFailure  =2;
  const uint Insert_NodesMapFailure = 3;

  uint insert_if_unique(  
     uint &unique_count   // this counter incremented if file is inserted in the archive
    ,const string &store_path // path to the node storage directory, the node storage directory holds the nodes (number named unique files)
    ,node_number_allocator &nna // provides the next number name for a file to be written into the node storage directory
    ,nodes_map &a_nodes_map  // in memory index for the node storage directory, will be updated should the proposed file be inserted
    ,const file_record &source_file_record  // file proposed for inclusion, descriptor class, the file name is in the pathname field
  ){
    bool inserted = false;
    int fds = open_read(source_file_record.pathname);
    signature source_signature;
    source_signature.sign(fds);
    nodes_map::iterator node_set_it;

    // check if already in archive
    //
      if(find(store_path ,a_nodes_map ,source_signature ,fds ,node_set_it)){ // if found sets node_set_it
        node_set_it->second.sources.insert(source_file_record);    
        if(node_set_it->second.mtime > source_file_record.mtime)  node_set_it->second.mtime = source_file_record.mtime;
      close(fds);
      RETURN Insert_NotInserted;
      }

    // insert file in archive
    //
      // insert node phrase in nodes_map
      unique_count++;
      inserted = true;
      node_set  np;
      if( !nna.alloc(np.node) ){
        cerr << "could not allocate a new node number" << endl;
        RETURN Insert_NameAllocationFailure;
      };
      np.mtime = source_file_record.mtime;
      np.sources.insert(source_file_record);
      np.node_signature = source_signature;
      a_nodes_map.insert(pair<size_t ,node_set>(np.node ,np));

      // copy source file into archive
      stringstream ss;
      ss << store_path << "/" << np.node;
      int fda = open_write( ss );
      if( fda == -1 ){ // oh no, we can't write into the archive
        cerr << "could not create node in the archive, exiting: " << ss.str() << endl;
        cerr << strerror(errno) << endl;
        RETURN Insert_StorageFailure;
      }
      if( !copy(fds ,fda) ){
        cerr << "copy of source file node failed! node: "
             << ss.str()  
             << " source file: \"" 
             << source_file_record.pathname 
             << "\""
             << endl;
        RETURN Insert_StorageFailure;
      }
    close(fda);
    close(fds);
    RETURN Insert_Inserted;
  }

/*--------------------------------------------------------------------------------
  Traverses the directory tree found at 'source_path'.  For each file that does
  not match an excluded pattern, it calls 'insert_if_unique(file)'.

    starts by calling nodes_map::parse to recover the node map from the sources file found
    at 'taxonomy_pathname'.

    tells the nodes_map object in memory to print an ascii imagege back to a tax/sources
    files.

 */
  const uint AI_Success = 0;
  const uint AI_OpenFail = 1;
  const uint AI_ParseFail = 2;
  const uint AI_SystemErr = 3;

  uint arch_insert(
     const string &taxonomy_pathname // path to the node index file that will become the nodes_map in memory
    ,const string &source_path // source files from here
    ,const string &store_path  // directory that holds the file 'nodes' - a node is a unique file with a number for a name
    ,const list<regex> &excludes // source files with names that match any of these regexs should not be put in the archive
    ,bool verbose // progress messgaes
    ,bool list_insert // individual file insert commands to be used for incremental backup on mirrors
  ){

    // open, parse into memory, and close, the sources file 
    //
      ifstream is(taxonomy_pathname);
      if( !is.good() ){
        cerr << "could not open taxonomy file: \"" << taxonomy_pathname << "\"" << endl;
        RETURN AI_OpenFail;
      }

      if(verbose) cout << "parsing taxonomy file: \"" << taxonomy_pathname << "\" to get the nodes_map.. " << endl;
      uint lineno=0;
      nodes_map a_nodes_map;
      if( a_nodes_map.parse(is ,taxonomy_pathname ,lineno) != ParseStatus::Found){ // filename passed for err messes
        cerr << "parse failed" << endl;
        RETURN AI_ParseFail;
      }
      if(verbose) cout << "parse complete" << endl;

      node_number_allocator nna;
      nna.parse(a_nodes_map);

      is.close();

    // makes list of source files
    //
      if(verbose) cout << "traversing source directory on disk.. ";
      file_record_list source_files; // see directory.h for file_record_list, file_records hold a lot of information about a file, including its name
      file_record_list link_targets; // nothing is done with this right now!  we need to hunt down the link targets
      list_files(source_path ,source_files ,link_targets, excludes);  // see directory.h, traverse source_path makes list of file names 'source_files'
      if(verbose) cout << "found " << source_files.size() << " files" << endl;

    // find and insert unique source files 
    //
      if(verbose) cout << "inserting files not already in the archive and not excluded" << endl;
      file_record_list::iterator i = source_files.begin();
      uint count = 0;
      uint unique_count = 0;
      uint return_code;
      while( i != source_files.end() ){
        if( verbose && count > 1 && (count % 1000) == 0) 
          cout << "examined: " << count << " inserted: " << unique_count << ".." << endl;
        return_code = insert_if_unique(unique_count ,store_path ,nna ,a_nodes_map ,*i);
        if( list_insert && return_code==Insert_Inserted ){
          cout << "insert-file \"" << i->pathname << "\"" << endl;
        } 
        if( return_code != Insert_Inserted && return_code != Insert_NotInserted){
          RETURN AI_SystemErr;
        }
      count++;
      i++;
      };
      if( verbose ) cout << "examined: " << count << " inserted: " << unique_count << endl;

    // write out the modified taxonomy map to disk
    //
      ofstream os(taxonomy_pathname);
      if( !os.good() ){
        cerr << "could not open taxonomy file for writing: \"" << taxonomy_pathname << "\"";
        RETURN AI_OpenFail;
      }
      a_nodes_map.print(os);

  RETURN AI_Success;
  }

/*--------------------------------------------------------------------------------

   This is called from the shell. See the Vogon poetry at the top of this file for
   the usage message.

   The goal is to insert files found in a source directory tree into the archive.

   1. This parses the command line and gets the options
   2. makes a tempory file of the tax/source, though this seems unnecessary as the next
      step only reads the tax/source temporary file
   3. calls 'arch_insert' to put the source files into the archive and to update the index
   4. writes the index back out to a new tax/source file

*/
  int main(int argc ,char **argv){

    //----------------------------------------
    // parse options
    // 
      list<char *> args;
      list<regex> excludes;
      bool verbose=false;
      bool list_insert=false;
      bool bad_parms=false;
      bool help=false;

      if(argv == 0){
        cerr << "serious problem here, argv was zero when the program was called" << endl;
        RETURN Exit_InternalError;
      }
      if( !strcmp(*argv, "insert") ){
        cerr << "insert: Does not a rose by any other name not smell just as sweet? Why have you renamed the program?" << endl;
      }
      if(argc == 1){
        cerr << vogon_poetry;
        RETURN Exit_BadParms;
      }

      for( argv++ ; *argv; argv++ ){
        // check for options
        //
          if( (*argv)[0] == '-' ){
            if( !strcmp(*argv, "-x") || !strcmp(*argv, "--exclude") ){
              argv++;
              if( *argv ){
                excludes.push_back(regex(*argv));
                CONTINUE;
              }
              cerr << "expected an ECMAscript regex after exclude option" << endl;
              bad_parms=true;
              CONTINUE;
            }

            if( !strcmp(*argv, "-h") || !strcmp(*argv, "--help") ){
              cout << vogon_poetry;
              bad_parms=true;
              CONTINUE;
            }

            if( !strcmp(*argv, "--list_insert") ){
              list_insert=true;
              CONTINUE;
            }

            if( !strcmp(*argv, "-v") || !strcmp(*argv, "--verbose") ){
              verbose=true;
              CONTINUE;
            }

            bad_parms=true;
            cerr << "unrecognized option: " << *argv << endl;
            CONTINUE;
          }

        // if it isn't and option, it is an arg
        //
          args.push_back(*argv);
          CONTINUE;
      }

  //----------------------------------------
  // pull out the program arguments: the archive and source paths
  //    see file.h for blaze_path - makes directories along the whole path if needed
  // 
    if( args.size() != 2 ){
      cerr << "need two arguments, but found " << args.size() << " argument";
      if(args.size() != 1)  cerr << "s";
      cerr << endl;
      RETURN Exit_BadParms;
    }

    string arch_path = args.front();
    strip_trailing(arch_path);

    string source_path = args.back();
    strip_trailing(source_path);

    string tax_path = arch_path;
    tax_path += "/tax";
    if( !blaze_path(tax_path) ){ 
      cerr << "couldn't create archive tax directory: \"" << tax_path << "\"" << endl; 
      bad_parms = true;
    }

    string store_path = arch_path;
    store_path += "/store";
    if( !blaze_path(store_path) ){ 
      cerr << "couldn't create archive store directory: \"" << source_path << "\"" << endl; 
      bad_parms = true;
    }

    if(bad_parms){
      cerr << "errors when parsing parameters, nothing done" << endl;
      RETURN Exit_BadParms;
    }

  //----------------------------------------
  // add version if not present or check version  ...... to be added here ..
  //

  //----------------------------------------
  // make a temporary file to hold updates to the source taxonomy while we are working
  //   see file.h for exists(), copy(), etc.
  //
    stringstream temp_tax_pathname;
    temp_tax_pathname << tax_path <<  "/sources-" << getpid();
    if( exists(temp_tax_pathname) ){
      if( unlink( temp_tax_pathname.str().c_str()) == -1 ){
        cerr << "temporary file \"" << temp_tax_pathname.str() << "\" already exists and can't be deleted, exiting." << endl;
        RETURN 1;
      }
    }

    /*
      We make a copy of the source taxonomy and work on that.  If all goes well, we will
      copy it back.  However, if we stop in an intermediate state, new nodes may have
      been created in store, and those would still need to be cleaned up.
    */
      stringstream original_taxonomy_pathname;
      original_taxonomy_pathname  << tax_path << "/sources" << ";" << 0;
      int fd0, fd1;
      if( exists(original_taxonomy_pathname) ){
        if( !copy( original_taxonomy_pathname, temp_tax_pathname ) ){
          cerr << "error making working version of source taxonomy, exiting" << endl;
          RETURN 1;
        }
      }else{
        if( !touch( temp_tax_pathname ) ){
          cerr << "error making working version of source taxonomy, exiting" << endl;
          RETURN 1;
        }
      }

  //----------------------------------------
  // call this routine to do the heavy lifting:
  //
    if( verbose ){
      cout << "sourcing files from: \"" << source_path << "\"" << endl;
      cout << "placing nodes in store at: \"" << store_path << "\"" << endl;
    }
    if( arch_insert(temp_tax_pathname.str() ,source_path ,store_path ,excludes ,verbose ,list_insert) != AI_Success){
      cerr << "Internal error when inserting into archive. Check for extraneous temp files and nodes." << endl;
      RETURN Exit_InternalError;
    }

  //----------------------------------------
  // move the temporary sources tax file to a permanent file
  //
    stringstream a_pathname;
    if( exists( original_taxonomy_pathname ) ){
      a_pathname << tax_path << "/sources"; // original_taxonomy_pathname_not_versioned 
      rev(a_pathname.str());  // increases rev number on all the tax_path/sources files on disk, making space for version 1
    }
    if(verbose) cout << "writing nodes_map back to: " << original_taxonomy_pathname.str() << endl;
    if( !copy(temp_tax_pathname, original_taxonomy_pathname) ){
      cerr << "all done, but not allowed to copy the temp file back.." << endl;
    }
    unlink(temp_tax_pathname.str().c_str());


  RETURN Exit_NoError;
  }
