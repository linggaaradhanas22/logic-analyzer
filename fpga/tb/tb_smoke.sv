`timescale 1ns/1ps

module tb_smoke;
  integer vector_fd;
  integer result_fd;
  integer scanned;
  integer actual_value;
  integer expected_value;
  string vector_path;
  string result_path;

  initial begin
    if (!$value$plusargs("VECTOR_FILE=%s", vector_path)) begin
      $fatal(1, "VECTOR_FILE plusarg is required");
    end
    if (!$value$plusargs("RESULT_FILE=%s", result_path)) begin
      $fatal(1, "RESULT_FILE plusarg is required");
    end
    vector_fd = $fopen(vector_path, "r");
    result_fd = $fopen(result_path, "w");
    if (vector_fd == 0 || result_fd == 0) begin
      $fatal(1, "vector/result file could not be opened");
    end
    scanned = $fscanf(vector_fd, "%d %d", actual_value, expected_value);
    if (scanned != 2 || actual_value != expected_value) begin
      $fdisplay(result_fd, "FAIL: vector/result mismatch");
      $fclose(vector_fd);
      $fclose(result_fd);
      $fatal(1, "vector/result mismatch");
    end
    $fdisplay(result_fd, "PASS: tb_smoke");
    $fclose(vector_fd);
    $fclose(result_fd);
    $display("PASS: tb_smoke");
    $finish;
  end
endmodule
