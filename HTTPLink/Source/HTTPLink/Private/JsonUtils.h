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


template<class T> struct ToJObject {};
template<class T> struct ToJArray {};
template<class T> struct NoExportStruct {};

class JObjectBase
{
public:
    // std::is_arithmetic は bool や char も含んでしまうため、独自に用意
    template <typename T> struct IsNumeric
    {
        static constexpr bool Value =
            std::is_same_v<T, int32> || std::is_same_v<T, uint32> ||
            std::is_same_v<T, int64> || std::is_same_v<T, uint64> ||
            std::is_same_v<T, float> || std::is_same_v<T, double>;
    };

    template <typename T> struct IsChar
    {
        static constexpr bool Value =
            std::is_same_v<T, ANSICHAR> || std::is_same_v<T, UTF8CHAR> || std::is_same_v<T, TCHAR>;
    };
    template <typename T> struct IsCharPtr { static constexpr bool Value = false; };
    template <typename T> struct IsCharPtr<T*> { static constexpr bool Value = IsChar<T>::Value; };
    template <typename T> struct IsCharPtr<const T*> { static constexpr bool Value = IsChar<T>::Value; };

    template <typename T> struct IsStringView { static constexpr bool Value = false; };
    template <typename T> struct IsStringView<TStringView<T>> { static constexpr bool Value = true; };

    template <typename T> struct IsStringLiteral { static constexpr bool Value = false; };
    template <typename T, size_t N> struct IsStringLiteral<T[N]> { static constexpr bool Value = IsChar<T>::Value; };

    template <typename T> struct IsScalar
    {
        static constexpr bool Value =
            std::is_same_v<T, TSharedPtr<FJsonValue>> || std::is_same_v<T, TSharedPtr<FJsonObject>> ||
            std::is_same_v<T, bool> || IsNumeric<T>::Value ||
            std::is_same_v<T, FString> || IsStringView<T>::Value || IsCharPtr<T>::Value;
    };

    template <typename T, typename = void>
    struct HasStaticStruct
    {
        static constexpr bool Value = false;
    };
    template <typename T>
    struct HasStaticStruct<T, std::void_t<decltype(T::StaticStruct())>>
    {
        static constexpr bool Value = true;
    };

    template <typename T, typename = void>
    struct IsNoExportStruct
    {
        static constexpr bool Value = false;
    };
    template <typename T>
    struct IsNoExportStruct<T, std::void_t<decltype(NoExportStruct<T>::StaticStruct())>>
    {
        static constexpr bool Value = true;
    };

    template <typename T, typename = void>
    struct HasToJObject
    {
        static constexpr bool Value = false;
    };
    template <typename T>
    struct HasToJObject<T, std::void_t<decltype(ToJObject<T>::Get(T()))>>
    {
        static constexpr bool Value = true;
    };

    template <typename T, typename = void>
    struct HasToJArray
    {
        static constexpr bool Value = false;
    };
    template <typename T>
    struct HasToJArray<T, std::void_t<decltype(ToJArray<T>::Get(T()))>>
    {
        static constexpr bool Value = true;
    };

    template <typename T, typename = void>
    struct IsObject
    {
        static constexpr bool Value = HasToJObject<T>::Value || HasStaticStruct<T>::Value || IsNoExportStruct<T>::Value;
    };

    template <typename T> struct IsArray;
    template <typename T> struct IsMap;

    template <typename T> struct IsArrayElement
    {
        static constexpr bool Value = IsScalar<T>::Value || IsArray<T>::Value || IsMap<T>::Value ||
            IsObject<T>::Value || HasToJObject<T>::Value || HasToJArray<T>::Value;
    };

    template <typename T> struct IsArray { static constexpr bool Value = HasToJArray<T>::Value; };
    template <typename T> struct IsArray<TArray<T>> { static constexpr bool Value = IsArrayElement<T>::Value; };
    template <typename T> struct IsArray<TArrayView<T>> { static constexpr bool Value = IsArrayElement<T>::Value; };
    template <typename T, size_t N> struct IsArray<T[N]> { static constexpr bool Value = IsArrayElement<T>::Value; };
    template <typename T, size_t N> struct IsArray<TStaticArray<T, N>> { static constexpr bool Value = IsArrayElement<T>::Value; };
    template <typename T> struct IsArray<std::initializer_list<T>> { static constexpr bool Value = IsArrayElement<T>::Value; };

    template <typename T> struct IsMap { static constexpr bool Value = false; };
    template <typename T> struct IsMap<TMap<FString, T>> { static constexpr bool Value = IsArrayElement<T>::Value; };

private:
    // scalar
    template<class T, std::enable_if_t<IsScalar<T>::Value>* = nullptr>
    static TSharedPtr<FJsonValue> MakeValue_(const T& Value)
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
        else if constexpr (IsNumeric<T>::Value) {
            return MakeShared<FJsonValueNumber>((double)Value);
        }
        else if constexpr (std::is_same_v<T, FString> || IsStringView<T>::Value || IsCharPtr<T>::Value) {
            return MakeShared<FJsonValueString>(Value);
        }
        else {
            // no return to be compile error
        }
    }

    // string literal
    template<class T, std::enable_if_t<IsStringLiteral<T>::Value>* = nullptr>
    static TSharedPtr<FJsonValue> MakeValue_(const T& Value)
    {
        return MakeShared<FJsonValueString>(Value);
    }

    // array
    template<class T, std::enable_if_t<IsArray<T>::Value>* = nullptr>
    static TSharedPtr<FJsonValue> MakeValue_(const T& Value)
    {
        if constexpr (HasToJArray<T>::Value) {
            return ToJArray<T>::Get(Value);
        }
        else {
            TArray<TSharedPtr<FJsonValue>> Data;
            for (auto& V : Value) {
                Data.Add(MakeValue_(V));
            }
            return MakeShared<FJsonValueArray>(Data);
        }
    }

    // map
    template<class T, std::enable_if_t<IsMap<T>::Value>* = nullptr>
    static TSharedPtr<FJsonValue> MakeValue_(const T& Value)
    {
        auto Data = MakeShared<FJsonObject>();
        for (auto& KVP : Value) {
            Data->SetField(KVP.Key, MakeValue_(KVP.Value));
        }
        return MakeShared<FJsonValueObject>(Data);
    }

    // struct
    template<class T, std::enable_if_t<IsObject<T>::Value>* = nullptr>
    static TSharedPtr<FJsonValue> MakeValue_(const T& Value)
    {
        if constexpr (HasToJObject<T>::Value) {
            return ToJObject<T>::Get(Value);
        }
        else if constexpr (HasStaticStruct<T>::Value) {
            return MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(Value));
        }
        else if constexpr (IsNoExportStruct<T>::Value) {
            auto Struct = NoExportStruct<T>::StaticStruct();
            auto* Ops = Struct->GetCppStructOps();
            if (Ops && Ops->HasExportTextItem()) {
                FString StrValue;
                Ops->ExportTextItem(StrValue, &Value, nullptr, nullptr, PPF_None, nullptr);
                return MakeShared<FJsonValueString>(StrValue);
            }
            else {
                auto Json = MakeShared<FJsonObject>();
                if (FJsonObjectConverter::UStructToJsonObject(Struct, &Value, Json)) {
                    return MakeShared<FJsonValueObject>(Json);
                }
            }
            // should not be here
            check(false);
            return nullptr;
        }
    }

public:
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

//// ToJObject 使用例
//
//template<>
//struct ToJObject<FVector>
//{
//    static JObject Get(const FVector& V)
//    {
//        return JObject({ {"x", V.X}, {"y", V.Y}, {"z", V.Z} });
//    }
//};
