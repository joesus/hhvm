(*
 * Copyright (c) 2019, Facebook, Inc.
 * All rights reserved.
 *)

(* Put any initialization code necessary here *)
let initialize () : unit = ()

(* Provide top ranked completion items and modify them inplace in the original list *)
let rank_autocomplete_result
    ~(query_text : string)
    ~(results : AutocompleteTypes.complete_autocomplete_result list)
    ~(max_results : int)
    ~(context : SearchUtils.autocomplete_type option)
    ~(kind_filter : SearchUtils.si_kind option)
    ~(ranking_context : AutocompleteTypes.autocomplete_ranking_context) :
    AutocompleteTypes.complete_autocomplete_result list =
  let _ = query_text in
  let _ = max_results in
  let _ = context in
  let _ = kind_filter in
  let _ = ranking_context in
  results

let log_ranked_autocomplete
    ~(sienv : SearchUtils.si_env)
    ~(results : int)
    ~(context : SearchUtils.autocomplete_type option)
    ~(start_time : float) : unit =
  let _ = sienv in
  let _ = results in
  let _ = context in
  let _ = start_time in
  ()
