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

#ifndef incl_HPHP_JIT_UNWIND_ITANIUM_H_
#define incl_HPHP_JIT_UNWIND_ITANIUM_H_

#include "hphp/runtime/base/types.h"
#include "hphp/runtime/base/rds.h"
#include "hphp/runtime/base/typed-value.h"

#include "hphp/runtime/vm/jit/types.h"

#include "hphp/util/asm-x64.h"

#include <cstddef>

#ifndef _MSC_VER
#include <unwind.h>
#else
#include "hphp/util/unwind-itanium-msvc.h"
#endif

namespace HPHP {

struct ActRec;
struct EHFrameWriter;

namespace jit {

///////////////////////////////////////////////////////////////////////////////

/*
 * Information the unwinder needs stored in RDS, and the rds::Link for it.
 * Used to pass values between unwinder code and catch traces.
 */
struct UnwindRDS {
  /* When a cleanup (non-side-exiting) catch trace is executing, this will
   * point to the currently propagating exception, to be passed to
   * _Unwind_Resume at the end of cleanup. */
  _Unwind_Exception* exn;

  /* Some helpers need to signal an error along with a TypedValue to be pushed
   * on the eval stack. When present, that value lives here. */
  TypedValue tv;

  /* This will be true iff the currently executing catch trace should side exit
   * to somewhere else in the TC, rather than resuming the unwind process. */
  bool doSideExit;
};
extern rds::Link<UnwindRDS, rds::Mode::Normal> g_unwind_rds;

#define IMPLEMENT_OFF(Name, member)                             \
  inline ptrdiff_t unwinder##Name##Off() {                      \
    return g_unwind_rds.handle() + offsetof(UnwindRDS, member); \
  }
IMPLEMENT_OFF(Exn, exn)
IMPLEMENT_OFF(TV, tv)
IMPLEMENT_OFF(SideExit, doSideExit)
#undef IMPLEMENT_OFF

///////////////////////////////////////////////////////////////////////////////

/*
 * The personality routine for code emitted by the jit.
 */
_Unwind_Reason_Code
tc_unwind_personality(int version,
                      _Unwind_Action actions,
                      uint64_t exceptionClass,
                      _Unwind_Exception* exceptionObj,
                      _Unwind_Context* context);

using PersonalityFunc = _Unwind_Reason_Code(*)(int, _Unwind_Action, uint64_t,
                                               _Unwind_Exception*,
                                               _Unwind_Context*);

/*
 * Resume unwinding of jitted PHP frames.
 *
 * This is called from the endCatchHelper stub, which is hit at the end of
 * every catch trace.  It returns the new value of vmfp(), as well as the catch
 * trace to jump to---or nullptr if there is none, in which case the native
 * unwinder should be invoked via _Unwind_Resume().
 */
struct TCUnwindInfo {
  TCA catchTrace;
  ActRec* fp;
};
TCUnwindInfo tc_unwind_resume(ActRec* fp);
TCUnwindInfo tc_unwind_resume_stublogue(ActRec* fp, TCA savedRip);

/*
 * Called to initialize the unwinder and register an .eh_frame that covers the
 * TC.
 */
void initUnwinder(TCA base, size_t size, PersonalityFunc fn);

///////////////////////////////////////////////////////////////////////////////

}}

#endif
