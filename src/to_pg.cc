/*
  Copies the archive to a PostgreSQL database.

*/

// before we start, a bit of Vogon poetry:
//
  const char *vogon_poetry = R"VOGON_POETRY(
     to_pg <archive> <user> <db>

    <archive> the name of the archive
         <user> the database user
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

PGconn *open_pg(const string &user, const string&db){
  stringstream query;
  query << "user=" << user << " " << "dbname=" << db;
      
  PGconn *conn = PQconnectdb(query.str().c_str());
  if (PQstatus(conn) == CONNECTION_BAD) {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    return 0;
  }
  return conn;
}

/*--------------------------------------------------------------------------------
  Writes the taxonomy and node files to a postgres db.

 */
  const uint PG_Success = 0;
  const uint PG_OpenFail = 1;
  const uint PG_ParseFail = 2;
  const uint PG_SystemErr = 3;

  uint tax_to_pg(
     const string &taxonomy_pathname 
     ,PGconn *conn
  ){
    // open, parse into memory, and close, the sources file 
    //
      ifstream is(taxonomy_pathname);
      if( !is.good() ){
        cerr << "could not open taxonomy file: \"" << taxonomy_pathname << "\"" << endl;
        RETURN PG_OpenFail;
      }

      cout << "parsing taxonomy file: \"" << taxonomy_pathname << "\" to get the nodes_map.. " << endl;
      uint lineno=0;
      nodes_map a_nodes_map;
      if( a_nodes_map.parse(is ,taxonomy_pathname ,lineno) != ParseStatus::Found){ // filename passed for err messes
        cerr << "parse failed" << endl;
        RETURN PG_ParseFail;
      }
      cout << "parse complete" << endl;
      is.close();

    // copy the taxonomy map to the db
    //
      cout << "writing db" << endl;
      stringstream query;
      PGresult *res;
      nodes_map::iterator nm_it = a_nodes_map.begin();
      while( nm_it != a_nodes_map.end() ){
        node_set &ns = nm_it->second; // the sources list
        size_t node = nm_it->second.node;

        query
          << "INSERT INTO arch_nodes (node ,mtime ,signature) VALUES ("
          << dec << node
          << " ," << nm_it->second.mtime
          << " ,"
          << "'";
        nm_it->second.node_signature.print(query);
        query 
          << "');";
        res = PQexec(conn, query.str().c_str());
        query.str(std::string());
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
          cerr << PQresultErrorMessage(res) << endl;
        }

        file_record_list::iterator fr_it = nm_it->second.sources.begin();
        file_record_list::iterator fr_end = nm_it->second.sources.end();
        while( fr_it != fr_end ){
          query
            << "INSERT INTO arch_source (node ,mtime ,pathname) VALUES ("
            << dec << node
            << " ," << fr_it->mtime 
            << " ,'" << fr_it->pathname << "'"
            << ");";
          res = PQexec(conn, query.str().c_str());
          query.str(std::string());
          if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            cerr << PQresultErrorMessage(res) << endl;
          }
          fr_it++;
        }
        nm_it++;
      }
  PQfinish(conn);
  RETURN PG_Success;
  }

/*--------------------------------------------------------------------------------

   This is called from the shell. See the Vogon poetry at the top of this file for
   the usage message.

   The goal is to insert files found in a source directory tree into the archive.

   1. This parses the command line and gets the options
   2. makes a tempory file of the tax/source, though this seems unnecessary as the next
      step only reads the tax/source temporary file
   3. calls 'to_pg' to put the source files into the archive and to update the index
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

            if( !strcmp(*argv, "-h") || !strcmp(*argv, "--help") ){
              cout << vogon_poetry;
              bad_parms=true;
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
    if(bad_parms){
      cerr << "errors when parsing parameters, nothing done" << endl;
      RETURN Exit_BadParms;
    }

  //----------------------------------------
  // pull out the program arguments: the archive and source paths
  //    see file.h for blaze_path - makes directories along the whole path if needed
  // 
    if( args.size() != 3 ){
      cerr << "need three arguments, but found " << args.size() << " argument";
      if(args.size() != 1)  cerr << "s";
      cerr << endl;
      RETURN Exit_BadParms;
    }
    string arch_path = args.front();
    args.pop_front();
    string user = args.front();
    args.pop_front();
    string db = args.front();
    args.pop_front();
    strip_trailing(arch_path);

    stringstream taxonomy_pathname;
    taxonomy_pathname << arch_path << "/tax/sources" << ";" << "0";
    if( !exists(taxonomy_pathname.str()) ){
      cerr << "taxonomy_pathname not found: " << "\"" << taxonomy_pathname.str() << "\"" << endl;
      RETURN 1;
    }

    string store_path = arch_path;
    store_path += "/store";
    if( !exists(store_path) ){
      cerr << "store path not found: " << "\"" << store_path << "\"" << endl;
      RETURN 1;
    }

  //----------------------------------------
  // open the database
  //
    PGconn *conn = open_pg(user, db);
    if( !conn ){
      cerr << "attempt to connect to specified database failed" << endl;
      RETURN 1;
    }

  // first move the taxonomy the db, then move the store contents
  //
    if( tax_to_pg(taxonomy_pathname.str(), conn) != PG_Success){
      cerr << "Error transfering tax to pq" << endl;
      RETURN 1;
    }

  RETURN Exit_NoError;
  }
