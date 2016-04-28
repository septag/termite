#pragma once

#include "../bx/allocator.h"
#include "pool.h"

namespace bx
{
    enum class JsonType
    {
        Null = 0,
        Object,
        Array,
        String,
        Int,
        Float,
        Bool
    };

    class JsonNode
    {
        friend class bx::Pool<JsonNode>;
        friend class JsonNodeAllocator;
        friend JsonNode* createJsonNode(bx::AllocatorI*, const char*, JsonType);
        friend JsonNode* parseJsonImpl(char*, char**, const char**, int*, bx::AllocatorI*);

        BX_CLASS(JsonNode, 
                 NO_COPY, 
                 NO_ASSIGNMENT);
    private:
        JsonNode();

    public:
        ~JsonNode();

        JsonNode& addChild(JsonNode* node);

        JsonNode* getParent() const   {   return parent;    }
        JsonNode* getNext() const     {   return next;      }
        const char* valueString() const     {   return value.s ? value.s : ""; }
        float valueFloat() const            {   return value.f; }
        int valueInt() const                {   return value.i; }
        bool valueBool() const              {   return value.b; }
        bool isNull() const                 {   return type == JsonType::Null;  }
        JsonType getType() const            {   return type;    }
        const char* getName() const         {   return name;    }
        int getArrayCount() const           {   return numChildItems;    }
        int getChildCount() const           {   return numChildItems;    }

        // Note: Strings should be handled by caller, will not be handled by the node
        JsonNode* setString(const char* str)
        {
            value.s = const_cast<char*>(str);  
            type = JsonType::String;
            return this;
        }

        JsonNode* setFloat(float f)
        {
            value.f = f;
            type = JsonType::Float;
            return this;
        }

        JsonNode* setBool(bool b)
        {
            value.b = b;
            type = JsonType::Bool;
            return this;
        }

        JsonNode* setInt(int i)
        {
            value.i = i;
            type = JsonType::Int;
            return this;
        }

        const JsonNode* findChild(const char* name) const;
        const JsonNode* getArrayItem(int index) const;

        void destroy();

    private:
        JsonType type;
        char* name;
        JsonNode* parent;
        JsonNode* next;
        JsonNode* prev;
        JsonNode* firstChild;
        JsonNode* lastChild;
        bx::AllocatorI* alloc;
        int numChildItems;

        union   {
            float f;
            int i;
            char* s;
            bool b;
        } value;

    private:
        static const JsonNode None;
    };

    struct JsonError
    {
        char pos[16];
        const char* desc;
        int line;
    };

    // JsonNodeAllocator is a fast, pool based, node allocator
    // Helps to parse and make fast json nodes in case you don't want to use the slow allocators like heap
    class JsonNodeAllocator : public bx::AllocatorI
    {
        BX_CLASS(JsonNodeAllocator
                 , NO_COPY
                 , NO_ASSIGNMENT
                 );

    public:
        JsonNodeAllocator(bx::AllocatorI* alloc, int bucketSize = 256)
        {
            m_pool.create(bucketSize, alloc);
            m_alloc = alloc;
        }

        virtual ~JsonNodeAllocator()
        {
            m_pool.destroy();
        }

        void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) override
        {
            if (_size == 0) {
                // free
                m_pool.deleteInstance((JsonNode*)_ptr);
                return NULL;
            } else if (_ptr == NULL) {
                // malloc
                return m_pool.newInstance();
            } else {
                // realloc
                JsonNode* node = (JsonNode*)_ptr;
                node->~JsonNode();
                return new(node) JsonNode();
            }
        }

    private:
        bx::AllocatorI* m_alloc;
        bx::Pool<JsonNode> m_pool;
    };

    // Parse json string
    // String should stay in memory until working with nodes are done. Data content is all in-place
    JsonNode* parseJson(char* str, bx::AllocatorI* nodeAlloc, JsonError* errors);

    // Make json string from given root node
    // Returned char* should be freed by caller using the provided 'alloc'
    char* makeJson(const JsonNode* root, bx::AllocatorI* alloc, bool packed);

    // Creates a Json node
    JsonNode* createJsonNode(bx::AllocatorI* nodeAlloc, const char* name = nullptr, JsonType type = JsonType::Null);

}


#ifdef BX_IMPLEMENT_JSON

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cassert>    

#include "array.h"
#include "string.h"

namespace bx
{
    // convert string to integer
    static char *atoi(char *first, char *last, int *out)
    {
        int sign = 1;
        if (first != last)    {
            if (*first == '-')  {
                sign = -1;
                ++first;
            } else if (*first == '+') {
                ++first;
            }
        }

        int result = 0;
        for (; first != last && isdigit(*first); ++first)  {
            result = 10 * result + (*first - '0');
        }
        *out = result * sign;

        return first;
    }

    // convert hexadecimal string to unsigned integer
    static char *hatoui(char *first, char *last, unsigned int *out)
    {
        unsigned int result = 0;
        for (; first != last; ++first)    {
            int digit;
            if (isdigit(*first))   {
                digit = *first - '0';
            }   else if (*first >= 'a' && *first <= 'f')    {
                digit = *first - 'a' + 10;
            }   else if (*first >= 'A' && *first <= 'F')    {
                digit = *first - 'A' + 10;
            }   else    {
                break;
            }
            result = 16 * result + (unsigned int)digit;
        }
        *out = result;

        return first;
    }

    // convert string to floating point
    static char *atof(char *first, char *last, float *out)
    {
        // sign
        float sign = 1;
        if (first != last)    {
            if (*first == '-')  {
                sign = -1;
                ++first;
            } else if (*first == '+') {
                ++first;
            }
        }

        // integer part
        float result = 0;
        for (; first != last && isdigit(*first); ++first)  {
            result = 10 * result + (float)(*first - '0');
        }

        // fraction part
        if (first != last && *first == '.') {
            ++first;

            float inv_base = 0.1f;
            for (; first != last && isdigit(*first); ++first)  {
                result += (float)(*first - '0') * inv_base;
                inv_base *= 0.1f;
            }
        }

        // result w\o exponent
        result *= sign;

        // exponent
        bool exponent_negative = false;
        int exponent = 0;
        if (first != last && (*first == 'e' || *first == 'E'))    {
            ++first;

            if (*first == '-')        {
                exponent_negative = true;
                ++first;
            }   else if (*first == '+')     {
                ++first;
            }

            for (; first != last && isdigit(*first); ++first)  {
                exponent = 10 * exponent + (*first - '0');
            }
        }

        if (exponent)    {
            float power_of_ten = 10;
            for (; exponent > 1; exponent--)        {
                power_of_ten *= 10;
            }

            if (exponent_negative)        {
                result /= power_of_ten;
            }   else        {
                result *= power_of_ten;
            }
        }

        *out = result;

        return first;
    }


#define JSON_ERROR(it, desc)\
    *error_pos = it;\
    *error_desc = desc;\
    *error_line = 1 - escaped_newlines;\
    for (char *c = it; c != source; --c)\
        if (*c == '\n') ++*error_line;\
    return 0    

#define CHECK_TOP() if (!top) {JSON_ERROR(it, "Unexpected character");}

    JsonNode* createJsonNode(bx::AllocatorI* nodeAlloc, const char* name, JsonType type)
    {
        JsonNode* node = BX_NEW(nodeAlloc, JsonNode);
        node->alloc = nodeAlloc;
        node->name = const_cast<char*>(name);
        node->type = type;

        return node;
    }

    JsonNode* parseJsonImpl(char* source, char** error_pos, const char** error_desc, int* error_line, bx::AllocatorI* alloc)
    {
        JsonNode* root = nullptr;
        JsonNode* top = nullptr;

        char* name = nullptr;
        char* it = source;

        int escaped_newlines = 0;

        while (*it)    {
            // skip white space
            while (*it == '\x20' || *it == '\x9' || *it == '\xD' || *it == '\xA')
                ++it;

            switch (*it)    {
            case '\0':
                break;
            case '{':
            case '[':
                {
                    // create new value
                    JsonNode* object = createJsonNode(alloc);
                    if (!object)
                        return nullptr;

                    // name
                    object->name = name;
                    name = 0;

                    // type
                    object->type = (*it == '{') ? JsonType::Object : JsonType::Array;

                    // skip open character
                    ++it;

                    // set top and root
                    if (top)                {
                        top->addChild(object);
                    } else if (!root) {
                        root = object;
                    } else {
                        JSON_ERROR(it, "Second root. Only one root allowed");
                    }
                    top = object;
                }
                break;

            case '}':
            case ']':
                {
                    if (!top || top->type != ((*it == '}') ? JsonType::Object : JsonType::Array))  {
                        JSON_ERROR(it, "Mismatch closing brace/bracket");
                    }

                    // skip close character
                    ++it;

                    // set top
                    top = top->parent;
                }
                break;

            case ':':
                if (!top || top->type != JsonType::Object)  {
                    JSON_ERROR(it, "Unexpected character");
                }
                ++it;
                break;

            case ',':
                CHECK_TOP();
                ++it;
                break;

            case '"':
                {
                    CHECK_TOP();

                    // skip '"' character
                    ++it;

                    char *first = it;
                    char *last = it;
                    while (*it)                {
                        if ((unsigned char)*it < '\x20')  {
                            JSON_ERROR(first, "Control characters not allowed in strings");
                        } else if (*it == '\\')  {
                            switch (it[1])
                            {
                            case '"':
                                *last = '"';
                                break;
                            case '\\':
                                *last = '\\';
                                break;
                            case '/':
                                *last = '/';
                                break;
                            case 'b':
                                *last = '\b';
                                break;
                            case 'f':
                                *last = '\f';
                                break;
                            case 'n':
                                *last = '\n';
                                ++escaped_newlines;
                                break;
                            case 'r':
                                *last = '\r';
                                break;
                            case 't':
                                *last = '\t';
                                break;
                            case 'u':
                                {
                                    unsigned int codepoint;
                                    if (hatoui(it + 2, it + 6, &codepoint) != it + 6)  {
                                        JSON_ERROR(it, "Bad unicode codepoint");
                                    }

                                    if (codepoint <= 0x7F)  {
                                        *last = (char)codepoint;
                                    }
                                    else if (codepoint <= 0x7FF)  {
                                        *last++ = (char)(0xC0 | (codepoint >> 6));
                                        *last = (char)(0x80 | (codepoint & 0x3F));
                                    }
                                    else if (codepoint <= 0xFFFF)   {
                                        *last++ = (char)(0xE0 | (codepoint >> 12));
                                        *last++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                                        *last = (char)(0x80 | (codepoint & 0x3F));
                                    }
                                }
                                it += 4;
                                break;
                            default:
                                JSON_ERROR(first, "Unrecognized escape sequence");
                            }

                            ++last;
                            it += 2;
                        }
                        else if (*it == '"')
                        {
                            *last = 0;
                            ++it;
                            break;
                        }
                        else
                        {
                            *last++ = *it++;
                        }
                    }

                    if (!name && top->type == JsonType::Object)   {
                        // field name in object
                        name = first;
                    }  else  {
                        // new string value
                        JsonNode *object = createJsonNode(alloc);
                        if (!object)
                            return nullptr;

                        object->name = name;
                        name = 0;

                        object->type = JsonType::String;
                        object->value.s = first;

                        top->addChild(object);
                    }
                }
                break;

            case 'n':
            case 't':
            case 'f':
                {
                    CHECK_TOP();

                    // new null/bool value
                    JsonNode* object = createJsonNode(alloc);
                    if (!object)
                        return nullptr;

                    object->name = name;
                    name = 0;

                    // null
                    if (it[0] == 'n' && it[1] == 'u' && it[2] == 'l' && it[3] == 'l')  {
                        object->type = JsonType::Null;
                        it += 4;
                    }
                    // true
                    else if (it[0] == 't' && it[1] == 'r' && it[2] == 'u' && it[3] == 'e')   {
                        object->type = JsonType::Bool;
                        object->value.b = true;
                        it += 4;
                    }
                    // false
                    else if (it[0] == 'f' && it[1] == 'a' && it[2] == 'l' && it[3] == 's' && it[4] == 'e')   {
                        object->type = JsonType::Bool;
                        object->value.b = false;
                        it += 5;
                    }
                    else
                    {
                        JSON_ERROR(it, "Unknown identifier");
                    }

                    top->addChild(object);
                }
                break;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                {
                    CHECK_TOP();

                    // new number value
                    JsonNode* object = createJsonNode(alloc);
                    if (!object)
                        return nullptr;

                    object->name = name;
                    name = 0;

                    object->type = JsonType::Int;

                    char *first = it;
                    while (*it != '\x20' && *it != '\x9' && *it != '\xD' && *it != '\xA' &&
                           *it != ',' && *it != ']' && *it != '}')  {
                        if (*it == '.' || *it == 'e' || *it == 'E')
                        {
                            object->type = JsonType::Float;
                        }
                        ++it;
                    }

                    if (object->type == JsonType::Int && atoi(first, it, &object->value.i) != it)   {
                        JSON_ERROR(first, "Bad integer number");
                    }

                    if (object->type == JsonType::Float && atof(first, it, &object->value.f) != it)   {
                        JSON_ERROR(first, "Bad float number");
                    }

                    top->addChild(object);
                }
                break;

            default:
                JSON_ERROR(it, "Unexpected character");
            }
        }

        if (top)    {
            JSON_ERROR(it, "Not all objects/arrays have been properly closed");
        }

        return root;
    }

    JsonNode* parseJson(char* str, bx::AllocatorI* nodeAlloc, JsonError* errors)
    {
        char* err_pos;
        const char* err_desc;
        int err_line;

        JsonNode* root = parseJsonImpl(str, &err_pos, &err_desc, &err_line, nodeAlloc);
        if (!root)  {
            if (errors) {
                strncpy(errors->pos, err_pos, sizeof(errors->pos));
                errors->desc = err_desc;
                errors->line = err_line;
            }
            return nullptr;
        }

        return root;
    }
    
    static void makeJsonFromNode(const JsonNode* node, bx::Array<char>* buff, bool packed, bool isRoot)
    {
        // Name
        if (!isRoot && node->getName()) {
            int s = (int)strlen(node->getName());
            char* name = buff->pushMany(s + 3);
            name[0] = '\"';
            memcpy(&name[1], node->getName(), s);
            name[s + 1] = '\"';
            name[s + 2] = ':';
        }

        // Value
        switch (node->getType()) {
        case JsonType::Object:
        {
            *buff->push() = '{';
            for (int i = 0, c = node->getChildCount(); i < c; i++) {
                makeJsonFromNode(node->getArrayItem(i), buff, packed, false);
                if (i < c - 1)
                    *buff->push() = ',';
            }
            *buff->push() = '}';
            break;
        }
        case JsonType::Array:
        {
            *buff->push() = '[';
            for (int i = 0, c = node->getArrayCount(); i < c; i++) {
                makeJsonFromNode(node->getArrayItem(i), buff, packed, false);
                if (i < c - 1)
                    *buff->push() = ',';
            }
            *buff->push() = ']';
            break;
        }
        case JsonType::Bool:
        {
            const char* value = node->valueBool() ? "true" : "false";
            int s = (int)strlen(value);
            char* valuestr = buff->pushMany(s);
            memcpy(valuestr, value, s);
            break;
        }
        case JsonType::Float:
        {
            bx::String32 f;
            f.format("%f", node->valueFloat());
            int s = f.getLength();
            char* valuestr = buff->pushMany(s);
            memcpy(valuestr, f.cstr(), s);
            break;
        }

        case JsonType::Int:
        {
            bx::String32 i;
            i.format("%d", node->valueInt());
            int s = i.getLength();
            char* valuestr = buff->pushMany(s);
            memcpy(valuestr, i.cstr(), s);
            break;
        }

        case JsonType::Null:
        {
            const char* value = "null";
            int s = (int)strlen(value);
            char* valuestr = buff->pushMany(s);
            memcpy(valuestr, value, s);
            break;
        }

        case JsonType::String:
        {
            int s = (int)strlen(node->valueString());
            char* value = buff->pushMany(s + 2);
            value[0] = '\"';
            memcpy(&value[1], node->valueString(), s);
            value[s + 1] = '\"';
            break;
        }
        }
    }

    char* makeJson(const JsonNode* root, bx::AllocatorI* alloc, bool packed)
    {
        bx::Array<char> buff;
        buff.create(512, 2048, alloc);

        makeJsonFromNode(root, &buff, packed, true);
        *buff.push() = '\0';    // terminate the string
        
        int size;
        char* r = buff.detach(&size);
        return r;
    }


    // JsonNode
    const JsonNode JsonNode::None;

    JsonNode::JsonNode()
    {
        type = JsonType::Null;
        alloc = nullptr;
        name = nullptr;
        parent = next = prev = firstChild = lastChild = nullptr;
        alloc = nullptr;
        value.s = nullptr;
        numChildItems = 0;
    }

    JsonNode::~JsonNode()
    {
        assert(type == JsonType::Null);
    }

    JsonNode& JsonNode::addChild(JsonNode* node)
    {
        assert(this->type != JsonType::Null);

        // Add to linked list of traces
        if (this->lastChild)    {
            this->lastChild->next = node;
            node->prev = this->lastChild;
            this->lastChild = node;
        } else {
            this->firstChild = this->lastChild = node;
        }

        this->numChildItems ++;

        node->parent = this;
        return *this;            
    }

    const JsonNode* JsonNode::findChild(const char* name) const
    {
        JsonNode* node = this->firstChild;
        while (node)    {
            if (strcmp(node->name, name) == 0)
                return node;
            node = node->next;
        }
        return &JsonNode::None;            
    }

    const JsonNode* JsonNode::getArrayItem(int index) const
    {
        assert(index < this->numChildItems);

        int i = 0;
        JsonNode* node = this->firstChild;
        while (node)    {
            if (i == index)
                return node;
            i++;
            node = node->next;
        }
        return &JsonNode::None;            
    }

    void JsonNode::destroy()
    {
        // Remove from children
        JsonNode* parent = this->parent;
        if (parent) {
            if (parent->firstChild != this)   {
                if (this->next)
                    this->next->prev = this->prev;
                if (this->prev)
                    this->prev->next = this->next;
            }   else    {
                if (this->next)
                    this->next->prev = nullptr;
                parent->firstChild = this->next;
            }
        }

        // Destroy children
        JsonNode* child = this->firstChild;
        while (child)   {
            JsonNode* next_child = child->next;
            child->destroy();
            child = next_child;
        }

        this->type = JsonType::Null;

        BX_DELETE(this->alloc, this);            
    }
}   // namespace: bx
#endif


