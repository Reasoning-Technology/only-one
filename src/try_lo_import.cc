//g++ -lpq try_lo_import.cc -o try_lo_import

#include <errno.h>
#include <string.h>
#include <string>
#include <sstream>
#include <iostream>
#include<libpq-fe.h>

using namespace std;

int main(int argc, char **argv){

  stringstream query;
  query << "user=" << argv[1] << " " << "dbname=" << argv[2];
  PGconn *conn = PQconnectdb(query.str().c_str());
  if (PQstatus(conn) == CONNECTION_BAD) {
    fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
    PQfinish(conn);
    return 0;
  }

  PQexec(conn, "BEGIN TRANSACTION");
  const char *filename = "test_archive/store/2";
  Oid file_id = lo_import(conn, filename);
  if( file_id == InvalidOid){
    cerr << PQerrorMessage(conn) << endl;
  }
  PQexec(conn, "COMMIT");
  PQfinish(conn);

}

/*
  postgres=# select * from pg_largeobject;
   loid | pageno | data 
  ------+--------+------
  (0 rows)
*/
