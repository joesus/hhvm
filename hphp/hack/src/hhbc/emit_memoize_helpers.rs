// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.
pub mod emit_memoize_helpers {
    use emit_fatal_rust as emit_fatal;
    use hhas_param_rust::HhasParam;
    use instruction_sequence_rust::Instr;
    use local_rust as local;
    use oxidized::{aast::FunParam, pos::Pos};

    fn get_memo_key_list(local: local::Id, index: usize, name: String) -> Vec<Instr> {
        vec![
            Instr::make_instr_getmemokeyl(local::Type::Named(name)),
            Instr::make_instr_setl(local::Type::Unnamed(local + index)),
            Instr::make_instr_popc(),
        ]
    }

    pub fn param_code_sets(params: Vec<HhasParam>, local: local::Id) -> Instr {
        Instr::gather(
            params
                .into_iter()
                .enumerate()
                .map(|(i, param)| get_memo_key_list(local, i, param.name))
                .flatten()
                .collect(),
        )
    }

    pub fn param_code_gets(params: Vec<HhasParam>) -> Instr {
        Instr::gather(
            params
                .into_iter()
                .map(|param| Instr::make_instr_cgetl(local::Type::Named(param.name)))
                .collect(),
        )
    }

    pub fn check_memoize_possible<Ex, Fb, En, Hi>(
        pos: Pos,
        params: &[&FunParam<Ex, Fb, En, Hi>],
        is_method: bool,
    ) -> Result<(), emit_fatal::Error> {
        if params.iter().any(|param| param.is_reference) {
            return emit_fatal::raise_fatal_runtime(
                pos,
                String::from(
                    "<<__Memoize>> cannot be used on functions with args passed by reference",
                ),
            );
        };
        if !is_method && params.iter().any(|param| param.is_variadic) {
            return emit_fatal::raise_fatal_runtime(
                pos,
                String::from("<<__Memoize>> cannot be used on functions with variable arguments"),
            );
        }
        Ok(())
    }
}
