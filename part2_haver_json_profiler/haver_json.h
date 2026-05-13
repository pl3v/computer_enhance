#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#define EXIT_FAILURE 1

double atof(const char *);
long atol(const char *);
void exit(int);

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;

typedef double f64;
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

struct AllocOffset {
	u8 *data;
	u64 size;
	u64 capacity;
};

static void AOInit(AllocOffset *a, u64 capacity){
	a->data = (u8 *) calloc(capacity, 100);
	a->capacity = capacity;
	a->size = 0;
}

static u8 *AOAlloc(AllocOffset *a, u64 amount){
	assert(a != NULL);

	if (a->size + amount >= a->capacity) {
		while (a->size + amount >= a->capacity) {
			a->capacity *= 2;
		}
		a->data = (u8 *) realloc(a->data, a->capacity);
		assert(a->data != NULL);
	}

	u8 *ret = a->data + a->size;
	a->size += amount;
	return ret;
}

static void AOFree(AllocOffset *a){
	assert(a != NULL);
	free(a->data);
}

static void *OffsetToPtr(AllocOffset *a, u32 offset)
{
	assert(a != NULL && offset >= 0);
	// assert(((u64) a->data + offset) <= ((u64) a->data + a->size));
	return (void *) (a->data + offset);
}

static u32 PtrToOffset(AllocOffset *a, void *ptr)
{
	assert(a != NULL && ptr != NULL);
	u32 offset = (((u8 *)ptr) - a->data);
	assert(0 <= offset && offset <= a->size);
	return offset;
}

enum TokenTypes {
	ErrorToken,

	// Obj, Arr start
	LeftBrace,
	RightBrace,
	LeftSquare,
	RightSquare,

	// Separators
	Colon,
	Comma,

	// Values
	LiteralTrue,
	LiteralFalse,
	LiteralNull,
	Number,
	NumberFloat,
	String,

	TokenCount,
};

struct Token {
	TokenTypes type;

	union {
		s64 number;
		f64 fraction;
		u32 string_offset;
	};
};

struct Node {
	u32 next_offset;
	u32 prev_offset;
	u32 token_offset;
};

static void PrintTokenList(AllocOffset *a, Node *head);
static TokenTypes CharToType(char c, FILE *fp);
static u32 ExtractTokens(FILE *fp, AllocOffset *a);

enum ValueType {
	ValueNone,

	ValueString,
	ValueNumber,
	ValueFraction,
	ValueObject,
	ValueArray,
	ValueTrue,
	ValueFalse,
	ValueNull,

	ValueCount,
};

struct Object;
struct Array;

struct Value {
	ValueType type;
	union {
		u32 string_offset;
		u64 number;
		f64 fraction;
		u32 object_offset;
		u32 array_head_offset;
	};
};

struct ObjectItem {
	u32 key_offset;
	u32 value_offset;
};

struct Object {
	u32 prev_offset;
	u32 next_offset;
	u32 item_offset;
};

struct Array {
	u32 prev_offset;
	u32 next_offset;
	u32 value_offset;
};

struct ArrayHead {
	u64 size;
	u32 array_offset;
};

struct Json {
	AllocOffset a;
	u32 head_offset;
};

static void ParseValue(AllocOffset *a, u32 value_offset, u32 *node_offset);
static void ParseObject(AllocOffset *a, u32 object_offset, u32 *node_offset);
static void ParseArray(AllocOffset *a, u32 array_offset, u32 *node_offset);

static void ParseValueSep(AllocOffset *a_json, u32 value_offset, AllocOffset *a_token, u32 *node_offset);
static void ParseObjectSep(AllocOffset *a_json, u32 object_offset, AllocOffset *a_token, u32 *node_offset);
static void ParseArraySep(AllocOffset *a_json, u32 array_head_offset, AllocOffset *a_token, u32 *node_offset);

static void PrintValue(FILE *dst, AllocOffset *a, u32 value_offset);
static void PrintObject(FILE *dst, AllocOffset *a, u32 object_offset);
static void PrintArray(FILE *dst, AllocOffset *a, u32 array_head_offset);
static void PrintJson(FILE *dst, Json *json);

// static Object *ParseJson(Alloc *allocator, char *filename);
static void ParseJson(Json *json, char *filename);
