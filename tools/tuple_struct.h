#ifndef EMP_TUPLE_STRUCT_H
#define EMP_TUPLE_STRUCT_H

//////////////////////////////////////////////////////////////////////////////////////////
//
//  These macros will build a tuple and accessors to that tuple's members inside of a
//  class definintion.
//
//  "But WHY???" I hear you ask. Let me explain: Keeping a tuple allows us to easily
//  track the members in the stuct or class, and makes possible powerful types of
//  reflection including identifying all members and performing an action on each
//  (such as serialization).  Since tuples instantiate members directly, these benefits
//  should come at no cost to performance.
//
//  The instantial these tuple member, inside the class definition use:
//
//    EMP_BUILD_TUPLE( int, MyInt,
//                     char, MyChar,
//                     int, MyInt2,
//                     std::string, MyString );
//
//  This will create a std::tuple<int, char, int, std::string> and create accessors.
//  For example the accessors created for MyInt2 would look like:
//
//    const int & MyInt2() const;
//    int & MyInt2();
//    int & MyInt2(int);
//
//  The default tuple will be called "emp__tuple_body".  If you want to use a different
//  name for it (for example if you want more than one tuple in the class), you need to
//  call EMP_BUILD_NAMED_TUPLE, which is identical to EMP_BUILD_TUPLE, but takes the
//  name to be used for the tuple as its first argument.
//

#include <tuple>
#include "macros.h"

// For each entry, we want to build accessors to easily get and set each value.
#define EMP_BUILD_TUPLE_ACCESSORS(TUPLE_NAME, POS, TYPE, NAME)     \
  const TYPE & NAME() const { return std::get<POS>(TUPLE_NAME); }  \
  TYPE & NAME() { return std::get<POS>(TUPLE_NAME); }              \
  TYPE & NAME(const std::decay<TYPE>::type & _in) {                \
    std::get<POS>(TUPLE_NAME) = _in;                               \
    return std::get<POS>(TUPLE_NAME);                              \
  }


// We have to figure out how many arguments we have.
// To get the last arg first (and know its position), reverse all arguments before sending them on.
#define EMP_BUILD_TUPLE_IMPL(TUPLE_NAME, ...) \
  EMP_ASSEMBLE_MACRO(EMP_BUILD_TUPLE_IMPL_, EMP_COUNT_ARGS(__VA_ARGS__), \
                     TUPLE_NAME, EMP_REVERSE_ARGS(__VA_ARGS__) )

#define EMP_BUILD_TUPLE_IMPL_2(TNAME, NAME, TYPE)  \
  EMP_BUILD_TUPLE_ACCESSORS(TNAME, 0, TYPE, NAME)
#define EMP_BUILD_TUPLE_IMPL_4(TNAME, NAME, TYPE, ...)  \
  EMP_BUILD_TUPLE_IMPL_2(TNAME, __VA_ARGS__) EMP_BUILD_TUPLE_ACCESSORS(TNAME, 1, TYPE, NAME)
#define EMP_BUILD_TUPLE_IMPL_6(TNAME, NAME, TYPE, ...)  \
  EMP_BUILD_TUPLE_IMPL_4(TNAME, __VA_ARGS__) EMP_BUILD_TUPLE_ACCESSORS(TNAME, 2, TYPE, NAME)

// Some debug additions!
#define EMP_BUILD_TUPLE_IMPL_0(...) { static_assert(false, "No args in BUILD_TUPLE!"); }
#define EMP_BUILD_TUPLE_IMPL_1(...) { static_assert(false, "Odd number of args in BUILD_TUPLE!"); }

#define EMP_BUILD_NAMED_TUPLE(TUPLE_NAME, ...)              \
  std::tuple< EMP_GET_ODD_ARGS(__VA_ARGS__) > TUPLE_NAME;   \
  EMP_BUILD_TUPLE_IMPL(TUPLE_NAME, __VA_ARGS__)

#define EMP_BUILD_TUPLE(...) EMP_BUILD_NAMED_TUPLE(emp__tuple_body, __VA_ARGS__)

#endif
