
#ifndef PHRASE_H
#define PHRASE_H

#include "utils.h"
#include "types.h"
#include "parse_status.h"
#include <sstream>
#include <iostream>
#include <string>
#include <list>
using namespace std;

// basically just does a getline from is into a stringstream
// for single line phrase control, i.e. parse1 style. the verbatim phrase line is returne din phrase_stream
// for line pair control style, i.e. parse2 style, the control line returned in phrase_stream, and the data line in data_stream
ParseStatus getphraseline( istream &is ,size_t &lineno ,string input_filename ,stringstream &phrase_stream ,stringstream &data_stream ){
  string phrase;
  string data;

  if( !getline(is, phrase) ) RETURN ParseStatus::NotFound;
  lineno++;

  if( phrase.length() == 0 ) RETURN ParseStatus::NullObject;

  phrase_stream.clear();
  phrase_stream.str(phrase);

  if(phrase[0] == '.'){

    string data_line;
    if( !getline(is, data_line) ){
      cerr << input_filename << ":" << lineno << " data line expected after top control but not found " << endl;
      RETURN ParseStatus::Malformed;
    }
    lineno++;

    data_stream.clear();
    data_stream.str(data_line);
  }

  RETURN ParseStatus::Found;
}


/*--------------------------------------------------------------------------------
  a phrase is a typed sequence of string tokens.  The first token is the type.

*/
  class phrase : public list<string> {
      public:
      string comment;

      void clear(){
        list<string>::clear();
        comment = "";
      }

      void print(ostream &os) const {
        const_iterator it = begin();
        if( it == end()) RETURN;
        print_token(os ,*it);
        it++;
        while(it != end()){ os << " "; print_token(os ,*it); it++;}
        if( comment.length() > 0 ) os << " " << comment;
        os << endl;
      }

      iterator first(){
        RETURN begin();
      }

      iterator rest(){ // removes head, destructive
        iterator the_tail = begin();
        if(the_tail != end()) the_tail++;
        RETURN the_tail;
      }

      ParseStatus parse(stringstream &iphrase ,stringstream &idata , const string &pathname ,uint lineno ,stringstream &err) {
        // cout << "parsing text for phrase: " << iphrase.str() << endl;
        if( iphrase.eof() ) RETURN ParseStatus::NullObject;
        if( iphrase.str()[0] == '.' )
          RETURN parse2( iphrase ,idata ,pathname ,lineno ,err);
        else
          RETURN parse1( iphrase ,pathname ,lineno ,err);
      }

  protected:
     
      bool is_printable_ASCII( unsigned char ch) const {
        RETURN
          ch >=0x21 && ch <=0x7E && ch != '(' && ch != ')' && ch != '`' && ch != '\'' && ch != '\"'
          || ch >= 0x80 && ch <= 0xFE  && ch != 0xF9  && ch != 0xFA
          ;
      }

      void print_token( ostream &os , const string &tok ) const {
        string::const_iterator it = tok.begin();
        while( it != tok.end() && is_printable_ASCII(*it) ) it++;
        if( it == tok.end()){ // then all characters in the token are printable
          os << "# " << tok;
        }else{
          os << "#" << tok.length() << " ";
          os.write( tok.c_str() ,tok.length() );
        }
      }

      /* 
        on is:
         '# ' command means next token is white space delineated
         '#nn..n'  command means next token comes from field of length 'nn..n'
          a #0 command will be discarded (will be lost if parse then print)
          ; semicolon means the remainder is a comment
        pathname, lineno, and err are for reporting errors
      */
      ParseStatus parse1(istream &is , const string &pathname ,uint lineno ,stringstream &err) {
        string control;
        string data;
        size_t data_length;
        stringstream ss;
        char sep;

        #if 0
          cout << "phrase parsing this: ";
          printis(is);
          cout << endl;
       #endif

        bool found_control = false;
        while( is >> control ){
          if( control == string("#") ){
            found_control = true;
            if( !(is >> data) ){
              err << pathname << ":" << lineno << " found \'#\' but no following data" << endl;
              RETURN ParseStatus::Malformed;
            }
            push_back(data);
          } else if( control[0] == '#' ){
            found_control = true;
            control.erase(0 ,1);
            ss.str(control);
            ss.clear();
            if( !(ss >> data_length)){
              err << pathname << ":" << lineno << " found \'#\' but following token is not a number" << endl;
              RETURN ParseStatus::Malformed;
            }
            if( data_length > 0){
              char *buf = new char[data_length+1];
              is.get(buf ,2);
              if( buf[0] != ' '){
                err << pathname << ":" << lineno << " space should follow #nn..n phrase control command" << endl;
                RETURN ParseStatus::Malformed;
              }
              is.get(buf ,data_length + 1);
              push_back( string(buf) );
              delete buf;
            }
          } else if( control[0] == ';' ){
            found_control = true;
            comment += control;
            int ch;
            while( (ch = is.get()) != -1){
              comment += ch;
            }
          } else{
            err << pathname << ":" << lineno << " unrecognized phrase control command" << endl;
            RETURN ParseStatus::Malformed;
          }
        };
        if( data.size() > 0 ) RETURN ParseStatus::Found;
        if( found_control ) RETURN ParseStatus::NullObject; // a line with one or more '#0' commands and comments only
        RETURN ParseStatus::NotFound;
      };

      /* 
        This version of parse uses a top control channel. 

        Control starts with a dot and a space.  The first two characters of the data channel are ignored.

        After the control start, as noted above, a continguious dot stream indicates a token.  dot streams
        are separated by spaces.
        
        if the data stream stops but there are more dots, we take the missing data to be spaces

        if the data stream continues beyond the control stream then a) it starts with a ';' the data
        is taken as a comment.  Otherwise we signal an error as the data will be lost when the header
        is written out again.

        pathname, lineno, and err are for reporting errors
      */ 
      ParseStatus parse2(istream &icontrol ,istream &idata , const string &pathname ,uint lineno ,stringstream &err) {
        int ch_data;
        int ch_control;
        if(    !get_ch(icontrol ,idata ,ch_control ,ch_data) || ch_control != '.'
            || !get_ch(icontrol ,idata ,ch_control ,ch_data) || ch_control != ' '
        ){
          err << pathname << ":" << lineno << "control channel start should be a period followed by a space over a data channel of at least two chars" << endl;
          RETURN ParseStatus::Malformed;
        }

        bool found_tok = false; // part of a bug fix, there might be a better way to do this ..
        string token;
        while( get_ch(icontrol ,idata ,ch_control ,ch_data) ){

          if( ch_control == ' ' && !collapse_spaces(icontrol ,idata ,ch_control ,ch_data) ) BREAK;

          token = "";
          if( ch_control == '.' && !gather_token(icontrol ,idata ,ch_control ,ch_data ,token) ){
            push_back(token);
            found_tok = true;
            BREAK;
          }
          push_back(token);
                        
          if( ch_control != ' '  && ch_control != '.'){
            err << pathname << ":" << lineno << " unrecognized top line control character" << endl;
            RETURN ParseStatus::Malformed;
          }

        }
        if( ch_data != -1 ){
          while( (ch_data = idata.get()) == ' ' );
          if( ch_data == ';' ){
            comment += ch_data;   
            while( (ch_data = idata.get()) != -1 ) comment += ch_data;
          } else {
            err << pathname << ":" << lineno << " data continues beyond control" << endl;
            RETURN ParseStatus::Malformed;
          }
        }
        if( found_tok ) RETURN ParseStatus::Found;
        RETURN ParseStatus::NotFound;
      }
         
      // ch_data and ch_control must have an initial value before calling this routine
      // we pad the trailing end of the data channel with spaces when control says it continues but it doesn't
      bool get_ch(istream &icontrol ,istream &idata , int &ch_control ,int &ch_data){
        if(ch_control == -1) RETURN false; // makes the get_ch routine stable at the end of the data stream
        ch_control = icontrol.get();
        if(ch_data != -1) ch_data = idata.get();
        if( ch_control == '.'  && ch_data == -1){
          ch_data = ' ';
          RETURN true; 
        }
        return ch_control != -1;
      }

      bool collapse_spaces(istream &icontrol ,istream &idata ,int &ch_control ,int &ch_data){
        bool got_char_flag;
        while( ch_control == ' ' && (got_char_flag = get_ch( icontrol ,idata ,ch_control ,ch_data)) );
        return got_char_flag;
      }

      bool gather_token(istream &icontrol ,istream &idata ,int &ch_control ,int &ch_data ,string &token){
        bool got_char_flag = true;
        while( ch_control == '.' ){
          token += ch_data;
          if( !(got_char_flag=get_ch( icontrol ,idata ,ch_control ,ch_data)) ) BREAK;
        }
        return got_char_flag;
      }

    };

#endif
