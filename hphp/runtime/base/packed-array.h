/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-present Facebook, Inc. (http://www.facebook.com)  |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/
#ifndef incl_HPHP_PACKED_ARRAY_H_
#define incl_HPHP_PACKED_ARRAY_H_

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "hphp/runtime/base/array-common.h"
#include "hphp/runtime/base/array-data.h"
#include "hphp/runtime/base/data-walker.h"
#include "hphp/runtime/base/header-kind.h"
#include "hphp/runtime/base/tv-val.h"
#include "hphp/runtime/base/sort-flags.h"
#include "hphp/runtime/base/typed-value.h"

#include "hphp/util/type-scan.h"

namespace HPHP {

//////////////////////////////////////////////////////////////////////

struct Variant;
struct ArrayData;
struct StringData;
struct MixedArray;
struct APCArray;
struct APCHandle;

//////////////////////////////////////////////////////////////////////

/*
 * Packed arrays are a specialized array layout for vector-like data.  That is,
 * php arrays with zero-based contiguous integer keys, and values of mixed
 * types.  The TypedValues are placed right after the array header.
 */
struct PackedArray final : type_scan::MarkCollectable<PackedArray> {
  static constexpr uint32_t SmallSize = 3;
  // the smallest and largest MM size classes we use for allocating PackedArrays
  static constexpr size_t SmallSizeIndex = 3;
  static constexpr size_t MaxSizeIndex = 121;

  // Used in static_asserts near code that will have to change if/when we
  // disaggregate the TypedValues in PackedArray.
  static constexpr bool stores_typed_values = true;

  static_assert(MaxSizeIndex <= std::numeric_limits<uint8_t>::max(),
                "Size index must fit into 8-bits");

  static void Release(ArrayData*);
  // Recursively register {allocation, rootAPCHandle} with APCGCManager
  static void RegisterUncountedAllocations(ArrayData* ad,
                                           APCHandle* rootAPCHandle);
  static void ReleaseUncounted(ArrayData*);
  static tv_rval NvGetInt(const ArrayData*, int64_t ki);
  static constexpr auto NvTryGetInt = &NvGetInt;
  static tv_rval NvGetStr(const ArrayData*, const StringData*);
  static constexpr auto NvTryGetStr = &NvGetStr;
  static ssize_t NvGetIntPos(const ArrayData*, int64_t k);
  static constexpr auto NvGetIntPosVec = &NvGetIntPos;
  static ssize_t NvGetStrPos(const ArrayData*, const StringData* k);
  static constexpr auto NvGetStrPosVec = &NvGetStrPos;
  static tv_rval RvalInt(const ArrayData* ad, int64_t k) {
    assertx(ad->isPacked());
    return NvGetInt(ad, k);
  }
  static tv_rval RvalIntStrict(const ArrayData* ad, int64_t k) {
    assertx(ad->isPacked());
    return NvTryGetInt(ad, k);
  }
  static tv_rval RvalStr(const ArrayData* ad, const StringData* k) {
    assertx(ad->isPacked());
    return NvGetStr(ad, k);
  }
  static tv_rval RvalStrStrict(const ArrayData* ad, const StringData* k) {
    assertx(ad->isPacked());
    return NvTryGetStr(ad, k);
  }
  static tv_rval RvalAtPos(const ArrayData* ad, ssize_t pos) {
    assertx(ad->isPacked());
    return GetValueRef(ad, pos);
  }
  static Cell NvGetKey(const ArrayData*, ssize_t pos);
  static ArrayData* SetInt(ArrayData*, int64_t k, Cell v);
  static ArrayData* SetIntInPlace(ArrayData*, int64_t k, Cell v);
  static ArrayData* SetStr(ArrayData*, StringData* k, Cell v);
  static ArrayData* SetStrInPlace(ArrayData*, StringData* k, Cell v);
  static size_t Vsize(const ArrayData*);
  static tv_rval GetValueRef(const ArrayData* ad, ssize_t pos);
  static bool IsVectorData(const ArrayData*) {
    return true;
  }
  static bool ExistsInt(const ArrayData* ad, int64_t k);
  static bool ExistsStr(const ArrayData*, const StringData*);
  static arr_lval LvalInt(ArrayData*, int64_t k, bool copy);
  static arr_lval LvalStr(ArrayData*, StringData* k, bool copy);
  static arr_lval LvalSilentInt(ArrayData*, int64_t, bool copy);
  static arr_lval LvalSilentStr(ArrayData*, StringData*, bool copy);
  static arr_lval LvalForceNew(ArrayData*, bool copy);
  static ArrayData* RemoveInt(ArrayData*, int64_t k);
  static ArrayData* RemoveIntInPlace(ArrayData*, int64_t k);
  static ArrayData* RemoveStr(ArrayData*, const StringData* k);
  static constexpr auto RemoveStrInPlace = &RemoveStr;
  static ssize_t IterBegin(const ArrayData*);
  static ssize_t IterLast(const ArrayData*);
  static ssize_t IterEnd(const ArrayData*);
  static ssize_t IterAdvance(const ArrayData*, ssize_t pos);
  static ssize_t IterRewind(const ArrayData*, ssize_t pos);
  static ArrayData* Copy(const ArrayData* ad);
  static ArrayData* CopyStatic(const ArrayData*);
  static ArrayData* EscalateForSort(ArrayData*, SortFunction);
  static void Ksort(ArrayData*, int, bool);
  static void Sort(ArrayData*, int, bool);
  static void Asort(ArrayData*, int, bool);
  static bool Uksort(ArrayData*, const Variant&);
  static bool Usort(ArrayData*, const Variant&);
  static bool Uasort(ArrayData*, const Variant&);
  static ArrayData* Append(ArrayData*, Cell v);
  static ArrayData* AppendInPlace(ArrayData*, Cell v);
  static ArrayData* PlusEq(ArrayData*, const ArrayData* elems);
  static ArrayData* Merge(ArrayData*, const ArrayData* elems);
  static ArrayData* Pop(ArrayData*, Variant& value);
  static ArrayData* Dequeue(ArrayData*, Variant& value);
  static ArrayData* Prepend(ArrayData*, Cell v);
  static ArrayData* ToPHPArray(ArrayData*, bool);
  static constexpr auto ToPHPArrayIntishCast = &ToPHPArray;
  static ArrayData* ToVArray(ArrayData*, bool);
  static ArrayData* ToDArray(ArrayData*, bool);
  static ArrayData* ToDict(ArrayData*, bool);
  static ArrayData* ToVec(ArrayData*, bool);
  static void Renumber(ArrayData*) {}
  static void OnSetEvalScalar(ArrayData*);
  static ArrayData* Escalate(const ArrayData* ad) {
    return const_cast<ArrayData*>(ad);
  }

  static constexpr auto ToKeyset = &ArrayCommon::ToKeyset;

  static tv_rval NvTryGetIntVec(const ArrayData*, int64_t);
  static tv_rval NvTryGetStrVec(const ArrayData*, const StringData*);
  static ArrayData* SetIntVec(ArrayData*, int64_t, Cell);
  static ArrayData* SetIntInPlaceVec(ArrayData*, int64_t, Cell);
  static ArrayData* SetStrVec(ArrayData*, StringData*, Cell);
  static constexpr auto SetStrInPlaceVec = &SetStrVec;
  static ArrayData* RemoveIntVec(ArrayData*, int64_t);
  static ArrayData* RemoveIntInPlaceVec(ArrayData*, int64_t);
  static arr_lval LvalIntVec(ArrayData*, int64_t, bool);
  static arr_lval LvalStrVec(ArrayData*, StringData*, bool);
  static constexpr auto LvalSilentIntVec = &LvalSilentInt;
  static constexpr auto LvalSilentStrVec = &LvalSilentStr;
  static ArrayData* PlusEqVec(ArrayData*, const ArrayData*);
  static ArrayData* ToPHPArrayVec(ArrayData*, bool);
  static constexpr auto ToPHPArrayIntishCastVec = &ToPHPArrayVec;
  static ArrayData* ToVArrayVec(ArrayData*, bool);
  static ArrayData* ToDictVec(ArrayData*, bool);
  static ArrayData* ToVecVec(ArrayData*, bool);

  static constexpr auto MergeVec = &Merge;
  static constexpr auto ReleaseVec = &Release;
  static constexpr auto NvGetIntVec = &NvGetInt;
  static constexpr auto NvGetStrVec = &NvGetStr;
  static constexpr auto NvGetKeyVec = &NvGetKey;
  static constexpr auto VsizeVec = &Vsize;
  static constexpr auto GetValueRefVec = &GetValueRef;
  static constexpr auto IsVectorDataVec = &IsVectorData;
  static constexpr auto ExistsIntVec = &ExistsInt;
  static constexpr auto ExistsStrVec = &ExistsStr;
  static constexpr auto LvalForceNewVec = &LvalForceNew;
  static constexpr auto RemoveStrVec = &RemoveStr;
  static constexpr auto RemoveStrInPlaceVec = &RemoveStr;
  static constexpr auto IterBeginVec = &IterBegin;
  static constexpr auto IterLastVec = &IterLast;
  static constexpr auto IterEndVec = &IterEnd;
  static constexpr auto IterAdvanceVec = &IterAdvance;
  static constexpr auto IterRewindVec = &IterRewind;
  static constexpr auto EscalateForSortVec = &EscalateForSort;
  static constexpr auto KsortVec = &Ksort;
  static constexpr auto SortVec = &Sort;
  static constexpr auto AsortVec = &Asort;
  static constexpr auto UksortVec = &Uksort;
  static constexpr auto UsortVec = &Usort;
  static constexpr auto UasortVec = &Uasort;
  static constexpr auto CopyVec = &Copy;
  static constexpr auto CopyStaticVec = &CopyStatic;
  static constexpr auto AppendVec = &Append;
  static constexpr auto AppendInPlaceVec = &AppendInPlace;
  static constexpr auto PopVec = &Pop;
  static constexpr auto DequeueVec = &Dequeue;
  static constexpr auto PrependVec = &Prepend;
  static constexpr auto RenumberVec = &Renumber;
  static constexpr auto OnSetEvalScalarVec = &OnSetEvalScalar;
  static constexpr auto EscalateVec = &Escalate;
  static constexpr auto ToKeysetVec = &ArrayCommon::ToKeyset;
  static constexpr auto ToDArrayVec = &ToDArray;

  static tv_rval RvalIntVec(const ArrayData* ad, int64_t k) {
    assertx(ad->isVecArray());
    return NvGetIntVec(ad, k);
  }
  static tv_rval RvalIntStrictVec(const ArrayData* ad, int64_t k) {
    assertx(ad->isVecArray());
    return NvTryGetIntVec(ad, k);
  }

  //////////////////////////////////////////////////////////////////////

  // Like LvalInt, but without any bounds checking. Used to implement the
  // Vector / ImmVector collection types in ext/collections. This function
  // never copies the array, so we can return tv_lval instead of arr_lval.
  static tv_lval LvalUncheckedInt(ArrayData*, int64_t);

  /////////////////////////////////////////////////////////////////////

  static bool checkInvariants(const ArrayData*);

  /*
   * Accepts any array of any kind satisfying isVectorData() and makes a
   * static packed copy, like CopyStatic().
   */
  static ArrayData* ConvertStatic(const ArrayData*);

  static ptrdiff_t entriesOffset();

  static uint32_t capacity(const ArrayData*);
  static size_t heapSize(const ArrayData*);
  static uint16_t packSizeIndexAndAuxBits(uint8_t, uint8_t);

  static void scan(const ArrayData*, type_scan::Scanner&);

  static ArrayData* MakeReserve(uint32_t capacity);
  static ArrayData* MakeReserveVArray(uint32_t capacity);
  static ArrayData* MakeReserveVec(uint32_t capacity);

  /*
   * Allocate a PackedArray containing `size' values, in the reverse order of
   * the `values' array. This can only be used to populate the array with cells,
   * not refs.
   *
   * This function takes ownership of the Cells in `values'.
   */
  static ArrayData* MakePacked(uint32_t size, const Cell* values);
  static ArrayData* MakeVArray(uint32_t size, const Cell* values);
  static ArrayData* MakeVec(uint32_t size, const Cell* values);

  /*
   * Like MakePacked, but with `values' array in natural (not reversed) order.
   */
  static ArrayData* MakePackedNatural(uint32_t size, const Cell* values);
  static ArrayData* MakeVArrayNatural(uint32_t size, const Cell* values);
  static ArrayData* MakeVecNatural(uint32_t size, const Cell* values);

  static ArrayData* MakeUninitialized(uint32_t size);
  static ArrayData* MakeUninitializedVArray(uint32_t size);
  static ArrayData* MakeUninitializedVec(uint32_t size);

  static ArrayData* MakeUncounted(
      ArrayData* array, bool withApcTypedValue = false,
      DataWalker::PointerMap* seen = nullptr
  );
  static ArrayData* MakeUncounted(
      ArrayData* array, int, DataWalker::PointerMap* seen = nullptr
  ) = delete;
  static ArrayData* MakeUncounted(
      ArrayData* array, size_t extra, DataWalker::PointerMap* seen = nullptr
  ) = delete;
  static ArrayData* MakeUncountedHelper(ArrayData* array, size_t extra);

  static ArrayData* MakeVecFromAPC(const APCArray* apc);
  static ArrayData* MakeVArrayFromAPC(const APCArray* apc);

  static bool VecEqual(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecNotEqual(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecSame(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecNotSame(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecLt(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecLte(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecGt(const ArrayData* ad1, const ArrayData* ad2);
  static bool VecGte(const ArrayData* ad1, const ArrayData* ad2);
  static int64_t VecCmp(const ArrayData* ad1, const ArrayData* ad2);

  // Fast iteration
  template <class F, bool inc = true>
  static void IterateV(const ArrayData* arr, F fn);
  template <class F, bool inc = true>
  static void IterateKV(const ArrayData* arr, F fn);

  static MixedArray* ToMixed(ArrayData*);
  static MixedArray* ToMixedCopy(const ArrayData*);
  static MixedArray* ToMixedCopyReserve(const ArrayData*, size_t);

  static size_t capacityToSizeIndex(size_t);

  static constexpr auto SizeIndexOffset = HeaderAuxOffset + 1;
private:
  static uint8_t sizeClass(const ArrayData*);

  static MixedArray* ToMixedHeader(const ArrayData*, size_t);

  static ArrayData* Grow(ArrayData*, bool);
  static ArrayData* PrepareForInsert(ArrayData*, bool);
  static SortFlavor preSort(ArrayData*);

  static ArrayData* MakeReserveImpl(uint32_t, HeaderKind, ArrayData::DVArray);

  template<bool reverse>
  static ArrayData* MakePackedImpl(uint32_t, const Cell*,
                                   HeaderKind, ArrayData::DVArray);

  static void CopyPackedHelper(const ArrayData* adIn, ArrayData* ad);

  static bool VecEqualHelper(const ArrayData*, const ArrayData*, bool);
  static int64_t VecCmpHelper(const ArrayData*, const ArrayData*);

  static ArrayData* RemoveImpl(ArrayData*, int64_t, bool);
  static ArrayData* RemoveImplVec(ArrayData*, int64_t, bool);

  static ArrayData* AppendImpl(ArrayData*, Cell v, bool copy);

  struct VecInitializer;
  static VecInitializer s_vec_initializer;

  struct VArrayInitializer;
  static VArrayInitializer s_varr_initializer;
};

//////////////////////////////////////////////////////////////////////

}

#endif
