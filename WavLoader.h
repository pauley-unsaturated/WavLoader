/*
 * Random-Access Wave file loader
 */

#ifndef WAVLOADER_H
#define WAVLOADER_H

#include <cstdint>
#include <string>

const char kRiffHeader[4] = {'R', 'I', 'F', 'F'};
const char kChunkID[4] = {'W', 'A', 'V', 'E'};
const char kFormatChunkID[4] = {'f', 'm', 't', ' '};
const char kDataChunkID[4] = {'d', 'a', 't', 'a'};

const uint16_t kPCMFormat = 0x01;

struct __attribute__((packed)) WavHeader
{
	char ChunkID[4];
	uint32_t ChunkSize;
	char Format[4];
	char SubChunk1ID[4];
	uint32_t SubChunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
	char SubChunk2ID[4];
	uint32_t SubChunk2Size;
};

// Needs to work with
//	open / close AND SdFat.h

class FileWrapper {
 public:

 FileWrapper(std::string fileName)
	 : _fileName(fileName){};

 FileWrapper(const char* fileName)
	 : _fileName(fileName){};
 
	virtual ~FileWrapper(){};
	
	virtual size_t	write(const void* buf, size_t size) = 0;
	virtual size_t	read(void* buf, size_t size) = 0;
	virtual bool		seek(size_t pos) = 0;
	virtual long		position() = 0;
	virtual long		size() = 0;
	
	virtual void		flush() { };
	virtual bool		open() = 0;
	virtual void		close() = 0;
	
	std::string& fileName() { return _fileName; }
	
 private:
	std::string _fileName;
};



#ifdef USE_POSIX
#include <cstdio>

class PosixFileWrapper : public FileWrapper {

 public:
 PosixFileWrapper(std::string fileName)
	 : FileWrapper(fileName), _mode("rw"), _file(NULL) {
		
	}

 PosixFileWrapper(std::string fileName, const char* mode)
	 : FileWrapper(fileName), _mode(mode), _file(NULL) {

	}

	~PosixFileWrapper() {
		if (_file) {
			fclose(_file);
			_file = NULL;
		}
	}

	virtual bool open() {
		if(_file) {
			close();
		}
		_file = fopen(fileName().c_str(), _mode);
		return _file != NULL;
	}
	
	virtual size_t	write(const void* buf, size_t size) {
		if (_file) {
			return fwrite(buf, 1, size, _file);
		}
		return 0;
	}
	
	virtual size_t	read(void* buf, size_t size) {
		if (_file) {
			return fread(buf, 1, size, _file);
		}
		return 0;
	}
	
	virtual bool seek(size_t pos) {
		if (_file) {
			return (-1 != fseek(_file, pos, SEEK_SET));
		}
		return false;
	}

	virtual long position() {
		if(_file) {
			return ftell(_file);
		}
		return -1;
	}
	
	virtual long size() {
		long pos = -1;
		if (_file) {
			long save_pos = position();
			fseek(_file, 0, SEEK_END);
			pos = ftell(_file);
			fseek(_file, save_pos, SEEK_SET);
		}
		return pos;
	}
	
	virtual void flush() {
		if (_file) {
			fflush(_file);
		}
	}

	virtual void close() {
		if (_file) {
			fclose(_file);
			_file = NULL;
		}
	}

 private:
	FILE* _file;
	const char* _mode;
};
#else

#include <SdFat.h>

// Arduino / esp8266 file interface wrapper
// TODO: get rid of the virtual functions.
//	We know darn well what kind of object this is.
class SDFileWrapper : public FileWrapper {
 public:
 SDFileWrapper(std::string fileName)
	 : FileWrapper(fileName)
	{
	}

 SDFileWrapper(const char* fileName)
	 : FileWrapper(fileName)
	{
	}
	
	virtual size_t	write(const void* buf, size_t size) {
		if (!_file) {
			return 0;
		}
		size_t result = _file.write((const uint8_t*)buf, size);
		return result;
	}
	
	virtual size_t	read(void* buf, size_t size) {
		if (!_file) {
			return 0;
		}
		// ¯\_(ツ)_/¯ 64k ought to be enough for anybody
		int result = _file.read(buf, (uint16_t)size);
		return (size_t)result;
	}

	virtual bool		seek(size_t pos) {
		if(_file) {
			_file.seek((uint32_t)pos);
		}
	}
	
	virtual long		position() {
		if (_file) {
			return _file.position();
		}
		return -1;
	}
	
	virtual long		size() {
		if (_file) {
			return _file.size();
		}
		return -1;
	}
	
	virtual void		flush() {
		_file.flush();
	}

	virtual bool		open() {
		return _file.open(fileName().c_str(), (uint8_t)O_READ);
	}
	
	virtual void		close() {
		if(_file) {
			_file.close();
		}
	}

 private:
	File _file;
};
#endif

class WavLoader
{
	
 public:

 WavLoader()
	 : _header(WavHeader()), _file(NULL), _blockNum(-1), _bufferLen(-1)
		{};
	
	bool open(FileWrapper* file);
	void close();

	/* Seeks in terms of samples */
	bool seek(uint32_t position);
	uint32_t position();
	
	/* Reads full samples only */
	uint32_t read(void* buf, uint32_t bufSize);
	
	uint32_t sampleRate() { return _header.SampleRate; }
	uint16_t bitsPerSample() { return _header.BitsPerSample; }
	uint16_t numChannels() { return _header.NumChannels; }
	uint32_t numSamples() { return _header.SubChunk2Size / (_header.BlockAlign); }
	uint16_t frameAlignment() { return _header.BlockAlign; }

	uint32_t fileSize() { return _header.ChunkSize + 8; }
	
	WavHeader header() { return _header; }
	
	~WavLoader() { }
	
 private:
	static constexpr int BLOCKSIZE = 512;
	
	WavHeader _header;
	FileWrapper* _file;
	uint32_t	_position; // file position in samples
	int32_t	 _blockNum;
	int32_t	 _bufferLen;
	
	// buffer a whole block at a time
	// so we don't thrash the underlying disk buffer
	uint8_t	 _buffer[BLOCKSIZE]; 
															 
};

#endif // WAVLOADER_H
