/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2016-2017
 *
 *  @file Ptr.h
 *  @brief A wrapper for pointers that does careful memory tracking (but only in debug mode).
 *  @note Status: BETA
 *
 *  Ptr objects behave as normal pointers under most conditions.  However, if a program is
 *  compiled with EMP_TRACK_MEM set, then these pointers perform extra tests to ensure that
 *  they point to valid memory and that memory is freed before pointers are released.
 *
 *  If you trip an assert, you can re-do the run a track a specific pointer by defining
 *  EMP_ABORT_PTR_NEW or EMP_ABORT_PTR_DELETE to the ID of the pointer in question.  This will
 *  allow you to track the pointer more easily in a debugger.
 *
 *  @todo Track information about emp::vector and emp::array objects to make sure we don't
 *    point directly into them? (A resize() could make such pointers invalid!)
 */

#ifndef EMP_PTR_H
#define EMP_PTR_H

#include <unordered_map>

#include "assert.h"
#include "vector.h"

namespace {
  /// An anonymous log2 calculator for hashing below.
  static constexpr size_t Log2(size_t x) { return x <= 1 ? 0 : (Log2(x/2) + 1); }
}

namespace emp {

  namespace {
    static bool ptr_debug = false;
  }
  void SetPtrDebug(bool _d = true) { ptr_debug = _d; }
  bool GetPtrDebug() { return ptr_debug; }

  enum class PtrStatus { DELETED=0, ACTIVE, ARRAY };

  class PtrInfo {
  private:
    const void * ptr;   ///< Which pointer are we keeping data on?
    int count;          ///< How many of this pointer do we have?
    PtrStatus status;   ///< Has this pointer been deleted? (i.e., we should no longer access it!)
    size_t array_bytes; ///< How big is the array pointed to (in bytes)?

  public:
    PtrInfo(const void * _ptr) : ptr(_ptr), count(1), status(PtrStatus::ACTIVE), array_bytes(0) {
      if (ptr_debug) std::cout << "Created info for pointer: " << ptr << std::endl;
    }
    PtrInfo(const void * _ptr, size_t _array_bytes)
      : ptr(_ptr), count(1), status(PtrStatus::ARRAY), array_bytes(_array_bytes)
    {
      emp_assert(_array_bytes >= 1);
      if (ptr_debug) {
        std::cout << "Created info for array pointer (bytes=" << array_bytes << "): "
                  << ptr << std::endl;
      }
    }
    PtrInfo(const PtrInfo &) = default;
    PtrInfo(PtrInfo &&) = default;
    PtrInfo & operator=(const PtrInfo &) = default;
    PtrInfo & operator=(PtrInfo &&) = default;

    ~PtrInfo() {
      if (ptr_debug) std::cout << "Deleted info for pointer " << ptr << std::endl;
    }

    /// What pointer does this one hold information about?
    const void * GetPtr() const { return ptr; }

    /// How many Ptr objects point to the associated position?
    int GetCount() const { return count; }

    /// If this ptr is to an array, how many bytes large is the array (may be different from size!)
    size_t GetArrayBytes() const { return array_bytes; }

    /// Is this pointer currently valid to access?
    bool IsActive() const { return (bool) status; }

    /// Is this pointer pointing to an array?
    bool IsArray()  const { return status == PtrStatus::ARRAY; }

    /// Denote that this pointer is an array.
    void SetArray(size_t bytes) { array_bytes = bytes; status = PtrStatus::ARRAY; }

    /// Add one more pointer.
    void Inc() {
      if (ptr_debug) std::cout << "Inc info for pointer " << ptr << std::endl;
      emp_assert(status != PtrStatus::DELETED, "Incrementing deleted pointer!");
      count++;
    }

    /// Remove a pointer.
    void Dec() {
      if (ptr_debug) std::cout << "Dec info for pointer " << ptr << std::endl;

      // Make sure that we have more than one copy, -or- we've already deleted this pointer
      emp_assert(count > 1 || status == PtrStatus::DELETED, "Removing last reference to owned Ptr!");
      count--;
    }

    /// Indicate that the associated position has been deleted.
    void MarkDeleted() {
      if (ptr_debug) std::cout << "Marked deleted for pointer " << ptr << std::endl;
      emp_assert(status != PtrStatus::DELETED, "Deleting same emp::Ptr a second time!");
      status = PtrStatus::DELETED;
    }

  };


  /// Facilitate tracking of all Ptr objects in this run.
  class PtrTracker {
  private:
    std::unordered_map<const void *, size_t> ptr_id;  ///< Associate raw pointers with unique IDs
    emp::vector<PtrInfo> id_info;                     ///< Associate IDs with pointer information.

    // Make PtrTracker a singleton.
    PtrTracker() : ptr_id(), id_info() { ; }
    PtrTracker(const PtrTracker &) = delete;
    PtrTracker(PtrTracker &&) = delete;
    PtrTracker & operator=(const PtrTracker &) = delete;
    PtrTracker & operator=(PtrTracker &&) = delete;

    PtrInfo & GetInfo(const void * ptr) { return id_info[ptr_id[ptr]]; }
  public:
    ~PtrTracker() {
      // Track stats about pointer record.
      size_t total = 0;
      size_t remain = 0;

      // Scan through live pointers and make sure all have been deleted.
      for (const auto & info : id_info) {
        total++;
        if (info.GetCount()) remain++;

        emp_assert(info.IsActive() == false, info.GetPtr(), info.GetCount(), info.IsActive());
      }

      std::cout << "EMP_TRACK_MEM: No memory leaks found!\n "
                << total << " pointers found; "
                << remain << " still exist with a non-null value (but have been properly deleted)"
                << std::endl;
    }

    /// Treat this class as a singleton with a single Get() method to retrieve it.
    static PtrTracker & Get() { static PtrTracker tracker; return tracker; }

    /// Determine if a pointer is being tracked.
    bool HasPtr(const void * ptr) const {
      if (ptr_debug) std::cout << "HasPtr: " << ptr << std::endl;
      return ptr_id.find(ptr) != ptr_id.end();
    }

    /// Retrive the ID associated with a pointer.
    size_t GetCurID(const void * ptr) { emp_assert(HasPtr(ptr)); return ptr_id[ptr]; }

    /// Lookup how many pointers are being tracked.
    size_t GetNumIDs() const { return id_info.size(); }

    /// How big is an array associated with an ID?
    size_t GetArrayBytes(size_t id) const { return id_info[id].GetArrayBytes(); }

    /// Check if an ID is for a pointer that has been deleted.
    bool IsDeleted(size_t id) const {
      if (id == (size_t) -1) return false;   // Not tracked!
      if (ptr_debug) std::cout << "IsDeleted: " << id << std::endl;
      return !id_info[id].IsActive();
    }

    /// If a pointer active and ready to be used?
    bool IsActive(const void * ptr) {
      if (ptr_debug) std::cout << "IsActive: " << ptr << std::endl;
      if (ptr_id.find(ptr) == ptr_id.end()) return false; // Not in database.
      return GetInfo(ptr).IsActive();
    }

    /// Is an ID associated with an array?
    bool IsArrayID(size_t id) {
      if (ptr_debug) std::cout << "IsArrayID: " << id << std::endl;
      return id_info[id].IsArray();
    }

    /// How many Ptr objects are associated with an ID?
    int GetIDCount(size_t id) const {
      if (ptr_debug) std::cout << "Count:  " << id << std::endl;
      return id_info[id].GetCount();
    }

    /// This pointer was just created as a Ptr!
    size_t New(const void * ptr) {
      emp_assert(ptr);     // Cannot track a null pointer.
      size_t id = id_info.size();
#ifdef EMP_ABORT_PTR_NEW
      if (id == EMP_ABORT_PTR_NEW) {
        std::cerr << "Aborting at creation of Ptr id " << id << std::endl;
        abort();
      }
#endif
      if (ptr_debug) std::cout << "New:    " << id << " (" << ptr << ")" << std::endl;
      // Make sure pointer is not already stored -OR- hase been deleted (since re-use is possible).
      emp_assert(!HasPtr(ptr) || IsDeleted(GetCurID(ptr)), id);
      id_info.emplace_back(ptr);
      ptr_id[ptr] = id;
      return id;
    }

    /// This pointer was just created as a Ptr ARRAY!
    size_t NewArray(const void * ptr, size_t array_bytes) {
      size_t id = New(ptr);  // Build the new pointer.
      if (ptr_debug) std::cout << "  ...Array of size " << array_bytes << std::endl;
      id_info[id].SetArray(array_bytes);
      return id;
    }

    /// Increment the nuber of Pointers associated with an ID
    void IncID(size_t id) {
      if (id == (size_t) -1) return;   // Not tracked!
      if (ptr_debug) std::cout << "Inc:    " << id << std::endl;
      id_info[id].Inc();
    }

    /// Decrement the nuber of Pointers associated with an ID
    void DecID(size_t id) {
      if (id == (size_t) -1) return;   // Not tracked!
      auto & info = id_info[id];
      if (ptr_debug) std::cout << "Dec:    " << id << "(" << info.GetPtr() << ")" << std::endl;
      emp_assert(info.GetCount() > 0, "Decrementing Ptr, but already zero!",
                 id, info.GetPtr(), info.IsActive());
      info.Dec();
    }

    /// Mark the pointers associated with this ID as deleted.
    void MarkDeleted(size_t id) {
#ifdef EMP_ABORT_PTR_DELETE
      if (id == EMP_ABORT_PTR_DELETE) {
        std::cerr << "Aborting at creation of Ptr id " << id << std::endl;
        abort();
      }
#endif
      if (ptr_debug) std::cout << "Delete: " << id << std::endl;
      id_info[id].MarkDeleted();
    }
  };


//////////////////////////////////
//
//  --- Ptr implementation ---
//
//////////////////////////////////

#ifdef EMP_TRACK_MEM

  namespace {
    // @CAO: Build this for real!
    template <typename FROM, typename TO>
    bool PtrIsConvertable(FROM * ptr) { return true; }
    // emp_assert( (std::is_same<TYPE,T2>() || dynamic_cast<TYPE*>(in_ptr)) );

    // Debug information provided for each pointer type.
    struct PtrDebug {
      size_t current;
      size_t total;
      PtrDebug() : current(0), total(0) { ; }
      void AddPtr() { current++; total++; }
      void RemovePtr() { current--; }
    };
  }

  template <typename TYPE>
  class Ptr {
  public:
    TYPE * ptr;                 ///< The raw pointer associated with this Ptr object.
    size_t id;                  ///< A unique ID for this pointer type.
    using element_type = TYPE;  ///< Type being pointed at.

    static PtrDebug & DebugInfo() { static PtrDebug info; return info; } // Debug info for each type
    static PtrTracker & Tracker() { return PtrTracker::Get(); }  // Single tracker for al Ptr types

    /// Construct a null Ptr by default.
    Ptr() : ptr(nullptr), id((size_t) -1) {
      if (ptr_debug) std::cout << "null construct: " << ptr << std::endl;
    }

    /// Construct using copy constructor
    Ptr(const Ptr<TYPE> & _in) : ptr(_in.ptr), id(_in.id) {
      if (ptr_debug) std::cout << "copy construct: " << ptr << std::endl;
      Tracker().IncID(id);
    }

    /// Construct using move constructor
    Ptr(Ptr<TYPE> && _in) : ptr(_in.ptr), id(_in.id) {
      if (ptr_debug) std::cout << "move construct: " << ptr << std::endl;
      _in.id = (size_t) -1;
      // No IncID or DecID in Tracker since we just move the id.
    }

    /// Construct from a raw pointer of campatable type.
    template <typename T2>
    Ptr(T2 * in_ptr, bool track=false) : ptr(in_ptr), id((size_t) -1)
    {
      if (ptr_debug) std::cout << "raw construct: " << ptr << ". track=" << track << std::endl;
      emp_assert( (PtrIsConvertable<T2, TYPE>(in_ptr)) );

      // If this pointer is already active, link to it.
      if (Tracker().IsActive(ptr)) {
        id = Tracker().GetCurID(ptr);
        Tracker().IncID(id);
      }
      // If we are not already tracking this pointer, but should be, add it.
      else if (track) {
        id = Tracker().New(ptr);
        DebugInfo().AddPtr();
      }
    }

    /// Construct from a raw pointer of campatable ARRAY type.
    template <typename T2>
    Ptr(T2 * _ptr, size_t array_size, bool track) : ptr(_ptr), id((size_t) -1)
    {
      const size_t array_bytes = array_size * sizeof(T2);
      if (ptr_debug) std::cout << "raw ARRAY construct: " << ptr
                               << ". size=" << array_size << "(" << array_bytes
                               << " bytes); track=" << track << std::endl;
      emp_assert( (PtrIsConvertable<T2, TYPE>(_ptr)) );

      // If this pointer is already active, link to it.
      if (Tracker().IsActive(ptr)) {
        id = Tracker().GetCurID(ptr);
        Tracker().IncID(id);
        emp_assert(Tracker().GetArrayBytes(id) == array_bytes); // Make sure pointer is consistent.
      }
      // If we are not already tracking this pointer, but should be, add it.
      else if (track) {
        id = Tracker().NewArray(ptr, array_bytes);
        DebugInfo().AddPtr();
      }
    }

    /// Construct from another Ptr<> object of compatable type.
    template <typename T2>
    Ptr(Ptr<T2> _in) : ptr(_in.Raw()), id(_in.GetID()) {
      if (ptr_debug) std::cout << "inexact copy construct: " << ptr << std::endl;
      emp_assert( (PtrIsConvertable<T2, TYPE>(_in.Raw())), id );
      Tracker().IncID(id);
    }

    /// Construct from nullptr.
    Ptr(std::nullptr_t) : Ptr() {
      if (ptr_debug) std::cout << "null construct 2." << std::endl;
    }

    /// Destructor.
    ~Ptr() {
      if (ptr_debug) {
        std::cout << "destructing Ptr instance ";
        if (ptr) std::cout << id << " (" << ptr << ")\n";
        else std::cout << "(nullptr)\n";
      }
      Tracker().DecID(id);
    }

    /// Is this Ptr currently nullptr?
    bool IsNull() const { return ptr == nullptr; }

    /// Convert this Ptr to a raw pointer that isn't going to be tracked.
    TYPE * Raw() {
      emp_assert(Tracker().IsDeleted(id) == false, "Do not convert deleted Ptr to raw.", id);
      return ptr;
    }

    /// Convert this Ptr to a const raw pointer that isn't going to be tracked.
    const TYPE * const Raw() const {
      emp_assert(Tracker().IsDeleted(id) == false, "Do not convert deleted Ptr to raw.", id);
      return ptr;
    }

    /// Cast this Ptr to a different type.
    template <typename T2> Ptr<T2> Cast() {
      emp_assert(Tracker().IsDeleted(id) == false, "Do not cast deleted pointers.", id);
      return (T2*) ptr;
    }

    /// Cast this Ptr to a const Ptr of a different type.
    template <typename T2> const Ptr<const T2> Cast() const {
      emp_assert(Tracker().IsDeleted(id) == false, "Do not cast deleted pointers.", id);
      return (T2*) ptr;
    }

    /// Dynamically cast this Ptr to another type; throw an assert of the cast fails.
    template <typename T2> Ptr<T2> DynamicCast() {
      emp_assert(dynamic_cast<T2*>(ptr) != nullptr);
      emp_assert(Tracker().IsDeleted(id) == false, "Do not cast deleted pointers.", id);
      return (T2*) ptr;
    }

    /// Get the unique ID associated with this pointer.
    size_t GetID() const { return id; }

    /// Reallocate this Ptr to a newly allocated value using arguments passed in.
    template <typename... T>
    void New(T &&... args) {
      Tracker().DecID(id);                            // Remove a pointer to any old memory...

      // ptr = new TYPE(std::forward<T>(args)...); // Special new that uses allocated space.
      ptr = (TYPE*) malloc (sizeof(TYPE));            // Build a new raw pointer.
      emp_emscripten_assert(ptr);                     // No exceptions in emscripten; assert alloc!
      ptr = new (ptr) TYPE(std::forward<T>(args)...); // Special new that uses allocated space.

      if (ptr_debug) std::cout << "Ptr::New() : " << ptr << std::endl;
      id = Tracker().New(ptr);                        // And track it!
      DebugInfo().AddPtr();
    }

    /// Reallocate this Ptr to a newly allocated array using the size passed in.
    void NewArray(size_t array_size) {
      Tracker().DecID(id);                              // Remove a pointer to any old memory...

      //ptr = new TYPE[array_size];                     // Build a new raw pointer to an array.
      ptr = (TYPE*) malloc (array_size * sizeof(TYPE)); // Build a new raw pointer.
      emp_emscripten_assert(ptr, array_size);           // No exceptions in emscripten; assert alloc!
      for (size_t i = 0; i < array_size; i++) {
        new (ptr + i*sizeof(TYPE)) TYPE();
      }

      if (ptr_debug) std::cout << "Ptr::NewArray() : " << ptr << std::endl;
      id = Tracker().NewArray(ptr, array_size * sizeof(TYPE));   // And track it!
      DebugInfo().AddPtr();
    }

    /// Delete this pointer (ust NOT be an array).
    void Delete() {
      emp_assert(id < Tracker().GetNumIDs(), id, "Deleting Ptr that we are not resposible for.");
      emp_assert(ptr, "Deleting null Ptr.");
      emp_assert(Tracker().IsArrayID(id) == false, id, "Trying to delete array pointer as non-array.");
      Tracker().MarkDeleted(id);
      DebugInfo().RemovePtr();
      if (ptr_debug) std::cout << "Ptr::Delete() : " << ptr << std::endl;
      delete ptr;
    }

    /// Delete this pointer to an array (must be an array).
    void DeleteArray() {
      emp_assert(id < Tracker().GetNumIDs(), id, "Deleting Ptr that we are not resposible for.");
      emp_assert(ptr, "Deleting null Ptr.");
      emp_assert(Tracker().IsArrayID(id), id, "Trying to delete non-array pointer as array.");
      Tracker().MarkDeleted(id);
      DebugInfo().RemovePtr();
      if (ptr_debug) std::cout << "Ptr::DeleteArray() : " << ptr << std::endl;
      delete [] ptr;
    }

    /// Convert this pointer to a hash value.
    size_t Hash() const {
      // Chop off useless bits of pointer...
      static constexpr size_t shift = Log2(1 + sizeof(TYPE));
      return (size_t)(ptr) >> shift;
    }
    struct hash_t { size_t operator()(const Ptr<TYPE> & t) const { return t.Hash(); } };

    /// Copy assignment
    Ptr<TYPE> & operator=(const Ptr<TYPE> & _in) {
      if (ptr_debug) std::cout << "copy assignment" << std::endl;
      emp_assert(Tracker().IsDeleted(_in.id) == false, _in.id, "Do not copy deleted pointers.");
      if (id != _in.id) {        // Assignments only need to happen if ptrs are different.
        Tracker().DecID(id);
        ptr = _in.ptr;
        id = _in.id;
        Tracker().IncID(id);
      }
      return *this;
    }

    /// Move assignment
    Ptr<TYPE> & operator=(Ptr<TYPE> && _in) {
      if (ptr_debug) std::cout << "move assignment" << std::endl;
      emp_assert(Tracker().IsDeleted(_in.id) == false, _in.id, "Do not move deleted pointers.");
      if (id != _in.id) {
        Tracker().DecID(id);   // Decrement references to former pointer at this position.
        ptr = _in.ptr;
        id = _in.id;
        _in.ptr = nullptr;
        _in.id = (size_t) -1;
      }
      return *this;
    }

    /// Assign to a raw pointer of the correct type; if this is already tracked, hooked in
    /// correctly, otherwise don't track.
    template <typename T2>
    Ptr<TYPE> & operator=(T2 * _in) {
      if (ptr_debug) std::cout << "raw assignment" << std::endl;
      emp_assert( (PtrIsConvertable<T2, TYPE>(_in)) );

      Tracker().DecID(id);    // Decrement references to former pointer at this position.
      ptr = _in;              // Update to new pointer.

      // If this pointer is already active, link to it.
      if (Tracker().IsActive(ptr)) {
        id = Tracker().GetCurID(ptr);
        Tracker().IncID(id);
      }
      // Otherwise, since this ptr was passed in as a raw pointer, we do not manage it.
      else {
        id = (size_t) -1;
      }

      return *this;
    }

    /// Assign to a convertable Ptr
    template <typename T2>
    Ptr<TYPE> & operator=(Ptr<T2> _in) {
      if (ptr_debug) std::cout << "convert-copy assignment" << std::endl;
      emp_assert( (PtrIsConvertable<T2, TYPE>(_in.Raw())), _in.id );
      emp_assert(Tracker().IsDeleted(_in.id) == false, _in.id, "Do not copy deleted pointers.");
      Tracker().DecID(id);
      ptr = _in.Raw();
      id = _in.GetID();
      Tracker().IncID(id);
      return *this;
    }

    /// Dereference a pointer.
    TYPE & operator*() {
      // Make sure a pointer is active and non-null before we dereference it.
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(ptr != nullptr, "Do not dereference a null pointer!");
      return *ptr;
    }

    /// Dereference a pointer to a const type.
    const TYPE & operator*() const {
      // Make sure a pointer is active before we dereference it.
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(ptr != nullptr, "Do not dereference a null pointer!");
      return *ptr;
    }

    /// Follow a pointer.
    TYPE * operator->() {
      // Make sure a pointer is active before we follow it.
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(ptr != nullptr, "Do not follow a null pointer!");
      return ptr;
    }

    /// Follow a pointer to a const target.
    const TYPE * const operator->() const {
      // Make sure a pointer is active before we follow it.
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(ptr != nullptr, "Do not follow a null pointer!");
      return ptr;
    }

    /// Indexing into array
    TYPE & operator[](size_t pos) {
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(Tracker().IsArrayID(id), "Only arrays can be indexed into.", id);
      emp_assert(Tracker().GetArrayBytes(id) > (pos*sizeof(TYPE)),
        "Indexing out of range.", id, ptr, pos, sizeof(TYPE), Tracker().GetArrayBytes(id));
      emp_assert(ptr != nullptr, "Do not follow a null pointer!");
      return ptr[pos];
    }

    /// Indexing into const array
    const TYPE & operator[](size_t pos) const {
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);
      emp_assert(Tracker().IsArrayID(id), "Only arrays can be indexed into.", id);
      emp_assert(Tracker().GetArrayBytes(id) > (pos*sizeof(TYPE)),
        "Indexing out of range.", id, ptr, pos, sizeof(TYPE), Tracker().GetArrayBytes(id));
      emp_assert(ptr != nullptr, "Do not follow a null pointer!");
      return ptr[pos];
    }

    /// Auto-case to raw pointer type.
    operator TYPE *() {
      // Make sure a pointer is active before we convert it.
      emp_assert(Tracker().IsDeleted(id) == false /*, typeid(TYPE).name() */, id);

      // We should not automatically convert managed pointers to raw pointers; use .Raw()
      emp_assert(id == (size_t) -1 /*, typeid(TYPE).name() */, id);
      return ptr;
    }

    /// Does this pointer exist?
    operator bool() { return ptr != nullptr; }

    /// Does this const pointer exist?
    operator bool() const { return ptr != nullptr; }

    /// Does this Ptr point to the same memory position?
    bool operator==(const Ptr<TYPE> & in_ptr) const { return ptr == in_ptr.ptr; }

    /// Does this Ptr point to different memory positions?
    bool operator!=(const Ptr<TYPE> & in_ptr) const { return ptr != in_ptr.ptr; }

    /// Does this Ptr point to a memory position before another?
    bool operator<(const Ptr<TYPE> & in_ptr)  const { return ptr < in_ptr.ptr; }

    /// Does this Ptr point to a memory position before or equal to another?
    bool operator<=(const Ptr<TYPE> & in_ptr) const { return ptr <= in_ptr.ptr; }

    /// Does this Ptr point to a memory position after another?
    bool operator>(const Ptr<TYPE> & in_ptr)  const { return ptr > in_ptr.ptr; }

    /// Does this Ptr point to a memory position after or equal to another?
    bool operator>=(const Ptr<TYPE> & in_ptr) const { return ptr >= in_ptr.ptr; }


    /// Does this Ptr point to the same memory position as a raw pointer?
    bool operator==(const TYPE * in_ptr) const { return ptr == in_ptr; }

    /// Does this Ptr point to different memory positions as a raw pointer?
    bool operator!=(const TYPE * in_ptr) const { return ptr != in_ptr; }

    /// Does this Ptr point to a memory position before a raw pointer?
    bool operator<(const TYPE * in_ptr)  const { return ptr < in_ptr; }

    /// Does this Ptr point to a memory position before or equal to a raw pointer?
    bool operator<=(const TYPE * in_ptr) const { return ptr <= in_ptr; }

    /// Does this Ptr point to a memory position after a raw pointer?
    bool operator>(const TYPE * in_ptr)  const { return ptr > in_ptr; }

    /// Does this Ptr point to a memory position after or equal to a raw pointer?
    bool operator>=(const TYPE * in_ptr) const { return ptr >= in_ptr; }


    /// Some debug testing functions
    int DebugGetCount() const { return Tracker().GetIDCount(id); }

    // Prevent use of new and delete on Ptr
    // static void* operator new(std::size_t) noexcept {
    //   emp_assert(false, "No Ptr::operator new; use emp::NewPtr for clarity.");
    //   return nullptr;
    // }
    // static void* operator new[](std::size_t sz) noexcept {
    //   emp_assert(false, "No Ptr::operator new[]; use emp::NewPtrArray for clarity.");
    //   return nullptr;
    // }
    //
    // static void operator delete(void* ptr, std::size_t sz) {
    //   emp_assert(false, "No Ptr::operator delete; use Delete() member function for clarity.");
    // }
    // static void operator delete[](void* ptr, std::size_t sz) {
    //   emp_assert(false, "No Ptr::operator delete[]; use DeleteArray() member function for clarity.");
    // }

  };

#else


  template <typename TYPE>
  class Ptr {
  private:
    TYPE * ptr;

  public:
    using element_type = TYPE;

    Ptr() : ptr(nullptr) {}                                              ///< Default constructor
    Ptr(const Ptr<TYPE> & _in) : ptr(_in.ptr) {}                         ///< Copy constructor
    Ptr(Ptr<TYPE> && _in) : ptr(_in.ptr) {}                              ///< Move constructor
    template <typename T2> Ptr(T2 * in_ptr, bool=false) : ptr(in_ptr) {} ///< Construct from raw ptr
    template <typename T2> Ptr(T2 * _ptr, size_t, bool) : ptr(_ptr) {}   ///< Construct from array
    template <typename T2> Ptr(Ptr<T2> _in) : ptr(_in.Raw()) {}          ///< From compatible Ptr
    Ptr(std::nullptr_t) : Ptr() {}                                       ///< From nullptr
    ~Ptr() { ; }                                                         ///< Destructor

    bool IsNull() const { return ptr == nullptr; }
    TYPE * Raw() { return ptr; }
    const TYPE * const Raw() const { return ptr; }
    template <typename T2> Ptr<T2> Cast() { return (T2*) ptr; }
    template <typename T2> const Ptr<const T2> Cast() const { return (T2*) ptr; }
    template <typename T2> Ptr<T2> DynamicCast() { return dynamic_cast<T2*>(ptr); }

    template <typename... T>
    void New(T &&... args) { ptr = new TYPE(std::forward<T>(args)...); }  // New raw pointer.
    void NewArray(size_t array_size) { ptr = new TYPE[array_size]; }
    void Delete() { delete ptr; }
    void DeleteArray() { delete [] ptr; }

    size_t Hash() const {
      static constexpr size_t shift = Log2(1 + sizeof(TYPE));  // Chop off useless bits...
      return (size_t)(ptr) >> shift;
    }
    struct hash_t { size_t operator()(const Ptr<TYPE> & t) const { return t.Hash(); } };

    // Copy/Move assignments
    Ptr<TYPE> & operator=(const Ptr<TYPE> & _in) { ptr = _in.ptr; return *this; }
    Ptr<TYPE> & operator=(Ptr<TYPE> && _in) { ptr = _in.ptr; _in.ptr = nullptr; return *this; }

    // Assign to compatible Ptr or raw (non-managed) pointer.
    template <typename T2> Ptr<TYPE> & operator=(T2 * _in) { ptr = _in; return *this; }
    template <typename T2> Ptr<TYPE> & operator=(Ptr<T2> _in) { ptr = _in.Raw(); return *this; }

    // Dereference a pointer.
    TYPE & operator*() { return *ptr; }
    const TYPE & operator*() const { return *ptr; }

    // Follow a pointer.
    TYPE * operator->() { return ptr; }
    const TYPE * const operator->() const { return ptr; }

    // Indexing into array
    TYPE & operator[](size_t pos) { return ptr[pos]; }
    const TYPE & operator[](size_t pos) const { return ptr[pos]; }

    // Auto-case to raw pointer type.
    operator TYPE *() { return ptr; }

    operator bool() { return ptr != nullptr; }
    operator bool() const { return ptr != nullptr; }

    // Comparisons to other Ptr objects
    bool operator==(const Ptr<TYPE> & in_ptr) const { return ptr == in_ptr.ptr; }
    bool operator!=(const Ptr<TYPE> & in_ptr) const { return ptr != in_ptr.ptr; }
    bool operator<(const Ptr<TYPE> & in_ptr)  const { return ptr < in_ptr.ptr; }
    bool operator<=(const Ptr<TYPE> & in_ptr) const { return ptr <= in_ptr.ptr; }
    bool operator>(const Ptr<TYPE> & in_ptr)  const { return ptr > in_ptr.ptr; }
    bool operator>=(const Ptr<TYPE> & in_ptr) const { return ptr >= in_ptr.ptr; }

    // Comparisons to raw pointers.
    bool operator==(const TYPE * in_ptr) const { return ptr == in_ptr; }
    bool operator!=(const TYPE * in_ptr) const { return ptr != in_ptr; }
    bool operator<(const TYPE * in_ptr)  const { return ptr < in_ptr; }
    bool operator<=(const TYPE * in_ptr) const { return ptr <= in_ptr; }
    bool operator>(const TYPE * in_ptr)  const { return ptr > in_ptr; }
    bool operator>=(const TYPE * in_ptr) const { return ptr >= in_ptr; }

  };

#endif

  // IO
  template <typename T>
  std::ostream & operator<<(std::ostream & out, const emp::Ptr<T> & ptr) {
    out << ptr.Raw();
    return out;
  }

  // @CAO: Reading a pointer from a stream seems like a terrible idea in most situations, but I
  // can imagine limited circumstances where it would be needed.
  template <typename T, typename... Ts>
  std::istream & operator>>(std::istream & is, emp::Ptr<T> & ptr) {
    T * val;
    is >> val;
    ptr = val;
    return is;
  }


  // Create a helper to replace & operator.
  template <typename T> Ptr<T> ToPtr(T * _in, bool own=false) { return Ptr<T>(_in, own); }
  template <typename T> Ptr<T> TrackPtr(T * _in, bool own=true) { return Ptr<T>(_in, own); }

  template <typename T, typename... ARGS> Ptr<T> NewPtr(ARGS &&... args) {
    auto ptr = new T(std::forward<ARGS>(args)...);
    emp_emscripten_assert(ptr);     // Trigger emscripten-only assert on allocation (no exceptions available)
    return Ptr<T>(ptr, true);
  }
  template <typename T, typename... ARGS> Ptr<T> NewArrayPtr(size_t array_size) {
    auto ptr = new T[array_size];
    emp_emscripten_assert(ptr, array_size);     // Trigger emscripten-only assert on allocation (no exceptions available)
    return Ptr<T>(ptr, array_size, true);
  }
}

#endif // EMP_PTR_H
