static u16 GenMask(register_access Reg)
{
    // NOTE: upshift by 8 moves 0xff to 0xff00
    return Reg.Count == 2 ? 0xffff : 0xff << (8*(Reg.Offset&1));
}

static void SetFlags(operation_type Type, u16 a, u16 b, u16 Result)
{
    Registers[Register_flags] = 0;

    // Carry flag == 1
    if (Type == Op_add) {
        Registers[Register_flags] |= (((((u32)a)+b) >> 16) & 1);
    } else if (Type == Op_sub || Type == Op_cmp) {
        // a < b when a-b (assuming unsigned I think)
        Registers[Register_flags] |= (a < b);
    }

    // Parity flag == 3
    auto Par = 0;
    auto tmp = Result;
    for (u8 i = 0; i < 16; i++) {
        Par += tmp & 1;
        tmp >>= 1;
    }
    Registers[Register_flags] |= (Par % 2 == 0) << 3;

    // Auxiliary flag == 5
    if (Type == Op_add) {
        Registers[Register_flags] |= (((((u16)(a & 0xf)) + (b & 0xf)) >> 8) & 1) << 5;
    } else if (Type == Op_sub || Type == Op_cmp) {
        Registers[Register_flags] |= ((a & 0xf) < (b & 0xf)) << 5;
    }

    // Zero flag == 7
    Registers[Register_flags] |= (Result == 0) << 7;

    // Sign flag == 8
    Registers[Register_flags] |= ((Result & 0x8000) >> 15) << 8;

    // Overflow flag == 12
    if (Type == Op_add) {
        Registers[Register_flags] |= (((~(a ^ b) & (a ^ Result)) & 0x8000) != 0) << 12;
    } else if (Type == Op_sub || Type == Op_cmp) {
        Registers[Register_flags] |= ((((a ^ b) & (a ^ Result)) & 0x8000) != 0) << 12;
    }
}

static u16 EffectiveAddressValue(effective_address_base Base)
{
    switch (Base) {
    case EffectiveAddress_direct:
        return 0;
    case EffectiveAddress_bx_si:
        return Registers[Register_b] + Registers[Register_si];
    case EffectiveAddress_bx_di:
        return Registers[Register_b] + Registers[Register_di];
    case EffectiveAddress_bp_si:
        return Registers[Register_bp] + Registers[Register_si];
        break;
    case EffectiveAddress_bp_di:
        return Registers[Register_bp] + Registers[Register_di];
        break;
    case EffectiveAddress_si:
        return Registers[Register_si];
        break;
    case EffectiveAddress_di:
        return Registers[Register_di];
        break;
    case EffectiveAddress_bp:
        return Registers[Register_bp];
        break;
    case EffectiveAddress_bx:
        return Registers[Register_b];
    default:
        fprintf(stdout, "Effective addr enum member not implemented!\n");
        exit(EXIT_FAILURE);
        break;
    }
}

static void ExecAsm8086(memory *Memory, segmented_access ByteStart, u32 BytesCount)
{
    segmented_access At = ByteStart;
    Registers[Register_ip] = At.SegmentBase + At.SegmentOffset;
    u32 ByteEnd = At.SegmentBase + At.SegmentOffset + BytesCount;

    disasm_context Context = DefaultDisAsmContext();

    while (Registers[Register_ip] < ByteEnd)
    {
        instruction Instruction = DecodeInstruction(&Context, Memory, &At);
        Registers[Register_ip] += Instruction.Size;

        switch (Instruction.Op) {
            case Op_mov:
                ExecOpMov(Instruction, Memory);
                break;
            case Op_add:
                ExecOpAdd(Instruction, Memory);
                break;
            case Op_sub:
                ExecOpSub(Instruction, Memory);
                break;
            case Op_cmp:
                ExecOpCmp(Instruction, Memory);
                break;
            case Op_jne:
            case Op_je:
            case Op_jb:
            case Op_jp:
            case Op_loopnz:
            case Op_loop:
                ExecOpJmp(Instruction, Memory);
                At.SegmentOffset = Registers[Register_ip];
                break;
            default:
                PrintInstruction(Instruction, stderr);
                fprintf(stderr, "operation not implemented!\n");
                goto err_exit;
        }
    }
    err_exit:

    fprintf(stdout, "\n\nFinal registers:\n");
    for (u32 i = 1; i < Register_count-1; ++i) {
        if (Registers[i] == 0) {
            continue;
        }
        register_access name = {(register_index) i,1,2};
        fprintf(stdout, "\t%s: 0x%04x (%d)\n", GetRegName(name), Registers[i], Registers[i]);
    }

    register_access name = {(register_index) Register_flags,1,2};
    fprintf(stdout, "\t%s: ", GetRegName(name));
    PrintFlags(Registers[Register_flags], stdout);
    fprintf(stdout, "\n");
}

static void ExecOpMov(instruction Instruction, memory *Memory)
{
    auto dst = Instruction.Operands[0];
    auto src = Instruction.Operands[1];

    u32 LoadAddr;
    u32 val;
    switch (src.Type) {
        case Operand_Immediate: 
            val = src.ImmediateU32;
            break;

        case Operand_RelativeImmediate:
            val = src.ImmediateS32;
            break;

        case Operand_Register:
            val = Registers[src.Register.Index] & GenMask(src.Register);
            break;

        case Operand_Memory:
            LoadAddr = GetAbsoluteAddressOf(Registers[src.Address.Segment], EffectiveAddressValue(src.Address.Base), src.Address.Displacement);
            val = (ReadMemory(Memory, LoadAddr+1) << 8) + ReadMemory(Memory, LoadAddr);
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }

    u32 OldData;
    u32 StoreAddr;
    switch (dst.Type) { 
        case Operand_Immediate: 
            printf("WARN: moving to immmed val");
            break;

        case Operand_RelativeImmediate:
            printf("WARN: moving to immmed val");
            break;

        case Operand_Register:
            OldData = Registers[dst.Register.Index];
            if (dst.Register.Count == 2){
                Registers[dst.Register.Index] = val;
            }else{
                Registers[dst.Register.Index] &= 0xff00 >> (8*((dst.Register.Offset&1)));
                Registers[dst.Register.Index] |= val << (8*(dst.Register.Offset&1));
            }
            break;

        case Operand_Memory:
            StoreAddr = GetAbsoluteAddressOf(Registers[dst.Address.Segment], EffectiveAddressValue(dst.Address.Base), dst.Address.Displacement);
            WriteMemory(Memory, StoreAddr, 0xff & val);
            WriteMemory(Memory, StoreAddr+1, 0xff & (val >> 8));
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }
    // example: mov ax, 8738 ; ax:0x0->0x2222 
    PrintInstruction(Instruction, stdout);
    register_access name = {(register_index) dst.Register.Index,1,2};
    fprintf(stdout, " ; %s:0x%x->0x%x ; ip:0x%x->0x%x\n", GetRegName(name), OldData, Registers[dst.Register.Index], Registers[Register_ip]-Instruction.Size, Registers[Register_ip]+Instruction.Size);
}


static void ExecOpAdd(instruction Instruction, memory *Memory)
{
    auto dst = Instruction.Operands[0];
    auto src = Instruction.Operands[1];

    u32 LoadAddr;
    u16 val;
    switch (src.Type) {
        case Operand_Immediate: 
            val = src.ImmediateU32;
            break;

        case Operand_RelativeImmediate:
            val = src.ImmediateS32;
            break;

        case Operand_Register:
            val = Registers[src.Register.Index] & GenMask(src.Register);
            break;

        case Operand_Memory:
            LoadAddr = GetAbsoluteAddressOf(Registers[src.Address.Segment], EffectiveAddressValue(src.Address.Base), src.Address.Displacement);
            val = (ReadMemory(Memory, LoadAddr+1) << 8) + ReadMemory(Memory, LoadAddr);
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }

    u16 LeftVal;
    u16 RightVal;
    u32 StoreAddr;
    switch (dst.Type) { 
        case Operand_Immediate: 
            printf("WARN: moving to immmed val");
            break;

        case Operand_RelativeImmediate:
            printf("WARN: moving to immmed val");
            break;

        case Operand_Register:
            LeftVal = Registers[dst.Register.Index];
            if (dst.Register.Count == 2){
                Registers[dst.Register.Index] += val;
                RightVal = val;
            }else{
                Registers[dst.Register.Index] += val << (8*(dst.Register.Offset&1));
                RightVal = val << (8*(dst.Register.Offset&1));
            }
            break;

        case Operand_Memory:
            StoreAddr = GetAbsoluteAddressOf(Registers[dst.Address.Segment], EffectiveAddressValue(dst.Address.Base), dst.Address.Displacement);
            WriteMemory(Memory, StoreAddr, 0xff & val);
            WriteMemory(Memory, StoreAddr+1, 0xff & (val >> 8));
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }

    SetFlags(Instruction.Op, LeftVal, RightVal, Registers[dst.Register.Index]);

    PrintInstruction(Instruction, stdout);
    register_access name = {(register_index) dst.Register.Index,1,2};
    fprintf(stdout, " ; %s:0x%x->0x%x ; ip:0x%x->0x%x ; flags-> ", GetRegName(name), LeftVal, Registers[dst.Register.Index], Registers[Register_ip]-Instruction.Size, Registers[Register_ip]+Instruction.Size);
    PrintFlags(Registers[Register_flags], stdout);
    fprintf(stdout, "\n");
}

static void ExecOpSub(instruction Instruction, memory *Memory)
{
    auto dst = Instruction.Operands[0];
    auto src = Instruction.Operands[1];

    u32 LoadAddr;
    u16 val;
    switch (src.Type) {
        case Operand_Immediate: 
            val = src.ImmediateU32;
            break;

        case Operand_RelativeImmediate:
            val = src.ImmediateS32;
            break;

        case Operand_Register:
            val = Registers[src.Register.Index] & GenMask(src.Register);
            break;

        case Operand_Memory:
            LoadAddr = GetAbsoluteAddressOf(Registers[src.Address.Segment], EffectiveAddressValue(src.Address.Base), src.Address.Displacement);
            val = (ReadMemory(Memory, LoadAddr+1) << 8) + ReadMemory(Memory, LoadAddr);
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }

    u32 StoreAddr;
    u16 LeftVal;
    u16 RightVal;
    switch (dst.Type) { 
        case Operand_Immediate: 
            printf("WARN: moving to immmed val");
            break;

        case Operand_RelativeImmediate:
            printf("WARN: moving to immmed val");
            break;

        case Operand_Register:
            LeftVal = Registers[dst.Register.Index];
            if (dst.Register.Count == 2){
                Registers[dst.Register.Index] -= val;
                RightVal = val;
            }else{
                Registers[dst.Register.Index] -= val << (8*(dst.Register.Offset&1));
                RightVal = val << (8*(dst.Register.Offset&1));
            }
            break;

        case Operand_Memory:
            StoreAddr = GetAbsoluteAddressOf(Registers[dst.Address.Segment], EffectiveAddressValue(dst.Address.Base), dst.Address.Displacement);
            WriteMemory(Memory, StoreAddr, 0xff & val);
            WriteMemory(Memory, StoreAddr+1, 0xff & (val >> 8));

            break;
        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }
    SetFlags(Instruction.Op, LeftVal, RightVal, Registers[dst.Register.Index]);

    PrintInstruction(Instruction, stdout);
    register_access name = {(register_index) dst.Register.Index,1,2};
    fprintf(stdout, " ; %s:0x%x->0x%x ; ip:0x%x->0x%x ; flags-> ", GetRegName(name), LeftVal, Registers[dst.Register.Index], Registers[Register_ip]-Instruction.Size, Registers[Register_ip]+Instruction.Size);
    PrintFlags(Registers[Register_flags], stdout);
    fprintf(stdout, "\n");
}

static void ExecOpCmp(instruction Instruction, memory *Memory)
{
    auto dst = Instruction.Operands[0];
    auto src = Instruction.Operands[1];

    u32 LoadAddr;
    u16 val;
    switch (src.Type) {
        case Operand_Immediate: 
            val = src.ImmediateU32;
            break;

        case Operand_RelativeImmediate:
            val = src.ImmediateS32;
            break;

        case Operand_Register:
            val = Registers[src.Register.Index] & GenMask(src.Register);
            break;

        case Operand_Memory:
            LoadAddr = GetAbsoluteAddressOf(Registers[src.Address.Segment], EffectiveAddressValue(src.Address.Base), src.Address.Displacement);
            val = (ReadMemory(Memory, LoadAddr+1) << 8) + ReadMemory(Memory, LoadAddr);
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }

    u32 StoreAddr;
    u16 LeftVal;
    u16 RightVal;
    u16 tmp;
    switch (dst.Type) {
        case Operand_Immediate:
            printf("WARN: moving to immmed val");
            break;

        case Operand_RelativeImmediate:
            printf("WARN: moving to immmed val");
            break;

        case Operand_Register:
            LeftVal = Registers[dst.Register.Index];
            tmp = LeftVal;
            if (dst.Register.Count == 2){
                tmp -= val;
                RightVal = val;
            }else{
                tmp -= val << (8*(dst.Register.Offset&1));
                RightVal = val << (8*(dst.Register.Offset&1));
            }
            break;

        case Operand_Memory:
            StoreAddr = GetAbsoluteAddressOf(Registers[dst.Address.Segment], EffectiveAddressValue(dst.Address.Base), dst.Address.Displacement);
            WriteMemory(Memory, StoreAddr, 0xff & val);
            WriteMemory(Memory, StoreAddr+1, 0xff & (val >> 8));
            break;

        case Operand_None:
            printf("No operand type found\n");
            exit(EXIT_FAILURE);
    }
    SetFlags(Instruction.Op, LeftVal, RightVal, tmp);

    PrintInstruction(Instruction, stdout);
    register_access name = {(register_index) dst.Register.Index,1,2};
    fprintf(stdout, " ; ip:0x%x->0x%x ; flags-> ", Registers[Register_ip]-Instruction.Size, Registers[Register_ip]+Instruction.Size);
    PrintFlags(Registers[Register_flags], stdout);
    fprintf(stdout, "\n");
}

static void ExecOpJmp(instruction Instruction, memory *Memory)
{
    s16 JmpIp = Instruction.Operands[0].ImmediateS32;

    Registers[Register_ip] -= Instruction.Size;
    u16 OldIp = Registers[Register_ip];

    switch (Instruction.Op) {
        case Op_jne:
            if (((Registers[Register_flags] >> 7)&1) == 0) {
                Registers[Register_ip] += JmpIp ;
            }
            break;
        case Op_je:
            if ((Registers[Register_flags] >> 7)&1) {
                Registers[Register_ip] += JmpIp ;
            }
            break;
        case Op_jb:
            // TODO: cannot do cause I don't have carry flag implemented
            if ((Registers[Register_flags])&1) {
                Registers[Register_ip] += JmpIp ;
            }
            break;
        case Op_jp:
            if ((Registers[Register_flags] >> 3)&1) {
                Registers[Register_ip] += JmpIp ;
            }
            break;
        case Op_loopnz:
            // <https://www.felixcloutier.com/x86/loop:loopcc>
            if (--Registers[Register_c] != 0) {
                Registers[Register_ip] += JmpIp ;
            }
            break;
        default:
            fprintf(stdout, "either not a jump or a jump that is not implemented!\n");
            break;
    }

    // NOTE(xypp3): if jump does not occur then walk forward len of jmp instruction
    if (OldIp == Registers[Register_ip]) {
        Registers[Register_ip] += Instruction.Size;
    }

    PrintInstruction(Instruction, stdout);
    fprintf(stdout, " ; ip:0x%x->0x%x ; flags-> ", OldIp, Registers[Register_ip]);
    PrintFlags(Registers[Register_flags], stdout);
    fprintf(stdout, "\n");
}
