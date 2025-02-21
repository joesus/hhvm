// Copyright (c) Facebook, Inc. and its affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.

#![allow(dead_code)]

mod label_rewriter {
    use env::emitter::Emitter;
    use hhas_param_rust::HhasParam;
    use hhbc_ast_rust::{
        FcallArgs, GenDelegation, Instruct, InstructCall, InstructControlFlow, InstructIterator,
        InstructMisc,
    };
    use instruction_sequence_rust::instr_seq;
    use instruction_sequence_rust::Instr;
    use label_rust::{Id, Label};

    use std::collections::{HashMap, HashSet};

    fn create_label_to_offset_map(instrseq: &Instr) -> HashMap<Id, usize> {
        let mut folder = |(i, mut map): (usize, HashMap<Id, usize>), instr: &Instruct| match instr {
            Instruct::ILabel(l) => {
                if let Ok(id) = Label::id(l) {
                    map.insert(id, i);
                    (i, map)
                } else {
                    panic!("Label should've been rewritten by this point")
                }
            }
            _ => (i + 1, map),
        };
        instr_seq::fold_left(instrseq, &mut folder, (0, HashMap::new())).1
    }

    fn lookup_def(l: &Id, defs: &HashMap<Id, usize>) -> usize {
        match defs.get(l) {
            Some(ix) => *ix,
            None => panic!("lookup_def: label missing"),
        }
    }

    fn get_regular_labels(instr: &Instruct) -> Vec<Label> {
        use GenDelegation::*;
        use Instruct::*;
        use InstructCall::*;
        use InstructControlFlow::*;
        use InstructIterator::*;
        use InstructMisc::*;
        match instr {
            IIterator(IterInit(_, l))
            | IIterator(IterNext(_, l))
            | IIterator(IterBreak(l, _))
            | IGenDelegation(YieldFromDelegate(_, l))
            | IMisc(MemoGet(l, _))
            | IContFlow(Jmp(l))
            | IContFlow(JmpNS(l))
            | IContFlow(JmpZ(l))
            | IContFlow(JmpNZ(l))
            | ICall(FCall(FcallArgs(_, _, _, _, Some(l))))
            | ICall(FCallClsMethod(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallClsMethodD(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallClsMethodS(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallClsMethodSD(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallFunc(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallFuncD(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallObjMethod(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallObjMethodD(FcallArgs(_, _, _, _, Some(l)), _, _)) => vec![l.clone()],
            IContFlow(Switch(_, _, ls)) => ls.to_vec(),
            IContFlow(SSwitch(pairs)) => pairs.iter().map(|x| x.1.clone()).collect::<Vec<_>>(),
            IMisc(MemoGetEager(l1, l2, _)) => vec![l1.clone(), l2.clone()],
            _ => vec![],
        }
    }

    fn create_label_ref_map(
        defs: &HashMap<Id, usize>,
        params: &Vec<HhasParam>,
        body: &Instr,
    ) -> (HashSet<Id>, HashMap<Id, usize>) {
        let process_ref =
            |(mut n, (mut used, mut refs)): (usize, (HashSet<Id>, HashMap<Id, usize>)),
             l: &Label| {
                if let Ok(id) = Label::id(l) {
                    let ix = lookup_def(&id, defs);
                    if !refs.contains_key(&ix) {
                        used.insert(id);
                        refs.insert(ix, n);
                        n += 1;
                    }
                    (n, (used, refs))
                } else {
                    panic!("Label should've been rewritten by this point")
                }
            };
        let gather_using = |acc: (usize, (HashSet<Id>, HashMap<Id, usize>)),
                            instrseq: &Instr,
                            get_labels: fn(&Instruct) -> Vec<Label>| {
            let mut folder = |acc: (usize, (HashSet<Id>, HashMap<Id, usize>)), instr: &Instruct| {
                (get_labels(instr)).iter().fold(acc, process_ref)
            };
            instr_seq::fold_left(instrseq, &mut folder, acc)
        };
        let init = gather_using(
            (0, (HashSet::new(), HashMap::new())),
            body,
            get_regular_labels,
        );
        let (_, map) = params.iter().fold(
            init,
            |acc: (usize, (HashSet<Id>, HashMap<Id, usize>)), param: &HhasParam| match &param
                .default_value
            {
                None => acc,
                Some((l, _)) => process_ref(acc, &l),
            },
        );
        map
    }

    fn relabel_instr<F>(instr: &mut Instruct, relabel: &mut F)
    where
        F: FnMut(&mut Label),
    {
        use GenDelegation::*;
        use Instruct::*;
        use InstructCall::*;
        use InstructControlFlow::*;
        use InstructIterator::*;
        use InstructMisc::*;
        match instr {
            IIterator(IterInit(_, l))
            | IIterator(IterNext(_, l))
            | IIterator(IterBreak(l, _))
            | IGenDelegation(YieldFromDelegate(_, l))
            | ICall(FCall(FcallArgs(_, _, _, _, Some(l))))
            | ICall(FCallClsMethod(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallClsMethodD(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallClsMethodS(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallClsMethodSD(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallFunc(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallFuncD(FcallArgs(_, _, _, _, Some(l)), _))
            | ICall(FCallObjMethod(FcallArgs(_, _, _, _, Some(l)), _, _))
            | ICall(FCallObjMethodD(FcallArgs(_, _, _, _, Some(l)), _, _))
            | IContFlow(Jmp(l))
            | IContFlow(JmpNS(l))
            | IContFlow(JmpZ(l))
            | IContFlow(JmpNZ(l))
            | IMisc(MemoGet(l, _))
            | ILabel(l) => relabel(l),
            IContFlow(Switch(_, _, ll)) => ll.iter_mut().for_each(|l| relabel(l)),
            IContFlow(SSwitch(pairs)) => pairs.iter_mut().for_each(|(_, l)| relabel(l)),
            IMisc(MemoGetEager(l1, l2, _)) => {
                relabel(l1);
                relabel(l2);
            }
            _ => (),
        }
    }

    fn rewrite_params_and_body(
        defs: &HashMap<Id, usize>,
        used: &HashSet<Id>,
        refs: &HashMap<Id, usize>,
        params: &mut Vec<HhasParam>,
        body: &mut Instr,
    ) {
        let relabel_id = |id: &mut Id| {
            *id = *refs
                .get(&lookup_def(&id, defs))
                .expect("relabel_instrseq: offset not in refs")
        };
        let relabel_define_label_id = |id: &mut Id| {
            if used.contains(&id) {
                *id = refs.get(&lookup_def(&id, defs)).map(|x| *x).unwrap_or(*id);
            }
        };
        let mut rewrite_instr = |instr: &mut Instruct| {
            if let Instruct::ILabel(l) = instr {
                l.map_mut(relabel_define_label_id);
            } else {
                relabel_instr(instr, &mut |l| l.map_mut(relabel_id));
            }
        };
        let rewrite_param = |param: &mut HhasParam| {
            if let Some((l, _)) = &mut param.default_value {
                l.map_mut(relabel_id);
            }
        };
        params.iter_mut().for_each(|param| rewrite_param(param));
        instr_seq::map_mut(body, &mut rewrite_instr);
    }

    pub fn relabel_function(params: &mut Vec<HhasParam>, body: &mut Instr) {
        let defs = create_label_to_offset_map(body);
        let (used, refs) = create_label_ref_map(&defs, &params, body);
        rewrite_params_and_body(&defs, &used, &refs, params, body)
    }

    pub fn clone_with_fresh_regular_labels(emitter: &mut Emitter, block: &mut Instr) {
        let mut folder =
            |(mut regular, mut named): (HashMap<Id, Label>, HashMap<String, Label>),
             instr: &Instruct| {
                match instr {
                    Instruct::ILabel(Label::Regular(id)) => {
                        regular.insert(*id, emitter.label_gen_mut().next_regular());
                    }
                    Instruct::ILabel(Label::Named(name)) => {
                        named.insert(name.to_string(), emitter.label_gen_mut().next_regular());
                    }
                    _ => (),
                }
                (regular, named)
            };
        let (regular_labels, named_labels) =
            instr_seq::fold_left(block, &mut folder, (HashMap::new(), HashMap::new()));

        if !regular_labels.is_empty() || !named_labels.is_empty() {
            let relabel = |l: &mut Label| {
                let lopt = match l {
                    Label::Regular(id) => regular_labels.get(id),
                    Label::Named(name) => named_labels.get(name),
                    _ => None,
                };
                lopt.unwrap_or(l).to_owned()
            };
            instr_seq::map_mut(block, &mut |instr| {
                relabel_instr(instr, &mut |l| *l = relabel(l))
            })
        }
    }
}
