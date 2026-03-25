#pragma once

#include "JSONParser.h"

// JSON parser that reads from and writes to in-memory string buffers
// instead of files. All JSON serialization logic is inherited from
// CRFL_JSONParser; only the I/O entry points are replaced.
class CRFL_JSONBufferParser : public CRFL_JSONParser
{
public:
	// --- Output to string buffer ---

	// Call instead of BeginOutput(fileName). Prepares the document for writing.
	bool BeginOutputToBuffer()
	{
		m_currentObject = &m_document.SetObject();
		BeginObject();
		return true;
	}

	// Call instead of EndOutput(). Serializes the document into the internal
	// string buffer which can be retrieved with GetOutputBuffer().
	void EndOutputToBuffer()
	{
		EndObject(JTAG_ROOT);
		StringBuffer buffer;
		PrettyWriter<StringBuffer> writer(buffer);
		m_document.Accept(writer);

		m_outputBuffer = buffer.GetString();
		m_outputBuffer += "\n";
	}

	// Returns the serialized JSON string after EndOutputToBuffer() has been called.
	const std::string& GetOutputBuffer() const { return m_outputBuffer; }

	// Moves the serialized JSON string out (avoids a copy for large payloads).
	std::string MoveOutputBuffer() { return std::move(m_outputBuffer); }

	// --- Input from string buffer ---

	// Call instead of BeginInput(fileName). Parses the provided JSON string.
	bool BeginInputFromBuffer(const std::string& jsonString)
	{
		if (jsonString.empty()) {
			ReflectionDebug.print("Empty JSON string provided to buffer parser\n");
			return false;
		}

		m_document.Parse(jsonString.c_str());
		if (m_document.HasParseError())
		{
			ReflectionDebug.print("JSON parse error in buffer parser\n");
			return false;
		}

		m_currentObject = nullptr;
		m_currentObject = &m_document.FindMember(JTAG_ROOT)->value;
		return true;
	}

	// Call instead of EndInput(). No file handles to close; included for
	// symmetry with the base class API.
	void EndInputFromBuffer()
	{
		// Nothing to close – no file resources are held.
	}

	// --- Disable file-based I/O so callers don't accidentally use it ---

	bool BeginOutput(const std::string&) override
	{
		ReflectionDebug.print("CRFL_JSONBufferParser: use BeginOutputToBuffer() instead of BeginOutput()\n");
		return false;
	}
	void EndOutput() override
	{
		ReflectionDebug.print("CRFL_JSONBufferParser: use EndOutputToBuffer() instead of EndOutput()\n");
	}
	bool BeginInput(const std::string&) override
	{
		ReflectionDebug.print("CRFL_JSONBufferParser: use BeginInputFromBuffer() instead of BeginInput()\n");
		return false;
	}
	void EndInput() override
	{
		ReflectionDebug.print("CRFL_JSONBufferParser: use EndInputFromBuffer() instead of EndInput()\n");
	}

private:
	std::string m_outputBuffer;
};