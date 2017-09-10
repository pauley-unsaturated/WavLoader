
CXX=clang++
CXXARGS=-g -O0 -DUSE_POSIX -std=c++11 -fsanitize=address
#CXXARGS=-O2 -DUSE_POSIX -std=c++11

testWavLoader: Makefile AudioStream.h WavLoader.h WavLoader.cpp test.cpp
	$(CXX) $(CXXARGS) WavLoader.cpp test.cpp -o $@

test: testWavLoader
	./testWavLoader

clean:
	rm -rf *.o *~ testWavLoader
