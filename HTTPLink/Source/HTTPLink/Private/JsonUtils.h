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
    struct HasToJsonValue<T, std::void_t<decltype(ToJsonValue<T>::Get(std::declval<T>()))>>
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
    static FString MakeName_(const T& V)
    {
        if constexpr (IsString<T>::Value) {
            return V;
        }
        else if constexpr (HasToString<T>::Value) {
            return V.ToString();
        }
    }

    template<class T>
    static TSharedPtr<FJsonValue> MakeValue_(const T& V)
    {
        // json types
        if constexpr (std::is_same_v<T, TSharedPtr<FJsonValue>>) {
            return V;
        }
        else if constexpr (std::is_same_v<T, TSharedPtr<FJsonObject>>) {
            return MakeShared<FJsonValueObject>(V);
        }
        // user defined converter
        else if constexpr (HasToJsonValue<T>::Value) {
            return ToJsonValue<T>::Get(V);
        }
        // bool
        else if constexpr (std::is_same_v<T, bool>) {
            return MakeShared<FJsonValueBoolean>(V);
        }
        // numeric
        else if constexpr (std::is_arithmetic_v<T>) {
            return MakeShared<FJsonValueNumber>((double)V);
        }
        // string
        else if constexpr (IsString<T>::Value) {
            return MakeShared<FJsonValueString>(V);
        }
        // struct
        else if constexpr (IsStruct<T>::Value) {
            if constexpr (HasStaticStruct<T>::Value) {
                return MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(V));
            }
            else if constexpr (IsNoExportStruct<T>::Value) {
                auto Struct = NoExportStruct<T>::StaticStruct();
                auto Ops = Struct->GetCppStructOps();
                if (Ops && Ops->HasExportTextItem()) {
                    FString StrValue;
                    Ops->ExportTextItem(StrValue, &V, nullptr, nullptr, PPF_None, nullptr);
                    return MakeShared<FJsonValueString>(StrValue);
                }
                else {
                    auto Json = MakeShared<FJsonObject>();
                    if (!FJsonObjectConverter::UStructToJsonObject(Struct, &V, Json)) {
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
                for (auto& KVP : V) {
                    Data->SetField(MakeName_(KVP.Key), MakeValue_(KVP.Value));
                }
                return MakeShared<FJsonValueObject>(Data);
            }
            // others to Json Array
            else {
                TArray<TSharedPtr<FJsonValue>> Data;
                for (auto& E : V) {
                    Data.Add(MakeValue_(E));
                }
                return MakeShared<FJsonValueArray>(Data);
            }
        }
        // all others have ToString()
        else if constexpr (HasToString<T>::Value) {
            return MakeShared<FJsonValueString>(V.ToString());
        }
        else {
            // no conversion. will be compile error.
        }
    }

    template<typename... T>
    static TSharedPtr<FJsonValue> MakeValue(T&&... Values)
    {
        if constexpr (sizeof...(T) == 1) {
            return MakeValue_(std::forward<T>(Values)...);
        }
        else {
            TArray<TSharedPtr<FJsonValue>> Data{ MakeValue_(Values)... };
            return MakeShared<FJsonValueArray>(Data);
        }
    }
};

class JObject : public JObjectBase
{
public:
    struct Field
    {
        FString Name;
        TSharedPtr<FJsonValue> Value;

        Field() = default;
        Field(Field&&) noexcept = default;
        Field& operator=(const Field&) = default;
        Field& operator=(Field&&) noexcept = default;

        template<class T>
        Field(const FString& N, const T& V)
            : Name(N), Value(MakeValue(V))
        {}
        template<class T>
        Field(const FString& N, std::initializer_list<T>&& V)
            : Name(N), Value(MakeValue(MoveTemp(V)))
        {}
        template<typename... T>
        Field(const FString& N, T&&... V)
            : Name(N), Value(MakeValue(std::forward<T>(V)...))
        {}
    };

    struct Proxy
    {
        JObject* Host;
        const FString* Name;

        template<class T>
        JObject& operator=(const T& Value) const
        {
            Host->Set(*Name, Value);
            return *Host;
        }
    };

public:
    JObject() = default;
    JObject(JObject&&) noexcept = default;

    JObject(Field&& V)
    {
        Object->SetField(V.Name, V.Value);
    }
    JObject(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
    }
    template<typename... T>
    JObject(const FString& Name, T&&... Values)
    {
        Set(Name, std::forward<T>(Values)...);
    }

    JObject& Set(std::initializer_list<Field>&& Fields)&
    {
        for (auto& V : Fields) {
            Object->SetField(V.Name, V.Value);
        }
        return *this;
    }
    JObject&& Set(std::initializer_list<Field>&& Fields)&&
    {
        for (auto& V : Fields) {
            Object->SetField(V.Name, V.Value);
        }
        return MoveTemp(*this);
    }

    template<typename... T>
    JObject& Set(const FString& Name, T&&... Values)&
    {
        Object->SetField(Name, MakeValue(std::forward<T>(Values)...));
        return *this;
    }
    template<typename... T>
    JObject&& Set(const FString& Name, T&&... Values)&&
    {
        Object->SetField(Name, MakeValue(std::forward<T>(Values)...));
        return MoveTemp(*this);
    }

    Proxy operator[](const FString& Name) { return { this, &Name }; }

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
    JArray(JArray&&) noexcept = default;

    template<typename... T>
    JArray(T&&... Values)
    {
        Add(std::forward<T>(Values)...);
    }

    template<typename... T>
    JArray& Add(T&&... Values)&
    {
        for (auto Value : { MakeValue(Values)... }) {
            Elements.Add(Value);
        }
        return *this;
    }
    template<typename... T>
    JArray&& Add(T&&... Values)&&
    {
        for (auto Value : { MakeValue(Values)... }) {
            Elements.Add(Value);
        }
        return MoveTemp(*this);
    }

    TArray<TSharedPtr<FJsonValue>> ToArray() const { return Elements; }
    TSharedPtr<FJsonValue> ToValue() const { return MakeShared<FJsonValueArray>(Elements); }

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
template<class Char>
struct ToJsonValue<std::basic_string<Char>>
{
    static TSharedPtr<FJsonValue> Get(const std::basic_string<Char>& V)
    {
        return JObject::MakeValue(V.c_str());
    }
};
