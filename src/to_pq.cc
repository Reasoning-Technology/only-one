/*
  to_db <archive> <db>

*/

// before we start, a bit of Vogon poetry:
//
  const char *vogon_poetry = R"VOGON_POETRY(
     to_db <archive> <db>

    <archive> the name of the archive
         <db> a database, schema already setup

    options:

      -h --help            this message
         --list_insert     prints 'insert-file <filename>' on cout for each file included in the archive
      -v --verbose         progress information, does not do --list_insert as that can get very long

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

#include<libpq-fe.h>

// local objects used
#include "file.h"
#include "directory.h"
#include "taxonomy.h"



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

  uint to_db(
     const string &taxonomy_pathname 
  ){
    // open, parse into memory, and close, the sources file 
    //
      ifstream is(taxonomy_pathname);
      if( !is.good() ){
        cerr << "could not open taxonomy file: \"" << taxonomy_pathname << "\"" << endl;
        RETURN AI_OpenFail;
      }

      cout << "parsing taxonomy file: \"" << taxonomy_pathname << "\" to get the nodes_map.. " << endl;
      uint lineno=0;
      nodes_map a_nodes_map;
      if( a_nodes_map.parse(is ,taxonomy_pathname ,lineno) != ParseStatus::Found){ // filename passed for err messes
        cerr << "parse failed" << endl;
        RETURN AI_ParseFail;
      }
      cout << "parse complete" << endl;
      is.close();

    // copy the taxonomy map to the db
    //
      

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
