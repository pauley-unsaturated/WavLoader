/*
 * Random-Access Wave file loader
 */

#include "WavLoader.h"

#include <cstdint>

bool WavLoader::open(FileWrapper* wrapper) {
	if(!wrapper) {
		return false;
	}

	_file = wrapper;

	bool didOpen = _file->open();
	if(!didOpen) {
		_file = NULL;
		return false;
	}
	
	/* Read and verify the RIFF / WAV header */
	size_t numRead = _file->read(&_header, sizeof(_header));
	if(numRead != sizeof(_header)) {
		_file->close();
		return false;
	}

	if (memcmp(_header.ChunkID, kRiffHeader,
						 sizeof(kRiffHeader) * sizeof(kRiffHeader[0]))) {
		_file->close();
		return false;
	}
	_position = numRead;
	
	return true;
}

static inline uint32_t MIN(uint32_t a, uint32_t b) {
	return a<b?a:b;
}

static inline uint32_t MAX(uint32_t a, uint32_t b) {
	return a>=b?a:b;
}

static inline uint32_t sample_to_byte_pos(WavLoader& loader,
																					uint32_t sample_pos) {
	uint32_t result =	 MIN( ((uint32_t)sizeof(WavHeader)
													 + sample_pos * (loader.frameAlignment())),
													loader.fileSize() );
	return result;
}

uint32_t WavLoader::position() {
	return (_position - (uint32_t)sizeof(WavHeader)) / frameAlignment();
}

bool WavLoader::seek(uint32_t position) {
	//uint32_t samplePos = MIN(position, numSamples());
	//_position = sample_to_byte_pos(*this, samplePos);
	_position = position;
	return _file->seek(_position);
}

// FIXME: this caching bullshit should be in the
//	file wrapper, not in the WavLoader.
//	or better yet: in some sort of buffering streamer
//	that runs outside of the audio callback.

uint32_t WavLoader::read(void* buf, uint32_t bufSize) {
	uint32_t clippedSize = MIN(bufSize, _file->size() - _position);
	uint32_t endBlockNum = (_position + clippedSize) >> 9;
	uint32_t endOffset = (_position + clippedSize) & 0x1FF;
	uint8_t* cur = (uint8_t*)buf;
	uint32_t result = 0;
	bool shortRead = false;

	while(!shortRead && (cur < (uint8_t*)buf + clippedSize)) {
		uint32_t curBlockNum = _position >> 9;
		uint32_t curOffset = _position & 0x1FF;
		if (curBlockNum == _blockNum) {
			// feed the buffer from the cache
			uint32_t end = _bufferLen;
			if(curBlockNum == endBlockNum) {
				end = MIN(end, endOffset);
				if (end < endOffset) {
					shortRead = true;
				}
			}
			if (curOffset < _bufferLen) {
				result = end - curOffset;
				memcpy(cur, _buffer + curOffset, result);
			}
			else {
				result = 0;
				shortRead = true;
			}
		}
		else if (curBlockNum == endBlockNum
						 || curOffset > 0 ) {
			// read into the cache and fill from there
			// only need to cache the last one
			_file->seek(curBlockNum * BLOCKSIZE);
			_bufferLen = _file->read(_buffer, BLOCKSIZE);
			_blockNum = curBlockNum;
			uint32_t theEnd = MIN(_bufferLen, endOffset);
			shortRead = theEnd < endOffset;
			if(curOffset < theEnd) {
				result = theEnd - curOffset;
				memcpy(cur, _buffer + curOffset, result);
			}
			else {
				result = 0;
				shortRead = true;
			}
		}
		else {
			// read to the end of the current block
			// directly into the destination buffer
			result = _file->read(cur, BLOCKSIZE);
			shortRead = result < BLOCKSIZE;
		}
		_position += result;
		cur += result;
	} 
	
	return cur - (uint8_t*)buf;
}

void WavLoader::close() {
	if (_file) {
		_file->close();
	}
}
