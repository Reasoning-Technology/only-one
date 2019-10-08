HFILES= $(wildcard *.h)
EXEC= to_pg insert libpq_version pq_version
EXEC_TEST= test_phrase_1 test_phrase_2 test_nodes_map_1 test_nodes_map_2
EXEC_TRY=  try_md5

GCC= g++ -std=c++11 -g -lssl -lcrypto 

all: $(EXEC)
try: $(EXEC_TRY)


install: insert
	mv insert ../bin

clean:
	rm -f $(EXEC) $(EXEC_TEST) $(EXEC_TRY)

#arch_restore: arch_restore.cc $(HFILES) 
#	$(GCC) arch_restore.cc -o arch_restore

to_pg: to_pg.cc $(HFILES) 
	$(GCC) -lpq to_pg.cc -o to_pg

insert: insert.cc $(HFILES) 
	$(GCC) insert.cc -o insert

libpq_version: libpq_version.cc
	$(GCC) -lpq libpq_version.cc -o libpq_version

pq_version: pq_version.cc
	$(GCC) -lpq pq_version.cc -o pq_version

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

test: all try $(EXEC_TEST)
	rm -f test_phrase_1_out.txt test_phrase_2_out.txt test_nodes_map_1_out.txt test_insert_out.txt
	./test_phrase_1
	diff test_phrase_1_out.txt test_phrase_1_out.txt_expected 
	./test_phrase_2
	diff test_phrase_2_out.txt test_phrase_1_out.txt_expected # test has different input format, but output same as for test_phrase_1
	./test_nodes_map_1
	diff test_nodes_map_1_out.txt test_nodes_map_1_out.txt_expected
	./test_nodes_map_2
	./test_insert.sh >& test_insert_out.txt
	diff test_insert_out.txt test_insert_out.txt_expected




