#pragma once

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


class JObject
{
public:
    template <typename T> struct IsValidJScalarType
    {
        static constexpr bool Value =
            std::is_same_v<T, TSharedPtr<FJsonValue>> || std::is_same_v<T, TSharedPtr<FJsonObject>> || std::is_same_v<T, bool> || std::is_arithmetic_v<T> ||
            std::is_same_v<T, FString> || std::is_same_v<T, const TCHAR*> || std::is_same_v<T, const ANSICHAR*> || std::is_same_v<T, const UTF8CHAR*>;
    };

    template <typename T> struct IsValidJArrayType { static constexpr bool Value = false; };
    template <typename T> struct IsValidJArrayType<TArray<T>> { static constexpr bool Value = IsValidJScalarType<T>::Value; };
    template <typename T> struct IsValidJArrayType<std::initializer_list<T>> { static constexpr bool Value = IsValidJScalarType<T>::Value; };

    template <typename T> struct IsValidJObjectType { static constexpr bool Value = false; };
    template <typename T> struct IsValidJObjectType<TMap<FString, T>> { static constexpr bool Value = IsValidJScalarType<T>::Value || IsValidJArrayType<T>::Value; };

    // scalar value
    template<class T, std::enable_if_t<IsValidJScalarType<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        if constexpr (std::is_same_v<T, TSharedPtr<FJsonValue>>) {
            return Value;
        }
        else if constexpr (std::is_same_v<T, TSharedPtr<FJsonObject>>) {
            return MakeShared<FJsonValueObject>(Value);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return MakeShared<FJsonValueBoolean>(Value);
        }
        else if constexpr (std::is_arithmetic_v<T>) {
            return MakeShared<FJsonValueNumber>((double)Value);
        }
        else if constexpr (std::is_same_v<T, FString>) {
            return MakeShared<FJsonValueString>(Value);
        }
        else if constexpr (std::is_same_v<T, const TCHAR*>) {
            return MakeShared<FJsonValueString>(Value);
        }
        else if constexpr (std::is_same_v<T, const ANSICHAR*>) {
            return MakeShared<FJsonValueString>(Value);
        }
        else if constexpr (std::is_same_v<T, const UTF8CHAR*>) {
            return MakeShared<FJsonValueString>(Value);
        }
        else {
            // won't be here
            check(false);
            return nullptr;
        }
    }

    // array value
    template<class T, std::enable_if_t<IsValidJArrayType<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        for (auto& V : Value) {
            Data.Add(JValue(V));
        }
        return MakeShared<FJsonValueArray>(Data);
    }

    // object value
    template<class T, std::enable_if_t<IsValidJObjectType<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        auto Data = MakeShared<FJsonValueObject>();
        for (auto& KVP : Value) {
            Data.SetField(KVP.Name, KVP.Value);
        }
        return Data;
    }

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
        Field(const FString& InName, const T& InValue)
            : Name(InName), Value(JValue(InValue))
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

    JObject(std::initializer_list<Field>&& Fields)
    {
        for (auto& V : Fields) {
            Object->SetField(V.Name, V.Value);
        }
    }

    template<class T>
    JObject& Set(const FString& Name, const T& Value)&
    {
        Object->SetField(Name, JValue(Value));
        return *this;
    }
    template<class T>
    JObject&& Set(const FString& Name, const T& Value)&&
    {
        Object->SetField(Name, JValue(Value));
        return MoveTemp(*this);
    }

    Proxy operator[](const FString& Name) { return { this, &Name }; }

    operator TSharedRef<FJsonObject>() { return Object.ToSharedRef(); }
    operator TSharedPtr<FJsonObject>() { return Object; }
    operator TSharedRef<FJsonValue>() { return MakeShared<FJsonValueObject>(Object); }
    operator TSharedPtr<FJsonValue>() { return MakeShared<FJsonValueObject>(Object); }

public:
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
};
