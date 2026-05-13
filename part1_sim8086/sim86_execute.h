u16 Registers[Register_count];

static u16 GenMask(register_access Reg);
static void SetFlags(operation_type Type, u16 a, u16 b, u16 Result);
static u16 EffectiveAddressValue(effective_address_base Base);

static void ExecAsm8086(memory *Memory, segmented_access ByteStart, u32 BytesCount);

static void ExecOpMov(instruction Instruction, memory *Memory);
static void ExecOpAdd(instruction Instruction, memory *Memory);
static void ExecOpSub(instruction Instruction, memory *Memory);
static void ExecOpCmp(instruction Instruction, memory *Memory);
static void ExecOpJmp(instruction Instruction, memory *Memory);
