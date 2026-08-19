export module IO.BasicStream;

import std;

export class BasicStream
{
public:
	enum class SeekMethod
	{
		Begin = 0,
		Current = 1,
		End = 2,
	};

	virtual bool Write(const char* src, std::size_t count) = 0;
	virtual bool Read(char* dst, std::size_t count) = 0;

	virtual std::size_t GetSize() const = 0;

	virtual std::int64_t SeekG(std::int64_t index, SeekMethod method) = 0;
	virtual std::int64_t GetG() = 0;

	virtual bool IsValid() const { return true; }

	virtual void StartReading() {}
	virtual void StopReading()  {}

	virtual void StartWriting() {}
	virtual void StopWriting()  {}
};

export class ReadSession
{
public:
	ReadSession(BasicStream& stream) : pStream(&stream)
	{
		stream.StartReading();
	}
	
	~ReadSession()
	{
		if (pStream != nullptr)
			pStream->StopReading();
	}

	ReadSession(ReadSession&& session)
	{
		std::swap(pStream, session.pStream);
	}

	ReadSession(const ReadSession&) = delete;

private:
	BasicStream* pStream = nullptr;
};

export class WriteSession
{
public:
	WriteSession(BasicStream& stream) : pStream(&stream)
	{
		stream.StartWriting();
	}

	~WriteSession()
	{
		if (pStream != nullptr)
			pStream->StopWriting();
	}

	WriteSession(WriteSession&& session)
	{
		std::swap(pStream, session.pStream);
	}

	WriteSession(const WriteSession&) = delete;
	
private:
	BasicStream* pStream = nullptr;
};