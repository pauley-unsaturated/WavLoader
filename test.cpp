#include "WavLoader.h"
#include "AudioStream.h"
#include <iostream>
#include <string>

using namespace Unsaturated;

int main(int argc, const char** argv) {

	WavLoader loader;
	std::string fileName{"break001.wav"};

	bool success;

	std::cout << "Opening file " << fileName << " ...";
	PosixFileWrapper fileWrapper{fileName};
	success = loader.open(&fileWrapper);

	if(!success) {
		std::cout << "[FAILED]" << std::endl;
		exit(-1);
	}

	std::cout << "[SUCCESS]" << std::endl;
	
	std::cout << "File " << fileName << std::endl
						<< loader.bitsPerSample() << " bits per sample" << std::endl
						<< loader.numChannels() << " channels" << std::endl
						<< loader.sampleRate() << " Hz" << std::endl
						<< loader.numSamples() / ((double)loader.sampleRate() * loader.numChannels()) 
						<< " Seconds"
						<< std::endl;

	AudioSamplerStream<int16_t,44100,2> stream;

	AudioSamplerError e;
	if( AudioSamplerError::NoErr == (e = stream.load(&fileWrapper)) ) {
		std::cout << "Sampler loaded successfully" << std::endl;
	}
	else {
		std::cout << "Error loading sampler: " << e << std::endl;
		return 1;
	}

	
	std::cout << "Priming the stream";
	while(stream.prime()) {
		std::cout << "+";
	}
	
	std::cout << std::endl << "Streaming";

	for (int i = 0; i < 3; i++) {
		int16_t buf[32];
		while(!stream.atEOF()) {
			if(stream.read(buf, 32)) {
				std::cout << ".";
			}
			else {
				std::cout << "-";
			}
			if(stream.prime()) {
				std::cout << "+";
			}
		}
		std::cout << std::endl << "Reset" << std::endl;
		stream.reset();
	}
	
	return 0;
}
