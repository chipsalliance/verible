# String Libraries

<!--*
freshness: { owner: 'hzeller' reviewed: '2020-10-04' }
*-->

This directory is a home to functions that work with text-as-strings. The
interface to most functions in here are `std::string` and `absl::string_view`
(eventually `std::string_view` once minimum library requirements move beyond
C++11), and `std::istream` and `std::ostream`.

If there's a pure string operation that is not already covered by
[absl's string library](https://github.com/abseil/abseil-cpp/tree/master/absl/strings),
it should go here.
