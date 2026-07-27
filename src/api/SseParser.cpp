#include "SseParser.h"

void SseParser::feed(const std::string &bytes)
{
    m_buf.append(bytes);

    // Extract every line terminated by \n. The remainder (no trailing \n yet)
    // stays in m_buf for the next feed().
    std::string::size_type start = 0;
    while (true)
    {
        auto nl = m_buf.find('\n', start);
        if (nl == std::string::npos)
            break;
        std::string line = m_buf.substr(start, nl - start);
        // Normalize CRLF → strip a single trailing \r.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        processLine(line);
        start = nl + 1;
    }
    m_buf.erase(0, start);
}

void SseParser::reset()
{
    m_buf.clear();
    m_eventData.clear();
    m_haveData = false;
}

void SseParser::processLine(const std::string &line)
{
    // Blank line → end of event.
    if (line.empty())
    {
        dispatchEvent();
        return;
    }
    // SSE comment (starts with ':') — ignore.
    if (line[0] == ':')
        return;
    // `data:` line. Per spec, the field value is everything after "data:" with
    // exactly one leading space consumed if present.
    static const std::string kData = "data:";
    if (line.compare(0, kData.size(), kData) == 0)
    {
        std::string::size_type p = kData.size();
        if (p < line.size() && line[p] == ' ')
            ++p;
        if (m_haveData)
            m_eventData.push_back(
                '\n'); // multi-line data: → join with \n (spec)
        m_eventData.append(line, p, std::string::npos);
        m_haveData = true;
        return;
    }
    // Other SSE fields (event:, id:, retry:) — not used by OpenAI chat stream.
}

void SseParser::dispatchEvent()
{
    if (!m_haveData)
    {
        m_eventData.clear();
        return;
    }
    if (m_eventData == "[DONE]")
    {
        if (onDone)
            onDone();
    }
    else if (onData)
    {
        onData(m_eventData);
    }
    m_eventData.clear();
    m_haveData = false;
}
