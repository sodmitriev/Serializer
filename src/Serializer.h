// Copyright 2019 Sviatoslav Dmitriev
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#ifndef SERIALIZER_SERIALIZER_H
#define SERIALIZER_SERIALIZER_H


#include <tuple>
#include <vector>
#include <forward_list>
#include <cstring>


//! Byte order of serialized variables
enum ByteOrder
{
    LittleEndian,               ///<Little endian byte order
    BigEndian,                  ///<Big endian byte order
#if BYTE_ORDER == BIG_ENDIAN
    Host = BigEndian,           ///<Host byte order for big endian systems
#else
    Host = LittleEndian,        ///<Host byte order for little endian systems
#endif
    Network = BigEndian
};

namespace serializerTuple
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"

    template<class T, class = void>
    struct is_std_tuple
    {
    private:
        template<class C>
        static constexpr auto test_size(int) -> decltype(std::tuple_size<C>::value, std::true_type());

        template<class>
        static constexpr std::false_type test_size(...);

        template<class C>
        static constexpr auto test_get(int) -> decltype(std::get<0>(std::declval<C>()), std::true_type());

        template<class>
        static constexpr std::false_type test_get(...);

        template<class C>
        static constexpr auto
        test_element(int) -> decltype(std::declval<typename std::tuple_element<0, C>::type>(), std::true_type());

        template<class>
        static constexpr std::false_type test_element(...);

    public:
        static constexpr bool value = decltype(test_size<T>(0))::value && decltype(test_get<T>(0))::value &&
                                      decltype(test_element<T>(0))::value;
    };

#pragma GCC diagnostic pop

    template<class T>
    struct is_std_tuple<T, std::enable_if_t<!std::is_class_v<T>>>
    {
        static constexpr bool value = false;
    };

    template<class T>
    constexpr bool is_std_tuple_v = is_std_tuple<T>::value;

/*!
 * Marks if type is custom serializable
 */
    template<class T, class = void>
    struct is_tuple_serializable : std::false_type
    {
    };

    template<class T>
    struct is_tuple_serializable<T, std::enable_if_t<is_std_tuple_v<T>>> : std::true_type
    {
    };

    template<class T>
    constexpr bool is_tuple_serializable_v = is_tuple_serializable<T>::value;

/*!
 * Get amount of elements in type
 */
    template<class T, class = void>
    struct tuple_size
    {
    };

    template<class T>
    struct tuple_size<T, std::enable_if_t<is_std_tuple_v<T>>>
    {
        static constexpr size_t value = std::tuple_size_v<T>;
    };

    template<class T>
    constexpr size_t tuple_size_v = tuple_size<T>::value;

/*!
 * Get value with index i from tuple serializable value
 */
    template<class T, size_t i, class = void>
    struct tuple_get
    {
    };

    template<class T, size_t i>
    struct tuple_get<T, i, std::enable_if_t<is_std_tuple_v<T>>>
    {
        static auto &func(T &val)
        { return std::get<i>(val); }

        static const auto &func(const T &val)
        { return std::get<i>(val); }
    };

    template<class T, size_t i>
    auto &tuple_get_f(T &val)
    { return tuple_get<T, i>::func(val); }

    template<class T, size_t i>
    const auto &tuple_get_f(const T &val)
    { return tuple_get<T, i>::func(val); }

/*!
 * Get type of value with index i from custom serializable value
 */
    template<class T, size_t i, class = void>
    struct tuple_element
    {
    };

    template<class T, size_t i>
    struct tuple_element<T, i, std::enable_if_t<is_std_tuple_v<T>>>
    {
        typedef std::tuple_element_t<i, T> type;
    };

    template<class T, size_t i>
    using tuple_element_t = typename tuple_element<T, i>::type;

}

namespace Serialization
{
    class DeserializationError: public std::runtime_error
    {
    public:
        explicit DeserializationError(const std::string& msg) : std::runtime_error(msg) {}
    };
}

/*!
 * Variable serializer class
 * @note Cant be used to serialize arithmetic values, c-style arrays and most of standard c++ containers
 * @tparam order Serialization byte order
 * @tparam sizeT Type to represent container size, if void then size type provided by container will be used
 */
template<ByteOrder order = Host, class sizeT = void>
class Serializer
{
public:

    //! Type of values recognized by serializer
    enum ValueType
    {
        Optimized,              ///<Type is specifically optimized to be serialized in an efficient way
        Arithmetic,             ///<Type is arithmetic and will be simply converted to bytes (e.g. int)
        Enum,                   ///<Type is enum and will be simply converted to bytes
        ArithmeticArray,        ///<Type is an array of arithmetic values, it will me copied bytewise (e.g. int[4])
        ArithmeticContiguous,   ///<Type stores arithmetic values contiguously, it's data will me copied bytewise with addition of size (e.g. std::vector<int>)
        Array,                  ///<Type is an array of custom values, each element will be serialized consequently (e.g. std::string[4])
        Tuple,                  ///<Type is a tuple of custom values, each element will be serialized consequently (e.g. std::pair<int, int>)
        Iterable,               ///<Type is an iterable container of custom values, each element will be serialized consequently (e.g. std::vector<std::string>)
        NonSerializable         ///<Type can not be serialized
    };

private:

    template<class T>
    static constexpr bool is_tuple_serializable_v = serializerTuple::is_tuple_serializable_v<T>;

    template<class T>
    static constexpr size_t tuple_size_v = serializerTuple::tuple_size_v<T>;

    template<class T, size_t i>
    static auto &tuple_get_f(T &val)
    { return serializerTuple::tuple_get_f<T, i>(val); }

    template<class T, size_t i>
    static const auto &tuple_get_f(const T &val)
    { return serializerTuple::tuple_get_f<T, i>(val); }

    template<class T, size_t i>
    using tuple_element_t = typename serializerTuple::tuple_element_t<T, i>;

    template<class T>
    using plain_value = std::remove_cv_t<std::remove_reference_t<T>>;

    template<class T>
    static constexpr std::add_lvalue_reference_t<T> ldeclval();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wctor-dtor-privacy"

    template<class T>
    struct is_complete_type
    {
    private:
        template<class C>
        static constexpr std::true_type test(int(*)[sizeof(C)]);

        template<class>
        static constexpr std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template<class T>
    static constexpr bool is_complete_type_v = is_complete_type<T>::value;

    template<class T, class = void>
    struct is_resizable
    {
    private:
        template<class C>
        static constexpr auto
        test(int) -> decltype(ldeclval<C>().resize(ldeclval<decltype(std::size(ldeclval<C>()))>()), std::true_type());

        template<class>
        static constexpr std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template<class T>
    struct is_resizable<T, std::enable_if_t<!std::is_class_v<T>>>
    {
        static constexpr bool value = false;
    };

    template<class T>
    static constexpr bool is_resizable_v = is_resizable<T>::value;

    template<class T, class = void>
    struct has_size
    {
    private:
        template<class C>
        static constexpr auto test(int) -> decltype(std::enable_if_t<std::is_integral_v<decltype(std::size(
                ldeclval<C>()))>>(), std::true_type());

        template<class>
        static constexpr std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template<class T>
    static constexpr bool has_size_v = has_size<T>::value;

    template<class T, class = void>
    struct is_insertable
    {
    private:
        template<class C>
        static constexpr auto
        test(int) -> decltype(ldeclval<C>().insert(ldeclval<C>().end(), *ldeclval<C>().begin()), std::true_type());

        template<class>
        static constexpr std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    template<class T>
    struct is_insertable<T, std::enable_if_t<!std::is_class_v<T>>>
    {
        static constexpr bool value = false;
    };

    template<class T>
    static constexpr bool is_insertable_v = is_insertable<T>::value;

#pragma GCC diagnostic pop

    template<class T, template<class, size_t> class Ref>
    struct is_std_array : std::false_type
    {
    };

    template<template<class, size_t> class Ref, class T, size_t size>
    struct is_std_array<Ref<T, size>, Ref> : std::true_type
    {
    };

    template<class T>
    static constexpr bool is_std_array_v = is_std_array<T, std::array>::value;

    template<class T>
    struct is_forward_list : std::false_type {};

    template<class T>
    struct is_forward_list<std::forward_list<T>> : std::true_type {};

    template<class T>
    static constexpr bool is_forward_list_v = is_forward_list<T>::value;

    template<class T, size_t i = tuple_size_v<T> - 1>
    struct tuple_has_const
    {
        static constexpr bool value = std::is_const_v<tuple_element_t<T, i>> || tuple_has_const<T, i - 1>::value;
    };

    template<class T>
    struct tuple_has_const<T, 0>
    {
        static constexpr bool value = std::is_const_v<tuple_element_t<T, 0>>;
    };

    template<class T>
    static constexpr bool tuple_has_const_v = tuple_has_const<T>::value;

    template<class T, class sizeT1>
    struct size_type
    {
        static_assert(std::is_integral_v<plain_value<T>>, "Container size must be integral");
        static_assert(std::is_integral_v<sizeT1>, "Template size type must be integral or void");
        static_assert(sizeof(sizeT1) >= sizeof(plain_value<T>),
                      "Template size type can't be smaller than container size type");
        typedef sizeT1 type;
    };

    template<class T>
    struct size_type<T, void>
    {
        typedef plain_value<T> type;
    };

    template<class T>
    using size_type_t = typename size_type<T, sizeT>::type;

    template<class T, ByteOrder from, ByteOrder to>
    static constexpr plain_value<T> reorder(plain_value<T> val)
    {
        static_assert(std::is_arithmetic_v<plain_value<T>>);
        if constexpr(from != to && sizeof(plain_value<T>) > 1)
        {
            plain_value<T> ret = 0;
            for (size_t i = 0; i < sizeof(val); ++i)
            {
                ret = plain_value<T>(ret << 8);
                ret |= val & 0xFF;
                val = plain_value<T>(val >> 8);
            }
            return ret;
        }
        else
        {
            return val;
        }
    }

    static constexpr ValueType typePriority[] = {Optimized,
                                                 Arithmetic,
                                                 Enum,
                                                 ArithmeticArray,
                                                 ArithmeticContiguous,
                                                 Array,
                                                 Tuple,
                                                 Iterable,
                                                 NonSerializable};

    template<class T, ValueType tag, class = void>
    struct qualifies : public std::false_type
    {
    };

    template<class T, ValueType tag>
    static constexpr bool qualifies_v = qualifies<T, tag>::value;

    template<class T>
    struct qualifies<T, Optimized,
            std::enable_if_t<is_std_array_v<T> &&
                             (qualifies_v<std::remove_pointer_t<decltype(std::data(ldeclval<T>()))>, Arithmetic> ||
                              qualifies_v<std::remove_pointer_t<decltype(std::data(
                                      ldeclval<T>()))>, ArithmeticArray>) &&
                             order == Host>>
            : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Optimized, std::enable_if_t<is_forward_list_v<T>>> : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Arithmetic, std::enable_if_t<std::is_arithmetic_v<plain_value<T>>>> : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Enum, std::enable_if_t<std::is_enum_v<plain_value<T>>>> : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, ArithmeticArray,
            std::enable_if_t<std::is_array_v<plain_value<T>> &&
                             std::is_integral_v<decltype(std::rank_v<plain_value<T>>)> &&
                             (std::rank_v<plain_value<T>> > 0) &&
                             (std::extent_v<plain_value<T>> > 0) &&
                             (qualifies_v<std::remove_extent_t<plain_value<T>>, Arithmetic> ||
                              qualifies_v<std::remove_extent_t<plain_value<T>>, ArithmeticArray> ||
                              qualifies_v<std::remove_extent_t<plain_value<T>>, Enum>) &&
                             order == Host>>
            : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, ArithmeticContiguous,
            std::enable_if_t<std::is_convertible_v<decltype(std::data(ldeclval<plain_value<T>>())), void *> &&
                             std::is_integral_v<decltype(std::size(ldeclval<plain_value<T>>()))> &&
                             (qualifies_v<std::remove_pointer_t<decltype(std::data(
                                     ldeclval<plain_value<T>>()))>, Arithmetic> ||
                              qualifies_v<std::remove_pointer_t<decltype(std::data(
                                      ldeclval<plain_value<T>>()))>, Enum>) &&
                             is_resizable_v<plain_value<T>> &&
                             order == Host>>
            : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Array,
            std::enable_if_t<std::is_array_v<plain_value<T>> &&
                             std::is_integral_v<decltype(std::rank_v<plain_value<T>>)> &&
                             (std::rank_v<plain_value<T>> > 0) &&
                             (std::extent_v<plain_value<T>> > 0)>>
            : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Tuple, std::enable_if_t<is_tuple_serializable_v<T>>> : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, Iterable,
            std::enable_if_t<std::is_base_of_v<std::input_iterator_tag,
                    typename std::iterator_traits<decltype(std::begin(
                            ldeclval<plain_value<T>>()))>::iterator_category> &&
                             std::is_base_of_v<std::input_iterator_tag,
                                     typename std::iterator_traits<decltype(std::end(
                                             ldeclval<plain_value<T>>()))>::iterator_category> &&
                             is_insertable_v<plain_value<T>> && has_size_v<plain_value<T>> &&
                             !std::is_same_v<std::vector<bool>, T>>>
            : public std::true_type
    {
    };

    template<class T>
    struct qualifies<T, NonSerializable> : public std::true_type
    {
    };

    template<class T, size_t i = 0>
    static constexpr ValueType priority_type()
    {
        if constexpr (i == std::size(typePriority) || qualifies_v<T, typePriority[i]>)
        {
            return typePriority[i];
        }
        else
        {
            return priority_type<T, i + 1>();
        }
    }

    //Calculate size

    template<class T, class = void>
    struct byte_size
    {
        static_assert(priority_type<T>() != NonSerializable, "Value must be serializable");
    };

    template<class T>
    static constexpr size_t byte_size_f(const T &val)
    {
        return byte_size<T>::get(val);
    }

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Optimized && is_std_array_v<T>>>
    {
        static constexpr size_t get(const T &val)
        {
            return val.size() * sizeof(typename T::value_type);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Optimized && is_forward_list_v<T>>>
    {
        static constexpr size_t get(const T &val)
        {
            auto size = sizeof(size_type_t<typename T::size_type>);
            for (auto &i : val)
            {
                size += byte_size_f<decltype(i)>(i);
            }
            return size;
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Arithmetic>>
    {
        static constexpr size_t get(const T &)
        {
            return sizeof(T);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Enum>>
    {
        static constexpr size_t get(const T &)
        {
            return sizeof(T);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == ArithmeticArray>>
    {
        static constexpr size_t get(const T &val)
        {
            return std::extent_v<T> * byte_size_f(val[0]);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == ArithmeticContiguous>>
    {
        static constexpr size_t get(const T &val)
        {
            return sizeof(size_type_t<decltype(std::size(val))>) +
                   std::size(val) * sizeof(std::remove_pointer_t<decltype(std::data(val))>);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Array>>
    {
        static constexpr size_t get(const T &val)
        {
            size_t size = 0;
            for (auto &i : val)
            {
                size += byte_size_f(i);
            }
            return size;
        }
    };

    template<class T, size_t i = 0>
    static constexpr size_t tuple_byte_size(const T &val)
    {
        if constexpr (i + 1 == tuple_size_v<T>)
        {
            return byte_size_f(tuple_get_f<T, i>(val));
        }
        else
        {
            return byte_size_f(tuple_get_f<T, i>(val)) + tuple_byte_size<T, i + 1>(val);
        }
    }

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Tuple>>
    {
        static constexpr size_t get(const T &val)
        {
            return tuple_byte_size(val);
        }
    };

    template<class T>
    struct byte_size<T, std::enable_if_t<priority_type<T>() == Iterable>>
    {
        static constexpr size_t get(const T &val)
        {
            size_t size = sizeof(size_type_t<decltype(std::size(val))>);
            for (auto &i : val)
            {
                size += byte_size_f(i);
            }
            return size;
        }
    };

    //Calculate size via sizeof

    template<class T, class = void>
    struct byte_minsize
    {
        static_assert(priority_type<T>() != NonSerializable, "Value must be serializable");
    };

    template<class T>
    static constexpr size_t byte_minsize_v = byte_minsize<T>::value;

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Optimized && is_std_array_v<T>>>
    {
        static constexpr size_t value = byte_minsize_v<T::value_type>()*tuple_size_v<T>;
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Optimized && is_forward_list_v<T>>>
    {
        static constexpr size_t value = byte_minsize_v<size_type_t<typename T::size_type>>;
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Arithmetic>>
    {
        static constexpr size_t value = sizeof(T);
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Enum>>
    {
        static constexpr size_t value = sizeof(T);
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == ArithmeticArray>>
    {
        static constexpr size_t value = std::extent_v<T> * byte_minsize_v<std::remove_extent_t<T>>;
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == ArithmeticContiguous>>
    {
        static constexpr size_t value = byte_minsize_v<size_type_t<decltype(std::size(std::declval<T>()))>>;
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Array>>
    {
        static constexpr size_t value = std::extent_v<T> * byte_minsize_v<std::remove_extent_t<T>>;
    };

    template<class T, size_t i = 0>
    static constexpr size_t tuple_byte_minsize()
    {
        if constexpr (i + 1 == tuple_size_v<T>)
        {
            return byte_minsize_v<tuple_element_t<T, tuple_size_v<T> - 1>>;
        }
        else
        {
            return byte_minsize_v<tuple_element_t<T, i>> + tuple_byte_minsize<T, i + 1>();
        }
    }

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Tuple>>
    {
        static constexpr size_t value =  tuple_byte_minsize<T>();
    };

    template<class T>
    struct byte_minsize<T, std::enable_if_t<priority_type<T>() == Iterable>>
    {
        static constexpr size_t value = byte_minsize_v<size_type_t<decltype(std::size(std::declval<T>()))>>;
    };

    //Append

    template<class T, class = void>
    struct append
    {
        static_assert(priority_type<T>() != NonSerializable, "Value must be serializable");
    };

    template<class T>
    static constexpr void append_f(char *&ptr, const T &val)
    {
        return append<T>::get(ptr, val);
    }

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Optimized && is_std_array_v<T>>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            static_assert(order == Host);
            auto size = byte_size_f(val);
            memcpy(ptr, std::data(val), size);
            ptr += size;
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Optimized && is_forward_list_v<T>>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            auto size = static_cast<size_type_t<typename T::size_type>>(std::distance(val.begin(), val.end()));
            append_f(ptr, size);
            for (auto &i : val)
            {
                append_f(ptr, i);
            }
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Arithmetic>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            auto ordval = reorder<decltype(val), Host, order>(val);
            memcpy(ptr, &ordval, sizeof(ordval));
            ptr += sizeof(ordval);
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Enum>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            append_f(ptr, *reinterpret_cast<const std::underlying_type_t<plain_value<T>> *>(&val));
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == ArithmeticArray>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            static_assert(order == Host);
            auto size = byte_size_f(val);
            memcpy(ptr, val, size);
            ptr += size;
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == ArithmeticContiguous>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            static_assert(order == Host);
            auto size = static_cast<size_type_t<decltype(std::size(val))>>(std::size(val));
            append_f(ptr, size);
            memcpy(ptr, std::data(val), size * sizeof(std::remove_pointer_t<decltype(std::data(val))>));
            ptr += size * sizeof(std::remove_pointer_t<decltype(std::data(val))>);
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Array>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            for (auto &i : val)
            {
                append_f(ptr, i);
            }
        }
    };

    template<class T, size_t i = 0>
    static void append_tuple(char *&ptr, const T &val)
    {
        append_f(ptr, tuple_get_f<T, i>(val));
        if constexpr (i + 1 < tuple_size_v<T>)
        {
            append_tuple<T, i + 1>(ptr, val);
        }
    }

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Tuple>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            append_tuple(ptr, val);
        }
    };

    template<class T>
    struct append<T, std::enable_if_t<priority_type<T>() == Iterable>>
    {
        static constexpr void get(char *&ptr, const T &val)
        {
            append_f(ptr, static_cast<size_type_t<decltype(std::size(val))>>(std::size(val)));
            for (auto i = std::begin(val); i != std::end(val); ++i)
            {
                append_f(ptr, *i);
            }
        }
    };

    // Take

    static void check_size(size_t &sizeLeft, size_t currentSize)
    {
        if (sizeLeft < currentSize)
        {
            throw Serialization::DeserializationError("Provided serialized data size is too small");
        }
        sizeLeft -= currentSize;
    }

    template<class T, class = void>
    struct take
    {
        static_assert(priority_type<T>() != NonSerializable, "Value must be serializable");
    };

    template<class T>
    static plain_value<T> take_f(const char *&ptr, size_t &restSize)
    {
        static_assert(!take<T>::useReference, "This value can only be assigned by reference");
        return take<T>::get(ptr, restSize);
    }

    template<class T>
    static void take_f(const char *&ptr, size_t &restSize, plain_value<T> &val)
    {
        static_assert(take<T>::useReference, "This value can not be assigned by reference");
        take<T>::get(ptr, restSize, val);
    }

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Optimized && is_std_array_v<T>>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            static_assert(order == Host);
            auto size = byte_size_f(val);
            check_size(restSize, size);
            memcpy(std::data(val), ptr, val.size() * sizeof(typename T::value_type));
            ptr += size;
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Optimized && is_forward_list_v<T>>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            size_type_t<typename plain_value<T>::size_type> size;
            if constexpr(take<typename T::size_type>::useReference)
            {
                take_f<decltype(size)>(ptr, restSize, size);
            }
            else
            {
                size = take_f<decltype(size)>(ptr, restSize);
            }
            if(size*byte_minsize_v<typename plain_value<T>::value_type> > restSize)
            {
                throw Serialization::DeserializationError("Provided serialized data size is too small");
            }
            auto it = val.before_begin();
            for (size_t i = 0; i < size; ++i)
            {
                if constexpr (take<typename plain_value<T>::value_type>::useReference)
                {
                    typename plain_value<T>::value_type el;
                    take_f<typename plain_value<T>::value_type>(ptr, restSize, el);
                    it = val.insert_after(it, std::move(el));
                }
                else
                {
                    it = val.insert_after(it, take_f<typename plain_value<T>::value_type>(ptr, restSize));
                }
            }
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Arithmetic>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            plain_value<decltype(val)> nval;
            check_size(restSize, sizeof(nval));
            memcpy(&nval, ptr, sizeof(nval));
            ptr += sizeof(nval);
            val = reorder<decltype(nval), order, Host>(nval);
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Enum>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            take_f<std::underlying_type_t<plain_value<T>>>(ptr, restSize,
                                                           *reinterpret_cast<std::underlying_type_t<plain_value<T>> *>(&val));
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == ArithmeticArray>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            static_assert(order == Host);
            auto size = byte_size_f(val);
            check_size(restSize, size);
            memcpy(std::data(val), ptr, byte_size_f(val));
            ptr += size;
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == ArithmeticContiguous>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            static_assert(order == Host);
            size_type_t<decltype(std::size(val))> length;
            if constexpr(take<decltype(std::size(val))>::useReference)
            {
                take_f<decltype(length)>(ptr, restSize, length);
            }
            else
            {
                length = take_f<size_type_t<decltype(std::size(val))>>(ptr, restSize);
            }
            auto size = length * sizeof(std::remove_pointer_t<decltype(std::data(val))>);
            check_size(restSize, size);
            val.resize(length);
            memcpy(std::data(val), ptr, std::size(val) * sizeof(std::remove_pointer_t<decltype(std::data(val))>));
            ptr += size;
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Array>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            for (auto &i : val)
            {
                if constexpr (take<decltype(i)>::useReference)
                {
                    take_f<decltype(i)>(ptr, restSize, i);
                }
                else
                {
                    i = take_f<decltype(i)>(ptr, restSize);
                }
            }
        }
    };


    template<class T, size_t i = 0, class ... Args>
    static plain_value<T> take_tuple_construct(const char *&ptr, size_t &restSize, Args &... args)
    {
        if constexpr (take<plain_value<tuple_element_t<T, i>>>::useReference)
        {
            plain_value<tuple_element_t<T, i>> el;
            take_f<plain_value<tuple_element_t<T, i>>>(ptr, restSize, el);
            if constexpr (i + 1 < tuple_size_v<plain_value<T>>)
            {
                return take_tuple_construct<T, i + 1>(ptr, restSize, args..., el);
            }
            else
            {
                return {args..., el};
            }
        }
        else
        {
            auto el = take_f<plain_value<tuple_element_t<T, i>>>(ptr, restSize);
            if constexpr (i + 1 < tuple_size_v<plain_value<T>>)
            {
                return take_tuple_construct<T, i + 1>(ptr, restSize, args..., el);
            }
            else
            {
                return {args..., el};
            }
        }

    }

    template<class T, size_t i = 0>
    static void take_tuple_set(const char *&ptr, size_t &restSize, T &tuple)
    {
        if constexpr (take<plain_value<tuple_element_t<T, i>>>::useReference)
        {
            take_f<plain_value<tuple_element_t<T, i>>>(ptr, restSize, tuple_get_f<T, i>(tuple));
            if constexpr (i + 1 < tuple_size_v<plain_value<T>>)
            {
                take_tuple_set<T, i + 1>(ptr, restSize, tuple);
            }
        }
        else
        {
            tuple_get_f<T, i>(tuple) = take_f<plain_value<tuple_element_t<T, i>>>(ptr, restSize);
            if constexpr (i + 1 < tuple_size_v<plain_value<T>>)
            {
                take_tuple_set<T, i + 1>(ptr, restSize, tuple);
            }
        }

    }

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Tuple>>
    {
        static constexpr bool useReference = !tuple_has_const_v<T>;

        static constexpr plain_value<T> get(const char *&ptr, size_t &restSize)
        {
            return take_tuple_construct<T>(ptr, restSize);
        }

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            return take_tuple_set(ptr, restSize, val);
        }
    };

    template<class T>
    struct take<T, std::enable_if_t<priority_type<T>() == Iterable>>
    {
        static constexpr bool useReference = true;

        static constexpr void get(const char *&ptr, size_t &restSize, plain_value<T> &val)
        {
            size_type_t<typename plain_value<T>::size_type> size;
            if constexpr(take<decltype(size)>::useReference)
            {
                take_f<decltype(size)>(ptr, restSize, size);
            }
            else
            {
                size = take_f<decltype(size)>(ptr, restSize);
            }
            if(size * byte_minsize_v<typename plain_value<T>::value_type> > restSize)
            {
                throw Serialization::DeserializationError("Provided serialized data size is too small");
            }
            if constexpr (take<typename plain_value<T>::value_type>::useReference)
            {
                if constexpr (is_resizable_v<T>)
                {
                    val.resize(size);
                    for (auto &i : val)
                    {
                        take_f<typename plain_value<T>::value_type>(ptr, restSize, i);
                    }
                }
                else
                {
                    for (size_t i = 0; i < size; ++i)
                    {
                        typename plain_value<T>::value_type el;
                        take_f<typename plain_value<T>::value_type>(ptr, restSize, el);
                        val.insert(val.end(), el);
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < size; ++i)
                {
                    val.insert(val.end(), take_f<typename plain_value<T>::value_type>(ptr, restSize));
                }
            }
        }
    };

public:

    /*!
     * Get serializer type of a type
     * @tparam T Type to get type of
     */
    template<class T>
    static constexpr ValueType priorityType = priority_type<T>();

    /*!
     * Serialize value into provided memory chunk
     * @tparam T Serializable value type
     * @param ptr Pointer to provided memory chunk
     * @param val Value to serialize
     * @note Provided memory chunk must be bigger than or equal to result of byteSize(val)
     */
    template<class T>
    static void writeData(char *ptr, const T &val)
    {
        append_f(ptr, val);
    }

    /*!
     * Serialize multiple values into provided memory chunk
     * @tparam T First serializable value type
     * @tparam Args Rest of serializable value types
     * @param ptr Pointer to provided memory chunk
     * @param val First value to serialize
     * @param args Rest of the values to be serialized
     * @note Provided memory chunk must be bigger than or equal to result of byteSize(val, args...)
     */
    template<class T, class ... Args>
    static void writeData(char *ptr, const T &val, const Args &... args)
    {
        append_f(ptr, val);
        writeData(ptr, args...);
    }

    /*!
     * Deserialize value from provided memory chunk
     * @tparam Serializable value type
     * @param ptr Pointer to provided memory chunk
     * @param size Size of provided memory chunk
     * @return Deserialized value
     */
    template<class T>
    static T readData(const char *ptr, size_t size)
    {
        if constexpr (take<T>::useReference)
        {
            plain_value<T> val;
            take_f<T>(ptr, size, val);
            return val;
        }
        else
        {
            return take_f<T>(ptr, size);
        }
    }

    /*!
     * Deserialize value from provided memory chunk
     * @tparam T Serializable value type
     * @param ptr Pointer to provided memory chunk
     * @param size Size of provided memory chunk
     * @param val Deserialized value will be stored here
     */
    template<class T>
    static void readData(const char *ptr, size_t size, T &val)
    {
        if constexpr (take<plain_value<T>>::useReference)
        {
            take_f<T>(ptr, size, val);
        }
        else
        {
            val = take_f<T>(ptr, size);
        }
    }

    /*!
     * Deserialize multiple values from provided memory chunk
     * @tparam T First serializable value type
     * @tparam Args Rest of serializable value types
     * @param ptr Pointer to provided memory chunk
     * @param size Size of provided memory chunk
     * @param val First deserialized value will be stored here
     * @param args Rest of deserialized values will be saved in respective values
     */
    template<class T, class ... Args>
    static void readData(const char *ptr, size_t size, T &val, Args &... args)
    {
        if constexpr (take<plain_value<T>>::useReference)
        {
            take_f<T>(ptr, size, val);
        }
        else
        {
            val = take_f<T>(ptr, size);
        }
        readData(ptr, size, args...);
    }

    /*!
     * Get byte size of a value after serialization
     * @tparam T Serializable value type
     * @param val Value to measure size of
     * @return Expected size of provided value after serialization
     */
    template<class T>
    static constexpr size_t byteSize(const T &val)
    {
        return byte_size_f(val);
    }

    /*!
     * Get a summed byte size of multiple values after serialization
     * @tparam T First serializable value type
     * @tparam Args Rest of serializable value types
     * @param val first value to measure size of
     * @param args Rest of values to measure size of
     * @return
     */
    template<class T, class ... Args>
    static constexpr size_t byteSize(const T &val, const Args &... args)
    {
        return byte_size_f(val) + byteSize(args...);
    }

    /*!
     * Serialize multiple values
     * @tparam Args Serializable values types
     * @param args Serializable values
     * @return Vector with serialized data
     */
    template<class ... Args>
    static std::vector<char> serialize(Args &... args)
    {
        std::vector<char> ret(byteSize(args...));
        writeData(ret.data(), args...);
        return ret;
    }

    /*!
     * Deserialize a single value from provided vector
     * @tparam T Serializable value type
     * @param data Serialized data
     * @return Deserialized value
     */
    template<class T>
    static T deserialize(const std::vector<char> &data)
    {
        return readData<T>(data.data(), data.size());
    }

    /*!
     * Deserialize multiple values
     * @tparam T First serializable value type
     * @tparam Args Rest of serializable value types
     * @param data Serialized data
     * @param val First deserialized value will be stored here
     * @param args Rest of deserialized values will be saved in respective values
     */
    template<class T, class ... Args>
    static void deserialize(const std::vector<char> &data, T &val, Args &... args)
    {
        readData(data.data(), data.size(), val, args...);
    }

};

#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define PP_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0

#define CONCATENATE(arg1, arg2) arg1##arg2

#define CUSTOM_SERIALIZABLE_FRIEND \
template <class T, size_t i, class> friend struct serializerTuple::tuple_get; \
template <class T, size_t i, class> friend struct serializerTuple::tuple_element

#define _SERIALIZER_ADD_METHOD_(pos, typeName, arg) ;\
        template<> struct serializerTuple::tuple_get<typeName, pos> \
        { \
            static_assert(!std::is_const_v<std::remove_reference_t<decltype(typeName::arg)>>, "Serializable parameter of a custom serializable type must not be const"); \
            static auto & func(typeName& val) { return val.arg;} \
            const static auto & func(const typeName& val) { return val.arg;} \
        }; \
        template<> struct serializerTuple::tuple_element<typeName, pos> { typedef decltype(typeName::arg) type; }

#define _SERIALIZER_ADD_METHODS_1(total, typeName, arg) _SERIALIZER_ADD_METHOD_(total - 1, typeName, arg)
#define _SERIALIZER_ADD_METHODS_2(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 2, typeName, arg) _SERIALIZER_ADD_METHODS_1(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_3(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 3, typeName, arg) _SERIALIZER_ADD_METHODS_2(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_4(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 4, typeName, arg) _SERIALIZER_ADD_METHODS_3(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_5(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 5, typeName, arg) _SERIALIZER_ADD_METHODS_4(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_6(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 6, typeName, arg) _SERIALIZER_ADD_METHODS_5(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_7(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 7, typeName, arg) _SERIALIZER_ADD_METHODS_6(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_8(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 8, typeName, arg) _SERIALIZER_ADD_METHODS_7(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_9(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 9, typeName, arg) _SERIALIZER_ADD_METHODS_8(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_10(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 10, typeName, arg) _SERIALIZER_ADD_METHODS_9(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_11(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 11, typeName, arg) _SERIALIZER_ADD_METHODS_10(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_12(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 12, typeName, arg) _SERIALIZER_ADD_METHODS_11(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_13(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 13, typeName, arg) _SERIALIZER_ADD_METHODS_12(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_14(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 14, typeName, arg) _SERIALIZER_ADD_METHODS_13(total, typeName, __VA_ARGS__)
#define _SERIALIZER_ADD_METHODS_15(total, typeName, arg, ...) \
         _SERIALIZER_ADD_METHOD_(total - 15, typeName, arg) _SERIALIZER_ADD_METHODS_14(total, typeName, __VA_ARGS__)

#define _SERIALIZER_ADD_ALL_METHODS_(size, typeName, ...) CONCATENATE(_SERIALIZER_ADD_METHODS_,size)(size, typeName, __VA_ARGS__)

#define CUSTOM_SERIALIZABLE(typeName, ...) \
template <> struct serializerTuple::is_tuple_serializable<typeName> : std::true_type {}; \
template <> struct serializerTuple::tuple_size<typeName> { static constexpr size_t value = PP_NARG(__VA_ARGS__); } \
_SERIALIZER_ADD_ALL_METHODS_(PP_NARG(__VA_ARGS__), typeName, __VA_ARGS__)

#endif //SERIALIZER_SERIALIZER_H
