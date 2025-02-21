// Copyright (c) 2019, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the "hack" directory of this source tree.

use proc_macro2::TokenStream;
use quote::quote;

pub fn gen_ty_param_bindings(tys: impl Iterator<Item = syn::Ident>) -> TokenStream {
    let bindings = tys.map(|ty| quote! {#ty = #ty, }).collect::<Vec<_>>();
    if bindings.is_empty() {
        quote! {}
    } else {
        quote! {<#(#bindings)*>}
    }
}

pub fn gen_ty_params(tys: impl Iterator<Item = syn::Ident>) -> TokenStream {
    let ty_idents = tys.map(|ty| quote! { #ty, }).collect::<Vec<_>>();
    if ty_idents.is_empty() {
        quote! {}
    } else {
        quote! {<#(#ty_idents)*>}
    }
}

pub fn gen_ty_params_with_self(tys: impl Iterator<Item = syn::Ident>) -> TokenStream {
    let ty_idents = tys.map(|ty| quote! { Self::#ty, }).collect::<Vec<_>>();
    if ty_idents.is_empty() {
        quote! {}
    } else {
        quote! {<#(#ty_idents)*>}
    }
}
