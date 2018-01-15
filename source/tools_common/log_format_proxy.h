#pragma once

namespace tee
{
    enum class LogProxyOptions : unsigned char
    {
        Text = 0,
        Json
    };

    class LogFormatProxy
    {
    public:
        explicit LogFormatProxy(LogProxyOptions options = LogProxyOptions::Text)
        {
            m_options = options;
        }

        void fatal(const char* fmt, ...);
        void warn(const char* fmt, ...);
        void text(const char* fmt, ...);

    private:
        LogProxyOptions m_options;
    };
} // namespace termite
