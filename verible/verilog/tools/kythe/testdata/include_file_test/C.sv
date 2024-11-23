// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @"\"A.svh\"" ref/includes Asvh
//- Asvh.node/kind file
`include "A.svh"
//- @"\"B.svh\"" ref/includes Bsvh
//- Bsvh.node/kind file
`include "B.svh"

//- @C defines/binding _
//- @A ref A
//- @B ref B
localparam C = A + B;
