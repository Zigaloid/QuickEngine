#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

/**
 * @brief Async interface to the LM Studio local API via WinHTTP.
 *
 * Call Query() from any thread. The response callback is queued and
 * dispatched safely on the main thread during Update().
 *
 * Requires LM Studio to be running with a model loaded.
 * The default endpoint is http://localhost:1234/v1/chat/completions.
 */
class AIAssistantService
{
public:
    using ResponseCallback = std::function<void(const std::string& response, bool isError)>;

    AIAssistantService() = default;
    ~AIAssistantService();

    // Non-copyable
    AIAssistantService(const AIAssistantService&) = delete;
    AIAssistantService& operator=(const AIAssistantService&) = delete;

    /**
     * @brief Submit a prompt to the locally running LM Studio model asynchronously.
     *        Returns immediately; callback fires during the next Update() call.
     * @param prompt   The user prompt to send.
     * @param callback Invoked on the main thread with the response text or an error message.
     */
    void Query(const std::string& prompt, ResponseCallback callback);

    /**
     * @brief Dispatch any completed responses. Call once per frame from the main thread.
     */
    void Update();

    /**
     * @brief Returns true if a query is currently in-flight.
     */
    bool IsQuerying() const { return m_isQuerying.load(); }

private:
    struct PendingResponse
    {
        ResponseCallback callback;
        std::string      result;
        bool             isError = false;
    };

    void        ExecuteQuery(const std::string& prompt, ResponseCallback callback);
    std::string PostToLMStudioAPI(const std::string& prompt);
    std::string ParseResponseText(const std::string& jsonBody);
    std::string EscapeJsonString(const std::string& input);

    std::atomic<bool>            m_isQuerying{ false };
    std::mutex                   m_mutex;
    std::queue<PendingResponse>  m_pendingResponses;
    std::thread                  m_workerThread;
};
