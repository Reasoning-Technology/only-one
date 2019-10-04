/*
This test reads the test nodes_map and creates the free node number lists


*/

#include <string>
#include <sstream>
#include <fstream>
using namespace std;


#include "taxonomy.h"
 

int main(int argc ,char **argv){

  uint errors=0;
  nodes_map hd;

  string nodes_map_input_file_path("test_nodes_map_2_in.txt");
  ifstream is(nodes_map_input_file_path);
  uint lineno=0;
  if( hd.parse(is ,nodes_map_input_file_path ,lineno) != ParseStatus::Found){
    errors++;
    cerr << "nodes_map parse failed" << endl;
  }
  is.close();
 
  node_number_allocator nna;
  nna.parse(hd);

  string expected_out = "[1 1] [6 6] [9 10] [12 +INF]";
  stringstream ss;

  nna.print(ss);

  if( ss.str() != expected_out){
    errors++;
    cerr << "free list mismatch" << endl;
  }

  if(errors)
    cerr << "test failed" << endl;
  else
    cerr << "test passed" << endl;

  RETURN errors;
}
