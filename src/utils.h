#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <sstream>
#include <string>
using namespace std;

#include "types.h"

void printss( stringstream &ss ){
  cout << ss.str() << endl;
}

void printis( istream &is){
  if( !is ) RETURN;
  streampos spot = is.tellg();
  cout << is.rdbuf();
  is.clear();
  is.seekg(spot);
  is.clear();

}

#endif
