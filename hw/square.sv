`include "quadra.vh"

module square
(
    input  x2_t x2,
    output sq_t sq
);
    always_comb sq = sq_t'((34'({1'b0, x2}) * 34'({1'b0, x2})) >> 22);
endmodule
