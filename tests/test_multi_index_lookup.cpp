// Doctest
#include <doctest/doctest.h>

// multi_index_lookup
#include <multi_index_lookup.hpp>

// Boost.MultiIndex
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

// std
#include <algorithm>
#include <concepts>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>


namespace multi_index_lookup
{

namespace
{

struct record_t
{
    int id;
    int group;
    std::string name;
};

struct by_id{};
struct by_group{};

// by_id is unique, by_group is not unique
using raw_container_t = boost::multi_index_container
<
    record_t,
    boost::multi_index::indexed_by
    <
        boost::multi_index::ordered_unique
        <
            boost::multi_index::tag<by_id>,
            boost::multi_index::member<record_t, int, &record_t::id>
        >,
        boost::multi_index::ordered_non_unique
        <
            boost::multi_index::tag<by_group>,
            boost::multi_index::member<record_t, int, &record_t::group>
        >
    >
>;

using container_t = container_extended_t<raw_container_t>;

// id: 1 2 3 4
// group: 10 10 20 30
container_t filled()
{
    return container_t{
        record_t{1, 10, "one"},
        record_t{2, 10, "two"},
        record_t{3, 20, "three"},
        record_t{4, 30, "four"}
    };
}

[[noreturn]] void throw_missing()
{
    throw std::runtime_error("missing");
}

template <typename Range>
std::vector<int> ids_of(const Range& range)
{
    std::vector<int> ids;
    for (const auto& record : range)
    {
        ids.push_back(record.id);
    }
    return ids;
}

}// namespace

TEST_SUITE("multi_index_lookup")
{

TEST_CASE("the container is built with the constructors of the wrapped container")
{
    const container_t empty;
    const container_t from_list{ record_t{7, 70, "seven"} };

    CHECK(empty.empty());
    REQUIRE(from_list.size() == 1);
    CHECK(from_list.try_find_by<by_id>(7)->get().name == "seven");
}

TEST_CASE("modify_result_et of the container is the namespace-level enumeration")
{
    static_assert(std::same_as<container_t::modify_result_et, modify_result_et>);

    CHECK(container_t::modify_result_et::absent == modify_result_et::absent);
}

TEST_CASE("try_find_iterator_by returns an iterator to the record")
{
    const auto container = filled();

    const auto found = container.try_find_iterator_by<by_id>(2);

    REQUIRE(found.has_value());
    CHECK((*found)->name == "two");
}

TEST_CASE("try_find_iterator_by returns empty when the key is absent")
{
    const auto container = filled();

    CHECK_FALSE(container.try_find_iterator_by<by_id>(9).has_value());
}

TEST_CASE("find_iterator_by_or_throw returns the iterator when the key is present")
{
    const auto container = filled();

    const auto it = container.find_iterator_by_or_throw<by_id>(3, throw_missing);

    CHECK(it->name == "three");
}

TEST_CASE("find_iterator_by_or_throw calls on_absent when the key is absent")
{
    const auto container = filled();

    CHECK_THROWS_AS(container.find_iterator_by_or_throw<by_id>(9, throw_missing), std::runtime_error);
}

TEST_CASE("try_find_by returns the record when the key is present")
{
    const auto container = filled();

    const auto found = container.try_find_by<by_id>(2);

    REQUIRE(found.has_value());
    CHECK(found->get().name == "two");
}

TEST_CASE("try_find_by returns empty when the key is absent")
{
    const auto container = filled();

    CHECK_FALSE(container.try_find_by<by_id>(9).has_value());
}

TEST_CASE("find_by_or_throw returns the stored record, not a copy")
{
    const auto container = filled();

    const auto& record = container.find_by_or_throw<by_id>(2, throw_missing);

    CHECK(record.name == "two");
    CHECK(&record == &container.try_find_by<by_id>(2)->get());
}

TEST_CASE("find_by_or_throw calls on_absent when the key is absent")
{
    const auto container = filled();

    CHECK_THROWS_AS(container.find_by_or_throw<by_id>(9, throw_missing), std::runtime_error);
}

TEST_CASE("find_by_and_do calls and_then_func with the index, the iterator and the record")
{
    const auto container = filled();

    const auto result = container.find_by_and_do<by_id>(
        2,
        [](const auto& index, auto it, const record_t& record)
        {
            CHECK(index.size() == 4);
            CHECK(&*it == &record);
            return record.name;
        },
        [](const auto&){ return std::string("absent"); }
    );

    CHECK(result == "two");
}

TEST_CASE("find_by_and_do calls or_else_func with the index when the key is absent")
{
    const auto container = filled();

    const auto result = container.find_by_and_do<by_id>(
        9,
        [](const auto&, auto, const record_t& record){ return record.name; },
        [](const auto& index){ return std::to_string(index.size()); }
    );

    CHECK(result == "4");
}

TEST_CASE("equal_range_by returns every record with the key")
{
    const auto container = filled();

    CHECK(ids_of(container.equal_range_by<by_group>(10)) == std::vector<int>{1, 2});
    CHECK(ids_of(container.equal_range_by<by_id>(2)) == std::vector<int>{2});
}

TEST_CASE("equal_range_by returns an empty range when the key is absent")
{
    const auto container = filled();

    CHECK(container.equal_range_by<by_group>(99).empty());
}

TEST_CASE("equal_range_by returns the type range_by_t")
{
    const auto container = filled();

    static_assert(std::same_as<
        decltype(container.equal_range_by<by_group>(10)),
        container_t::range_by_t<by_group, int>
    >);
    static_assert(std::ranges::forward_range<container_t::range_by_t<by_group, int>>);
}

TEST_CASE("range_between includes the lower and the upper key")
{
    const auto container = filled();

    CHECK(ids_of(container.range_between<by_id>(2, 3)) == std::vector<int>{2, 3});
    CHECK(ids_of(container.range_between<by_group>(10, 20)) == std::vector<int>{1, 2, 3});
}

TEST_CASE("range_between returns an empty range when no key is between")
{
    const auto container = filled();

    CHECK(container.range_between<by_id>(5, 9).empty());
    CHECK(container.range_between<by_id>(3, 2).empty());
}

TEST_CASE("find_range_by_and_do calls and_then_func with the index and the range")
{
    const auto container = filled();

    const auto result = container.find_range_by_and_do<by_group>(
        10,
        [](const auto& index, auto& range)
        {
            CHECK(index.size() == 4);
            return ids_of(range);
        },
        [](const auto&){ return std::vector<int>{}; }
    );

    CHECK(result == std::vector<int>{1, 2});
}

TEST_CASE("find_range_by_and_do calls or_else_func when the range is empty")
{
    const auto container = filled();

    const auto result = container.find_range_by_and_do<by_group>(
        99,
        [](const auto&, auto& range){ return ids_of(range); },
        [](const auto&){ return std::vector<int>{-1}; }
    );

    CHECK(result == std::vector<int>{-1});
}

TEST_CASE("try_find_by_and_modify returns absent when the key is absent")
{
    auto container = filled();
    bool is_called = false;

    const auto result = container.try_find_by_and_modify<by_id>(9, [&](record_t&){ is_called = true; });

    CHECK(result == modify_result_et::absent);
    CHECK_FALSE(is_called);
    CHECK(container.size() == 4);
}

TEST_CASE("try_find_by_and_modify returns modified and changes the record")
{
    auto container = filled();

    const auto result = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.name = "changed"; }
    );

    CHECK(result == modify_result_et::modified);
    CHECK(container.try_find_by<by_id>(1)->get().name == "changed");
}

TEST_CASE("try_find_by_and_modify updates the other indexes when a key changes")
{
    auto container = filled();

    const auto result = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.group = 30; }
    );

    CHECK(result == modify_result_et::modified);
    // the order of records with equal keys of a non-unique index is not specified
    auto moved = ids_of(container.equal_range_by<by_group>(30));
    std::ranges::sort(moved);

    CHECK(ids_of(container.equal_range_by<by_group>(10)) == std::vector<int>{2});
    CHECK(moved == std::vector<int>{1, 4});
}

TEST_CASE("try_find_by_and_modify returns rejected and erases the record when a unique key is taken")
{
    auto container = filled();

    const auto result = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.id = 2; }
    );

    // Boost.MultiIndex has no rollback here: modify erases the record that broke the unique index
    CHECK(result == modify_result_et::rejected);
    CHECK_FALSE(container.try_find_by<by_id>(1).has_value());
    CHECK(container.try_find_by<by_id>(2)->get().name == "two");
    CHECK(container.size() == 3);
}

TEST_CASE("try_find_by_and_modify with rollback returns rejected and keeps the record")
{
    auto container = filled();

    const auto result = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.id = 2; },
        [](record_t& record){ record.id = 1; }
    );

    CHECK(result == modify_result_et::rejected);
    REQUIRE(container.try_find_by<by_id>(1).has_value());
    CHECK(container.try_find_by<by_id>(1)->get().name == "one");
    CHECK(container.try_find_by<by_id>(2)->get().name == "two");
    CHECK(container.size() == 4);
}

TEST_CASE("try_find_by_and_modify with rollback erases the record when the rollback does not restore it")
{
    auto container = filled();

    const auto result = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.id = 2; },
        [](record_t&){}
    );

    CHECK(result == modify_result_et::rejected);
    CHECK_FALSE(container.try_find_by<by_id>(1).has_value());
    CHECK(container.size() == 3);
}

TEST_CASE("try_find_by_and_modify with rollback does not call rollback when modified or absent")
{
    auto container = filled();
    bool is_rolled_back = false;
    const auto rollback = [&](record_t&){ is_rolled_back = true; };

    const auto modified = container.try_find_by_and_modify<by_id>(
        1,
        [](record_t& record){ record.name = "changed"; },
        rollback
    );
    const auto absent = container.try_find_by_and_modify<by_id>(9, [](record_t&){}, rollback);

    CHECK(modified == modify_result_et::modified);
    CHECK(absent == modify_result_et::absent);
    CHECK_FALSE(is_rolled_back);
    CHECK(container.try_find_by<by_id>(1)->get().name == "changed");
}

TEST_CASE("find_by_and_modify with rollback calls on_rejected and keeps the record")
{
    auto container = filled();
    std::vector<std::string> calls;

    container.find_by_and_modify<by_id>(
        1,
        [&]{ calls.push_back("absent"); },
        [&]{ calls.push_back("rejected"); },
        [&](record_t& record){ calls.push_back("modify"); record.id = 2; },
        [&](record_t& record){ calls.push_back("rollback"); record.id = 1; }
    );

    CHECK(calls == std::vector<std::string>{"modify", "rollback", "rejected"});
    CHECK(container.try_find_by<by_id>(1).has_value());
}

TEST_CASE("find_by_and_modify with rollback calls on_absent when the key is absent")
{
    auto container = filled();
    std::vector<std::string> calls;

    container.find_by_and_modify<by_id>(
        9,
        [&]{ calls.push_back("absent"); },
        [&]{ calls.push_back("rejected"); },
        [&](record_t&){ calls.push_back("modify"); },
        [&](record_t&){ calls.push_back("rollback"); }
    );

    CHECK(calls == std::vector<std::string>{"absent"});
}

TEST_CASE("find_by_and_modify calls only on_absent when the key is absent")
{
    auto container = filled();
    std::vector<std::string> calls;

    container.find_by_and_modify<by_id>(
        9,
        [&]{ calls.push_back("absent"); },
        [&]{ calls.push_back("rejected"); },
        [&](record_t&){ calls.push_back("modify"); }
    );

    CHECK(calls == std::vector<std::string>{"absent"});
}

TEST_CASE("find_by_and_modify calls only on_rejected when the modification is rejected")
{
    auto container = filled();
    std::vector<std::string> calls;

    container.find_by_and_modify<by_id>(
        1,
        [&]{ calls.push_back("absent"); },
        [&]{ calls.push_back("rejected"); },
        [&](record_t& record){ calls.push_back("modify"); record.id = 2; }
    );

    CHECK(calls == std::vector<std::string>{"modify", "rejected"});
}

TEST_CASE("find_by_and_modify calls neither failure function when the record is modified")
{
    auto container = filled();
    std::vector<std::string> calls;

    container.find_by_and_modify<by_id>(
        1,
        [&]{ calls.push_back("absent"); },
        [&]{ calls.push_back("rejected"); },
        [&](record_t& record){ calls.push_back("modify"); record.name = "changed"; }
    );

    CHECK(calls == std::vector<std::string>{"modify"});
    CHECK(container.try_find_by<by_id>(1)->get().name == "changed");
}

TEST_CASE("find_by_and_modify propagates the exception of on_absent")
{
    auto container = filled();

    CHECK_THROWS_AS(
        container.find_by_and_modify<by_id>(
            9,
            throw_missing,
            []{ throw std::logic_error("rejected"); },
            [](record_t&){}
        ),
        std::runtime_error
    );
}

TEST_CASE("to_non_const returns the same record without const")
{
    const auto container = filled();
    const record_t& record = container.try_find_by<by_id>(1)->get();

    record_t& non_const = container_t::to_non_const(record);

    CHECK(&non_const == &record);
}

TEST_CASE("to_non_const converts a reference wrapper")
{
    const auto container = filled();
    const auto found = container.try_find_by<by_id>(1);

    const container_t::record_rt wrapper = container_t::to_non_const(*found);

    CHECK(&wrapper.get() == &found->get());
}

TEST_CASE("to_non_const converts an optional reference wrapper")
{
    const auto container = filled();

    const auto present = container_t::to_non_const(container.try_find_by<by_id>(1));
    const auto absent = container_t::to_non_const(container.try_find_by<by_id>(9));

    REQUIRE(present.has_value());
    CHECK(present->get().name == "one");
    CHECK_FALSE(absent.has_value());
}

TEST_CASE("to_non_const allows changing a field that no index uses")
{
    auto container = filled();

    container_t::to_non_const(container.try_find_by<by_id>(1))->get().name = "changed";

    CHECK(container.try_find_by<by_id>(1)->get().name == "changed");
}

}// TEST_SUITE

}// namespace multi_index_lookup
