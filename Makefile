
CXX=clang++
CXXARGS=-g -DUSE_POSIX -std=c++11

testWavLoader: WavLoader.h WavLoader.cpp test.cpp
	$(CXX) $(CXXARGS) WavLoader.cpp test.cpp -o $@

test: testWavLoader
	./testWavLoader

clean:
	rm -rf *.o *~ testWavLoader
