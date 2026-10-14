#pragma once

// multi_index_lookup
#include <multi_index_lookup.hpp>

// Boost.MultiIndex
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

// std
#include <cstddef>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>


// aggregates by name, including the root one
namespace example::aggregate_container
{

namespace detail
{

struct record_t
{
    std::string name;
    int vd;
    bool is_root;
};

// the key of the index by name: a field extraction
struct by_name
{
    using result_type = const std::string&;
    result_type operator()(const record_t& record) const
    {
        return record.name;
    }
};

struct by_vd{};

using namespace boost::multi_index;

using container_t = multi_index_container
<
    record_t,
    indexed_by
    <
        random_access<>,
        ordered_non_unique
        <
            tag<by_name>,
            by_name
        >,
        ordered_unique
        <
            tag<by_vd>,
            member<record_t, int, &record_t::vd>
        >
    >
>;

}// namespace detail

class container_t : public multi_index_lookup::container_extended_t<detail::container_t>
{
public:
    void add(const std::string& name, const int vd, const bool is_root)
    {
        push_back(detail::record_t{
            .name = name,
            .vd = vd,
            .is_root = is_root
        });
    }

    const detail::record_t& root() const
    {
        for (const auto& record : *this)
        {
            if (record.is_root)
            {
                return record;
            }
        }

        throw std::runtime_error("cannot find root aggregate");
    }

    const detail::record_t& find_by_vd(const int vd) const
    {
        return find_by_or_throw<detail::by_vd>(
            vd,
            [vd]
            {
                throw std::runtime_error("cannot find aggregate " + std::to_string(vd));
            }
        );
    }

    std::string describe(const int vd) const
    {
        return find_by_and_do<detail::by_vd>(
            vd,
            [](const auto&, const auto&, const detail::record_t& record)
            {
                return record.name + (record.is_root ? " (root)" : "");
            },
            [](const auto&)
            {
                return std::string("no such aggregate");
            }
        );
    }

    std::size_t count_by_name(const std::string& name) const
    {
        return find_range_by_and_do<detail::by_name>(
            name,
            [](const auto&, auto& range)
            {
                return static_cast<std::size_t>(std::ranges::distance(range));
            },
            [](const auto&)
            {
                return std::size_t{ 0 };
            }
        );
    }

    template <typename ... Args>
    decltype(auto) try_find_range_by_name(Args&& ... args) const
    {
        return equal_range_by<detail::by_name>(std::forward<Args>(args)...);
    }

    bool rename(const int vd, const std::string& new_name)
    {
        return try_find_by_and_modify<detail::by_vd>(
            vd,
            [&](detail::record_t& record){ record.name = new_name; }
        ) == modify_result_et::modified;
    }

    // the new vd may be taken: then the record stays as it was
    modify_result_et renumber(const int vd, const int new_vd)
    {
        return try_find_by_and_modify<detail::by_vd>(
            vd,
            [new_vd](detail::record_t& record){ record.vd = new_vd; },
            [vd](detail::record_t& record){ record.vd = vd; }
        );
    }
};

}// namespace example::aggregate_container
