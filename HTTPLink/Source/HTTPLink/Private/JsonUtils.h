#pragma once

#include "UObject/NoExportTypes.h"
#include "UObject/TemplateString.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"


// ちゃんと日本語を出力できる版 TJsonPrintPolicy<UTF8CHAR>
template <>
struct TJsonPrintPolicy<UTF8CHAR>
{
    using CharType = UTF8CHAR;

    static inline void WriteChar(FArchive* Stream, CharType Char)
    {
        Stream->Serialize(&Char, sizeof(CharType));
    }

    static inline void WriteString(FArchive* Stream, const FString& String)
    {
        uint8 Buf[4];
        for (TCHAR C : String) {
            int N = UE::Core::Private::FTCHARToUTF8_Convert::Utf8FromCodepoint(C, Buf, 4);
            Stream->Serialize(Buf, N);
        }
    }

    static inline void WriteStringRaw(FArchive* Stream, const FString& String)
    {
        for (TCHAR C : String) {
            WriteChar(Stream, static_cast<CharType>(C));
        }
    }

    static inline void WriteFloat(FArchive* Stream, float Value)
    {
        WriteStringRaw(Stream, FString::Printf(TEXT("%g"), Value));
    }

    static inline void WriteDouble(FArchive* Stream, double Value)
    {
        WriteStringRaw(Stream, FString::Printf(TEXT("%.17g"), Value));
    }
};


template<class T> struct ToJsonValue {};
template<class T> struct NoExportStruct {};

class JObjectBase
{
public:
    template <class T, class = void>
    struct HasToJsonValue
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct HasToJsonValue<T, std::void_t<decltype(std::declval<ToJsonValue<T>>()(std::declval<T>()))>>
    {
        static constexpr bool Value = true;
    };

    template <class T, class = void>
    struct IsString
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct IsString<T, std::void_t<decltype(FString(std::declval<T>()))>>
    {
        // any types FString::FString() accepts will be true
        static constexpr bool Value = true;
    };

    template <class T, class = void>
    struct HasStaticStruct
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct HasStaticStruct<T, std::void_t<decltype(T::StaticStruct())>>
    {
        static constexpr bool Value = true;
    };
    template <class T, class = void>
    struct IsNoExportStruct
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct IsNoExportStruct<T, std::void_t<decltype(NoExportStruct<T>::StaticStruct())>>
    {
        static constexpr bool Value = true;
    };
    template <class T>
    struct IsStruct
    {
        static constexpr bool Value = HasStaticStruct<T>::Value || IsNoExportStruct<T>::Value;
    };

    template <class T, class = void>
    struct HasToString
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct HasToString<T, std::void_t<decltype(std::declval<T>().ToString())>>
    {
        static constexpr bool Value = true;
    };

    template <class T, class = void>
    struct IsIteratable
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct IsIteratable<T, std::void_t<decltype(std::begin(std::declval<T&>()) != std::end(std::declval<T&>()))>>
    {
        static constexpr bool Value = true;
    };

    template <class T, class = void>
    struct HasStringKey
    {
        static constexpr bool Value = false;
    };
    template <class T>
    struct HasStringKey<T, std::void_t<typename T::KeyType>>
    {
        static constexpr bool Value = IsString<T::KeyType>::Value || HasToString<T::KeyType>::Value;
    };

    template<class T>
    static FString MakeKey(const T& Key)
    {
        if constexpr (IsString<T>::Value) {
            return Key;
        }
        else if constexpr (HasToString<T>::Value) {
            return Key.ToString();
        }
    }
    static const FString& MakeKey(const FString& Key)
    {
        return Key;
    }

    template<class T>
    static TSharedPtr<FJsonValue> MakeValue(const T& Value)
    {
        // json types
        if constexpr (std::is_same_v<T, TSharedPtr<FJsonValue>>) {
            return Value;
        }
        else if constexpr (std::is_same_v<T, TSharedPtr<FJsonObject>>) {
            return MakeShared<FJsonValueObject>(Value);
        }
        // user defined converter
        else if constexpr (HasToJsonValue<T>::Value) {
            return ToJsonValue<T>()(Value);
        }
        // bool
        else if constexpr (std::is_same_v<T, bool>) {
            return MakeShared<FJsonValueBoolean>(Value);
        }
        // numeric
        else if constexpr (std::is_arithmetic_v<T>) {
            return MakeShared<FJsonValueNumber>((double)Value);
        }
        // string
        else if constexpr (IsString<T>::Value) {
            return MakeShared<FJsonValueString>(Value);
        }
        // struct
        else if constexpr (IsStruct<T>::Value) {
            if constexpr (HasStaticStruct<T>::Value) {
                return MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(Value));
            }
            else if constexpr (IsNoExportStruct<T>::Value) {
                auto Struct = NoExportStruct<T>::StaticStruct();
                auto Ops = Struct->GetCppStructOps();
                if (Ops && Ops->HasExportTextItem()) {
                    FString StrValue;
                    Ops->ExportTextItem(StrValue, &Value, nullptr, nullptr, PPF_None, nullptr);
                    return MakeShared<FJsonValueString>(StrValue);
                }
                else {
                    auto Json = MakeShared<FJsonObject>();
                    if (!FJsonObjectConverter::UStructToJsonObject(Struct, &Value, Json)) {
                        // should not be here
                        check(false);
                    }
                    return MakeShared<FJsonValueObject>(Json);
                }
            }
        }
        // range based
        else if constexpr (IsIteratable<T>::Value) {
            // string key & value pairs to Json Object
            if constexpr (HasStringKey<T>::Value) {
                auto Data = MakeShared<FJsonObject>();
                for (auto& KVP : Value) {
                    Data->SetField(MakeKey(KVP.Key), MakeValue(KVP.Value));
                }
                return MakeShared<FJsonValueObject>(Data);
            }
            // others to Json Array
            else {
                TArray<TSharedPtr<FJsonValue>> Data;
                for (auto& E : Value) {
                    Data.Add(MakeValue(E));
                }
                return MakeShared<FJsonValueArray>(Data);
            }
        }
        // all others have ToString()
        else if constexpr (HasToString<T>::Value) {
            return MakeShared<FJsonValueString>(Value.ToString());
        }
        else {
            // no conversion. will be compile error.
        }
    }

    template<class V>
    static TSharedPtr<FJsonValue> MakeValue(std::initializer_list<V>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        for (auto& E : Values) {
            Data.Add(MakeValue(E));
        }
        return MakeShared<FJsonValueArray>(Data);
    }

    template<class... V>
    static TSharedPtr<FJsonValue> MakeValue(TTuple<V...>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        VisitTupleElements([&](auto& Value) { Data.Add(MakeValue(Value)); }, Values);
        return MakeShared<FJsonValueArray>(Data);
    }
};

class JObject : public JObjectBase
{
public:
    struct Field
    {
        FString Key;
        TSharedPtr<FJsonValue> Value;

        Field() = default;
        Field(Field&&) noexcept = default;
        Field& operator=(const Field&) = default;
        Field& operator=(Field&&) noexcept = default;

        template<class K, class V>
        Field(const K& InKey, const V& InValue)
            : Key(MakeKey(InKey)), Value(MakeValue(InValue))
        {}

        template<class K, class V>
        Field(const K& InKey, std::initializer_list<V>&& InValues)
            : Key(MakeKey(InKey)), Value(MakeValue(MoveTemp(InValues)))
        {}

        template<class K, class... V>
        Field(const K& InKey, TTuple<V...>&& InValues)
            : Key(MakeKey(InKey)), Value(MakeValue(MoveTemp(InValues)))
        {}
    };

    template<class K>
    struct Proxy
    {
        JObject* Host;
        K Key;

        template<class V>
        JObject& operator=(const V& Value) const
        {
            Host->Set(Key, Value);
            return *Host;
        }
        template<class V>
        JObject& operator=(std::initializer_list<V>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
            return *Host;
        }
        template<class... V>
        JObject& operator=(TTuple<V...>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
            return *Host;
        }
    };

public:
    JObject() = default;
    JObject(const JObject&) = default;
    JObject(JObject&&) noexcept = default;

    JObject(const Field& F)
    {
        Set(F);
    }
    JObject(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
    }

    void Set(const Field& F)
    {
        Object->SetField(F.Key, F.Value);
    }
    void Set(std::initializer_list<Field>&& Fields)
    {
        for (auto& F : Fields) {
            Object->SetField(F.Key, F.Value);
        }
    }
    template<class K, class V>
    void Set(const K& Key, const V& Value)
    {
        Object->SetField(MakeKey(Key), MakeValue(Value));
    }
    template<class K, class V>
    void Set(const K& Key, std::initializer_list<V>&& Values)
    {
        Object->SetField(MakeKey(Key), MakeValue(MoveTemp(Values)));
    }
    template<class K, class... V>
    void Set(const K& Key, TTuple<V...>&& Values)
    {
        Object->SetField(MakeKey(Key), MakeValue(MoveTemp(Values)));
    }

    JObject& operator+=(const Field& F)
    {
        Set(F);
        return *this;
    }
    JObject& operator+=(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
        return *this;
    }

    template<class K> auto operator[](const K& Key) { return Proxy<const K&>{ this, Key }; }
    template<class K> auto operator[](K&& Key) { return Proxy<FString>{ this, MakeKey(Key) }; }

    TSharedPtr<FJsonObject> ToObject() const { return Object; }
    TSharedPtr<FJsonValue> ToValue() const { return MakeShared<FJsonValueObject>(Object); }

    operator TSharedRef<FJsonObject>() { return ToObject().ToSharedRef(); }
    operator TSharedPtr<FJsonObject>() { return ToObject(); }
    operator TSharedRef<FJsonValue>() { return ToValue().ToSharedRef(); }
    operator TSharedPtr<FJsonValue>() { return ToValue(); }

public:
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
};

class JArray : public JObjectBase
{
public:
    JArray() = default;
    JArray(const JArray&) = default;
    JArray(JArray&&) noexcept = default;

    template<typename... V>
    JArray(V&&... Values)
    {
        Add(Forward<V>(Values)...);
    }
    template<class V>
    JArray(std::initializer_list<V>&& Values)
    {
        Add(MoveTemp(Values));
    }
    template<class... V>
    JArray(TTuple<V...>&& Values)
    {
        Add(MoveTemp(Values));
    }

    template<typename... V>
    void Add(V&&... Values)
    {
        ([&] { Elements.Add(MakeValue(Values)); } (), ...);
    }
    template<class V>
    void Add(std::initializer_list<V>&& Values)
    {
        for (auto& E : Values) {
            Elements.Add(MakeValue(E));
        }
    }
    template<class... V>
    void Add(TTuple<V...>&& Values)
    {
        VisitTupleElements([&](auto& Value) { Add(Value); }, Values);
    }

    template<typename... V>
    JArray& operator+=(V&&... Values)
    {
        Add(Forward<V>(Values)...);
        return *this;
    }
    template<class V>
    JArray& operator+=(std::initializer_list<V>&& Values)
    {
        Add(MoveTemp(Values));
        return *this;
    }
    template<class... V>
    JArray& operator+=(TTuple<V...>&& Values)
    {
        Add(MoveTemp(Values));
        return *this;
    }

    int Num() const { return Elements.Num(); }
    bool IsEmpty() const { return Elements.IsEmpty(); }

    TSharedPtr<FJsonValue> ToValue() const { return MakeShared<FJsonValueArray>(Elements); }
    TArray<TSharedPtr<FJsonValue>> ToArray() const { return Elements; }

    operator TSharedRef<FJsonValue>() { return ToValue().ToSharedRef(); }
    operator TSharedPtr<FJsonValue>() { return ToValue(); }
    operator TArray<TSharedPtr<FJsonValue>>() { return Elements; }

public:
    TArray<TSharedPtr<FJsonValue>> Elements;
};


#define DEF_ENUM(T)

#define DEF_STRUCT(T)\
    template<> struct NoExportStruct<T>\
    {\
        static UScriptStruct* StaticStruct() {\
            extern DLLIMPORT UScriptStruct* Z_Construct_UScriptStruct_##T();\
            return Z_Construct_UScriptStruct_##T();\
        }\
    }

#define DEF_CLASS(T)

#include "./NoExportTypes.def.h"

#undef DEF_ENUM
#undef DEF_STRUCT
#undef DEF_CLASS


// ToJsonValue example
template<>
struct ToJsonValue<JObject>
{
    TSharedPtr<FJsonValue> operator()(const JObject& V) const
    {
        return V.ToValue();
    }
};
template<>
struct ToJsonValue<JArray>
{
    TSharedPtr<FJsonValue> operator()(const JArray& V) const
    {
        return V.ToValue();
    }
};

template<class Char>
struct ToJsonValue<std::basic_string<Char>>
{
    TSharedPtr<FJsonValue> operator()(const std::basic_string<Char>& V) const
    {
        return JObject::MakeValue(V.c_str());
    }
};
