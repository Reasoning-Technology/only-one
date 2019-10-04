
#ifndef TAXONOMY_H
#define TAXONOMY_H


/*
 defines classes:
    node_set

 provides functions:
   parser for nodes_map 

 the node number allocator
*/


// for the md5 stuff
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// STL objects used
#include <iostream>
#include <string>
#include <sstream>
#include <list>
#include <set>
#include <map>


// locally defined objects
#include "types.h"
#include "directory.h"
#include "parse_status.h"
#include "phrase.h" 
#include "file.h"

const uint VERSION = 27;

using namespace std;



/* --------------------------------------------------------------------------------
  A node phrase consists of a node followed by one or more taxonomy co-ordinates 
  This is also called a 'node set'.

*/
  class node_set {
  public:
    size_t node;  // file name in store according to the node phrase of the node_set
    time_t mtime; // modify time for the node according to the node phrase
    signature node_signature;  // checksum for the node according to the node phrase

    file_record_list sources;  // the source phrases from this node_set
    list<phrase> other_phrases;

    void clear(){
      node=0;
      mtime=0;
      sources.clear();
      other_phrases.clear();
    }

    void print(ostream &os) const{

      // first we make a phrase from the node information, then print the phrase
      //
        phrase a_phrase; 

        a_phrase.push_back(string("node"));

        stringstream node_path;
        node_path << node;  // converts node number to a string
        a_phrase.push_back(node_path.str());

        stringstream mtime_stream;
        mtime_stream << mtime;  // converts mtime time_t to a string
        a_phrase.push_back(mtime_stream.str());

        stringstream node_signature_stream;
        node_signature.print(node_signature_stream);
        a_phrase.push_back(node_signature_stream.str());

        a_phrase.print(os);

      // then print the source phrases:
      //
        string source_tag("source");
        sources.print(os ,source_tag);
        list<phrase>::const_iterator it = other_phrases.begin();
        while( it != other_phrases.end() ){
          it->print(os);
          os << endl;
        it++;
        }

    }

    /*
       initializes a node_set instance by parsing off of 'is'

         arguments:
          'is' is the input stream, i.e. an open file
          'iput_filename' is for error messages
           'lineno' is the current line number, which 

         returns ParseStatus

         side effect of initializing the node_set data, node, mtime, .. etc.

       first phrase must be a node phrase
       each phrase set ends at next node phrase or at EOF

       This routine does not return immediately on a malformed parse, but rather should
       look for the next node phrase then returns, so that there are not a bunch of
       'first phrase not node phrase' errors on the next parse.
    */
    ParseStatus parse(istream &is ,const string &input_filename ,size_t lineno){

      streampos sp;

      uint errors = 0;
      stringstream err;
      uint tok_count;
      ParseStatus stat;

      phrase a_phrase;
      string data_line;
      stringstream phrase_stream;
      stringstream data_stream;
      stringstream ss;

      // phrase set must start with a node phrase
      //    # node # <number> # <mtime> # <signature> ; the last three fields constitute a file descriptor
      //
        if( (stat=getphraseline(is ,lineno ,input_filename ,phrase_stream ,data_stream)) != ParseStatus::Found ){
          RETURN stat; 
        }

        lineno++;
        stat = a_phrase.parse(phrase_stream ,data_stream ,input_filename ,lineno ,err);
        phrase::iterator pit = a_phrase.first();
        if( stat != ParseStatus::Found || a_phrase.size() != 4 || *pit != string("node")){
          if( stat != ParseStatus::Found) cerr << err.str();
          cerr << input_filename << ":" << lineno << " malformed nodes_map, first phrase is not a node phrase" << endl;
          RETURN ParseStatus::Malformed;
        }
        pit++;
        ss.clear();
        ss.str(*pit);
        if( !(ss >> node) ){
          cerr << input_filename << ":" << lineno << " malformed node number field in node phrase" << endl;
          RETURN ParseStatus::Malformed;
        }
        pit++;
        ss.clear();
        ss.str(*pit);
        if( !(ss >> mtime) ){
          cerr << input_filename << ":" << lineno << " malformed mtime field in node phrase" << endl;
          RETURN ParseStatus::Malformed;
        }

        pit++;
        if( !node_signature.parse(*pit) ){
          cerr << input_filename << ":" << lineno << " malformed signature field in node phrase" << endl;
          RETURN ParseStatus::Malformed;
        }


      // then continues with source phrases or other phrases (but not a node phrase, as that would start the next set)
      //
        sp = is.tellg(); // so we can unwrap should we hit the next "node" phrase
        while( !((stat=getphraseline(is ,lineno ,input_filename ,phrase_stream ,data_stream)).value & ParseStatus::NotFound) ){
          if(stat.value & ParseStatus::NullObject){
            CONTINUE;
          }
          if(stat.value & ParseStatus::Malformed){
           cerr << input_filename << ":" << lineno << " malformed input" << endl;
           errors++;
           CONTINUE;
          }

          a_phrase.clear();
          if( (stat=a_phrase.parse(phrase_stream ,data_stream ,input_filename ,lineno ,err)) != ParseStatus::Found){
            if( stat.value & (ParseStatus::NotFound | ParseStatus::NoObject | ParseStatus::NullObject) ){
              cerr << err.str();
              cerr << input_filename << ":" << lineno << " null phrase or line comment dropped (not supported)" << endl;
              sp = is.tellg();
              CONTINUE;
            }
            errors++;
            cerr << err.str();
            cerr << input_filename << ":" << lineno << " phrase parse failed" << endl;
            CONTINUE;
          }

          // another node set terminates the parse for this node set
          if(a_phrase.front() == string("node")){
            is.seekg(sp);
            is.clear();
            RETURN ParseStatus::Found;
          }

          // # source # <filename> # <mtime>  ; we do not need the checksum as all files in the nodeset are identical a source is just naming the file
          if(a_phrase.front() == string("source")){
            a_phrase.pop_front();
            file_record fr_arch;
            if( fr_arch.parse(a_phrase) == ParseStatus::Found ){
              sources.insert(fr_arch);
              sp = is.tellg();
              CONTINUE;
            }else{
              cerr << input_filename << ":" << lineno << " malformed source phrase - skipped" << endl;
              sp = is.tellg();
              CONTINUE;
            }
          }

          other_phrases.push_back(a_phrase);
          sp = is.tellg();
        }
        RETURN ParseStatus::Found;
    }

    // node numbers are unique
    bool operator < (const node_set &other) const {
      RETURN node < other.node;
    }
  };

/*--------------------------------------------------------------------------------
  
  Parses a taxonomy file into a map, where the key to the map is the node number (which is
  also the filename under store), and the value found in the map is the node_set for that
  node. 

  This is the top level parse for a taxonomy file.

  --compares might go faster if we mapped the node size rather than the node number ..
*/
  class nodes_map : public map<size_t ,node_set>{
  public:

    void print(ostream &os){
      iterator it = begin();
      while(it != end()){
        it->second.print(os);
        os << endl;
      it++;
      }
    };

    /*
       is is the open file we are parsing from.
       nodes_map_file_path is a string for error messages.
       lineno is the starting line.  .. it doesn't look to be implemented properly ..
    */
    ParseStatus parse(istream &is , const string &nodes_map_file_path ,size_t lineno){
      uint MaxMisparseCount = 10;

      node_set a_node_set;
      uint misparse_count=0;
      ParseStatus stat;
      while(!is.eof()){
        a_node_set.clear();
        if( (stat=a_node_set.parse(is ,nodes_map_file_path ,lineno)) == ParseStatus::Found)
          insert(pair<size_t ,node_set>(a_node_set.node ,a_node_set));
        else if(stat.value && (ParseStatus::NotFound | ParseStatus::NoObject | ParseStatus::NullObject) ) CONTINUE;
        else{
          misparse_count++;
          cerr << nodes_map_file_path << ":" << lineno << " misparsed nodeset skipped" << endl;
          if(misparse_count == MaxMisparseCount) RETURN ParseStatus::MaxErrors;
        }
      }
      if(misparse_count == 0) RETURN ParseStatus::Found;
      else RETURN ParseStatus::Malformed;
    }

  };

/*--------------------------------------------------------------------------------
  node number allocator

*/
  class node_number_allocator {
  public:
    const static size_t max_allocation = (size_t)(-1) >> 3;

    size_t max_node;
    set<size_t> free_nodes;

    void print(ostream &os){
      size_t next_in_sequence;
      set<size_t>::iterator it = free_nodes.begin();
      if( it != free_nodes.end() ){
        os << "[" << *it;  // open first interval
        next_in_sequence = *it + 1;
      it++;
      }
      bool within_sequence = true;
      while( it != free_nodes.end() ){
        if( *it == next_in_sequence ){
          next_in_sequence++;
        }else{      
          os << " " << next_in_sequence-1 << "]"; // close prior interval
          os << " [" << *it;  // open new interval
          next_in_sequence = *it + 1;
        }
      it++;
      }
      os << " " << next_in_sequence-1 << "]"; // close last interval
      os << " [" << max_node+1 << " +INF]";
    }

    void parse(const nodes_map &hd){
      max_node=0; // zero is not a valid node number
      size_t next_in_sequence = 1;
      nodes_map::const_iterator it = hd.begin();
      while(it != hd.end()){
        if( it->first > max_node ) max_node = it->first;
        while( next_in_sequence != it->first){
          free_nodes.insert(next_in_sequence);
        next_in_sequence++;
        }
      next_in_sequence++;
      it++;
      };
    };

    bool alloc( size_t &node_number){
      if(free_nodes.empty()) {
        if(max_node == max_allocation){ RETURN false;}
        max_node++;
        node_number = max_node;
      }else{
        set<size_t>::iterator it = free_nodes.begin();
        node_number = *it;
        free_nodes.erase(it);
      }
      RETURN true;
    }    

    bool dealloc(size_t &node_number){
      if(node_number == 0) RETURN false;
      if(node_number > max_node) RETURN false;
      if(node_number == max_node && max_node==1){
        if(!free_nodes.empty()){
          cerr << "taxonomy.h:dealloc internal error" << endl;
        }
        max_node=0;
        RETURN true;
      }
      if(node_number < max_node){
        free_nodes.insert(node_number);
      }
      // else node_number == max_node and max_node is > 1
      max_node--;
      set<size_t>::iterator it;
      while( max_node != 0 && (it=free_nodes.find(max_node)) != free_nodes.end()){
        max_node--;
        free_nodes.erase(it);
      }
      RETURN true;
    }

  };


#endif
