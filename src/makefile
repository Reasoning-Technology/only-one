


HFILES= $(wildcard *.h)
EXECS= insert test_phrase_1 test_phrase_2 test_nodes_map_1 test_nodes_map_2 try_md5

GCC= g++ -std=c++11 -g -lssl -lcrypto

all: insert test

install: insert
	mv insert ../bin

dist-clean:
	rm $(EXECS)

#arch_restore: arch_restore.cc $(HFILES) 
#	$(GCC) arch_restore.cc -o arch_restore

insert: insert.cc $(HFILES) 
	$(GCC) insert.cc -o insert

test_phrase_1: test_phrase_1.cc $(HFILES) 
	$(GCC) test_phrase_1.cc -o test_phrase_1

test_phrase_2: test_phrase_2.cc $(HFILES) 
	$(GCC) test_phrase_2.cc -o test_phrase_2

test_nodes_map_1: test_nodes_map_1.cc $(HFILES) 
	$(GCC) test_nodes_map_1.cc -o test_nodes_map_1

test_nodes_map_2: test_nodes_map_2.cc $(HFILES) 
	$(GCC) test_nodes_map_2.cc -o test_nodes_map_2

try_md5: try_md5.cc
	$(GCC) -o try_md5 try_md5.cc
	-rm try_md5.out
	./try_md5 try_md5.cc >& try_md5.out
	diff try_md5.out try_md5.out_expected

test: test_phrase_1 test_phrase_2 test_nodes_map_1 test_nodes_map_2 insert
	- rm test_phrase_1_out.txt test_phrase_2_out.txt test_nodes_map_1_out.txt test_insert_out.txt
	./test_phrase_1
	diff test_phrase_1_out.txt test_phrase_1_out.txt_expected 
	./test_phrase_2
	diff test_phrase_2_out.txt test_phrase_1_out.txt_expected # test has different input format, but output same as for test_phrase_1
	./test_nodes_map_1
	diff test_nodes_map_1_out.txt test_nodes_map_1_out.txt_expected
	./test_nodes_map_2
	./test_insert.sh >& test_insert_out.txt
	diff test_insert_out.txt test_insert_out.txt_expected




