#pragma once

// std
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <utility>


namespace multi_index_lookup
{

namespace detail
{

// A function that is called when the record is found:
// and_then(index, iterator, record)
//   index    - the index selected by ByTag
//   iterator - the iterator that points to the found record
//   record   - the found record (const)
//
template
<
    typename Callable,
    typename ByTag,
    typename Container,
    typename Record,
    typename Index = typename Container::template index<ByTag>::type
>
concept can_be_and_then_function = std::invocable
<
    Callable,
    Index&,
    typename Index::iterator,
    const Record&
>;

// A function that is called when the record is not found:
// or_else(index)
//   index - the index selected by ByTag
//
template
<
    typename Callable,
    typename ByTag,
    typename Container,
    typename Index = typename Container::template index<ByTag>::type
>
concept can_be_or_else_function = std::invocable
<
    Callable,
    Index&
>;

// A function that is called when the range is not empty:
// and_then(index, range)
//   index - the index that the range was taken from
//   range - the non-empty range of records
//
template
<
    typename Callable,
    typename Index,
    typename Range
>
concept can_be_range_and_then_function = std::invocable
<
    Callable,
    Index&,
    Range&
>;

// A function that changes the record: modify(record).
// index.modify calls the modifier as void(record&).
//
template <typename Callable, typename Record>
concept can_be_modify_function = std::invocable
<
    Callable,
    Record&
>;

// A function that returns the record to the state it had before the modifier was called:
// rollback(record). index.modify calls it as void(record&) when the modified record
// breaks an index; after it Boost.MultiIndex checks the indices again.
//
template <typename Callable, typename Record>
concept can_be_rollback_function = std::invocable
<
    Callable,
    Record&
>;

// A function without arguments for the failure branch (the record is absent or
// the modification was rejected).
// The find_iterator_by_or_throw and find_by_or_throw functions require it to throw:
// if it returns, std::optional::value throws std::bad_optional_access.
// The find_by_and_modify function does not require it to throw.
//
template <typename Callable>
concept can_be_throw_function = std::invocable
<
    Callable
>;

}// namespace detail


// The result of try_find_by_and_modify:
//   absent   - there is no record with the key, nothing is changed
//   rejected - the modification broke an index (for example, the new key of a unique index
//              is already taken):
//              - without a rollback function Boost.MultiIndex has erased the record
//              - with a rollback function the record is restored; it is erased only if
//                the indices are still broken after the rollback
//   modified - the record is changed
//
enum class modify_result_et { absent, rejected, modified };


// Adds lookup of records by an index tag to a Boost.MultiIndex container.
// The container is used through inheritance, so all its own interface stays available.
// Naming of functions:
//   try_find_*       - return an empty std::optional when nothing is found
//   find_*_or_throw  - take a function that is called when nothing is found; it must throw
//   *_and_do         - take functions for both outcomes: found and not found
//   *_and_modify     - change the found record through the index
//   equal_range_by, range_between - return a range, which is empty when nothing is found
// Type suffixes:
//   _t   - type
//   _et  - enumeration
//   _crt - std::reference_wrapper to const
//   _rt  - std::reference_wrapper
//   _ot  - std::optional
//   _ocrt, _ort - std::optional of the corresponding reference wrapper
//
template <typename MultiIndexContainer>
class container_extended_t : public MultiIndexContainer
{
public:
    // The constructors of the wrapped container (for example, from std::initializer_list of records)
    using MultiIndexContainer::MultiIndexContainer;

    // The wrapped Boost.MultiIndex container
    using base_t = MultiIndexContainer;

    // The record type and the optional references to it
    using record_t = typename MultiIndexContainer::value_type;
    using record_crt = std::reference_wrapper<const record_t>;
    using record_ocrt = std::optional<record_crt>;
    using record_rt = std::reference_wrapper<record_t>;
    using record_ort = std::optional<record_rt>;

    // The index type, its iterator type and the optional iterator by the tag ByTag
    template <typename ByTag>
    using index_by_t = typename MultiIndexContainer::template index<ByTag>::type;
    template <typename ByTag>
    using iterator_by_t = typename index_by_t<ByTag>::iterator;
    template <typename ByTag>
    using iterator_by_ot = std::optional<iterator_by_t<ByTag>>;

    // The type of the range that equal_range_by returns for the key type Value
    template <typename ByTag, typename Value>
    using range_by_t = decltype(std::ranges::subrange(
        std::declval<const index_by_t<ByTag>&>().equal_range(std::declval<Value>()).first,
        std::declval<const index_by_t<ByTag>&>().equal_range(std::declval<Value>()).second
    ));

    // The result of try_find_by_and_modify; it is available in the derived classes without qualification
    using modify_result_et = multi_index_lookup::modify_result_et;

    // Removes const from a record that was found in the container.
    // Changing a record this way is allowed only for fields that no index uses as a key.
    // testing: yes (unit)
    //
    static record_t& to_non_const(const record_t& record)
    {
        return const_cast<record_t&>(record);
    }

    // testing: yes (unit)
    //
    static record_rt to_non_const(const record_crt& crt)
    {
        return to_non_const(crt.get());
    }

    // testing: yes (unit)
    //
    static record_ort to_non_const(const record_ocrt& ocrt)
    {
        return ocrt.transform([](const record_crt& crt){ return to_non_const(crt); });
    }

    // Finds the record by the key and returns an iterator of the index ByTag,
    // or an empty optional if there is no such record.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Value>
    iterator_by_ot<ByTag> try_find_iterator_by(Value&& value) const
    {
        auto& index = index_by<ByTag>();

        if (auto it = index.find(std::forward<Value>(value)); it != index.end())
        {
            return it;
        }
        return {};
    }

    // Finds the record by the key and returns an iterator of the index ByTag.
    // If there is no such record, on_absent is called; it must throw.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Value, detail::can_be_throw_function AbsentFunc>
    iterator_by_t<ByTag> find_iterator_by_or_throw(Value&& value, AbsentFunc&& on_absent) const
    {
        return value_or_throw(
            try_find_iterator_by<ByTag>(std::forward<Value>(value)),
            std::forward<AbsentFunc>(on_absent)
        );
    }

    // Finds the record by the key and returns a reference to it,
    // or an empty optional if there is no such record.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Value>
    record_ocrt try_find_by(Value&& value) const
    {
        return try_find_iterator_by<ByTag>(std::forward<Value>(value))
            .transform([&](const auto& it){ return record_crt(*it); });
    }

    // Finds the record by the key and returns a reference to it.
    // If there is no such record, on_absent is called; it must throw.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Value, detail::can_be_throw_function AbsentFunc>
    const record_t& find_by_or_throw(Value&& value, AbsentFunc&& on_absent) const
    {
        return value_or_throw(
            try_find_by<ByTag>(std::forward<Value>(value)),
            std::forward<AbsentFunc>(on_absent)
        ).get();
    }

    // Finds the record by the key and calls and_then_func(index, iterator, record),
    // or or_else_func(index) if there is no such record.
    // Both functions must return the same type; it is the result of find_by_and_do.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_and_then_function<ByTag, base_t, record_t> AndThenFunc,
        detail::can_be_or_else_function<ByTag, base_t> OrElseFunc
    >
    decltype(auto) find_by_and_do(Value&& value, AndThenFunc&& and_then_func, OrElseFunc&& or_else_func) const
    {
        auto& index = index_by<ByTag>();

        if (auto found = try_find_iterator_by<ByTag>(std::forward<Value>(value)))
        {
            return std::forward<AndThenFunc>(and_then_func)(index, *found, **found);
        }
        return std::forward<OrElseFunc>(or_else_func)(index);
    }

    // Returns the range of all records with the key (equal_range).
    // The range is empty if there are no such records.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Value>
    decltype(auto) equal_range_by(Value&& value) const
    {
        auto& index = index_by<ByTag>();
        const auto found = index.equal_range(std::forward<Value>(value));
        return std::ranges::subrange(found.first, found.second);
    }

    // Returns the range from lower_bound(lower) to upper_bound(upper):
    // the key upper is included in the range.
    // testing: yes (unit)
    //
    template <typename ByTag, typename Lower, typename Upper>
    decltype(auto) range_between(Lower&& lower, Upper&& upper) const
    {
        auto& index = index_by<ByTag>();
        return std::ranges::subrange(
            index.lower_bound(std::forward<Lower>(lower)),
            index.upper_bound(std::forward<Upper>(upper))
        );
    }

    // Finds all records with the key and calls and_then_func(index, range)
    // if the range is not empty, or or_else_func(index) if it is empty.
    // Both functions must return the same type; it is the result of find_range_by_and_do.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_range_and_then_function<
            index_by_t<ByTag>,
            range_by_t<ByTag, Value>
        > AndThenFunc,
        detail::can_be_or_else_function<ByTag, base_t> OrElseFunc
    >
    decltype(auto) find_range_by_and_do(Value&& value, AndThenFunc&& and_then_func, OrElseFunc&& or_else_func) const
    {
        auto& index = index_by<ByTag>();
        auto range = equal_range_by<ByTag>(std::forward<Value>(value));

        if (!range.empty())
        {
            return std::forward<AndThenFunc>(and_then_func)(index, range);
        }
        return std::forward<OrElseFunc>(or_else_func)(index);
    }

    // Finds the record by the key and changes it with modify_func(record) through the index.
    // See modify_result_et for the results; note that the record is erased if the result is rejected,
    // use the overload with the rollback function to keep it.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_modify_function<record_t> ModifyFunc
    >
    modify_result_et try_find_by_and_modify(Value&& value, ModifyFunc&& modify_func)
    {
        auto found = try_find_iterator_by<ByTag>(std::forward<Value>(value));
        if (!found)
        {
            return modify_result_et::absent;
        }

        const bool is_modified = index_by<ByTag>().modify(
            *found,
            std::forward<ModifyFunc>(modify_func)
        );
        return is_modified ? modify_result_et::modified : modify_result_et::rejected;
    }

    // Does the same as try_find_by_and_modify, but if the modification breaks an index,
    // rollback_func(record) returns the record to its previous state and the record stays in the container.
    // The result rejected means that the record is restored.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_modify_function<record_t> ModifyFunc,
        detail::can_be_rollback_function<record_t> RollbackFunc
    >
    modify_result_et try_find_by_and_modify(Value&& value, ModifyFunc&& modify_func, RollbackFunc&& rollback_func)
    {
        auto found = try_find_iterator_by<ByTag>(std::forward<Value>(value));
        if (!found)
        {
            return modify_result_et::absent;
        }

        const bool is_modified = index_by<ByTag>().modify(
            *found,
            std::forward<ModifyFunc>(modify_func),
            std::forward<RollbackFunc>(rollback_func)
        );
        return is_modified ? modify_result_et::modified : modify_result_et::rejected;
    }

    // Does the same as try_find_by_and_modify, but calls on_absent() when there is no such record
    // and on_rejected() when the modification was rejected. Nothing is called when the record is modified.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_throw_function AbsentFunc,
        detail::can_be_throw_function RejectedFunc,
        detail::can_be_modify_function<record_t> ModifyFunc
    >
    void find_by_and_modify(
        Value&& value,
        AbsentFunc&& on_absent,
        RejectedFunc&& on_rejected,
        ModifyFunc&& modify_func
    )
    {
        call_by_modify_result(
            try_find_by_and_modify<ByTag>(std::forward<Value>(value), std::forward<ModifyFunc>(modify_func)),
            std::forward<AbsentFunc>(on_absent),
            std::forward<RejectedFunc>(on_rejected)
        );
    }

    // Does the same as find_by_and_modify, but with the rollback function, see try_find_by_and_modify.
    // testing: yes (unit)
    //
    template
    <
        typename ByTag,
        typename Value,
        detail::can_be_throw_function AbsentFunc,
        detail::can_be_throw_function RejectedFunc,
        detail::can_be_modify_function<record_t> ModifyFunc,
        detail::can_be_rollback_function<record_t> RollbackFunc
    >
    void find_by_and_modify(
        Value&& value,
        AbsentFunc&& on_absent,
        RejectedFunc&& on_rejected,
        ModifyFunc&& modify_func,
        RollbackFunc&& rollback_func
    )
    {
        call_by_modify_result(
            try_find_by_and_modify<ByTag>(
                std::forward<Value>(value),
                std::forward<ModifyFunc>(modify_func),
                std::forward<RollbackFunc>(rollback_func)
            ),
            std::forward<AbsentFunc>(on_absent),
            std::forward<RejectedFunc>(on_rejected)
        );
    }

private:
    // Calls on_absent() or on_rejected() according to the result; nothing is called for modified
    //
    template <typename AbsentFunc, typename RejectedFunc>
    static void call_by_modify_result(const modify_result_et result, AbsentFunc&& on_absent, RejectedFunc&& on_rejected)
    {
        switch (result)
        {
        case modify_result_et::modified:
            return;
        case modify_result_et::absent:
            std::forward<AbsentFunc>(on_absent)();
            return;
        case modify_result_et::rejected:
            std::forward<RejectedFunc>(on_rejected)();
            return;
        }
    }

    // The index of the container by the tag
    //
    template <typename ByTag>
    decltype(auto) index_by() const
    {
        return MultiIndexContainer::template get<ByTag>();
    }

    // The index of the container by the tag
    //
    template <typename ByTag>
    decltype(auto) index_by()
    {
        return MultiIndexContainer::template get<ByTag>();
    }

    // The value of the optional; if it is empty, on_absent is called first
    //
    template <typename Found, typename AbsentFunc>
    static decltype(auto) value_or_throw(Found&& found, AbsentFunc&& on_absent)
    {
        if (!found)
        {
            std::forward<AbsentFunc>(on_absent)();
        }
        return std::forward<Found>(found).value();
    }
};

}// namespace multi_index_lookup
