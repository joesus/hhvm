<?hh
/**
 * Copyright (c) 2014, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the "hack" directory of this source tree.
 *
 *
 */

abstract class D
  /* HH_FIXME[4010]*/ /* HH_FIXME[4101] */ extends C {
  public function __construct(): void {
    /* HH_FIXME[4101] */
    parent::__construct();
  }
}

abstract class C<T> {
  protected function __construct(): void {}
}
