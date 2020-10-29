// Copyright 2020 Google LLC.
// SPDX-License-Identifier: Apache-2.0

//- @"\"D.svh\"" ref/includes Dsvh
//- Dsvh.node/kind file
`include "D.svh"

//- @"\"A.svh\"" ref/includes Asvh1
//- Asvh1.node/kind file
`include "A.svh"

//- @"\"B.svh\"" ref/includes Bsvh2
//- Bsvh2.node/kind file
`include "B.svh"

//- @E defines/binding _
//- @D ref D
localparam E = D;

//- @F defines/binding _
//- @A ref A
//- @B ref B
localparam F = A + B;
