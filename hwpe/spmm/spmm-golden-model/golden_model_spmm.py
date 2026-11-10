import numpy as np

def generate_spmm_stimuli():
    # --- 1. CONFIGURAZIONE DEI PARAMETRI ---
    num_rows = 16
    num_cols = 16
    sparsity = 0.6  # Percentuale di zeri nella matrice sparsa A
    
    print(f"[*] Generazione matrice sparsa A ({num_rows}x{num_cols}) con densità {1-sparsity}...")
    
    # Generazione di una matrice casuale con zeri (INT8 tra -8 e 7)
    np.random.seed(42)
    A_dense = np.random.choice([0, 1, 2, -1, -2], size=(num_rows, num_cols), p=[sparsity, (1-sparsity)/4, (1-sparsity)/4, (1-sparsity)/4, (1-sparsity)/4]).astype(np.int8)
    
    # Matrice densa B (INT8)
    B = np.random.randint(-4, 4, size=(num_cols, 4), dtype=np.int8)

    # --- 2. CONVERSIONE IN FORMATO CSR ---
    row_ptr = [0]
    data_a = []
    col_id = []

    for i in range(num_rows):
        nnz_in_row = 0
        for j in range(num_cols):
            val = A_dense[i, j]
            if val != 0:
                data_a.append(int(val))
                col_id.append(int(j))
                nnz_in_row += 1
        row_ptr.append(row_ptr[-1] + nnz_in_row)

    total_nnz = len(data_a)
    print(f"[*] Totale elementi non-zero (NNZ): {total_nnz}")

    # --- 3. CALCOLO GOLDEN MODEL (C = A * B) ---
    C_golden = np.dot(A_dense.astype(np.int32), B.astype(np.int32))
    C_golden = np.clip(C_golden, -128, 127).astype(np.int8)

    # --- 4. ESPORTAZIONE IN FORMATO ESADECIMALE (`stim_data.txt`) ---
    output_filename = "stim_data.txt"
    print(f"[*] Scrittura dei file di stimolo in {output_filename}...")

    with open(output_filename, "w") as f:
        f.write("// --- ROW_PTR ---\n")
        for ptr in row_ptr:
            f.write(f"{ptr:08x}\n")
            
        f.write("// --- COL_ID ---\n")
        for cid in col_id:
            f.write(f"{cid:08x}\n")
            
        f.write("// --- DATA_A (NNZ values) ---\n")
        for val in data_a:
            # Conversione esplicita in int Python per evitare overflow NumPy
            hex_val = int(val) & 0xFF
            f.write(f"{hex_val:08x}\n")
            
        f.write("// --- MATRIX B ---\n")
        for row in B:
            for val in row:
                hex_val = int(val) & 0xFF
                f.write(f"{hex_val:08x}\n")

        f.write("// --- GOLDEN OUTPUT C ---\n")
        for row in C_golden:
            for val in row:
                hex_val = int(val) & 0xFF
                f.write(f"{hex_val:08x}\n")

    # --- 5. ESPORTAZIONE DEI FILE HEADER C (.h) PER LA CARTELLA sw/ ---
    print("[*] Generazione dei file header C (.h)...")

    # tensor_dim.h
    with open("tensor_dim.h", "w") as f:
        f.write("#ifndef TENSOR_DIM_H\n#define TENSOR_DIM_H\n\n")
        f.write(f"#define NUM_ROWS {num_rows}\n")
        f.write(f"#define NUM_COLS {num_cols}\n")
        f.write(f"#define TOTAL_NNZ {total_nnz}\n")
        f.write(f"#define TOTAL_PTRS {len(row_ptr)}\n\n")
        f.write("#endif\n")

    # row_ptr_input.h
    with open("row_ptr_input.h", "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const uint32_t row_ptr_inp[{len(row_ptr)}] = {{\n")
        f.write(", ".join([str(p) for p in row_ptr]) + "\n};\n")

    # col_id_input.h
    with open("col_id_input.h", "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const uint8_t col_id_inp[{len(col_id)}] = {{\n")
        f.write(", ".join([str(c) for c in col_id]) + "\n};\n")

    # a_input.h
    with open("a_input.h", "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const int8_t a_inp[{len(data_a)}] = {{\n")
        f.write(", ".join([str(val) for val in data_a]) + "\n};\n")

    # b_input.h
    with open("b_input.h", "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const int8_t b_inp[{B.size}] = {{\n")
        f.write(", ".join([str(val) for val in B.flatten()]) + "\n};\n")

    # golden.h
    with open("golden.h", "w") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(f"const int golden[{C_golden.size}] = {{\n")
        f.write(", ".join([str(val) for val in C_golden.flatten()]) + "\n};\n")

    print("[+] Golden Model eseguito con successo! File di testo e header .h generati.")

if __name__ == "__main__":
    generate_spmm_stimuli()