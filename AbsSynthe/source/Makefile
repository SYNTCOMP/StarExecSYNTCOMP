CXX=g++
CXXFLAGS=-std=c++11 -pthread -O3 -D NDEBUG
HEADERS=abssynthe.h logging.h aig.h aiger.h
SOURCES=abssynthe.cpp logging.cpp aig.cpp aiger.c aigtocnf.c algos.cpp
CUDD_PATH=cudd-2.5.1
CUDD_HDRS=$(CUDD_PATH)/include
CUDD_LIBS=$(CUDD_PATH)/obj/libobj.a \
	  $(CUDD_PATH)/cudd/libcudd.a \
	  $(CUDD_PATH)/mtr/libmtr.a \
	  $(CUDD_PATH)/st/libst.a \
	  $(CUDD_PATH)/util/libutil.a \
	  $(CUDD_PATH)/epd/libepd.a

abssynthe: $(HEADERS) $(SOURCES) $(CUDD_HDRS) $(CUDD_LIBS)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(CUDD_LIBS) -o abssynthe -I $(CUDD_HDRS)
clean:
	rm -f abssynthe
