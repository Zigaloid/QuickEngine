#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include "AIAssistantService.h"

// --- Lifecycle ----------------------------------------------------------------

AIAssistantService::~AIAssistantService()
{
    if (m_workerThread.joinable())
        m_workerThread.join();
}

// --- Public API ---------------------------------------------------------------

void AIAssistantService::Query(const std::string& prompt, ResponseCallback callback)
{
    if (m_isQuerying.load())
    {
        callback("A query is already in progress. Please wait.", true);
        return;
    }

    if (m_workerThread.joinable())
        m_workerThread.join();

    m_isQuerying.store(true);
    m_workerThread = std::thread(&AIAssistantService::ExecuteQuery, this, prompt, callback);
}

void AIAssistantService::Update()
{
    std::queue<PendingResponse> toDispatch;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(toDispatch, m_pendingResponses);
    }
    while (!toDispatch.empty())
    {
        auto& r = toDispatch.front();
        r.callback(r.result, r.isError);
        toDispatch.pop();
    }
}

// --- Internal -----------------------------------------------------------------

void AIAssistantService::ExecuteQuery(const std::string& prompt, ResponseCallback callback)
{
    std::string result;
    bool isError = false;
    try
    {
        result = PostToLMStudioAPI(prompt);
    }
    catch (const std::exception& e) { result = std::string("Exception: ") + e.what(); isError = true; }
    catch (...)                      { result = "Unknown error during API call.";       isError = true; }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingResponses.push({ std::move(callback), std::move(result), isError });
    }
    m_isQuerying.store(false);
}

std::string AIAssistantService::PostToLMStudioAPI(const std::string& prompt)
{
    // LM Studio local server — no API key required, uses whatever model is currently loaded.
    // Default endpoint: http://localhost:1234/v1/chat/completions
    const std::string body =
        "{\"model\":\"local-model\",\"max_tokens\":2048,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"" + EscapeJsonString(prompt) + "\"}]}";

    HINTERNET hSession = WinHttpOpen(L"QuickScope-CodeCompare/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return "Error: WinHttpOpen failed (" + std::to_string(GetLastError()) + ").";

    // LM Studio can be slow to respond depending on model size — use a 5-minute timeout.
    // Args: resolve, connect, send, receive (all in milliseconds)
    WinHttpSetTimeouts(hSession, 10000, 10000, 300000, 300000);

    // Plain HTTP on port 1234 — LM Studio's default local server port
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 1234, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return "Error: WinHttpConnect failed (" + std::to_string(GetLastError()) + ")."; }

    // No WINHTTP_FLAG_SECURE — plain HTTP
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/v1/chat/completions",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "Error: WinHttpOpenRequest failed (" + std::to_string(GetLastError()) + ")."; }

    const std::wstring headers = L"Content-Type: application/json\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (err == 12029) // ERROR_WINHTTP_CANNOT_CONNECT
            return "Error: Cannot connect to LM Studio (12029). Make sure LM Studio is running, a model is loaded, and the local server is started on port 1234.";
        if (err == 12002) // ERROR_WINHTTP_TIMEOUT
            return "Error: LM Studio took too long to respond (12002). The model may be overloaded or the prompt too large.";
        return "Error: Failed to send request (" + std::to_string(err) + ").";
    }

    std::string responseBody;
    DWORD bytesAvail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0)
    {
        std::string chunk(bytesAvail, '\0');
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, chunk.data(), bytesAvail, &bytesRead);
        responseBody.append(chunk.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return ParseResponseText(responseBody);
}

std::string AIAssistantService::ParseResponseText(const std::string& jsonBody)
{
    // OpenAI-compatible format: "choices":[{"message":{"content":"..."}}]
    const std::string key = "\"content\":";
    size_t pos = jsonBody.find(key);
    if (pos != std::string::npos)
    {
        pos += key.size();
        while (pos < jsonBody.size() && jsonBody[pos] != '"')
            ++pos;
        ++pos;
    }

    if (pos == std::string::npos || pos >= jsonBody.size())
    {
        // Check if LM Studio returned an error object
        const std::string errKey = "\"message\":";
        size_t epos = jsonBody.find(errKey);
        if (epos != std::string::npos)
        {
            epos += errKey.size();
            while (epos < jsonBody.size() && jsonBody[epos] != '"') ++epos;
            ++epos;
            size_t end = jsonBody.find('"', epos);
            return "API Error: " + jsonBody.substr(epos, end - epos);
        }
        if (jsonBody.empty())
            return "Error: No response from LM Studio. Is a model loaded and the server running on localhost:1234?";
        return "Error: Unexpected response format. Raw body:\n" + jsonBody;
    }

    std::string result;
    result.reserve(256);
    while (pos < jsonBody.size())
    {
        char c = jsonBody[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < jsonBody.size())
        {
            char esc = jsonBody[pos++];
            switch (esc)
            {
            case 'n':  result += '\n'; break;
            case 't':  result += '\t'; break;
            case 'r':  result += '\r'; break;
            case '"':  result += '"';  break;
            case '\\': result += '\\'; break;
            default:   result += esc;  break;
            }
        }
        else { result += c; }
    }
    return result;
}

std::string AIAssistantService::EscapeJsonString(const std::string& input)
{
    std::string out;
    out.reserve(input.size() + 32);
    for (char c : input)
    {
        switch (c)
        {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;      break;
        }
    }
    return out;
}
