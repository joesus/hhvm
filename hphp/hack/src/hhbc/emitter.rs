// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.

use label_rust as label;
use local_rust as local;
use options::Options;

use super::{iterator::Iter, GlobalState};

#[derive(Debug, Default)]
pub struct Emitter {
    /// Options are frozen/const after emitter is constructed
    opts: Options,
    /// State is also frozen and set after closure conversion
    state: GlobalState,
    /// systemlib is part of context, changed externally
    systemlib: bool,
    // the rest is being mutated during emittance
    label_gen: label::Gen,
    local_gen: local::Gen,
    iterator: Iter,

    // dynamic states are exposed because another crate
    // needs to inject the lazy set/get into these slots,
    // accessed via `emit_state()` on Emitter that
    // auto-implemented via the `lazy_emit_state!` macro
    // (each crate such as emit_XYZ only access one of these)
    pub adata_state: DynState,
    pub expression_state: DynState,
    pub statement_state: DynState,
    pub symbol_refs_state: DynState,
}

pub struct StateRef<'a> {
    pub label_gen: &'a mut label::Gen,
    pub local_gen: &'a mut local::Gen,
    pub iterator: &'a mut Iter,
}

impl Emitter {
    pub fn new(opts: Options, state: GlobalState) -> Emitter {
        Emitter {
            opts,
            state,
            ..Default::default()
        }
    }

    pub fn options(&self) -> &Options {
        &self.opts
    }

    /// Destruct the emitter but salvage its options (for use in emitting fatal program).
    pub fn into_options(self) -> Options {
        self.opts
    }

    pub fn context(&self) -> &dyn Context {
        self
    }
    pub fn context_mut(&mut self) -> &mut dyn Context {
        self
    }

    pub fn iterator_mut(&mut self) -> &mut Iter {
        &mut self.iterator
    }

    pub fn label_gen_mut(&mut self) -> &mut label::Gen {
        &mut self.label_gen
    }

    pub fn local_gen_mut(&mut self) -> &mut local::Gen {
        &mut self.local_gen
    }

    pub fn refined_state_mut(&mut self) -> StateRef {
        StateRef {
            label_gen: &mut self.label_gen,
            local_gen: &mut self.local_gen,
            iterator: &mut self.iterator,
        }
    }
}

/// Interface for changing the behavior, exposed to hh_single_compile
pub trait Context {
    fn set_systemlib(&mut self, flag: bool);
    fn systemlib(&self) -> bool;
}

impl Context for Emitter {
    fn set_systemlib(&mut self, flag: bool) {
        self.systemlib = flag;
    }
    fn systemlib(&self) -> bool {
        self.systemlib
    }
}

use std::any::Any;

/// Injects stateful type to emitter without adding dependency to another crate
/// Example:
/// ```
/// struct State {}
/// impl State {
///     fn init() -> Box<dyn std::any::Any> {
///         Box::new(State {})
///     }
/// }
/// env::lazy_emit_state!(adata_state, State, State::init);
/// ```
/// now the crate can call `emit_state_mut` (or `emit_state`),
/// which converts to the create-private type `&mut State` (or `State`).
#[macro_export]
macro_rules! lazy_emit_state {
    ($field: ident, $type: ty, $init: expr) => {
        // Note: if multiple decls or name clashes, do one of:
        // - add explicit name(s) as macro parameter(s)
        // - use crate paste/mashup to create unique trait/method names
        trait LazyState<T> {
            fn emit_state(&self) -> &T;
            fn emit_state_mut(&mut self) -> &mut T;
        }
        impl LazyState<$type> for Emitter {
            fn emit_state(&self) -> &$type {
                self.$field
                    .as_ref()
                    .expect(concat!("uninit'd ", module_path!(), " state"))
                    .downcast_ref::<$type>()
                    .expect(concat!("expected ", module_path!(), " state"))
            }

            fn emit_state_mut(&mut self) -> &mut $type {
                self.$field
                    .get_or_init($init)
                    .downcast_mut::<$type>()
                    .expect(concat!("expected ", module_path!(), " state"))
            }
        }
    };
}

/// The plumbing to make `lazy_emit_state!` macro work,
/// which gives user-friendly access to mutable crate-provided type
#[derive(Debug, Default)]
pub struct DynState(Option<Box<dyn Any>>);
impl DynState {
    pub fn get_or_init(&mut self, init: fn() -> Box<dyn Any>) -> &mut Box<dyn Any> {
        self.0.get_or_insert_with(init)
    }
    pub fn as_ref(&self) -> Option<&Box<dyn Any>> {
        self.0.as_ref()
    }
}
