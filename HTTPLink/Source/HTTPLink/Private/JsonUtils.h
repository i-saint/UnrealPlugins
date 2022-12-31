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
    // std::is_arithmetic は bool や char も含んでしまうため、独自に用意
    template <typename T> struct IsJNumeric
    {
        static constexpr bool Value =
            std::is_same_v<T, int32> || std::is_same_v<T, uint32> ||
            std::is_same_v<T, int64> || std::is_same_v<T, uint64> ||
            std::is_same_v<T, float> || std::is_same_v<T, double>;
    };

    template <typename T> struct IsJChar
    {
        static constexpr bool Value =
            std::is_same_v<T, ANSICHAR> || std::is_same_v<T, UTF8CHAR> || std::is_same_v<T, TCHAR>;
    };
    template <typename T> struct IsJCharPtr { static constexpr bool Value = false; };
    template <typename T> struct IsJCharPtr<T*> { static constexpr bool Value = IsJChar<T>::Value; };
    template <typename T> struct IsJCharPtr<const T*> { static constexpr bool Value = IsJChar<T>::Value; };

    template <typename T> struct IsJStringView { static constexpr bool Value = false; };
    template <typename T> struct IsJStringView<TStringView<T>> { static constexpr bool Value = true; };

    template <typename T> struct IsJStringLiteral { static constexpr bool Value = false; };
    template <typename T, size_t N> struct IsJStringLiteral<T[N]> { static constexpr bool Value = IsJChar<T>::Value; };

    template <typename T> struct IsJScalar
    {
        static constexpr bool Value =
            std::is_same_v<T, TSharedPtr<FJsonValue>> || std::is_same_v<T, TSharedPtr<FJsonObject>> ||
            std::is_same_v<T, bool> || IsJNumeric<T>::Value ||
            std::is_same_v<T, FString> || IsJStringView<T>::Value || IsJCharPtr<T>::Value;
    };

    template <typename T> struct IsJArray { static constexpr bool Value = false; };
    template <typename T> struct IsJArray<TArray<T>> { static constexpr bool Value = IsJScalar<T>::Value; };
    template <typename T> struct IsJArray<TArrayView<T>> { static constexpr bool Value = IsJScalar<T>::Value; };
    template <typename T, size_t N> struct IsJArray<T[N]> { static constexpr bool Value = IsJScalar<T>::Value; };
    template <typename T, size_t N> struct IsJArray<TStaticArray<T, N>> { static constexpr bool Value = IsJScalar<T>::Value; };
    template <typename T> struct IsJArray<std::initializer_list<T>> { static constexpr bool Value = IsJScalar<T>::Value; };

    template <typename T> struct IsJObject { static constexpr bool Value = false; };
    template <typename T> struct IsJObject<TMap<FString, T>>
    {
        static constexpr bool Value = IsJScalar<T>::Value || IsJArray<T>::Value || IsJObject<T>::Value;
    };

    // scalar
    template<class T, std::enable_if_t<IsJScalar<T>::Value>* = nullptr>
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
        else if constexpr (IsJNumeric<T>::Value) {
            return MakeShared<FJsonValueNumber>((double)Value);
        }
        else if constexpr (std::is_same_v<T, FString> || IsJStringView<T>::Value || IsJCharPtr<T>::Value) {
            return MakeShared<FJsonValueString>(Value);
        }
        else {
            // no return to be compile error
        }
    }

    // string literal
    template<class T, std::enable_if_t<IsJStringLiteral<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        return MakeShared<FJsonValueString>(Value);
    }

    // array
    template<class T, std::enable_if_t<IsJArray<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        for (auto& V : Value) {
            Data.Add(JValue(V));
        }
        return MakeShared<FJsonValueArray>(Data);
    }

    // object
    template<class T, std::enable_if_t<IsJObject<T>::Value>* = nullptr>
    static inline TSharedPtr<FJsonValue> JValue(const T& Value)
    {
        auto Data = MakeShared<FJsonObject>();
        for (auto& KVP : Value) {
            Data->SetField(KVP.Key, JValue(KVP.Value));
        }
        return MakeShared<FJsonValueObject>(Data);
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
        template<class T>
        Field(const FString& InName, std::initializer_list<T>&& InValue)
            : Name(InName), Value(JValue(MoveTemp(InValue)))
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
        Set(MoveTemp(Fields));
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
