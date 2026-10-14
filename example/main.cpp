// example
#include "aggregate_container.hpp"

// std
#include <iostream>
#include <string>
#include <vector>


namespace
{

int failures = 0;

void check(const bool is_ok, const std::string& what)
{
    std::cout << (is_ok ? "ok      " : "FAILED  ") << what << "\n";
    if (!is_ok)
    {
        ++failures;
    }
}

}// namespace

int main()
{
    using example::aggregate_container::container_t;
    using example::aggregate_container::detail::by_vd;

    container_t container;
    container.add("root", 0, true);
    container.add("user", 1, false);
    container.add("user", 2, false);
    container.add("order", 3, false);

    // lookup
    check(container.root().vd == 0, "root() finds the root aggregate");
    check(container.try_find_by<by_vd>(3)->get().name == "order", "try_find_by finds a record");
    check(!container.try_find_by<by_vd>(9).has_value(), "try_find_by returns empty for a missing key");
    check(container.describe(0) == "root (root)", "find_by_and_do calls and_then_func");
    check(container.describe(9) == "no such aggregate", "find_by_and_do calls or_else_func");

    bool is_thrown = false;
    try
    {
        container.find_by_vd(9);
    }
    catch (const std::runtime_error&)
    {
        is_thrown = true;
    }
    check(is_thrown, "find_by_or_throw calls the throwing function");

    // ranges
    std::vector<int> users;
    for (const auto& record : container.try_find_range_by_name("user"))
    {
        users.push_back(record.vd);
    }
    check(users == std::vector<int>{ 1, 2 }, "equal_range_by returns every record with the name");
    check(container.count_by_name("user") == 2, "find_range_by_and_do calls and_then_func");
    check(container.count_by_name("nobody") == 0, "find_range_by_and_do calls or_else_func");

    // modify
    check(container.rename(3, "orders"), "try_find_by_and_modify changes the record");
    check(container.describe(3) == "orders", "the changed record is found");
    check(!container.rename(9, "x"), "try_find_by_and_modify reports a missing key");

    using result_et = container_t::modify_result_et;
    check(container.renumber(3, 4) == result_et::modified, "renumber to a free vd");
    check(container.renumber(4, 1) == result_et::rejected, "renumber to a taken vd is rejected");
    check(container.try_find_by<by_vd>(4).has_value(), "the rejected record is restored by the rollback function");
    check(container.size() == 4, "no record is lost");

    return failures == 0 ? 0 : 1;
}
