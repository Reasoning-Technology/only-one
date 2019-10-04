


#include <string>
#include <iostream>
#include <fstream>
using namespace std;


#include "file.h"
#include "taxonomy.h"
 

int main(int argc ,char **argv){

  uint errors=0;
  nodes_map hd;

  string nodes_map_input_file_path("test_nodes_map_1_in.txt");
  ifstream is(nodes_map_input_file_path);
  uint lineno=0;
  if( hd.parse(is ,nodes_map_input_file_path ,lineno) != ParseStatus::Found){
    errors++;
    cerr << "nodes_map parse failed" << endl;
  }
  is.close();
 
  string nodes_map_output_file_path("test_nodes_map_1_out.txt");
  ofstream os(nodes_map_output_file_path);
  hd.print(os);
  os.close();

  int fd0 = open_read(nodes_map_input_file_path);
  int fd1 = open_read(nodes_map_output_file_path);

  if( !same(fd0,fd1)){
    errors++;
    cerr << "input and output nodes_maps differ" << endl;
  }
  close(fd0);
  close(fd1);

  if(errors != 0) cerr << "test failed, there were errors" << endl;
  else  cerr << "test passed" << endl;

  RETURN errors;
}
