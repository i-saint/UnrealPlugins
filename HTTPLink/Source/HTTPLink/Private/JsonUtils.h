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


template<class T> struct ToJsonKey {};
template<class T> struct ToJsonValue {};
template<class T> struct NoExportStruct {};

class JObjectBase
{
public:
#define DEF_VALUE(Name, Result)\
    template <class T>\
    struct Name\
    {\
        static constexpr bool Value = Result;\
    }

#define DEF_VALUE_C(Name, Cond, Result)\
    template <class T, class = void>\
    struct Name\
    {\
        static constexpr bool Value = false;\
    };\
    template <class T>\
    struct Name<T, std::void_t<Cond>>\
    {\
        static constexpr bool Value = Result;\
    }

    DEF_VALUE_C(HasToJsonKey, decltype(std::declval<ToJsonKey<T>>()(std::declval<T>())), true);
    DEF_VALUE_C(HasToJsonValue, decltype(std::declval<ToJsonValue<T>>()(std::declval<T>())), true);

    DEF_VALUE_C(CanConstructString, decltype(FString(std::declval<T>())), true);
    DEF_VALUE_C(HasToString, decltype(std::declval<T>().ToString()), true);
    DEF_VALUE_C(HasLexToString, decltype(LexToString(std::declval<T>())), true);
    DEF_VALUE(CanToString, HasToJsonKey<T>::Value || CanConstructString<T>::Value || HasToString<T>::Value || HasLexToString<T>::Value);

    DEF_VALUE_C(HasStaticStruct, decltype(T::StaticStruct()), true);
    DEF_VALUE_C(IsNoExportStruct, decltype(NoExportStruct<T>::StaticStruct()), true);
    DEF_VALUE(IsStruct, HasStaticStruct<T>::Value || IsNoExportStruct<T>::Value);

    DEF_VALUE_C(IsIteratable, decltype(std::begin(std::declval<T&>()) != std::end(std::declval<T&>())), true);
    DEF_VALUE_C(HasKeyType, typename T::KeyType, true);
    DEF_VALUE_C(HasStringCompatibleKeyType, typename T::KeyType, CanToString<typename T::KeyType>::Value);

    DEF_VALUE_C(CanConstructFromString, decltype(T(std::declval<FString>())), true);
    DEF_VALUE_C(CanConstructFromCString, decltype(T(std::declval<const TCHAR*>())), true);
    DEF_VALUE_C(CanLexFromString, decltype(LexFromString(std::declval<T&>(), std::declval<const TCHAR*>())), true);
    DEF_VALUE_C(IsArrayLikeContainer, decltype(std::declval<T>().Add(std::declval<typename T::ElementType>())), true);
    DEF_VALUE_C(IsMapLikeContainer, decltype(std::declval<T>().Add(std::declval<typename T::KeyType>(), std::declval<typename T::ValueType>())), true);

#undef DEF_VALUE
#undef DEF_VALUE_C

    template<class T>
    static FString ToString(const T& Key)
    {
        if constexpr (HasToJsonKey<T>::Value) {
            return ToJsonKey<T>()(Key);
        }
        else if constexpr (CanConstructString<T>::Value) {
            return Key;
        }
        else if constexpr (HasToString<T>::Value) {
            return Key.ToString();
        }
        else if constexpr (HasLexToString<T>::Value) {
            return LexToString(Key);
        }
    }

    template<class T>
    static FString MakeKey(const T& Key)
    {
        return ToString(Key);
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
        else if constexpr (CanConstructString<T>::Value) {
            return MakeShared<FJsonValueString>(Value);
        }
        // struct
        else if constexpr (HasStaticStruct<T>::Value) {
            return MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(Value));
        }
        else if constexpr (IsNoExportStruct<T>::Value) {
            auto Struct = NoExportStruct<T>::StaticStruct();
            auto Ops = Struct->GetCppStructOps();
            if (Ops && Ops->HasExportTextItem()) {
                FString StrValue;
                if (!Ops->ExportTextItem(StrValue, &Value, nullptr, nullptr, PPF_None, nullptr)) {
                    check(false);
                }
                return MakeShared<FJsonValueString>(StrValue);
            }
            else {
                auto Json = MakeShared<FJsonObject>();
                if (!FJsonObjectConverter::UStructToJsonObject(Struct, &Value, Json)) {
                    check(false);
                }
                return MakeShared<FJsonValueObject>(Json);
            }
        }
        // range based
        else if constexpr (IsIteratable<T>::Value) {
            // string key & value pairs to Json Object
            if constexpr (HasStringCompatibleKeyType<T>::Value) {
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
    template<class... V>
    static TSharedPtr<FJsonValue> MakeValue(TTuple<V&...>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        VisitTupleElements([&](auto& Value) { Data.Add(MakeValue(Value)); }, Values);
        return MakeShared<FJsonValueArray>(Data);
    }


    template<class T>
    static bool FromKey(const FString& Key, T& Dst)
    {
        if constexpr (CanConstructFromString<T>::Value) {
            Dst = T(Key);
            return true;
        }
        else if constexpr (CanConstructFromCString<T>::Value) {
            Dst = T(*Key);
            return true;
        }
        else if constexpr (CanLexFromString<T>::Value) {
            LexFromString(Dst, *Key);
            return true;
        }
    }

    template<class T>
    static bool FromValue(TSharedPtr<FJsonValue> Value, T& Dst)
    {
        if constexpr (std::is_same_v<T, TSharedPtr<FJsonValue>>) {
            Dst = Value;
            return true;
        }
        else if constexpr (std::is_same_v<T, TSharedPtr<FJsonObject>>) {
            return Value->TryGetObject(Dst);
        }
        // bool
        else if constexpr (std::is_same_v<T, bool>) {
            return Value->TryGetBool(Dst);
        }
        // numeric
        else if constexpr (std::is_arithmetic_v<T>) {
            return Value->TryGetNumber(Dst);
        }
        // struct
        else if constexpr (HasStaticStruct<T>::Value) {
            TSharedPtr<FJsonObject> Obj;
            if (Value->TryGetObject(Obj)) {
                return FJsonObjectConverter::JsonObjectToUStruct(Obj.ToSharedRef(), &Dst);
            }
            return false;
        }
        else if constexpr (IsNoExportStruct<T>::Value) {
            auto Struct = NoExportStruct<T>::StaticStruct();
            auto Ops = Struct->GetCppStructOps();
            if (Ops && Ops->HasImportTextItem()) {
                FString StrValue;
                if (Value->TryGetString(StrValue)) {
                    const TCHAR* Buf = *StrValue;
                    return Ops->ImportTextItem(Buf, &Dst, PPF_None, nullptr, nullptr);
                }
            }
            else {
                TSharedPtr<FJsonObject>* Obj;
                if (Value->TryGetObject(Obj)) {
                    return FJsonObjectConverter::JsonObjectToUStruct(Obj->ToSharedRef(), Struct, &Dst);
                }
            }
            return false;
        }
        // map like container
        else if constexpr (IsMapLikeContainer<T>::Value) {
            TSharedPtr<FJsonObject>* Obj;
            if (Value->TryGetObject(Obj)) {
                for (auto& KVP : (*Obj)->Values) {
                    typename T::ElementType Tmp;
                    if (FromKey(KVP.Key, Tmp.Key) && FromValue(KVP.Value, Tmp.Value)) {
                        Dst.Add(Tmp);
                    }
                }
                return true;
            }
            return false;
        }
        // array like container
        else if constexpr (IsArrayLikeContainer<T>::Value) {
            TArray<TSharedPtr<FJsonValue>>* Elements;
            if (Value->TryGetArray(Elements)) {
                for (auto& E : *Elements) {
                    typename T::ElementType Tmp;
                    if (FromValue(E, Tmp)) {
                        Dst.Add(Tmp);
                    }
                }
                return true;
            }
            return false;
        }
        // raw array
        else if constexpr (std::is_array_v<T>) {
            TArray<TSharedPtr<FJsonValue>>* Elements;
            if (Value->TryGetArray(Elements)) {
                int I = 0;
                for (auto& E : *Elements) {
                    if (I < std::size(Dst)) {
                        FromValue(E, Dst[I++]);
                    }
                }
                return true;
            }
            return false;
        }
        // all others can construct from string
        else if constexpr (CanConstructFromString<T>::Value) {
            FString Str;
            if (Value->TryGetString(Str)) {
                Dst = T(Str);
                return true;
            }
            return false;
        }
    }

    template<class... V>
    static bool FromValue(TSharedPtr<FJsonValue> Value, TTuple<V...>& Dsts)
    {
        TArray<TSharedPtr<FJsonValue>>* Elements;
        if (Value->TryGetArray(Elements)) {
            int I = 0;
            VisitTupleElements([&](auto& Dst) {
                if (I < Elements->Num()) {
                    FromValue((*Elements)[I++], Dst);
                }
                }, Dsts);
            return true;
        }
        return false;
    }
    template<class... V>
    static bool FromValue(TSharedPtr<FJsonValue> Value, TTuple<V&...>&& Dsts)
    {
        TArray<TSharedPtr<FJsonValue>>* Elements;
        if (Value->TryGetArray(Elements)) {
            int I = 0;
            VisitTupleElements([&](auto& Dst) {
                if (I < Elements->Num()) {
                    FromValue((*Elements)[I++], Dst);
                }
                }, Dsts);
            return true;
        }
        return false;
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
        template<class K, class... V>
        Field(const K& InKey, TTuple<V&...>&& InValues)
            : Key(MakeKey(InKey)), Value(MakeValue(MoveTemp(InValues)))
        {}
    };

    template<class K>
    struct Proxy
    {
        JObject* Host;
        K Key;

        template<class V>
        void operator=(const V& Value) const
        {
            Host->Set(Key, Value);
        }
        template<class V>
        void operator=(std::initializer_list<V>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
        }
        template<class... V>
        void operator=(TTuple<V...>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
        }
        template<class... V>
        void operator=(TTuple<V&...>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
        }
    };

public:
    JObject() = default;
    JObject(JObject&&) = default;
    JObject(const JObject&) = default;
    JObject& operator=(JObject&&) = default;
    JObject& operator=(const JObject&) = default;


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
    template<class K, class... V>
    void Set(const K& Key, TTuple<V&...>&& Values)
    {
        Object->SetField(MakeKey(Key), MakeValue(MoveTemp(Values)));
    }

    void operator+=(const Field& F)
    {
        Set(F);
    }
    void operator+=(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
    }


    template<class K, class V>
    bool Get(const K& Key, V& Dst)
    {
        if (auto Value = Object->Values.Find(MakeKey(Key))) {
            return FromValue(*Value, Dst);
        }
        return false;
    }
    template<class K, class... V>
    bool Get(const K& Key, TTuple<V&...>&& Dst)
    {
        if (auto Value = Object->Values.Find(MakeKey(Key))) {
            return FromValue(*Value, MoveTemp(Dst));
        }
        return false;
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
    using Container     = TMap<FString, TSharedPtr<FJsonValue>>;
    using Iterator      = Container::TRangedForIterator;
    using ConstIterator = Container::TRangedForConstIterator;

    bool IsValid() const { return Object != nullptr; }
    int Num() const      { return IsValid() ? Object->Values.Num() : 0; }
    bool IsEmpty() const { return Num() != 0; }

    // const_cast because TSharedPtr::operator-> always return non const raw pointer
    Iterator      begin()       { return Object->Values.begin(); }
    ConstIterator begin() const { return const_cast<const Container&>(Object->Values).begin(); }
    Iterator      end()         { return Object->Values.end(); }
    ConstIterator end() const   { return const_cast<const Container&>(Object->Values).end(); }

public:
    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
};

class JArray : public JObjectBase
{
public:
    JArray() = default;
    JArray(JArray&&) = default;
    JArray(const JArray&) = default;
    JArray& operator=(JArray&&) = default;
    JArray& operator=(const JArray&) = default;

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
    template<class... V>
    JArray(TTuple<V&...>&& Values)
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
    template<class... V>
    void Add(TTuple<V&...>&& Values)
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
    template<class... V>
    JArray& operator+=(TTuple<V&...>&& Values)
    {
        Add(MoveTemp(Values));
        return *this;
    }

    TSharedPtr<FJsonValue> ToValue() const { return MakeShared<FJsonValueArray>(Elements); }
    TArray<TSharedPtr<FJsonValue>> ToArray() const { return Elements; }

    operator TSharedRef<FJsonValue>() { return ToValue().ToSharedRef(); }
    operator TSharedPtr<FJsonValue>() { return ToValue(); }
    operator TArray<TSharedPtr<FJsonValue>>() { return Elements; }

public:
    using Container = TArray<TSharedPtr<FJsonValue>>;
    using Iterator = Container::RangedForIteratorType;
    using ConstIterator = Container::RangedForConstIteratorType;

    int Num() const { return Elements.Num(); }
    bool IsEmpty() const { return Elements.IsEmpty(); }

    Iterator      begin()       { return Elements.begin(); }
    ConstIterator begin() const { return Elements.begin(); }
    Iterator      end()         { return Elements.end(); }
    ConstIterator end() const   { return Elements.end(); }

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


// ToJsonKey & ToJsonValue

template<class Char>
struct ToJsonKey<std::basic_string<Char>>
{
    FString operator()(const std::basic_string<Char>& V) const
    {
        return FString(V.c_str());
    }
};

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
