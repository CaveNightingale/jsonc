#ifndef _CAVEJSONC_H
#define _CAVEJSONC_H
#include <stdlib.h>
/**
 * jsonc对该字符串生命周期的控制
 */
typedef enum cave_jsonc_string_lifecycle_control {
	/**
	 * jsonc 不处理任何分配和释放
	 */
	CAVE_JSONC_STRING_LIFECYCLE_NONE,
	/**
	 * jsonc 仅分配，不释放
	 */
	CAVE_JSONC_STRING_LIFECYCLE_ALLOC,
	/**
	 * jsonc 仅释放，不分配
	 */
	CAVE_JSONC_STRING_LIFECYCLE_FREE,
	/**
	 * jsonc 分配和释放
	 */
	CAVE_JSONC_STRING_LIFECYCLE_ALL,
} cave_jsonc_string_lifecycle_control;

/**
 * 表示字符在文件中的位置
 */
typedef struct cave_jsonc_position {
	size_t row, cols, index;
} cave_jsonc_position;

/**
 * 用于表示一个utf-8字符串
 * value的末尾有\0但不计入length
 */
typedef struct _cave_jsonc_string {
	/**
	 * C语言格式的\0结尾的字符串，但不保证中间没有\0
	 */
	char *value;
	/**
	 * 字符串的长度
	 */
	size_t length;
	/**
	 * 字符串内存生命周期的管理
	 */
	int free;
} *cave_jsonc_string;

/**
 * @deprecated 内部使用，用于表示数字对应的形式是否被计算
 */
typedef enum cave_jsonc_number_flag {
	CAVE_JSONC_NUM_FVAL = 1,
	CAVE_JSONC_NUM_IVAL = 2,
	CAVE_JSONC_NUM_RAW = 4,
} cave_jsonc_number_flag;

/**
 * @deprecated 内部使用，用于表示数字
 */
typedef struct _cave_jsonc_number {
	struct _cave_jsonc_string *raw;
	double fval;
	long long ival;
	char flag;
} *cave_jsonc_number;

/**
 * 用来表达数组
 */
typedef struct _cave_jsonc_array {
	/**
	 * 一个cave_jsonc_value数组
	 */
	struct _cave_jsonc_value **values;
	/**
	 * 数组长度
	 */
	size_t length;
} *cave_jsonc_array;

/**
 * 一个对象中的键值对
 * 同时只能出现在一个对象中
 */
typedef struct _cave_jsonc_kvpair {
	/**
	 * 键值对中键所处的位置
	 */
	cave_jsonc_position position;
	/**
	 * 键值对中的键
	 */
	cave_jsonc_string key;
	/**
	 * 键值对所处在的对象
	 */
	struct _cave_jsonc_object *object;
	/**
	 * 键值对中的值
	 */
	struct _cave_jsonc_value *value;
	/**
	 * 链表结构
	 */
	struct _cave_jsonc_kvpair *prev, *next;
} *cave_jsonc_kvpair;

/**
 * 用来表示一个json对象
 */
typedef struct _cave_jsonc_object {
	/**
	 * 键值对链表
	 */
	cave_jsonc_kvpair head, tail;
	/**
	 * 所属值
	 */
	struct _cave_jsonc_value *value;
} *cave_jsonc_object;

/**
 * 一个json文档，它负责内存分配和回收和错误记录
 */
typedef struct _cave_jsonc_document {
	/**
	 * 根节点和分配的所有值的链表
	 */
	struct _cave_jsonc_value *root, *all_allocated;
	/**
	 * 错误的链表
	 */
	struct _cave_jsonc_error *error_head, *error_tail;
	/**
	 * 记录是否有致命错误
	 */
	int fatal;
} *cave_jsonc_document;

typedef int cave_jsonc_boolean;

typedef enum cave_jsonc_type {
	CAVE_JSONC_UNDEFINED,
	CAVE_JSONC_NULL,
	CAVE_JSONC_BOOLEAN,
	CAVE_JSONC_NUMBER,
	CAVE_JSONC_STRING,
	CAVE_JSONC_OBJECT,
	CAVE_JSONC_ARRAY,
} cave_jsonc_type;

typedef struct _cave_jsonc_value {
	cave_jsonc_document document;
	cave_jsonc_position position;
	cave_jsonc_type type;
	struct _cave_jsonc_value *prev, *next;
	union {
		cave_jsonc_object object;
		cave_jsonc_array array;
		cave_jsonc_string string;
		cave_jsonc_number number;
		cave_jsonc_boolean boolean;
	} value;
} *cave_jsonc_value;

typedef struct _cave_jsonc_error {
	int fatal;
	const char *message;
	cave_jsonc_position position;	
	struct _cave_jsonc_error *next;
} *cave_jsonc_error;

cave_jsonc_document cave_jsonc_create_document(void);
void cave_jsonc_release_all_nodes_in_document(cave_jsonc_document doc);
void cave_jsonc_transform_node_document(cave_jsonc_value value, cave_jsonc_document target);
void cave_jsonc_release_document(cave_jsonc_document doc);

cave_jsonc_value cave_jsonc_get_document_root(cave_jsonc_document doc);
void cave_jsonc_set_document_root(cave_jsonc_document doc, cave_jsonc_value root);
cave_jsonc_value cave_jsonc_create_null_value(cave_jsonc_document doc);
cave_jsonc_value cave_jsonc_create_boolean_value(cave_jsonc_document doc, int value);
cave_jsonc_value cave_jsonc_create_number_value(cave_jsonc_document doc, const char *r, int lifecycle);
cave_jsonc_value cave_jsonc_create_integer_value(cave_jsonc_document doc, long long i);
cave_jsonc_value cave_jsonc_create_double_value(cave_jsonc_document doc, double f);
cave_jsonc_value cave_jsonc_create_null_termined_string_value(cave_jsonc_document doc, const char *s, int lifecycle);
cave_jsonc_value cave_jsonc_create_string_value(cave_jsonc_document doc, const char *s, size_t length, int lifecycle);
cave_jsonc_value cave_jsonc_create_object_value(cave_jsonc_document doc);
cave_jsonc_value cave_jsonc_create_array_value(cave_jsonc_document doc, size_t length);
void cave_jsonc_release_value(cave_jsonc_value value);
cave_jsonc_type cave_jsonc_get_value_type(cave_jsonc_value value);
void cave_jsonc_set_value_position(cave_jsonc_value value, cave_jsonc_position pos);
cave_jsonc_position cave_jsonc_get_value_position(cave_jsonc_value value);

cave_jsonc_kvpair cave_jsonc_create_kvpair_with_null_termined_key(cave_jsonc_object obj, const char *k, int lifecycle); 
cave_jsonc_kvpair cave_jsonc_create_kvpair(cave_jsonc_object obj, const char *k, size_t klength, int lifecycle);
void cave_jsonc_move_kvpair_to_object(cave_jsonc_kvpair pair, cave_jsonc_object target);
void cave_jsonc_set_value(cave_jsonc_kvpair pair, cave_jsonc_value value);
void cave_jsonc_set_null_termined_key(cave_jsonc_kvpair pair, const char *s, int lifecycle);
void cave_jsonc_set_key(cave_jsonc_kvpair pair, const char *s, size_t length, int lifecycle);
void cave_jsonc_release_kvpair(cave_jsonc_kvpair pair);
void cave_jsonc_set_key_position(cave_jsonc_kvpair value, cave_jsonc_position pos);
cave_jsonc_position cave_jsonc_get_key_position(cave_jsonc_kvpair value);

cave_jsonc_kvpair cave_jsonc_get_first_kvpair(cave_jsonc_object object);
cave_jsonc_kvpair cave_jsonc_get_last_kvpair(cave_jsonc_object object);
cave_jsonc_kvpair cave_jsonc_insert_first_kvpair(cave_jsonc_object object, cave_jsonc_kvpair pair);
cave_jsonc_kvpair cave_jsonc_insert_last_kvpair(cave_jsonc_object object, cave_jsonc_kvpair pair);
cave_jsonc_kvpair cave_jsonc_next_kvpair(cave_jsonc_kvpair pair);
cave_jsonc_kvpair cave_jsonc_previous_kvpair(cave_jsonc_kvpair pair);
cave_jsonc_kvpair cave_jsonc_take_kvpair_from_object(cave_jsonc_kvpair pair);
void cave_json_release_kvpair(cave_jsonc_kvpair pair);

cave_jsonc_array cave_jsonc_get_array(cave_jsonc_value value);
cave_jsonc_object cave_jsonc_get_object(cave_jsonc_value value);

cave_jsonc_document cave_jsonc_parse_document(int (*fgetc)(void *file), void *file);
int cave_jsonc_serialize_document(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file, int mininize);
int cave_jsonc_print_error(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file);
int cave_jsonc_print_error_full(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file,
		const char *filename, int (*fseek)(void *, size_t, int), int (*fgetc)(void *file), void *in);
int cave_jsonc_has_fatal_error(cave_jsonc_document doc);
int cave_jsonc_report_error(cave_jsonc_document doc, const char *message, cave_jsonc_position position, int fatal);

long long cave_jsonc_get_integer(cave_jsonc_value value);
double cave_jsonc_get_double(cave_jsonc_value value);
cave_jsonc_string cave_jsonc_get_raw_number(cave_jsonc_value value);
void cave_jsonc_warn_value(cave_jsonc_value value, const char *message, int fatal);
void cave_jsonc_warn_key(cave_jsonc_kvpair pair, const char *message, int fatal);
#endif