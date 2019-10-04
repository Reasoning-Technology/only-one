
#include "utils.h"
#include "phrase.h"
#include "types.h"
#include "parse_status.h"
#include "file.h"

#include <string>
#include <iostream>
#include <fstream>
using namespace std;

int main(int argc ,char **argv){
  uint errors=0;
  stringstream err;

  phrase a_phrase;
  string data_line;
  stringstream phrase_stream;
  stringstream data_stream;
  size_t lineno;

  string input_filename = string("test_phrase_1_in.txt");
  ifstream is(input_filename.c_str());
  int fd0;

  string output_filename("test_phrase_1_out.txt");
  ofstream os(output_filename.c_str());
  int fd1;

  lineno=0;
  ParseStatus stat;
  while( !((stat=getphraseline(is ,lineno ,input_filename ,phrase_stream ,data_stream)).value & ParseStatus::NotFound) ){
    if(stat.value & ParseStatus::NullObject){
     cerr << input_filename << ":" << lineno << " line is blank, will be disgarded - this will cause the file compare to fail" << endl;
     CONTINUE;
    }
    if(stat.value & ParseStatus::Malformed){
     cerr << input_filename << ":" << lineno << " malformed input" << endl;
     errors++;
     CONTINUE;
    }
    a_phrase.clear();
    if( a_phrase.parse(phrase_stream ,data_stream ,input_filename ,lineno ,err) != ParseStatus::Found){
      errors++;
      cerr << err.str();
      cerr << input_filename << ":" << lineno << " phrase parse failed" << endl;
    }
    //    a_phrase.print(cout);
    a_phrase.print(os);
  }

  is.close();
  os.close();
  fd0 = open_read(input_filename);
  fd1 = open_read(output_filename);

  if( !same(fd0,fd1)){
  errors++;
  cerr << "input and output headers differ" << endl;
  }


  is.close();
  if(errors != 0) cerr << "test failed, there were errors" << endl;
  else cerr << "test passed" << endl;

  RETURN errors;
}
