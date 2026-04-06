`include "quadra.vh"

module quadra
(
    input  x_t x_in,
    output y_t y_out
);
    x1_t x1;
    x2_t x2;
    assign x1 = x_in[X_W-1 : X2_W];
    assign x2 = x_in[X2_W-1 : 0];

    a_t a;
    b_t b;
    c_t c;
    lut u_lut (
        .x1(x1),
        .a (a),
        .b (b),
        .c (c)
    );
    
    sq_t sq;
    square u_square (
        .x2(x2),
        .sq(sq)
    );

    localparam int A_SHIFT  = S_F - A_F;
    localparam int T1_SHIFT = (B_F + X2_F) - S_F;
    localparam int T2_SHIFT = (C_F + SQ_F) - S_F;

    localparam int MULT1_W = B_W + X2_W + 1; 
    localparam int MULT2_W = C_W + SQ_W;

    logic signed [MULT1_W-1:0] t1_mult;
    logic signed [MULT2_W-1:0] t2_mult;

    logic signed [S_W-1:0] a_term;
    logic signed [S_W-1:0] t1_term;
    logic signed [S_W-1:0] t2_term;
    logic signed [S_W-1:0] sum;
    logic signed [S_W-1:0] sum_rnd;

    always_comb begin
        t1_mult = b * $signed({1'b0, x2});
        t2_mult = c * $signed({1'b0, sq});

        a_term  = S_W'($signed(a)) <<< A_SHIFT;
        t1_term = S_W'(t1_mult >>> T1_SHIFT);
        t2_term = S_W'(t2_mult >>> T2_SHIFT);

        sum = a_term + t1_term + t2_term;

        sum_rnd = sum + S_W'(8);
        if (sum[3:0] == 4'b1000) begin
            sum_rnd[4] = 1'b0;
        end

        y_out = y_t'(sum_rnd >>> 4);
    end
endmodule
