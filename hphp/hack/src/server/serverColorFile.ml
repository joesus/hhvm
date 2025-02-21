(*
 * Copyright (c) 2015, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

let go_quarantined ~(ctx : Provider_context.t) ~(entry : Provider_context.entry)
    =
  try
    let (tast, _errors) =
      Provider_utils.compute_tast_and_errors_quarantined ~ctx ~entry
    in
    Coverage_level.get_levels tast entry.Provider_context.path
  with _ ->
    (* The "Fixme Provider" will throw an exception if the file cannot be found.
     * Let's convert that exception to a plain result. *)
    ( [],
      {
        Coverage_level_defs.checked = 0;
        Coverage_level_defs.partial = 0;
        Coverage_level_defs.unchecked = 0;
      } )
