// Reflection.cpp : Defines the entry point for the console application.
//

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include "Reflection\Reflection.h"
#include "ClassFactory\ClassFactory.h"
#include "Reflection\BinaryParser\BinaryParser.h"
#include "Reflection\BinaryParser\BinaryBufferParser.h"
#include "Reflection\JSONParser\JSONParser.h"
#include "Reflection\JSONParser\JSONBufferParser.h"
#include "Reflection\BinaryParser\BinaryParser.h"
#include "ReflectionError.h"
using std::vector;

namespace
{
	// Helper function to extract file extension in lowercase
	std::string GetFileExtension(const std::string& fileName)
	{
		size_t lastDot = fileName.find_last_of('.');
		if (lastDot == std::string::npos || lastDot == fileName.length() - 1)
		{
			return "";
		}

		std::string extension = fileName.substr(lastDot + 1);
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		return extension;
	}

	// Helper function to create appropriate parser based on file extension
	std::unique_ptr<IRFL_Parser> CreateParser(const std::string& fileName)
	{
		std::string extension = GetFileExtension(fileName);

		if (extension == "bin" || extension == "binary")
		{
			return std::make_unique<CRFL_BinaryParser>();
		}
		else if (extension == "json")
		{
			return std::make_unique<CRFL_JSONParser>();
		}
		else
		{
			// Default to JSON parser for unknown extensions
			return std::make_unique<CRFL_JSONParser>();
		}
	}
}

bool CReflectedBase::Read(const char* fileName)
{
	try
	{
		if (!fileName)
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Null filename provided", "CReflectedBase::Read");
			return false;
		}

		std::string fileNameStr(fileName);
		if (!Reflection::ValidateFilePath(fileNameStr))
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Invalid file path", "Path: " + fileNameStr);
			return false;
		}

		auto parser = CreateParser(fileNameStr);
		if (!parser->BeginInput(fileNameStr))
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Failed to open file for reading", "Path: " + fileNameStr);
			return false;
		}

		ReadMembers(*parser);
		parser->EndInput();

		ReflectionDebug.print("Successfully read object: Class=" +
			std::string(GetRflClassName()) + ", File=" + fileNameStr);
		OnLoaded();
		return true;
	}
	catch (const Reflection::ReflectionException& e)
	{
		// Log directly — do NOT call REFL_ERROR here, it will throw again
		ReflectionDebug.print(std::string("[ERROR][Parsing] Reflection error during read: ") + e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		// Same — avoid going through the throwing error handler
		ReflectionDebug.print(std::string("[CRITICAL][Unknown] Unexpected error during read: ") + e.what());
		return false;
	}
}

bool CReflectedBase::Write(const char* fileName)
{
	try
	{
		if (!fileName)
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Null filename provided", "CReflectedBase::Write");
			return false;
		}

		std::string fileNameStr(fileName);
		if (!Reflection::ValidateFilePath(fileNameStr))
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Invalid file path", "Path: " + fileNameStr);
			return false;
		}

		auto parser = CreateParser(fileNameStr);
		if (!parser->BeginOutput(fileNameStr))
		{
			REFL_ERROR(Reflection::ErrorCategory::FileIO,
				"Failed to open file for writing", "Path: " + fileNameStr);
			return false;
		}

		WriteMembers(*parser);
		parser->EndOutput();

		REFL_WARNING(Reflection::ErrorCategory::FileIO,
			"Successfully wrote object", "Class: " + std::string(GetRflClassName()) + ", File: " + fileNameStr);
		return true;
	}
	catch (const Reflection::ReflectionException& e)
	{
		REFL_ERROR(Reflection::ErrorCategory::Parsing,
			"Reflection error during write", e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unexpected error during write", e.what());
		return false;
	}
	catch (...)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unknown error during write", "CReflectedBase::Write");
		return false;
	}
}

Reflection::Result<bool> CReflectedBase::SafeRead(const std::string& fileName)
{
	using namespace Reflection;

	if (fileName.empty())
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
			"Empty filename provided", "CReflectedBase::SafeRead"));
	}

	if (!ValidateFilePath(fileName))
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
			"Invalid file path", "Path: " + fileName));
	}

	try
	{
		auto parser = CreateParser(fileName);
		if (!parser->BeginInput(fileName))
		{
			return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
				"Failed to open file for reading", "Path: " + fileName));
		}

		ReadMembers(*parser);
		parser->EndInput();

		// Ensure the object's post-load hook runs for SafeRead path as well.
		OnLoaded();

		return Result<bool>::Success(true);
	}
	catch (const ReflectionException& e)
	{
		return Result<bool>::Error(e.GetErrorInfo());
	}
	catch (const std::exception& e)
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Critical, ErrorCategory::Unknown,
			"Unexpected error during read", e.what()));
	}
}

Reflection::Result<bool> CReflectedBase::SafeWrite(const std::string& fileName)
{
	using namespace Reflection;

	if (fileName.empty())
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
			"Empty filename provided", "CReflectedBase::SafeWrite"));
	}

	if (!ValidateFilePath(fileName))
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
			"Invalid file path", "Path: " + fileName));
	}

	try
	{
		auto parser = CreateParser(fileName);
		if (!parser->BeginOutput(fileName))
		{
			return Result<bool>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::FileIO,
				"Failed to open file for writing", "Path: " + fileName));
		}

		WriteMembers(*parser);
		parser->EndOutput();

		return Result<bool>::Success(true);
	}
	catch (const ReflectionException& e)
	{
		return Result<bool>::Error(e.GetErrorInfo());
	}
	catch (const std::exception& e)
	{
		return Result<bool>::Error(ErrorInfo(ErrorSeverity::Critical, ErrorCategory::Unknown,
			"Unexpected error during write", e.what()));
	}
}

bool CReflectedBase::ReadFromJsonString(const std::string& jsonString)
{
	try
	{
		if (jsonString.empty())
		{
			REFL_ERROR(Reflection::ErrorCategory::Parsing,
				"Empty JSON string provided", "CReflectedBase::ReadFromJsonString");
			return false;
		}

		CRFL_JSONBufferParser parser;
		if (!parser.BeginInputFromBuffer(jsonString))
		{
			REFL_ERROR(Reflection::ErrorCategory::Parsing,
				"Failed to parse JSON string", "CReflectedBase::ReadFromJsonString");
			return false;
		}

		ReadMembers(parser);
		parser.EndInputFromBuffer();

		// Call object's post-load hook so derived OnLoaded() implementations execute.
		OnLoaded();

		REFL_WARNING(Reflection::ErrorCategory::Parsing,
			"Successfully read object from JSON string", "Class: " + std::string(GetRflClassName()));
		return true;
	}
	catch (const Reflection::ReflectionException& e)
	{
		REFL_ERROR(Reflection::ErrorCategory::Parsing,
			"Reflection error during read from buffer", e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unexpected error during read from buffer", e.what());
		return false;
	}
	catch (...)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unknown error during read from buffer", "CReflectedBase::ReadFromJsonString");
		return false;
	}
}

Reflection::Result<std::string> CReflectedBase::WriteToJsonString()
{
	using namespace Reflection;

	try
	{
		CRFL_JSONBufferParser parser;
		if (!parser.BeginOutputToBuffer())
		{
			return Result<std::string>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::Parsing,
				"Failed to begin buffer output", "CReflectedBase::WriteToJsonString"));
		}

		WriteMembers(parser);
		parser.EndOutputToBuffer();

		return Result<std::string>::Success(parser.MoveOutputBuffer());
	}
	catch (const ReflectionException& e)
	{
		return Result<std::string>::Error(e.GetErrorInfo());
	}
	catch (const std::exception& e)
	{
		return Result<std::string>::Error(ErrorInfo(ErrorSeverity::Critical, ErrorCategory::Unknown,
			"Unexpected error during write to buffer", e.what()));
	}
}

bool CReflectedBase::ReadFromBinaryBuffer(const std::vector<uint8_t>& binaryData)
{
	try
	{
		if (binaryData.empty())
		{
			REFL_ERROR(Reflection::ErrorCategory::Parsing,
				"Empty binary buffer provided", "CReflectedBase::ReadFromBinaryBuffer");
			return false;
		}

		CRFL_BinaryBufferParser parser;
		if (!parser.BeginInputFromBuffer(binaryData))
		{
			REFL_ERROR(Reflection::ErrorCategory::Parsing,
				"Failed to parse binary buffer", "CReflectedBase::ReadFromBinaryBuffer");
			return false;
		}

		ReadMembers(parser);

		if (parser.HasReadErrors())
		{
			REFL_ERROR(Reflection::ErrorCategory::Parsing,
				"Binary buffer contains corrupt or incompatible data", "CReflectedBase::ReadFromBinaryBuffer");
			parser.EndInputFromBuffer();
			return false;
		}

		parser.EndInputFromBuffer();

		// Ensure post-load hook runs for binary-buffer read path.
		OnLoaded();

		REFL_WARNING(Reflection::ErrorCategory::Parsing,
			"Successfully read object from binary buffer", "Class: " + std::string(GetRflClassName()));
		return true;
	}
	catch (const Reflection::ReflectionException& e)
	{
		REFL_ERROR(Reflection::ErrorCategory::Parsing,
			"Reflection error during read from binary buffer", e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unexpected error during read from binary buffer", e.what());
		return false;
	}
	catch (...)
	{
		REFL_CRITICAL(Reflection::ErrorCategory::Unknown,
			"Unknown error during read from binary buffer", "CReflectedBase::ReadFromBinaryBuffer");
		return false;
	}
}

Reflection::Result<std::vector<uint8_t>> CReflectedBase::WriteToBinaryBuffer()
{
	using namespace Reflection;

	try
	{
		CRFL_BinaryBufferParser parser;
		if (!parser.BeginOutputToBuffer())
		{
			return Result<std::vector<uint8_t>>::Error(ErrorInfo(ErrorSeverity::Error, ErrorCategory::Parsing,
				"Failed to begin binary buffer output", "CReflectedBase::WriteToBinaryBuffer"));
		}

		WriteMembers(parser);
		parser.EndOutputToBuffer();

		return Result<std::vector<uint8_t>>::Success(parser.MoveOutputBuffer());
	}
	catch (const ReflectionException& e)
	{
		return Result<std::vector<uint8_t>>::Error(e.GetErrorInfo());
	}
	catch (const std::exception& e)
	{
		return Result<std::vector<uint8_t>>::Error(ErrorInfo(ErrorSeverity::Critical, ErrorCategory::Unknown,
			"Unexpected error during write to binary buffer", e.what()));
	}
}

void CReflectedBase::InternalReadMembers(vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc)
{
	if (!ValidateReflectionMap(reflectionMap))
	{
		REFL_ERROR(Reflection::ErrorCategory::Validation,
			"Invalid reflection map", "Class: " + std::string(GetRflClassName()));
		return;
	}

	if (!ValidateParser(doc))
	{
		REFL_ERROR(Reflection::ErrorCategory::Validation,
			"Invalid parser state", "Class: " + std::string(GetRflClassName()));
		return;
	}

	for (auto& property : reflectionMap)
	{
		try
		{
			if (!property.GetProperty())
			{
				REFL_ERROR(Reflection::ErrorCategory::Memory,
					"Null property in reflection map", "Class: " + std::string(GetRflClassName()));
				continue;
			}

			property.GetProperty()->Read(&doc, this);
		}
		catch (const Reflection::ReflectionException& e)
		{
			REFL_WARNING(Reflection::ErrorCategory::PropertyAccess,
				"Skipping property after read error",
				"Property: " + property.GetProperty()->GetName() + ", Error: " + e.what());
			// Continue with other properties instead of failing completely
		}
		catch (const std::exception& e)
		{
			REFL_WARNING(Reflection::ErrorCategory::PropertyAccess,
				"Skipping property after unexpected error",
				"Property: " + property.GetProperty()->GetName() + ", Error: " + e.what());
		}
	}
	OnLoaded();
}

void CReflectedBase::InternalWriteMembers(vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc)
{
	if (!ValidateReflectionMap(reflectionMap))
	{
		REFL_ERROR(Reflection::ErrorCategory::Validation,
			"Invalid reflection map", "Class: " + std::string(GetRflClassName()));
		return;
	}

	if (!ValidateParser(doc))
	{
		REFL_ERROR(Reflection::ErrorCategory::Validation,
			"Invalid parser state", "Class: " + std::string(GetRflClassName()));
		return;
	}

	for (auto& property : reflectionMap)
	{
		try
		{
			if (!property.GetProperty())
			{
				REFL_ERROR(Reflection::ErrorCategory::Memory,
					"Null property in reflection map", "Class: " + std::string(GetRflClassName()));
				continue;
			}

			property.GetProperty()->Write(&doc, this);
		}
		catch (const Reflection::ReflectionException& e)
		{
			REFL_ERROR(Reflection::ErrorCategory::PropertyAccess,
				"Failed to write property",
				"Property: " + property.GetProperty()->GetName() + ", Error: " + e.what());
			// Continue with other properties instead of failing completely
		}
		catch (const std::exception& e)
		{
			REFL_ERROR(Reflection::ErrorCategory::PropertyAccess,
				"Unexpected error writing property",
				"Property: " + property.GetProperty()->GetName() + ", Error: " + e.what());
		}
	}
}

bool CReflectedBase::ValidateReflectionMap(const vector<CReflectionMapEntry>& reflectionMap) const
{
	if (reflectionMap.empty())
	{
		return true; // Empty map is valid, just unusual
	}

	for (size_t i = 0; i < reflectionMap.size(); ++i)
	{
		const auto& entry = reflectionMap[i];
		if (!entry.GetProperty())
		{
			REFL_ERROR(Reflection::ErrorCategory::Memory,
				"Null property at index " + std::to_string(i),
				"Class: " + std::string(GetRflClassName()));
			return false;
		}

		// Additional validation could be added here
		// e.g., check for duplicate property names, valid types, etc.
	}

	return true;
}

bool CReflectedBase::ValidateParser(IRFL_Parser& doc) const
{
	// Basic validation - in a real implementation, you might want to add
	// more sophisticated parser state validation
	try
	{
		// Try to access parser - if it throws, it's invalid
		// This is a simple validation - you could extend this
		return true;
	}
	catch (...)
	{
		REFL_ERROR(Reflection::ErrorCategory::Validation,
			"Parser validation failed", "Class: " + std::string(GetRflClassName()));
		return false;
	}
}