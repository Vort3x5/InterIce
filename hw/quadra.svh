// quadra.vh

`ifndef QUADRA_VH
`define QUADRA_VH

typedef logic ck_t; // clock
typedef logic rs_t; // reset
typedef logic dv_t; // data valid

// --------------------------------------------------------------------------------
// I/O precision
// --------------------------------------------------------------------------------

// x in [0,2) -> u1.23
localparam int  X_I =  1;          //         =  1
localparam int  X_F = 23;          //         = 23
localparam int  X_W = X_I + X_F;   //  1 + 23 = 24 (u1.23)

typedef logic [X_W-1:0] x_t;

// y [-2,2) -> s2.23
localparam int  Y_I =  2;          //         =  2
localparam int  Y_F = 23;          //         = 23
localparam int  Y_W = Y_I + Y_F;   //  2 + 23 = 25 (s2.23)

typedef logic signed [Y_W-1:0] y_t;

// --------------------------------------------------------------------------------
// Internal precision:
// --------------------------------------------------------------------------------

localparam int X1_W = 7;
localparam int X2_W = 17;

localparam int A_W  = 21;
localparam int B_W  = 17;
localparam int C_W  = 10;

localparam int SQ_W = 24;

typedef logic [X1_W-1:0] x1_t;
typedef logic [X2_W-1:0] x2_t;

typedef logic signed [A_W-1:0] a_t;
typedef logic signed [B_W-1:0] b_t;
typedef logic signed [C_W-1:0] c_t;

typedef logic [SQ_W-1:0] sq_t;

// <challenge!>
localparam int X1_I = 1;
localparam int X1_F = X1_W - X1_I;

localparam int X2_I = 0;
localparam int X2_F = X_F;

localparam int A_I = 1;
localparam int B_I = 3;
localparam int C_I = 2;

localparam int A_F = A_W - A_I;
localparam int B_F = B_W - B_I;
localparam int C_F = C_W - C_I;

localparam int SQ_I = X2_I + X2_I;
localparam int SQ_F = SQ_W - SQ_I;

localparam int R_F = 4;
localparam int S_I = Y_I;
localparam int S_F = Y_F + R_F;
localparam int S_W = S_I + S_F;

`endif
