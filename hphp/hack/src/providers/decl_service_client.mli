(*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *)

(* `t` represents a decl service, i.e. something where you can make requests
for decls and get back answers. Often these requests will block upon IO. *)
type t = {
  hhi_root: Path.t;  (** where does the decl-service keep its hhi files *)
  rpc_get_gconst: string -> (string, Marshal_tools.error) result;
      (** fetches a global const *)
}

(** `init socket addr` will establish a connection to the assumed-running
decl service which is listening on unix domain socket `socket`. It trusts
that the decl service knows to use `addr` as its base addrss. *)
val init :
  decl_sock:Path.t ->
  base_addr:Decl_ipc_ffi_externs.sharedmem_base_address ->
  hhi_root:Path.t ->
  (t, Marshal_tools.error) result

(** `init_inproc` is for when you want to connect to an in-process decl service,
rather than connecting to an already-running out-of-proc decl service. *)
val init_inproc : naming_table:Path.t -> root:Path.t -> hhi_root:Path.t -> t
