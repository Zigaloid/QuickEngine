#pragma once

#include "BinaryParser.h"
#include <cstring>

// Binary parser that reads from and writes to in-memory byte buffers
// instead of files. All binary serialization logic is inherited from
// CRFL_BinaryParser; only the I/O entry points are replaced.
class CRFL_BinaryBufferParser : public CRFL_BinaryParser
{
public:
	// --- Output to byte buffer ---

	// Call instead of BeginOutput(fileName). Prepares an empty buffer for writing.
	bool BeginOutputToBuffer()
	{
		m_outputByteBuffer.clear();
		m_bufferWritePosition = 0;
		m_useBuffer = true;
		return true;
	}

	// Call instead of EndOutput(). The buffer is already populated; nothing to flush.
	void EndOutputToBuffer()
	{
		// Nothing to flush – data is already in m_outputByteBuffer.
	}

	// Returns a const reference to the serialized binary data.
	const std::vector<uint8_t>& GetOutputBuffer() const { return m_outputByteBuffer; }

	// Moves the serialized binary data out (avoids a copy for large payloads).
	std::vector<uint8_t> MoveOutputBuffer() { return std::move(m_outputByteBuffer); }

	// --- Input from byte buffer ---

	// Call instead of BeginInput(fileName). Reads from the provided byte buffer.
	bool BeginInputFromBuffer(const std::vector<uint8_t>& binaryData)
	{
		if (binaryData.empty()) {
			ReflectionDebug.print("Empty binary buffer provided to buffer parser\n");
			return false;
		}

		m_inputByteBuffer = binaryData;
		m_bufferReadPosition = 0;
		m_useBuffer = true;
		m_hasReadErrors = false;
		return true;
	}

	// Move overload to avoid copying large buffers.
	bool BeginInputFromBuffer(std::vector<uint8_t>&& binaryData)
	{
		if (binaryData.empty()) {
			ReflectionDebug.print("Empty binary buffer provided to buffer parser\n");
			return false;
		}

		m_inputByteBuffer = std::move(binaryData);
		m_bufferReadPosition = 0;
		m_useBuffer = true;
		m_hasReadErrors = false;
		return true;
	}

	// Call instead of EndInput(). No file handles to close; included for
	// symmetry with the base class API.
	void EndInputFromBuffer()
	{
		// Nothing to close – no file resources are held.
		m_inputByteBuffer.clear();
		m_bufferReadPosition = 0;
	}

	// --- Disable file-based I/O so callers don't accidentally use it ---

	bool BeginOutput(const std::string&) override
	{
		ReflectionDebug.print("CRFL_BinaryBufferParser: use BeginOutputToBuffer() instead of BeginOutput()\n");
		return false;
	}
	void EndOutput() override
	{
		ReflectionDebug.print("CRFL_BinaryBufferParser: use EndOutputToBuffer() instead of EndOutput()\n");
	}
	bool BeginInput(const std::string&) override
	{
		ReflectionDebug.print("CRFL_BinaryBufferParser: use BeginInputFromBuffer() instead of BeginInput()\n");
		return false;
	}
	void EndInput() override
	{
		ReflectionDebug.print("CRFL_BinaryBufferParser: use EndInputFromBuffer() instead of EndInput()\n");
	}

	// --- Override Read/Write to use in-memory buffers ---

	bool Write(const void* data, size_t size) override
	{
		if (!m_useBuffer) {
			return CRFL_BinaryParser::Write(data, size);
		}

		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
		m_outputByteBuffer.insert(m_outputByteBuffer.end(), bytes, bytes + size);
		m_bufferWritePosition += size;
		return true;
	}

	bool Read(void* data, size_t size) override
	{
		if (!m_useBuffer) {
			return CRFL_BinaryParser::Read(data, size);
		}

		if (m_bufferReadPosition + size > m_inputByteBuffer.size()) {
			ReflectionDebug.print("Binary buffer read overflow: requested " + std::to_string(size) +
				" bytes at position " + std::to_string(m_bufferReadPosition) +
				", buffer size " + std::to_string(m_inputByteBuffer.size()) + "\n");
			m_hasReadErrors = true;
			return false;
		}

		std::memcpy(data, m_inputByteBuffer.data() + m_bufferReadPosition, size);
		m_bufferReadPosition += size;
		return true;
	}

	// Returns true if any Read() call failed due to buffer overflow or other errors.
	bool HasReadErrors() const { return m_hasReadErrors; }

private:
	std::vector<uint8_t> m_outputByteBuffer;
	std::vector<uint8_t> m_inputByteBuffer;
	uint64_t m_bufferWritePosition = 0;
	uint64_t m_bufferReadPosition = 0;
	bool m_useBuffer = false;
	bool m_hasReadErrors = false;
};