// Copyright 2019 Sviatoslav Dmitriev
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "Serializer.h"

#include <list>
#include <unordered_set>

enum EnumTestType : uint32_t
{
    Zero,
    One,
    Two,
    Three
};

struct TestStruct
{
private:
    int v1;
    std::string v2;
    std::pair<long, int> v3;
public:
    TestStruct() = default;
    TestStruct(int v1, std::string v2, std::pair<long, int> v3) : v1(v1), v2(v2), v3(v3) {}
    auto& get1() { return v1; }
    auto& get2() { return v2; }
    auto& get3() { return v3; }
    CUSTOM_SERIALIZABLE_FRIEND;
};

CUSTOM_SERIALIZABLE(TestStruct, v1, v2, v3);

TEST_CASE("Serializer test")
{
    SECTION("Simple types")
    {
        SECTION("Arithmetic")
        {
            auto val = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Arithmetic);
            auto data = Serializer<>::serialize(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Enum")
        {
            auto val = Two;
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Enum);
            auto data = Serializer<>::serialize(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Arithmetic array")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            int val[5] = {valarr[0], valarr[1], valarr[2], valarr[3], valarr[4]};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::ArithmeticArray);
            auto data = Serializer<>::serialize<int[5]>(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Arithmetic contiguous")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            auto val = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::ArithmeticContiguous);
            auto data = Serializer<>::serialize(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + val.size() * sizeof(val[0]));
            auto nval = Serializer<>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Tuple")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max()))));
            std::tuple<char, short, int, long> val {valarr[0], valarr[1], valarr[2], valarr[3]};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Tuple);
            auto data = Serializer<>::serialize(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(char) + sizeof(short) + sizeof(int) + sizeof(long));
            auto nval = Serializer<>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Array")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            auto valarr = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            std::vector<int> val[5] {valarr[0], valarr[1], valarr[2], valarr[3], valarr[4]};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Array);
            auto data = Serializer<>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(std::vector<int>::size_type)*5 + sizeof(int)*(val[0].size() + val[1].size() +
                                                                                        val[2].size() + val[3].size() +
                                                                                        val[4].size()));
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Iterable")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            auto valarr = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            std::vector<std::vector<int>> val {valarr[0], valarr[1], valarr[2], valarr[3], valarr[4]};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Iterable);
            auto data = Serializer<>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + sizeof(std::vector<int>::size_type)*5 +
                                   sizeof(int)*(val[0].size() + val[1].size() +
                                                val[2].size() + val[3].size() +
                                                val[4].size()));
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Non serializable")
        {
            REQUIRE(Serializer<>::priorityType<char*> == Serializer<>::NonSerializable);
        }
    }
    SECTION("Standard containers of basic types")
    {
        SECTION("array")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::array<int, 5> val {valarr[0], valarr[1], valarr[2], valarr[3], valarr[4]};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Optimized);
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("vector")
        {
            std::vector<int> val = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("dequeue")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::deque<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("forward list")
        {
            REQUIRE(Serializer<>::priorityType<std::forward_list<int>> == Serializer<>::Optimized);
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::forward_list<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("list")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::list<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("set")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::set<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("map")
        {
            auto valarr1 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            auto valarr2 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::map<int, int> val { {valarr1[0], valarr2[0]},
                                     {valarr1[1], valarr2[1]},
                                     {valarr1[2], valarr2[2]},
                                     {valarr1[3], valarr2[3]},
                                     {valarr1[4], valarr2[4]}};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("multiset")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::multiset<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("multimap")
        {
            auto valarr1 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            auto valarr2 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::multimap<int, int> val { {valarr1[0], valarr2[0]},
                                     {valarr1[1], valarr2[1]},
                                     {valarr1[2], valarr2[2]},
                                     {valarr1[3], valarr2[3]},
                                     {valarr1[4], valarr2[4]}};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(std::equal(val.begin(), val.end(), nval.begin(), nval.end()));
        }
        SECTION("unordered set")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::unordered_set<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("unordered map")
        {
            auto valarr1 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            auto valarr2 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::unordered_map<int, int> val { {valarr1[0], valarr2[0]},
                                          {valarr1[1], valarr2[1]},
                                          {valarr1[2], valarr2[2]},
                                          {valarr1[3], valarr2[3]},
                                          {valarr1[4], valarr2[4]}};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("unordered multiset")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::unordered_multiset<int> val {valarr.begin(), valarr.end()};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("unordered multimap")
        {
            auto valarr1 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            auto valarr2 = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::unordered_multimap<int, int> val { {valarr1[0], valarr2[0]},
                                               {valarr1[1], valarr2[1]},
                                               {valarr1[2], valarr2[2]},
                                               {valarr1[3], valarr2[3]},
                                               {valarr1[4], valarr2[4]}};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("initializer list")
        {
            REQUIRE(Serializer<>::priorityType<std::initializer_list<int>> == Serializer<>::NonSerializable);
        }
        SECTION("bitset")
        {
            REQUIRE(Serializer<>::priorityType<std::bitset<5>> == Serializer<>::NonSerializable);
        }
        SECTION("pair")
        {
            auto valarr = GENERATE(take(1, chunk(2, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::pair<int, int> val {valarr[0], valarr[1]};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("tuple")
        {
            auto valarr = GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::tuple<int, int, int, int, int> val {valarr[0], valarr[1], valarr[2], valarr[3], valarr[4]};
            decltype(val) nval;
            Serializer<>::deserialize<decltype(val)>(Serializer<>::serialize(val), nval);
            REQUIRE(val == nval);
        }
        SECTION("vector<bool>")
        {
            REQUIRE(Serializer<>::priorityType<std::vector<bool>> == Serializer<>::NonSerializable);
        }
        SECTION("bitset")
        {
            REQUIRE(Serializer<>::priorityType<std::bitset<5>> == Serializer<>::NonSerializable);
        }
    }
    SECTION("Nested type")
    {
        std::tuple<int[2], std::string, std::vector<std::pair<std::vector<int>, int>>, std::array<std::map<int, std::string>, 3>>
                val;
        std::get<0>(val)[0] = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
        std::get<0>(val)[1] = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
        std::get<1>(val) = "testvalue";
        auto size = GENERATE(take(1, random(1, 1024)));
        std::get<2>(val) =
                {
                        {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))},
                        {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))},
                        {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))},
                        {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))},
                        {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))}
                };
        std::get<3>(val)[0] =
                {
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test1"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test2"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test3"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test4"}
                };
        std::get<3>(val)[1] =
                {
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test1"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test2"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test3"}
                };
        std::get<3>(val)[2] =
                {
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test1"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test2"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test3"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test4"},
                        {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))), "test5"}
                };
        decltype(val) nval;
        auto data = Serializer<>::serialize(val);
        REQUIRE(data.size() == Serializer<>::byteSize(val));
        Serializer<>::deserialize(data, nval);
        REQUIRE(std::equal(std::begin(std::get<0>(val)), std::end(std::get<0>(val)),
                           std::begin(std::get<0>(nval)), std::end(std::get<0>(nval))));
        REQUIRE(std::get<1>(val) == std::get<1>(nval));
        REQUIRE(std::get<2>(val) == std::get<2>(nval));
        REQUIRE(std::get<3>(val) == std::get<3>(nval));
    }
    SECTION("Multiple values")
    {
        auto val0 = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
        int val1[5] = {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                       GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                       GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                       GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                       GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))};
        auto size = GENERATE(take(1, random(1, 1024)));
        auto val2 = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
        std::tuple<char, short, int, long> val3 {GENERATE(take(1, random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()))),
                                                 GENERATE(take(1, random(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()))),
                                                 GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                                                 GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max())))};
        std::vector<int> val4[5] {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                  GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                  GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                  GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                  GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
        std::vector<std::vector<int>> val5 {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                            GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                            GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                            GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                            GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
        auto data = Serializer<>::serialize(val0, val1, val2, val3, val4, val5);
        REQUIRE(data.size() == Serializer<>::byteSize(val0) + Serializer<>::byteSize(val1) +
                               Serializer<>::byteSize(val2) + Serializer<>::byteSize(val3) +
                               Serializer<>::byteSize(val4) + Serializer<>::byteSize(val5));
        decltype(val0) nval0;
        decltype(val1) nval1;
        decltype(val2) nval2;
        decltype(val3) nval3;
        decltype(val4) nval4;
        decltype(val5) nval5;
        Serializer<>::deserialize(data, nval0, nval1, nval2, nval3, nval4, nval5);
        REQUIRE(val0 == nval0);
        REQUIRE(std::equal(std::begin(val1), std::end(val1), std::begin(nval1), std::end(nval1)));
        REQUIRE(val2 == nval2);
        REQUIRE(val3 == nval3);
        REQUIRE(std::equal(std::begin(val4), std::end(val4), std::begin(nval4), std::end(nval4)));
        REQUIRE(val5 == nval5);
    }
    SECTION("Change order")
    {
        constexpr ByteOrder order = Host == BigEndian ? LittleEndian : BigEndian;
        SECTION("Arithmetic")
        {
            auto val = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Arithmetic);
            auto data = Serializer<order>::serialize(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<order>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Enum")
        {
            auto val = Two;
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Enum);
            auto data = Serializer<order>::serialize(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<order>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Arithmetic array (Array due to reorder)")
        {
            int val[5] = {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))};
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Array);
            auto data = Serializer<order>::serialize<int[5]>(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            decltype(val) nval;
            Serializer<order>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Arithmetic contiguous (Iterable due to reorder)")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            auto val = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Iterable);
            auto data = Serializer<order>::serialize(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + val.size() * sizeof(val[0]));
            auto nval = Serializer<order>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Tuple")
        {
            std::tuple<char, short, int, long> val {GENERATE(take(1, random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max())))};
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Tuple);
            auto data = Serializer<order>::serialize(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(char) + sizeof(short) + sizeof(int) + sizeof(long));
            auto nval = Serializer<order>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Array")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            std::vector<int> val[5] {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Array);
            auto data = Serializer<order>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(std::vector<int>::size_type)*5 + sizeof(int)*(val[0].size() + val[1].size() +
                                                                                        val[2].size() + val[3].size() +
                                                                                        val[4].size()));
            decltype(val) nval;
            Serializer<order>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Iterable")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            std::vector<std::vector<int>> val = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            REQUIRE(Serializer<order>::priorityType<decltype(val)> == Serializer<order>::Iterable);
            auto data = Serializer<order>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<order>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + sizeof(std::vector<int>::size_type)*5 +
                                   sizeof(int)*(val[0].size() + val[1].size() +
                                                val[2].size() + val[3].size() +
                                                val[4].size()));
            decltype(val) nval;
            Serializer<order>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Non serializable")
        {
            REQUIRE(Serializer<order>::priorityType<char*> == Serializer<order>::NonSerializable);
        }
    }
    SECTION("Custom size type")
    {
        SECTION("Arithmetic")
        {
            auto val = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Arithmetic);
            auto data = Serializer<Network, uint64_t>::serialize(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<Network, uint64_t>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Enum")
        {
            auto val = Two;
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Enum);
            auto data = Serializer<Network, uint64_t>::serialize(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            auto nval = Serializer<Network, uint64_t>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Arithmetic array (Array due to reorder)")
        {
            int val[5] = {GENERATE_COPY(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE_COPY(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE_COPY(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE_COPY(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                          GENERATE_COPY(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))};
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Array);
            auto data = Serializer<Network, uint64_t>::serialize<int[5]>(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(val));
            decltype(val) nval;
            Serializer<Network, uint64_t>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Arithmetic contiguous (Iterable due to reorder)")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            std::vector<int> val = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Iterable);
            auto data = Serializer<Network, uint64_t>::serialize(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + val.size() * sizeof(val[0]));
            auto nval = Serializer<Network, uint64_t>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Tuple")
        {
            std::tuple<char, short, int, long> val {GENERATE(take(1, random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                                                    GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max())))};
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Tuple);
            auto data = Serializer<Network, uint64_t>::serialize(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(char) + sizeof(short) + sizeof(int) + sizeof(long));
            auto nval = Serializer<Network, uint64_t>::deserialize<decltype(val)>(data);
            REQUIRE(val == nval);
        }
        SECTION("Array")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            std::vector<int> val[5] {GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                     GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Array);
            auto data = Serializer<Network, uint64_t>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(std::vector<int>::size_type)*5 + sizeof(int)*(val[0].size() + val[1].size() +
                                                                                        val[2].size() + val[3].size() +
                                                                                        val[4].size()));
            decltype(val) nval;
            Serializer<Network, uint64_t>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Iterable")
        {
            auto size = GENERATE(take(1, random(1, 1024)));
            std::vector<std::vector<int>> val = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            REQUIRE(Serializer<Network, uint64_t>::priorityType<decltype(val)> == Serializer<Network, uint64_t>::Iterable);
            auto data = Serializer<Network, uint64_t>::serialize<decltype(val)>(val);
            REQUIRE(data.size() == Serializer<Network, uint64_t>::byteSize(val));
            REQUIRE(data.size() == sizeof(val.size()) + sizeof(std::vector<int>::size_type)*5 +
                                   sizeof(int)*(val[0].size() + val[1].size() +
                                                val[2].size() + val[3].size() +
                                                val[4].size()));
            decltype(val) nval;
            Serializer<Network, uint64_t>::deserialize<decltype(val)>(data, nval);
            REQUIRE(std::equal(std::begin(val), std::end(val), std::begin(nval), std::end(nval)));
        }
        SECTION("Non serializable")
        {
            REQUIRE(Serializer<Network, uint64_t>::priorityType<char*> == Serializer<Network, uint64_t>::NonSerializable);
        }
    }
    SECTION("Wrong data size")
    {
        SECTION("Smaller")
        {
            auto val0 = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
            int val1[5] = {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))};
            auto size = GENERATE(take(1, random(1, 1024)));
            auto val2 = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::tuple<char, short, int, long> val3 {GENERATE(take(1, random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max())))};
            std::vector<int> val4[5] {GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
            std::vector<std::vector<int>> val5 = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            auto data = Serializer<>::serialize(val0, val1, val2, val3, val4, val5);
            REQUIRE(data.size() == Serializer<>::byteSize(val0) + Serializer<>::byteSize(val1) +
                                   Serializer<>::byteSize(val2) + Serializer<>::byteSize(val3) +
                                   Serializer<>::byteSize(val4) + Serializer<>::byteSize(val5));
            decltype(val0) nval0;
            decltype(val1) nval1;
            decltype(val2) nval2;
            decltype(val3) nval3;
            decltype(val4) nval4;
            decltype(val5) nval5;
            data.resize(data.size() - GENERATE_REF(take(1, random(size_t(1), data.size() - 2))));
            REQUIRE_THROWS_AS(Serializer<>::deserialize(data, nval0, nval1, nval2, nval3, nval4, nval5), Serialization::DeserializationError);
        }
        SECTION("Larger")
        {
            auto val0 = GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
            int val1[5] = {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                           GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))};
            auto size = GENERATE(take(1, random(1, 1024)));
            auto val2 = GENERATE_COPY(take(1, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))));
            std::tuple<char, short, int, long> val3 {GENERATE(take(1, random(std::numeric_limits<char>::min(), std::numeric_limits<char>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<short>::min(), std::numeric_limits<short>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                                                     GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max())))};
            std::vector<int> val4[5] {GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))),
                                      GENERATE(take(1, chunk(5, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))))};
            std::vector<std::vector<int>> val5 = GENERATE_COPY(take(1, chunk(5, chunk(size, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))));
            auto data = Serializer<>::serialize(val0, val1, val2, val3, val4, val5);
            REQUIRE(data.size() == Serializer<>::byteSize(val0) + Serializer<>::byteSize(val1) +
                                   Serializer<>::byteSize(val2) + Serializer<>::byteSize(val3) +
                                   Serializer<>::byteSize(val4) + Serializer<>::byteSize(val5));
            decltype(val0) nval0;
            decltype(val1) nval1;
            decltype(val2) nval2;
            decltype(val3) nval3;
            decltype(val4) nval4;
            decltype(val5) nval5;
            data.resize(data.size() + GENERATE_REF(take(1, random(size_t(1), data.size() - 2))));
            Serializer<>::deserialize(data, nval0, nval1, nval2, nval3, nval4, nval5);
            REQUIRE(val0 == nval0);
            REQUIRE(std::equal(std::begin(val1), std::end(val1), std::begin(nval1), std::end(nval1)));
            REQUIRE(val2 == nval2);
            REQUIRE(val3 == nval3);
            REQUIRE(std::equal(std::begin(val4), std::end(val4), std::begin(nval4), std::end(nval4)));
            REQUIRE(val5 == nval5);
        }
    }
    SECTION("Custom type")
    {
        SECTION("Single")
        {
            REQUIRE(Serializer<>::priorityType<TestStruct> == Serializer<>::Tuple);
            TestStruct val {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                            "123",
                            {GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max()))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))}};
            REQUIRE(Serializer<>::priorityType<decltype(val)> == Serializer<>::Tuple);
            auto data = Serializer<>::serialize(val);
            REQUIRE(data.size() == Serializer<>::byteSize(val));
            REQUIRE(data.size() == sizeof(int) + sizeof(std::string::size_type) + 3 + sizeof(long) + sizeof(int));
            auto nval = Serializer<>::deserialize<decltype(val)>(data);
            REQUIRE(val.get1() == nval.get1());
            REQUIRE(val.get2() == nval.get2());
            REQUIRE(val.get3() == nval.get3());
        }
        SECTION("Nested")
        {
            std::vector<TestStruct> vec;
            TestStruct val {GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))),
                            "123",
                            {GENERATE(take(1, random(std::numeric_limits<long>::min(), std::numeric_limits<long>::max()))), GENERATE(take(1, random(std::numeric_limits<int>::min(), std::numeric_limits<int>::max())))}};
            vec.push_back(val);
            REQUIRE(Serializer<>::priorityType<decltype(vec)> == Serializer<>::Iterable);
            auto data = Serializer<>::serialize(vec);
            REQUIRE(data.size() == Serializer<>::byteSize(vec));
            decltype(vec) nvec;
            Serializer<>::deserialize(data, nvec);
            REQUIRE(nvec.size() == 1);
            auto & nval = nvec[0];
            REQUIRE(val.get1() == nval.get1());
            REQUIRE(val.get2() == nval.get2());
            REQUIRE(val.get3() == nval.get3());
        }
    }
}
