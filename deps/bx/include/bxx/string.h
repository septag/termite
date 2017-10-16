#pragma once

#include "../bx/string.h"
#include <cassert>
#include <string.h>

namespace bx
{
    template <int _Size>
    class String
    {
    public:
        String();
        explicit String(const char* text);
        explicit String(int value);
        explicit String(float value);

        String(const String<_Size>& str);

        bool isEqual(const char* text) const;
        bool isEqualNoCase(const char* text) const;
        bool isEmpty() const { return text[0] == 0; }

        String<_Size>& operator=(const String<_Size>& str);
        String<_Size>& operator=(const char* str);
        String<_Size> operator+(const char* str) const;
        String<_Size> operator+(const String<_Size>& str) const;
        String<_Size>& operator+=(const char* str);
        String<_Size>& operator+=(const String<_Size>& str);

        int getLength() const           {   return (int)strlen(text);    }
        char* getBuffer()               {   return text;    }
        const char* cstr() const        {   return text;    }

        void fromInt(int value);
        void fromFloat(float value);

        int toInt() const;
        float toFloat() const;
        bool toBool() const;
        void* toPointer() const;

        String<_Size>& format(const char* fmt, ...);
        String<_Size>& trimWhitespace();
        String<_Size>& replace(char replace_char, char with_char);
        String<_Size>& toLower();
        String<_Size>& toUpper();

        char& operator[](int index)
        {
            assert(index < _Size);
            return this->text[index];
        }

        const char operator[](int index) const
        {
            assert(index < _Size);
            return this->text[index];
        }

    protected:
        char text[_Size];
    };

    typedef String<512> String512;
    typedef String<256> String256;
    typedef String<128> String128;
    typedef String<64> String64;
    typedef String<32> String32;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// String
    template <int _Size>
    String<_Size>::String()
    {
        text[0] = 0;
    }

    template <int _Size>
    String<_Size>::String(const char* text)
    {
        bx::strCopy(this->text, _Size, text);
    }

    template <int _Size>
    String<_Size>::String(int value)
    {
        fromInt(value);
    }

    template <int _Size>
    String<_Size>::String(float value)
    {
        fromFloat(value);
    }

    template <int _Size>
    String<_Size>::String(const String<_Size>& str)
    {
        strcpy(this->text, str.text);
    }

    template <int _Size>
    bool String<_Size>::isEqual(const char* text) const
    {
        return strcmp(this->text, text) == 0;
    }

    template <int _Size>
    bool String<_Size>::isEqualNoCase(const char* text) const
    {
        return bx::strCmpI(this->text, text) == 0;
    }

    template <int _Size>
    String<_Size>& String<_Size>::operator=(const String<_Size>& str)
    {
        strcpy(this->text, str.text);
        return *this;
    }

    template <int _Size>
    String<_Size>& String<_Size>::operator=(const char* str)
    {
        bx::strCopy(this->text, sizeof(this->text), str);
        return *this;
    }

    template <int _Size>
    String<_Size> String<_Size>::operator+(const char* str) const
    {
        String<_Size> r;
        bx::strCopy(r.text, this->text, _Size);
        bx::strCat(r.text, _Size, str);
        return r;
    }

    template <int _Size>
    String<_Size> String<_Size>::operator+(const String<_Size>& str) const
    {
        String<_Size> r;
        bx::strCopy(r.text, _Size, this->text);
        bx::strCat(r.text, _Size, str.cstr());
        return r;
    }

    template <int _Size>
    String<_Size>& String<_Size>::operator+=(const char* str)
    {
        bx::strCat(this->text, _Size, str);
        return *this;
    }

    template <int _Size>
    String<_Size>& String<_Size>::operator+=(const String<_Size>& str)
    {
        bx::strCat(this->text, _Size, str.cstr());
        return *this;
    }

    template <int _Size>
    void String<_Size>::fromInt(int value)
    {
        snprintf(this->text, _Size, "%d", value);
    }

    template <int _Size>
    void String<_Size>::fromFloat(float value)
    {
        snprintf(this->text, _Size, "%f", value);
    }

    template <int _Size>
    int String<_Size>::toInt() const
    {
        return (int)strtol(this->text, nullptr, 10);
    }

    template <int _Size>
    float String<_Size>::toFloat() const
    {
        return (float)strtod(this->text, nullptr);
    }

    template <int _Size>
    bool String<_Size>::toBool() const
    {
        return bx::toBool(this->text);
    }

    template <int _Size>
    void* String<_Size>::toPointer() const
    {
        void* p;
        sscanf(this->text, "%p", &p);
        return p;
    }

    template <int _Size>
    String<_Size>& String<_Size>::format(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(this->text, _Size, fmt, args);
        va_end(args);

        return *this;
    }

    template <int _Size>
    String<_Size>& String<_Size>::trimWhitespace()
    {
        bx::strws(this->text); //-V530
        return *this;
    }

    template <int _Size>
    String<_Size>& String<_Size>::replace(char replace_char, char with_char)
    {
        int i = 0;
        char c;

        while ((c = this->text[i]) != 0)     {
            if (c == replace_char)     this->text[i] = with_char;
            i++;
        }
        return *this;
    }

    template <int _Size>
    bx::String<_Size>& bx::String<_Size>::toUpper()
    {
        for (int i = 0; this->text[i]; i++) 
            this->text[i] = toupper(this->text[i]);
        return *this;
    }

    template <int _Size>
    bx::String<_Size>& bx::String<_Size>::toLower()
    {
        for (int i = 0; this->text[i]; i++)
            this->text[i] = tolower(this->text[i]);
        return *this;
    }

}   // namespace: bx

