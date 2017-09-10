/*
 * AudioStream.h
 *
 * An Unsaturated audio library
 * optimized for embedded use.
 * 
 * © 2017 - Mark Pauley
 * 
 */

#ifndef UNS_AUDIOSTREAM_H
#define UNS_AUDIOSTREAM_H

#include "WavLoader.h"
#include <cstdint>
#include <vector>
#include <iostream>

namespace Unsaturated {

	/*
	 * Your typical "Pull" stream
	 */
	template <typename SubType,
		typename SampleType,
		unsigned int SR=44100,
		unsigned int NC=1 >
		class AudioInputStream {
	public:
	static constexpr unsigned int NumChannels = NC;
	static constexpr unsigned int SampleRate = SR;
  
	int read(SampleType* buf, unsigned int num_samples) {
		return static_cast<SubType>(this)->read(buf, num_samples);
	}

	operator bool() const {
		return (bool)(static_cast<SubType>(this));
	}
		};

	enum AudioSamplerError : uint8_t {
		NoErr = 0,
			BadFile = 1,
			BadSampleRate = 2,
			BadSampleSize = 3,
			BadNumChannels = 4
			};
	/*
	 * Simple version 1 non-pitch stretching sampler.
	 * optimized to play the beginning of the file quickly.
	 */
	template <typename SampleType,
		unsigned int SR=44100,
		unsigned int NC=1>
		class AudioSamplerStream
		: public AudioInputStream<AudioSamplerStream<SampleType,SR,NC>, SampleType, SR, NC> {
		using ThisClass = AudioSamplerStream<SampleType,SR,NC>;
	
	public:
		static constexpr int BlockSize = 512;
		static constexpr int NumBlocks = 3;
		static constexpr int SamplesPerBlock = BlockSize/sizeof(SampleType);

		/*FIXME: need to have a type that represents a frame 
		 * in other words SampleType[NumChannels]
		 */
	AudioSamplerStream() : _file() {
		}

		AudioSamplerError load(FileWrapper* file) {
			/* 
			 * Load the file and fail if the sample format is incompatible
			 */
			if(_file.open(file)) {
				if (_file.sampleRate() != ThisClass::SampleRate) {
					return AudioSamplerError::BadSampleRate;
				}
				if (_file.numChannels() != ThisClass::NumChannels) {
					return AudioSamplerError::BadNumChannels;
				}
				if (_file.bitsPerSample() != sizeof(SampleType) * 8) {
					return AudioSamplerError::BadSampleSize;
				}

				// TODO: deal with the file being smaller than the intro buffer.
				_file.read(_introBuf, sizeof _introBuf);
				_readHead = &_introBuf[0];
				_sampleIdx = 0;
				for (int i = 0; i < NumBlocks; i++) {
					_bufBlockMap[i] = UnmappedBlock;
				}
		
				return AudioSamplerError::NoErr;
			}
			return AudioSamplerError::BadFile;
		}

		int read(SampleType* buf, unsigned int numSamples) {	  
			unsigned int numSamplesLeft = numSamples;

			/* Make sure the readHead is still good */
			if (_sampleIdx < IntroBufSize) {
				_readHead = _introBuf + _sampleIdx;
			} 
	  
			if (_readHead < _ringBuf) {
				/* Read from introBuf */
				unsigned int to_read = numSamples;
				if (to_read + (_readHead - _introBuf) > IntroBufSize) {
					to_read = (_introBuf + IntroBufSize - _readHead);
				}
				memcpy(buf, _readHead, to_read * sizeof(SampleType));
				numSamplesLeft -= to_read;
				_readHead += to_read;
				buf += to_read;
				_sampleIdx += to_read;
			}
	  
			/* At this point we have either served the whole read
			 *  or we have exhausted the introBuf.
			 */
	  
			/* Read from the cache */
			while (numSamplesLeft > 0) {
				int rhb = ((uint8_t*)_readHead - (uint8_t*)_ringBuf) / BlockSize;
				int realrhb = -1;
				if (rhb < NumBlocks) {
					realrhb = _bufBlockMap[rhb];
				}
				int scb = ((_sampleIdx - IntroBufSize) / SamplesPerBlock) + 1;

				/* Make sure we're in the right place */
				if (realrhb != scb) {
					/* Find the new position for the read head */
					for (int i = 0; i < NumBlocks; i++) {
						int block = _bufBlockMap[i];
						if (block == scb) {
							_readHead = (i * SamplesPerBlock) + _ringBuf
								+ (scb % SamplesPerBlock);
							rhb = i;
							realrhb = block;
							break;
						}
					}
				}

				if (realrhb != scb) break;

				/* Clip the memcpy to the end of this block */
				unsigned int to_read = numSamplesLeft;
				SampleType* nextBlockStart = _ringBuf + (rhb + 1) * SamplesPerBlock;
				if ( nextBlockStart - _readHead < to_read ) {
					to_read = nextBlockStart - _readHead;
				}
				memcpy(buf, _readHead, to_read * sizeof(SampleType));
				numSamplesLeft -= to_read;
				_readHead += to_read;
				buf += to_read;
				_sampleIdx += to_read;
			}

			return numSamples - numSamplesLeft;
		}

		/* prime() MUST NOT block the read method, which will be
		 *	called from a real-time context like an interrupt handler.
		 */
		bool prime() {
			/* Find any block that could be loaded with a block that is closer
			 * to the read head and load that
			 */

			long int readHeadBlock = 0;
	  
			if (_sampleIdx >= IntroBufSize) {
				readHeadBlock = (_sampleIdx * sizeof(SampleType) / BlockSize);
			}
	  
			/* First find the furthest block from the read head */
			int idx = 0;
			long maxDiff = labs(_bufBlockMap[0] - readHeadBlock);
			for (int i = 1; i < NumBlocks; i++) {
				long thisDiff = labs(_bufBlockMap[i] - readHeadBlock);
				if (thisDiff > maxDiff) {
					idx = i;
					maxDiff = thisDiff;
				}
			}
			/* Determine if it could be filled with something better */
			if (maxDiff > 0) {
				for (int absDiff = 1; absDiff <= maxDiff; absDiff++) {
					int offset = absDiff;
					/* Look ahead and behind, in that order */
					for (int sign = 0; sign < 2; offset = -offset, sign++) {
						int block = readHeadBlock + offset;
						if (block <= 0) break;
						if (block * BlockSize > _file.fileSize()) break;
						/* Scan the blocks to see if we already have it
						 *	in the buffer
						 */
						for (int i = 0; i < NumBlocks; i++) {
							if (_bufBlockMap[i] == block) {
								block = -1;
								break;
							}
						}

						if (block > 0) {
							/* We found a block that is closer to the read head.
							 * Read this block and bail.
							 */
							if (!_file.seek((block * BlockSize))) {
								break;
							}

							size_t numRead = _file.read(((uint8_t*)_ringBuf) + (idx * BlockSize),
														BlockSize);
							if(numRead > 0) {
								_bufBlockMap[idx] = block;
								return true;
							}
							/* Handle a short read here */
						}
					}
				}
		  
			}
			return false;
		}

		bool atEOF() {
			return _sampleIdx >= _file.numSamples();
		};

		void setSampleIndex(uint32_t sampleIdx) {
			_sampleIdx = std::min(sampleIdx, _file.numSamples());
		}

		uint32_t sampleIndex() { return _sampleIdx; }

		void reset() {
			setSampleIndex(0);
		}
	
	private:
	
		static constexpr int IntroBufSize = (BlockSize - sizeof(WavHeader))
		/ sizeof(SampleType);
		static constexpr int CacheBufSize = (BlockSize * NumBlocks)
		/ sizeof(SampleType);
		static constexpr auto UnmappedBlock = std::numeric_limits<unsigned int>::max();

		/* Current offset in the buffer */
		SampleType* _readHead;

		/* Position in the file */
		uint32_t	_sampleIdx;

		WavLoader _file;

		/* Uses what I am calling a 'P-Buffer'
		 * Which is a ring-buffer with a lead-in
		 *	that optimizes for low-latency sample restarts.
		 */
		SampleType	_introBuf[IntroBufSize];
		SampleType	_ringBuf [CacheBufSize];
		unsigned int _bufBlockMap[NumBlocks];

	};
											  
} // namespace Unsaturated

#endif
