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

#pragma once

#include "hphp/runtime/vm/call-flags.h"
#include "hphp/runtime/vm/hhbc.h"

#include "hphp/runtime/vm/jit/code-cache.h"
#include "hphp/runtime/vm/jit/containers.h"
#include "hphp/runtime/vm/jit/types.h"
#include "hphp/runtime/vm/jit/phys-reg.h"
#include "hphp/runtime/vm/jit/srcdb.h"
#include "hphp/runtime/vm/jit/stack-offsets.h"

#include <string>
#include <vector>

namespace HPHP {

struct ActRec;
struct SrcKey;

namespace Debug { struct DebugInfo; }

namespace jit {

///////////////////////////////////////////////////////////////////////////////

constexpr int kNumFreeLocalsHelpers = 7;

/*
 * Addresses of various unique, long-lived JIT helper routines.
 *
 * The global set of unique stubs is emitted when we initialize the TC.
 */
struct UniqueStubs {
  /*
   * Unique stubs, ABIs, and stack alignment.
   *
   * A lot of complex control flow occurs amongst the various unique stubs, as
   * well as between the unique stubs and the TC.  This complexity is
   * exacerbated by the profusion of different ABIs used when entering and
   * exiting the stubs.
   *
   * The goal of the documentation in this file is to enumerate all the ways
   * each stub is reached, as well as to clarify the ABI boundaries they
   * implement to, if any.
   *
   * Among the most important ABI invariants in jitted code is this:
   *
   *    - In the body of a PHP function, after the initial instructions of the
   *      func prologue, the native stack pointer is always 16-byte aligned.
   *
   * This holds even when the register allocator inserts stack spills; we
   * always offset the native stack pointer in increments of 16.
   *
   * Many target architectures have 16-byte stack alignment restrictions.  On
   * x64, for example, the stack must be 16-byte aligned before calls, to allow
   * the compiler to safely use aligned movs, e.g., for saving callee-saved XMM
   * registers.  On AArch64, attempting to address the stack pointer when it is
   * not 16-byte aligned results in a fault.
   *
   * Our alignment invariant should be sufficient for architectures with
   * 16-byte (or looser) alignment constraints.  We don't currently support any
   * higher- order alignment restrictions.
   *
   * Our other ABI concern is calling convention.  HHVM supports three distinct
   * calling conventions:
   *    - native: C++ helper calls on the targeted architecture
   *    - PHP:    calls between PHP functions, as well as to those unique stubs
   *              which play the same role as various parts of PHP functions
   *    - stub:   calls to unique stubs that implement helper routines
   *
   * Maintaining native stack alignment, and appropriately stashing the return
   * address, is shared between the call, return, and callee prologue
   * instructions for each convention:
   *
   *            +---------------------+---------+-----------+
   *            | main call instrs    | return  | prologue  |
   *  +---------+---------------------+---------+-----------+
   *  | native  | call{,r,m,s}        | ret     | N/A       |
   *  +---------+---------------------+---------+-----------+
   *  | PHP     | phpcall, callunpack | phpret  | phplogue  |
   *  +---------+---------------------+---------+----------+
   *  | stub    | callstub            | stubret | stublogue |
   *  +---------+---------------------+---------+-----------+
   *
   * Further documentation on these calling conventions can be found in
   * vasm-instr.h, alongside the corresponding instructions.
   *
   * Each stub is documented below with a (hopefully) exhaustive list of ways
   * it is reached, as well as the context under which it is reached:
   *    - func guard:     pre-phplogue{}; an intermediate state where
   *                      addressing the stack and making native calls are
   *                      forbidden
   *    - func prologue:  implements a phplogue{}
   *    - func body:      dominated by a phplogue{}; already ABI-conforming
   *    - stub:           implements a stublogue{}
   */

  /////////////////////////////////////////////////////////////////////////////
  // Function entry.

  /*
   * Dynamically dispatch to the appropriate func prologue based on the
   * information in php_call_regs.
   *
   * @reached:  callphp from TC
   * @context:  func guard
   */
  TCA funcPrologueRedispatch;

  /*
   * Look up or emit a func prologue and jump to it---or, failing that, call
   * native routines that do the same work.
   *
   * All entries in the prologue tables of all Funcs are initialized to this
   * stub, so that we can lazily generate their prologues.  If codegen for the
   * prologue succeeds, we update the prologue table to point to the new
   * prologue instead of this stub.
   *
   * @reached:  callphp from TC
   *            jmp from immutableBindCallStub
   *            jmp from funcPrologueRedispatch
   * @context:  func prologue
   */
  TCA fcallHelperThunk;

  /*
   * Look up or emit a func body entry point and jump to it---or, failing that,
   * fall back to the interpreter.
   *
   * This func body is just the translation for Func::base(), for functions
   * with no DV init funclets.  For functions with DV funclets, the func body
   * first dispatches to any necessary funclets before jumping to the base()
   * translation.
   *
   * @reached:  call from enterTCHelper$callTC
   *            jmp from fcallUnpackHelper
   * @context:  func body
   */
  TCA funcBodyHelperThunk;

  /*
   * Call EventHook::onFunctionCall() and handle the case where it requests
   * that we skip the function.
   *
   * @reached:  vinvoke from func prologue
   *            jmp from functionSurprisedOrStackOverflow
   * @context:  stub
   */
  TCA functionEnterHelper;

  /*
   * Handle either a surprise condition or a stack overflow.
   *
   * Also gracefully handles spurious wake-ups that result from racing with a
   * background thread clearing surprise flags.
   *
   * @reached:  vinvoke from func prologue
   * @context:  stub
   */
  TCA functionSurprisedOrStackOverflow;


  /////////////////////////////////////////////////////////////////////////////
  // Function return.

  /*
   * Return from a function when the ActRec was pushed by the interpreter.
   *
   * The return IP on the ActRec will be set to one of these stubs, so if
   * someone tries to execute a return instruction, we get a chance to set up
   * state for a POST_INTERP_RET service request.
   *
   * Generators need a different stub because the ActRec for a generator is in
   * the heap.
   *
   * @reached:  phpret from TC
   * @context:  func body (after returning to caller)
   */
  TCA retHelper;
  TCA genRetHelper;       // version for generator
  TCA asyncGenRetHelper;  // version for async generators

  /*
   * Return from a function when the ActRec was pushed by an inlined call.
   *
   * This is the same as retHelper, but is kept separate to aid in debugging.
   *
   * @reached:  phpret from TC
   * @context:  func body (after returning to caller)
   */
  TCA retInlHelper;

  /*
   * Return from a resumed async function.
   *
   * Store result into the AsyncFunctionWaitHandle, mark it as finished and
   * unblock its parents. Check whether the first parent is eligible to be
   * resumed directly (it is an AsyncFunctionWaitHandle in the same context
   * with a non-null resume address), and do so if possible. Otherwise, jump
   * to asyncSwitchCtrl. Slow version doesn't try to resume and just returns
   * to the asio scheduler.
   *
   * rvmfp() should point to the ActRec of the AsyncFunctionWaitHandle that
   * is returning, rvmsp() should point to an uninitialized cell on the
   * stack containing garbage.
   *
   * @reached:  jmp from TC
   * @context:  func body
   */
  TCA asyncFuncRet;
  TCA asyncFuncRetSlow;

  /*
   * Async function finish-suspend-and-resume stub.
   *
   * Check for fast resumables on the AsioContext runnable queue.  If one is
   * found that is ready to run, jump to it directly; otherwise, leave the TC
   * and return to the scheduler.
   *
   * rvmfp() should point to the ActRec of the WaitHandle that is suspending.
   *
   * @reached:  jmp from TC
   * @context:  func body
   */
  TCA asyncSwitchCtrl;


  /////////////////////////////////////////////////////////////////////////////
  // Function calls.

  /*
   * Stub for immutable PHP calls.
   *
   * @reached:  callphp from TC
   * @context:  func prologue
   */
  TCA immutableBindCallStub;

  /*
   * Use interpreter functions to enter the pre-live ActRec that we place on
   * the stack (along with arguments, the array of arguments to unpack and
   * optionally the array of generics) in a CallUnpack instruction. The last
   * two args specify the total number of inputs, including unpack arguments and
   * generics, and a call flags (see CallFlags).
   *
   * @reached:  callunpack from TC
   * @context:  func prologue
   */
  TCA fcallUnpackHelper;


  /////////////////////////////////////////////////////////////////////////////
  // Interpreter stubs.

  /*
   * Restart execution based on the value of vmpc.  Used, e.g., to resume
   * execution after an InterpOne.
   *
   * Expects that all VM registers are synced.
   *
   * @reached:  jmp from funcBodyHelperThunk
   *            jmp from fcallHelperThunk
   *            call from enterTCHelper
   * @context:  func body
   */
  TCA resumeHelper;

  /*
   * Like resumeHelper, but interpret a basic block first to ensure we make
   * forward progress.
   *
   * interpHelper expects the correct value of vmpc to be in the first argument
   * register and syncs it, whereas interpHelperSyncedPC expects vmpc to be
   * synced a priori.  Both stubs will sync the vmsp and vmfp registers to
   * vmRegs before passing control to the interpreter.
   *
   * @reached:  jmp from TC
   * @context:  func body
   */
  TCA interpHelper;
  TCA interpHelperSyncedPC;

  /*
   * Stubs for each bytecode with the CF flag, which InterpOne the bytecode and
   * then call resumeHelper.
   *
   * These stubs expect rvmfp() and rvmsp() to be live, and rAsm to contain the
   * offset to the bytecode to interpret.
   *
   * @reached:  jmp from TC
   * @context:  func body
   */
  jit::fast_map<Op, TCA> interpOneCFHelpers;


  /////////////////////////////////////////////////////////////////////////////
  // DecRefs.

  /*
   * Expensive, generic decref of a value with an unknown (but known to be
   * refcounted) DataType.
   *
   * The value to be decref'd should be in the first two argument registers
   * (data, type).  All GP registers are saved around the destructor call.
   *
   * @reached:  callfaststub from TC
   * @context:  stub
   */
  TCA decRefGeneric;

  /*
   * Perform generic decrefs of locals on function return.
   *
   * Each freeLocalHelpers[i] is an entry point to a partially-unrolled loop.
   * freeManyLocalsHelper should be used instead when there are more than
   * kNumFreeLocalsHelpers locals.
   *
   * These helpers expect the address of the frame's last local variable (which
   * has the lowest address) to be passed in the second argument register.  The
   * first argument register is ignored.
   *
   * @reached:  vcall from TC
   * @context:  stub
   */
  TCA freeLocalsHelpers[kNumFreeLocalsHelpers];
  TCA freeManyLocalsHelper;


  /////////////////////////////////////////////////////////////////////////////
  // Other stubs.

  /*
   * Enter (or reenter) the TC.
   *
   * This is an assembly stub called from native code to transfer control
   * (back) to jitted PHP code.
   *
   * enterTCExit is the address returned to when we leave the TC.
   */
  void (*enterTCHelper)(TCA start, ActRec* fp, void* tl, Cell* sp,
                        ActRec* firstAR);
  TCA enterTCExit;

  /*
   * Return from this VM nesting level to the previous one.
   *
   * This has the same effect as a leavetc{} instruction---it pops the address
   * of enterTCExit off the stack and transfers control to it.
   *
   * @reached:  phpret from TC
   *            jmp from TC
   * @context:  func body
   */
  TCA callToExit;

  /*
   * Perform dispatch at the end of a catch block.
   *
   * The endCatchHelper passes the current vmfp() to the unwinder to determine
   * the catch trace of the return address of the parent frame.
   *
   * The endCatchStublogueHelper passes the current vmfp() and RIP saved in
   * the stublogue header. Unwinder uses it to determine the catch trace of
   * the return adddress belonging to the same logical vmfp().
   *
   * If the unwinder has set state indicating a return address to jump to, we
   * load vmfp and vmsp and jump there.  Otherwise, we call _Unwind_Resume.
   */
  TCA resumeCPPUnwind;
  TCA endCatchHelper;
  TCA endCatchHelperPast;
  TCA endCatchStublogueHelper;

  /*
   * Service request helper.
   *
   * Packs service request arguments into a struct on the stack before calling
   * the C++ service request handler.
   *
   * @reached:  jmp from TC
   * @context:  func body
   */
  TCA handleSRHelper;

  /////////////////////////////////////////////////////////////////////////////

  /*
   * Emit the full set of unique stubs to `code'.
   */
  void emitAll(CodeCache& code, Debug::DebugInfo& dbg);

  /*
   * Utility for logging stub addresses during startup and registering the gdb
   * symbols.  It's often useful to know where they were when debugging.
   */
  void add(const char* name,
           const CodeCache& code,
           TCA start,
           CodeCache::View view,
           TransLoc loc,
           Debug::DebugInfo& dbg);

  /*
   * If the given address is within one of the registered stubs, return a
   * string indicating which stub and how far in it is: "retHelper+0xfa".
   *
   * Otherwise, return a string representation of the raw address: "0xabcdef".
   */
  std::string describe(TCA addr) const;

private:
  /*
   * Emit all Resumable-related unique stubs to `code'.
   */
  void emitAllResumable(CodeCache& code, Debug::DebugInfo& dbg);

  struct StubRange {
    std::string name;
    TCA start, end;

    bool operator<(const StubRange& other) const {
      return start < other.start;
    }

    bool contains(TCA address) const {
      return start <= address && address < end;
    }
  };

  std::vector<StubRange> m_ranges;
};

////////////////////////////////////////////////////////////////////////////////

/*
 * Registers that are live on entry to an interpOneCFHelper.
 */
RegSet interp_one_cf_regs();

/*
 * Emit code to `v' which jumps to interpHelper with the proper arguments.
 */
void emitInterpReq(Vout& v, SrcKey sk, FPInvOffset spOff);

///////////////////////////////////////////////////////////////////////////////

/*
 * Wrappers around the enterTC*Helper stubs, called from enterTC*().
 */
void enterTCImpl(TCA start);

///////////////////////////////////////////////////////////////////////////////

}}
