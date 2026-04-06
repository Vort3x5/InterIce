module top (
    input  logic clk,
    input  logic sck,
    input  logic mosi,
    input  logic cs_n,
    output logic miso
);
    logic rst = 0;
    
    logic [23:0] x_data;
    logic        start;
    logic [24:0] y_data;
    logic        done;

    spi_slave spi_inst (
        .clk(clk),
        .rst(rst),
        .sck(sck),
        .mosi(mosi),
        .cs_n(cs_n),
        .miso(miso),
        .x_out(x_data),
        .start_out(start),
        .y_in(y_data),
        .done_in(done)
    );

    quadra quadra_inst (
        .clk(clk),
        .rst(rst),
        .x(x_data),
        .x_dv(start),
        .y(y_data),
        .y_dv(done)
    );
endmodule
