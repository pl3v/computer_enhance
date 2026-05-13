import sys

reg_table = [
    ["al", "ax"],  # 000
    ["cl", "cx"],  # 001
    ["dl", "dx"],  # 010
    ["bl", "bx"],  # 011
    ["ah", "sp"],  # 100
    ["ch", "bp"],  # 101
    ["dh", "si"],  # 110
    ["bh", "di"],  # 111
]
rm_mod_table = [
    "[bx + si",
    "[bx + di",
    "[bp + si",
    "[bp + di",
    "[si",
    "[di",
    "[bp",  # NOTE: this is a direct addr when mod == 0b00
    "[bx",
]

MOV_IMM_REG = 0b10110000
MASK_MOV_IMM_REG = 0b11110000


MOV_ACC_MEM = 0b10100010
MASK_MOV_ACC_MEM = 0b11111110


def to_signed(value, bits):
    """Convert an unsigned integer to a signed integer with the given bit width."""
    max_unsigned = 1 << bits
    sign_bit = 1 << (bits - 1)
    x = value - max_unsigned if value & sign_bit else value
    return "-" if x < 0 else "+", abs(x)


def decode_rm(data, i, w):
    mod = (data[i] & 0b11000000) >> 6
    reg = (data[i] & 0b00111000) >> 3
    rm = data[i] & 0b00000111
    if mod == 0b00:
        if rm == 0b110:
            i += 1
            val = data[i]
            if w == 1:
                i += 1
                val += (data[i] << 8) + val
            rm_str = f"[{val}]"
        else:
            rm_str = f"{rm_mod_table[rm]}]"
    elif mod == 0b01:
        i += 1
        sign, lo = to_signed(data[i], 8)
        if lo == 0:
            rm_str = f"{rm_mod_table[rm]}]"
        else:
            rm_str = f"{rm_mod_table[rm]} {sign} {lo}]"
    elif mod == 0b10:
        i += 1
        val = data[i]
        size = 8
        if w == 1:
            i += 1
            val = (data[i] << 8) + val
            size = 16
        rm_str = f"[{val}]"
        sign, b = to_signed(val, size)
        # if b == 0:
        #     rm_str = f"{rm_mod_table[rm]}]"
        # else:
        rm_str = f"{rm_mod_table[rm]} {sign} {b}]"
    else:
        rm_str = f"{reg_table[rm][w]}"  # NOTE: uses reg table cause they the same when r/m == 0b11
    return i, reg, rm_str


def decode_lo_hi(data, i, w):
    immed = data[i]
    if w == 1:
        i += 1
        immed = (data[i] << 8) + immed
    return i, immed


def is_reg_mem(byte):
    instr = {
        0b10001000: "mov",
        0b00000000: "add",
        0b00101000: "sub",
        0b00111000: "cmp",
    }
    byte = byte & 0b11111100
    if byte in instr:
        return instr[byte]
    else:
        return None


def is_reg_mem_imm(data, i):
    special_reg = {
        0b000: "add",  # FIX: needs specific reg
        0b101: "sub",  # FIX: needs specific reg
        0b111: "cmp",  # FIX: needs specific reg
    }
    MOV_MASK = 0b11111110
    OP_MASK = 0b11111100
    # print(data[i], OP_MASK, data[i] & OP_MASK, 0b10000000)
    if data[i] & MOV_MASK == 0b11000110:
        return "mov"
    elif data[i] & OP_MASK == 0b10000000:
        if i >= len(data) - 1:
            return "unknown"
        peek_reg = (data[i + 1] & 0b00111000) >> 3
        return special_reg.get(peek_reg, "uknown")
    else:
        return None


def is_mem_imm_to_acc(byte):
    instr = {
        0b10100000: "mov",
        0b00000100: "add",
        0b00101100: "sub",
        0b00111100: "cmp",
    }
    byte = byte & 0b11111110
    if byte in instr:
        return instr[byte]
    else:
        return None


def is_jmp(byte):
    jump_opcode_map = {
        0b01110100: "je",  # JE / JZ
        0b01111100: "jl",  # JL / JNGE
        0b01111110: "jle",  # JLE / JNG
        0b01110010: "jb",  # JB / JNAE
        0b01110110: "jbe",  # JBE / JNA
        0b01111010: "jp",  # JP / JPE
        0b01110000: "jo",  # JO
        0b01111000: "js",  # JS
        0b01110101: "jne",  # JNE / JNZ
        0b01111101: "jnl",  # JNL / JGE
        0b01111111: "jnle",  # JNLE / JG
        0b01110011: "jnb",  # JNB / JAE
        0b01110111: "jnbe",  # JNBE / JA
        0b01111011: "jnp",  # JNP / JPO
        0b01110001: "jno",  # JNO
        0b01111001: "jns",  # JNS
        0b11100010: "loop",  # LOOP
        0b11100001: "loopz",  # LOOPZ / LOOPE
        0b11100000: "loopnz",  # LOOPNZ / LOOPNE
        0b11100011: "jcxz",  # JCXZ
    }
    return jump_opcode_map.get(byte, None)


def disassemble(fname, data):
    label_table = {}
    i_to_str = {}

    instrs = f";produced by this binary file {fname}\n\nbits 16\n\n"
    i = 0
    while i < len(data):
        # print(i)
        if name := is_reg_mem(data[i]):
            d = (data[i] & 0b10) >> 1
            w = data[i] & 0b01
            i, reg, rm_str = decode_rm(data, i + 1, w)
            dst, src = (
                (reg_table[reg][w], rm_str) if d == 1 else (rm_str, reg_table[reg][w])
            )
            instrs += f"{name} {dst}, {src}\n"

        elif name := is_reg_mem_imm(data, i):
            w = data[i] & 0b1
            sign_ext = (data[i] >> 1) & 0b1

            i, _, rm_str = decode_rm(data, i + 1, w)
            # TODO: what does sign bit extend even do here?
            # like data[i+1] is 2 as it should be so I'm not doing something with the sign extend right
            # print(rm_str, data[i + 1], w, sign_ext)
            i, immed = decode_lo_hi(data, i + 1, w & (~sign_ext))

            size = 16 if w == 1 else 8
            sign, immed = to_signed(immed, size) if sign_ext == 1 else ("+", immed)
            sign = "" if sign == "+" else "-"

            instrs += f"{name} {rm_str}, {sign}{immed}\n"

        elif data[i] & MASK_MOV_IMM_REG == MOV_IMM_REG:
            w = (data[i] & 0b1000) >> 3
            reg = data[i] & 0b111
            i, immed = decode_lo_hi(data, i + 1, w)
            instrs += f"mov {reg_table[reg][w]}, {immed}\n"

        elif name := is_mem_imm_to_acc(data[i]):
            w = data[i] & 0b1
            sign_ext = (data[i] >> 1) & 0b1

            # print(name, i, sign_ext, data[i+1])
            i, addr = decode_lo_hi(data, i + 1, w)

            # NOTE: Cause it can be addr for mem or immed for math ops
            size = 16 if w == 1 else 8
            sign, addr = to_signed(addr, size)
            sign = "" if sign == "+" else "-"

            accum = "ax" if w == 1 else "al"
            instrs += f"{name} {accum}, {sign}{addr}\n"

        # TODO: make this generic, like is_reg_mem_imm
        elif data[i] & MASK_MOV_ACC_MEM == MOV_ACC_MEM:
            w = data[i] & 0b1
            i, addr = decode_lo_hi(data, i + 1, w)
            instrs += f"mov [{addr}], ax\n"

        elif name := is_jmp(data[i]):
            i += 1
            offset = to_signed(data[i], 8)
            pc = i + (-1*offset[1] if offset[0] == "-" else offset[1])
            if pc not in label_table:
                label_table[pc] = f"label_{len(label_table)}"
            instrs += f"{name} {label_table[pc]}\n"

        else:
            print(f"unknown instruction byte %b", data[i])
            break

        # NOTE: this i is where the jump PC offset
        # adds or substracts from so get this i,
        # instead of at top of loop
        i_to_str[i] = len(instrs)
        i += 1

    # NOTE: MUST insert in ascending order of PC 
    # in order to maintain correct s_offset
    label_table = {k: label_table[k] for k in sorted(label_table, key= lambda k: k)}
    s_offset = 0
    for i, label in label_table.items():
        j = i_to_str[i] + s_offset
        tmp = f"\n{label}:\n" 
        instrs = instrs[:j] + tmp + instrs[j:]
        s_offset += len(tmp)

    return instrs


def main():
    fname = sys.argv[1]
    with open(fname, "rb") as f:
        data = f.read()

    instrs = disassemble(fname, data)
    print(instrs)


if __name__ == "__main__":
    main()
