#pragma once

#include "UObject/NoExportTypes.h"
//#include "UObject/TemplateString.h"

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
            int N = Utf8FromCodepoint(C, Buf);
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

    // copy from FTCHARToUTF8_Convert because it is deprecated on 5.1
    template <typename BufferType, size_t Len>
    static int32 Utf8FromCodepoint(uint32 Codepoint, BufferType (&Dst)[Len])
    {
        if (!StringConv::IsValidCodepoint(Codepoint))
        {
            Codepoint = UNICODE_BOGUS_CHAR_CODEPOINT;
        }
        else if (StringConv::IsHighSurrogate(Codepoint) || StringConv::IsLowSurrogate(Codepoint)) // UTF-8 Characters are not allowed to encode codepoints in the surrogate pair range
        {
            Codepoint = UNICODE_BOGUS_CHAR_CODEPOINT;
        }

        // Do the encoding...
        using ToType = ANSICHAR;
        BufferType* It = Dst;
        if (Codepoint < 0x80)
        {
            *(It++) = (ToType)Codepoint;
        }
        else if (Codepoint < 0x800)
        {
            *(It++) = (ToType)((Codepoint >> 6) | 128 | 64);
            *(It++) = (ToType)((Codepoint & 0x3F) | 128);
        }
        else if (Codepoint < 0x10000)
        {
            *(It++) = (ToType)((Codepoint >> 12) | 128 | 64 | 32);
            *(It++) = (ToType)(((Codepoint >> 6) & 0x3F) | 128);
            *(It++) = (ToType)((Codepoint & 0x3F) | 128);
        }
        else
        {
            *(It++) = (ToType)((Codepoint >> 18) | 128 | 64 | 32 | 16);
            *(It++) = (ToType)(((Codepoint >> 12) & 0x3F) | 128);
            *(It++) = (ToType)(((Codepoint >> 6) & 0x3F) | 128);
            *(It++) = (ToType)((Codepoint & 0x3F) | 128);
        }

        return UE_PTRDIFF_TO_INT32(It - Dst);
    }
};


template<class T> struct ToJsonKey {};
template<class T> struct ToJsonValue {};
template<class T> struct FromJsonKey {};
template<class T> struct FromJsonValue {};
template<class T> struct NoExportStruct {};

class JObjectBase
{
public:
#pragma region Traits
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

    DEF_VALUE(CanToBool, (std::is_same_v<T, bool>));
    DEF_VALUE(CanToNumber, std::is_arithmetic_v<T>);

    DEF_VALUE_C(CanConstructString, decltype(FString(std::declval<T>())), true);
    DEF_VALUE_C(HasToString, decltype(std::declval<T>().ToString()), true);
    DEF_VALUE_C(HasLexToString, decltype(LexToString(std::declval<T>())), true);
    DEF_VALUE(CanToString, HasToJsonKey<T>::Value || CanConstructString<T>::Value || HasToString<T>::Value || HasLexToString<T>::Value);

    DEF_VALUE_C(HasStaticStruct, decltype(T::StaticStruct()), true);
    DEF_VALUE_C(IsNoExportStruct, decltype(NoExportStruct<T>::StaticStruct()), true);
    DEF_VALUE(IsStruct, HasStaticStruct<T>::Value || IsNoExportStruct<T>::Value);

    DEF_VALUE_C(IsIteratable, decltype(std::begin(std::declval<T&>()) != std::end(std::declval<T&>())), true);
    DEF_VALUE_C(IsContainerCanToObject, typename T::KeyType, IsIteratable<T>::Value && CanToString<typename T::KeyType>::Value);
    DEF_VALUE(CanToObject, IsStruct<T>::Value || IsContainerCanToObject<T>::Value);


    DEF_VALUE_C(HasFromJsonKey, decltype(std::declval<FromJsonKey<T>>()(std::declval<FString>(), std::declval<T&>())), true);
    DEF_VALUE_C(HasFromJsonValue, decltype(std::declval<FromJsonValue<T>>()(std::declval<const TSharedPtr<FJsonValue>>(), std::declval<T&>())), true);

    DEF_VALUE(CanFromBool, (std::is_same_v<T, bool>));
    DEF_VALUE(CanFromNumber, std::is_arithmetic_v<T>);

    DEF_VALUE_C(CanFromFString, decltype(T(std::declval<FString>())), true);
    DEF_VALUE_C(CanFromCString, decltype(T(std::declval<const TCHAR*>())), true);
    DEF_VALUE_C(CanLexFromString, decltype(LexFromString(std::declval<T&>(), std::declval<const TCHAR*>())), true);
    DEF_VALUE(CanFromString, CanFromFString<T>::Value || CanFromCString<T>::Value || CanLexFromString<T>::Value);

    DEF_VALUE_C(IsContainerCanFromObject, typename T::KeyType, CanFromString<typename T::KeyType>::Value);
    DEF_VALUE_C(IsContainerHasAdd, decltype(std::declval<T>().Add(std::declval<typename T::ElementType>())), true);
    DEF_VALUE_C(IsContainerHasIndexer, decltype(std::declval<T>()[std::declval<T>().Num()]=std::declval<typename T::ElementType>()), true);
    DEF_VALUE(CanFromArray, IsContainerHasAdd<T>::Value || IsContainerHasIndexer<T>::Value || std::is_array_v<T>);

#undef DEF_VALUE
#undef DEF_VALUE_C
#pragma endregion Traits

#pragma region ToJson
    template<class T>
    static FString ToString(const T& Key)
    {
        if constexpr (HasToJsonKey<T>::Value) {
            return ToJsonKey<T>()(Key);
        }
        else if constexpr (CanConstructString<T>::Value) {
            return FString(Key);
        }
        else if constexpr (HasToString<T>::Value) {
            return Key.ToString();
        }
        else if constexpr (HasLexToString<T>::Value) {
            return LexToString(Key);
        }
    }

    template<class T>
    static FString ToJKey(const T& Key)
    {
        return ToString(Key);
    }

    template<class T>
    static TSharedPtr<FJsonValue> ToJValue(const T& Value)
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
        else if constexpr (CanToBool<T>::Value) {
            return MakeShared<FJsonValueBoolean>(Value);
        }
        // number
        else if constexpr (CanToNumber<T>::Value) {
            return MakeShared<FJsonValueNumber>((double)Value);
        }
        // string (without conversion)
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
            if constexpr (IsContainerCanToObject<T>::Value) {
                auto Data = MakeShared<FJsonObject>();
                for (auto& KVP : Value) {
                    Data->SetField(ToJKey(KVP.Key), ToJValue(KVP.Value));
                }
                return MakeShared<FJsonValueObject>(Data);
            }
            // others to Json Array
            else {
                TArray<TSharedPtr<FJsonValue>> Data;
                for (auto& E : Value) {
                    Data.Add(ToJValue(E));
                }
                return MakeShared<FJsonValueArray>(Data);
            }
        }
        // all others can convert to string
        else if constexpr (CanToString<T>::Value) {
            return MakeShared<FJsonValueString>(ToString(Value));
        }
    }

    template<class V>
    static TSharedPtr<FJsonValue> ToJValue(std::initializer_list<V>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        for (auto& E : Values) {
            Data.Add(ToJValue(E));
        }
        return MakeShared<FJsonValueArray>(Data);
    }
    template<class... V>
    static TSharedPtr<FJsonValue> ToJValue(TTuple<V...>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        VisitTupleElements([&](auto& Value) { Data.Add(ToJValue(Value)); }, Values);
        return MakeShared<FJsonValueArray>(Data);
    }
    template<class... V>
    static TSharedPtr<FJsonValue> ToJValue(TTuple<V&...>&& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Data;
        VisitTupleElements([&](auto& Value) { Data.Add(ToJValue(Value)); }, Values);
        return MakeShared<FJsonValueArray>(Data);
    }
#pragma endregion ToJson

#pragma region FromJson
    template<class T>
    static bool FromString(const FString& Str, T& Dst)
    {
        if constexpr (HasFromJsonKey<T>::Value) {
            return FromJsonKey()(Str, Dst);
        }
        else if constexpr (CanFromFString<T>::Value) {
            Dst = T(Str);
            return true;
        }
        else if constexpr (CanFromCString<T>::Value) {
            Dst = T(*Str);
            return true;
        }
        else if constexpr (CanLexFromString<T>::Value) {
            LexFromString(Dst, *Str);
            return true;
        }
    }

    template<class T>
    static bool FromJKey(const FString& Key, T& Dst)
    {
        return FromString(Key, Dst);
    }

    template<class T>
    static bool FromJValue(const TSharedPtr<FJsonValue> Value, T& Dst)
    {
        if constexpr (std::is_same_v<T, TSharedPtr<FJsonValue>>) {
            Dst = Value;
            return true;
        }
        else if constexpr (std::is_same_v<T, TSharedPtr<FJsonObject>>) {
            return Value->TryGetObject(Dst);
        }
        // user defined converter
        else if constexpr (HasFromJsonValue<T>::Value) {
            return FromJsonValue<T>()(Value, Dst);
        }
        // bool
        else if constexpr (CanFromBool<T>::Value) {
            return Value->TryGetBool(Dst);
        }
        // numeric
        else if constexpr (CanFromNumber<T>::Value) {
            return Value->TryGetNumber(Dst);
        }
        // struct
        else if constexpr (HasStaticStruct<T>::Value) {
            TSharedPtr<FJsonObject>* Obj;
            if (Value->TryGetObject(Obj)) {
                return FJsonObjectConverter::JsonObjectToUStruct(Obj->ToSharedRef(), &Dst);
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
        else if constexpr (IsContainerCanFromObject<T>::Value) {
            TSharedPtr<FJsonObject>* Obj;
            if (Value->TryGetObject(Obj)) {
                bool Ok = true;
                for (auto& KVP : (*Obj)->Values) {
                    typename T::ElementType Tmp;
                    if (FromJKey(KVP.Key, Tmp.Key) && FromJValue(KVP.Value, Tmp.Value)) {
                        Dst.Add(MoveTemp(Tmp));
                    }
                    else {
                        Ok = false;
                    }
                }
                return Ok;
            }
            return false;
        }
        // array
        else if constexpr (CanFromArray<T>::Value) {
            TArray<TSharedPtr<FJsonValue>>* Array;
            if (Value->TryGetArray(Array)) {
                return FromJArray(*Array, Dst);
            }
            return false;
        }
        // all others can construct from string
        else if constexpr (CanFromString<T>::Value) {
            FString Str;
            if (Value->TryGetString(Str)) {
                return FromString(Str, Dst);
            }
            return false;
        }
    }
    template<class... V>
    static bool FromJValue(const TSharedPtr<FJsonValue> Value, TTuple<V...>& Dsts)
    {
        TArray<TSharedPtr<FJsonValue>>* Array;
        if (Value->TryGetArray(Array)) {
            return FromJArray(*Array, Dsts);
        }
        return false;
    }
    template<class... V>
    static bool FromJValue(const TSharedPtr<FJsonValue> Value, TTuple<V&...>&& Dsts)
    {
        TArray<TSharedPtr<FJsonValue>>* Array;
        if (Value->TryGetArray(Array)) {
            return FromJArray(*Array, MoveTemp(Dsts));
        }
        return false;
    }

    template<class T>
    static bool FromJArray(const TArray<TSharedPtr<FJsonValue>>& Values, T& Dst)
    {
        // container has Add()
        if constexpr (IsContainerHasAdd<T>::Value) {
            bool Ok = true;
            for (auto& E : Values) {
                typename T::ElementType Tmp;
                if (FromJValue(E, Tmp)) {
                    Dst.Add(MoveTemp(Tmp));
                }
                else {
                    Ok = false;
                }
            }
            return Ok;
        }
        // container has Num() and operator[]
        else if constexpr (IsContainerHasIndexer<T>::Value) {
            bool Ok = true;
            int I = 0;
            for (auto& E : Values) {
                if (I < Dst.Num()) {
                    if (!FromJValue(E, Dst[I++])) {
                        Ok = false;
                    }
                }
            }
            return Ok;
        }
        // raw array
        else if constexpr (std::is_array_v<T>) {
            bool Ok = true;
            int I = 0;
            for (auto& E : Values) {
                if (I < std::size(Dst)) {
                    if (!FromJValue(E, Dst[I++])) {
                        Ok = false;
                    }
                }
            }
            return Ok;
        }
    }
    template<class... V>
    static bool FromJArray(const TArray<TSharedPtr<FJsonValue>>& Values, TTuple<V...>& Dsts)
    {
        bool Ok = true;
        int I = 0;
        VisitTupleElements([&](auto& Dst) {
            if (I < Values.Num()) {
                if (!FromJValue(Values[I++], Dst)) {
                    Ok = false;
                }
            }
            }, Dsts);
        return Ok;
    }
    template<class... V>
    static bool FromJArray(const TArray<TSharedPtr<FJsonValue>>& Values, TTuple<V&...>&& Dsts)
    {
        bool Ok = true;
        int I = 0;
        VisitTupleElements([&](auto& Dst) {
            if (I < Values.Num()) {
                if (!FromJValue(Values[I++], Dst)) {
                    Ok = false;
                }
            }
            }, Dsts);
        return Ok;
    }
#pragma endregion FromJson
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

        template<class K, class... V>
        Field(const K& InKey, V&&... Value)
            : Key(ToJKey(InKey)), Value(ToJValue(Forward<V>(Value)...))
        {}
        template<class K, class V>
        Field(const K& InKey, std::initializer_list<V>&& InValues)
            : Key(ToJKey(InKey)), Value(ToJValue(MoveTemp(InValues)))
        {}
    };

    template<class K>
    struct Proxy
    {
        JObject* Host;
        K Key;

        template<class... V>
        void operator=(V&&... Value) const
        {
            Host->Set(Key, Forward<V>(Value)...);
        }
        template<class V>
        void operator=(std::initializer_list<V>&& Values) const
        {
            Host->Set(Key, MoveTemp(Values));
        }

        template<class... V>
        bool operator>>(V&&... Value) const
        {
            return Host->Get(Key, Forward<V>(Value)...);
        }
        template<class T>
        operator T() const
        {
            T Tmp{};
            Host->Get(Key, Tmp);
            return MoveTemp(Tmp);
        }
    };

public:
    JObject() = default;
    JObject(JObject&&) = default;
    JObject(const JObject&) = default;
    JObject& operator=(JObject&&) = default;
    JObject& operator=(const JObject&) = default;


    template<class... T>
    JObject(T&&... Value)
    {
        Set(Forward<V>(Value)...);
    }
    JObject(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
    }

    // Set("field1", 1);
    // Set("field2", MakeTuple(1, "abc", FVector(1, 2, 3)));
    // Set("field3", Tie(Some, Other, Variables));
    template<class K, class... V>
    void Set(const K& Key, V&&... Value)
    {
        Object->SetField(ToJKey(Key), ToJValue(Forward<V>(Value)...));
    }

    // Set("field", {1,2,3});
    template<class K, class V>
    void Set(const K& Key, std::initializer_list<V>&& Value)
    {
        Object->SetField(ToJKey(Key), ToJValue(MoveTemp(Value)));
    }

    // Set({{"field1", 1}, {"field2", "abc"}});
    void Set(std::initializer_list<Field>&& Fields)
    {
        for (auto& F : Fields) {
            Object->SetField(F.Key, F.Value);
        }
    }

    // Set(TMap<FString, int>{{"field1", 0}, {"field2", 1}});
    template<class T, class = std::enable_if_t<CanToObject<T>::Value>>
    void Set(const T& MapOrStruct)
    {
        TSharedPtr<FJsonValue> Val = ToJValue(MapOrStruct);
        TSharedPtr<FJsonObject>* Obj;
        if (Val && Val->TryGetObject(Obj)) {
            Object->Values.Append(MoveTemp((*Obj)->Values));
        }
    }

    template<class... T>
    void operator+=(T&&... Value)
    {
        Set(Forward<T>(Value)...);
    }
    void operator+=(std::initializer_list<Field>&& Fields)
    {
        Set(MoveTemp(Fields));
    }


    template<class K, class... V>
    bool Get(const K& Key, V&&... Dst)
    {
        if (auto Value = Object->Values.Find(ToJKey(Key))) {
            return FromJValue(*Value, Forward<V>(Dst)...);
        }
        return false;
    }


    template<class K> auto operator[](const K& Key) { return Proxy<const K&>{ this, Key }; }
    template<class K> auto operator[](K&& Key) { return Proxy<FString>{ this, ToJKey(Key) }; }

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

    template<class... V>
    JArray(V&&... Value)
    {
        Add(Forward<V>(Value)...);
    }
    template<class V>
    JArray(std::initializer_list<V>&& Values)
    {
        Add(MoveTemp(Values));
    }

    template<class... V>
    void Add(V&&... Values)
    {
        ([&] { Elements.Add(ToJValue(Values)); } (), ...);
    }
    template<class V>
    void Add(std::initializer_list<V>&& Values)
    {
        for (auto& E : Values) {
            Elements.Add(ToJValue(E));
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

    template<class... V>
    void operator+=(V&&... Value)
    {
        Add(Forward<V>(Value)...);
    }
    template<class V>
    void operator+=(std::initializer_list<V>&& Values)
    {
        Add(MoveTemp(Values));
    }


    template<class... V>
    bool Get(V&&... Dst)
    {
        return FromJArray(Elements, Forward<V>(Dst)...);
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
        return JObject::ToJValue(V.c_str());
    }
};

template<class Char>
struct FromJsonKey<std::basic_string<Char>>
{
    bool operator()(const FString& Key, std::basic_string<Char>& Dst) const
    {
        if constexpr (std::is_same_v<Char, char>) {
            Dst = TCHAR_TO_ANSI(*Key);
            return true;
        }
        else if constexpr (std::is_same_v<Char, wchar_t>) {
            Dst = *Key;
            return true;
        }
    }
};

template<class Char>
struct FromJsonValue<std::basic_string<Char>>
{
    bool operator()(const TSharedPtr<FJsonValue> Value, std::basic_string<Char>& Dst) const
    {
        FString Str;
        if (Value->TryGetString(Str)) {
            return FromJsonKey<std::basic_string<Char>>()(Str, Dst);
        }
        return false;
    }
};


