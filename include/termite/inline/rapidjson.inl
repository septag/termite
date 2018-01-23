#pragma once

namespace tee {
    namespace json {
        template <typename _T>
        void getIntArray(const _T& jvalue, int* n, int num)
        {
            assert(jvalue.IsArray());
            num = std::min<int>(jvalue.Size(), num);
            for (int i = 0; i < num; i++)
                n[i] = jvalue[i].GetInt();
        }


        template <typename _T>
        void getUInt16Array(const _T& jvalue, uint16_t* n, int num)
        {
            assert(jvalue.IsArray());
            num = std::min<int>(jvalue.Size(), num);
            for (int i = 0; i < num; i++)
                n[i] = (uint16_t)jvalue[i].GetInt();
        }

        template <typename _T>
        void getFloatArray(const _T& jvalue, float* f, int num)
        {
            assert(jvalue.IsArray());
            num = std::min<int>(jvalue.Size(), num);
            for (int i = 0; i < num; i++)
                f[i] = jvalue[i].GetFloat();
        }

        template <typename _T, typename _AllocT>
        _T createIntArray(const int* n, int num, _AllocT& alloc)
        {
            _T value(rapidjson::kArrayType);
            for (int i = 0; i < num; i++)
                value.PushBack(_T(n[i]).Move(), alloc);
            return value;
        }

        template <typename _T, typename _AllocT>
        _T createFloatArray(const float* f, int num, _AllocT& alloc)
        {
            _T value(rapidjson::kArrayType);
            for (int i = 0; i < num; i++)
                value.PushBack(_T(f[i]).Move(), alloc);
            return value;
        }
    }
}
