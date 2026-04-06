module spi_slave (
    input  logic clk,
    input  logic rst,
    input  logic sck,
    input  logic mosi,
    input  logic cs_n,
    output logic miso,
    
    output logic [23:0] x_out = 0,
    output logic        start_out = 0,
    input  logic [24:0] y_in,
    input  logic        done_in
);
    logic [2:0] sck_sync, cs_n_sync, mosi_sync;
    always_ff @(posedge clk) begin
        sck_sync  <= {sck_sync[1:0], sck};
        cs_n_sync <= {cs_n_sync[1:0], cs_n};
        mosi_sync <= {mosi_sync[1:0], mosi};
    end

    wire sck_rise  = (sck_sync[2:1] == 2'b01);
    wire sck_fall  = (sck_sync[2:1] == 2'b10);
    wire cs_active = ~cs_n_sync[1];
    wire cs_end    = (cs_n_sync[2:1] == 2'b01);

    logic [31:0] shift_reg;
    logic [5:0]  bit_cnt;
    logic [7:0]  addr;
    logic        rw;

    always_ff @(posedge clk) begin
        start_out <= 0;
        
        if (rst || !cs_active) begin
            bit_cnt <= 0;
            miso <= 0;
        end else begin
            if (sck_rise) begin
                shift_reg <= {shift_reg[30:0], mosi_sync[1]};
                bit_cnt   <= bit_cnt + 1;
            end
            
            if (sck_fall) begin
                if (bit_cnt == 8) begin
                    addr <= shift_reg[7:1];
                    rw   <= shift_reg[0];
                    if (shift_reg[0] && shift_reg[7:1] == 7'h04) begin
                        shift_reg <= {7'd0, y_in};
                        miso <= 0;
                    end else if (shift_reg[0] && shift_reg[7:1] == 7'h03) begin
                        shift_reg <= {31'd0, done_in};
                        miso <= 0;
                    end else begin
                        miso <= shift_reg[31];
                    end
                end else begin
                    miso <= shift_reg[31];
                end
            end
        end

        if (cs_end) begin
            if (!rw && addr == 7'h01 && bit_cnt == 32) x_out <= shift_reg[23:0];
            if (!rw && addr == 7'h02 && bit_cnt == 16) start_out <= shift_reg[0];
        end
    end
endmodule
