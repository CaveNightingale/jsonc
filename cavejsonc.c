#include "cavejsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cave_jsonc_document cave_jsonc_create_document(void) {
	cave_jsonc_document doc = malloc(sizeof(struct _cave_jsonc_document));
	doc->all_allocated = doc->root = NULL;
	doc->error_tail = doc->error_head = NULL;
	doc->fatal = 0;
	return doc;
}

void cave_jsonc_release_all_nodes_in_document(cave_jsonc_document doc) {
	while(doc->all_allocated)
		cave_jsonc_release_value(doc->all_allocated);
}

void cave_jsonc_transform_node_document(cave_jsonc_value value, cave_jsonc_document target) {
	if(value->document){
		if(value->document->all_allocated == value)
			value->document->all_allocated = value->next;
		if(value->prev)
			value->prev->next = value->next;
		if(value->next)
			value->next->prev = value->prev;
	}
	if(target) {
		value->prev = NULL;
		value->next = target->all_allocated;
		if(value->next)
			value->next->prev = value;
		target->all_allocated = value;
	}
	value->document = target;
}

void cave_jsonc_release_document(cave_jsonc_document doc) {
	cave_jsonc_error err = doc->error_head;
	while(err) {
		cave_jsonc_error next = err->next;
		free(err);
		err = next;
	}
	free(doc);
}

cave_jsonc_value cave_jsonc_get_document_root(cave_jsonc_document doc) {
	return doc->root;
}

void cave_jsonc_set_document_root(cave_jsonc_document doc, cave_jsonc_value root) {
	doc->root = root;
}

static cave_jsonc_value alloc_value(cave_jsonc_document doc, cave_jsonc_type type) {
	cave_jsonc_value value = malloc(sizeof(struct _cave_jsonc_value));
	value->document = NULL;
	cave_jsonc_transform_node_document(value, doc);
	value->type = type;
	value->position = (cave_jsonc_position) {-1, -1, -1};
	return value;
}

cave_jsonc_value cave_jsonc_create_null_value(cave_jsonc_document doc) {
	return alloc_value(doc, CAVE_JSONC_NULL);
}

cave_jsonc_value cave_jsonc_create_boolean_value(cave_jsonc_document doc, int value) {
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_BOOLEAN);
	rval->value.boolean = value;
	return rval;
}

cave_jsonc_string alloc_string(const char *r, size_t length, int lifecycle) {
	if(lifecycle & CAVE_JSONC_STRING_LIFECYCLE_ALLOC) {
		char *target = malloc(length + 1);
		memcpy(target, r, length);
		target[length] = 0;
		r = target;
	}
	cave_jsonc_string rval = malloc(sizeof(struct _cave_jsonc_string));
	rval->value = (char *)r;
	rval->free = lifecycle & CAVE_JSONC_STRING_LIFECYCLE_FREE;
	rval->length = length;
	return rval;
}

cave_jsonc_value cave_jsonc_create_number_value(cave_jsonc_document doc, const char *r, int lifecycle) {
	cave_jsonc_number num = malloc(sizeof(struct _cave_jsonc_number));
	num->flag = CAVE_JSONC_NUM_RAW;
	num->raw = alloc_string(r, strlen(r), lifecycle);
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_NUMBER);
	rval->value.number = num;
	return rval;
}

cave_jsonc_value cave_jsonc_create_integer_value(cave_jsonc_document doc, long long i) {
	cave_jsonc_number num = malloc(sizeof(struct _cave_jsonc_number));
	num->flag = CAVE_JSONC_NUM_IVAL;
	num->ival = i;
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_NUMBER);
	rval->value.number = num;
	return rval;
}

cave_jsonc_value cave_jsonc_create_double_value(cave_jsonc_document doc, double f)  {
	cave_jsonc_number num = malloc(sizeof(struct _cave_jsonc_number));
	num->flag = CAVE_JSONC_NUM_FVAL;
	num->fval = f;
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_NUMBER);
	rval->value.number = num;
	return rval;
}
cave_jsonc_value cave_jsonc_create_null_termined_string_value(cave_jsonc_document doc, const char *s, int lifecycle) {
	return cave_jsonc_create_string_value(doc, s, sizeof(s), lifecycle);
}

cave_jsonc_value cave_jsonc_create_string_value(cave_jsonc_document doc, const char *s, size_t length, int lifecycle) {
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_STRING);
	rval->value.string = alloc_string(s, length, lifecycle);
	return rval;
}

cave_jsonc_value cave_jsonc_create_object_value(cave_jsonc_document doc) {
	cave_jsonc_object object = malloc(sizeof(struct _cave_jsonc_object));
	object->head = object->tail = NULL;
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_OBJECT);
	rval->value.object = object;
	object->value = rval;
	return rval;
}

cave_jsonc_value cave_jsonc_create_array_value(cave_jsonc_document doc, size_t length) {
	cave_jsonc_array array = malloc(sizeof(struct _cave_jsonc_array));
	array->length = length;
	array->values = malloc(sizeof(cave_jsonc_value) * length);
	cave_jsonc_value rval = alloc_value(doc, CAVE_JSONC_OBJECT);
	rval->value.array = array;
	return rval;
}

void release_string(cave_jsonc_string string) {
	if(string->free & CAVE_JSONC_STRING_LIFECYCLE_FREE)
		free(string->value);
	free(string);
}

void cave_jsonc_release_value(cave_jsonc_value value) {
	switch (value->type) {
		case CAVE_JSONC_NULL:
		case CAVE_JSONC_BOOLEAN:
			break;
		case CAVE_JSONC_NUMBER:
			if(value->value.number->flag & CAVE_JSONC_NUM_RAW)
				release_string(value->value.number->raw);
			free(value->value.number);
			break;
		case CAVE_JSONC_STRING:
			release_string(value->value.string);
			break;
		case CAVE_JSONC_OBJECT:
			while(value->value.object->head)
				cave_jsonc_release_kvpair(cave_jsonc_take_kvpair_from_object(value->value.object->head));
			free(value->value.object);
			break;
		case CAVE_JSONC_ARRAY:
			free(value->value.array->values);
			free(value->value.array);
			break;
		case CAVE_JSONC_UNDEFINED:
			abort(); // IMPOSSIBLE
	}
	cave_jsonc_transform_node_document(value, NULL);
	free(value);
}

cave_jsonc_type cave_jsonc_get_value_type(cave_jsonc_value value) {
	return value ? value->type : CAVE_JSONC_UNDEFINED;
}

void cave_jsonc_set_value_position(cave_jsonc_value value, cave_jsonc_position pos) {
	value->position = pos;
}

cave_jsonc_position cave_jsonc_get_value_position(cave_jsonc_value value) {
	return value->position;
}

cave_jsonc_kvpair cave_jsonc_create_kvpair_with_null_termined_key(cave_jsonc_object obj, const char *k, int lifecycle) {
	return cave_jsonc_create_kvpair(obj, k, strlen(k), lifecycle);
}

cave_jsonc_kvpair cave_jsonc_create_kvpair(cave_jsonc_object obj, const char *k, size_t klength, int lifecycle) {
	cave_jsonc_kvpair rval = malloc(sizeof(struct _cave_jsonc_kvpair));
	rval->object = NULL;
	rval->key = alloc_string(k, klength, lifecycle);
	rval->value = NULL;
	rval->position = (cave_jsonc_position) {-1, -1, -1};
	cave_jsonc_move_kvpair_to_object(rval, obj);
	return rval;
}

void cave_jsonc_move_kvpair_to_object(cave_jsonc_kvpair pair, cave_jsonc_object target) {
	pair->object = target;
}

void cave_jsonc_set_value(cave_jsonc_kvpair pair, cave_jsonc_value value) {
	pair->value = value;
}

void cave_jsonc_set_null_termined_key(cave_jsonc_kvpair pair, const char *s, int lifecycle) {
	cave_jsonc_set_key(pair, s, strlen(s), lifecycle);
}

void cave_jsonc_set_key(cave_jsonc_kvpair pair, const char *s, size_t length, int lifecycle) {
	release_string(pair->key);
	pair->key = alloc_string(s, length, lifecycle);
}

void cave_jsonc_release_kvpair(cave_jsonc_kvpair pair) {
	release_string(pair->key);
	free(pair);
}

void cave_jsonc_set_key_position(cave_jsonc_kvpair value, cave_jsonc_position pos) {
	value->position = pos;
}

cave_jsonc_position cave_jsonc_get_key_position(cave_jsonc_kvpair value) {
	return value->position;
}

cave_jsonc_kvpair cave_jsonc_get_first_kvpair(cave_jsonc_object object) {
	return object->head;
}

cave_jsonc_kvpair cave_jsonc_get_last_kvpair(cave_jsonc_object object) {
	return object->tail;
}

cave_jsonc_kvpair cave_jsonc_insert_first_kvpair(cave_jsonc_object object, cave_jsonc_kvpair pair) {
	pair->next = object->head;
	pair->prev = NULL;
	if(pair->next) {
		pair->next->prev = pair;
	} else {
		object->tail = pair;
	}
	return pair;
}

cave_jsonc_kvpair cave_jsonc_insert_last_kvpair(cave_jsonc_object object, cave_jsonc_kvpair pair) {
	pair->prev = object->tail;
	pair->next = NULL;
	if(pair->prev) {
		pair->prev->next = pair;
	} else {
		object->head = pair;
	}
	object->tail = pair;
	return pair;
}

cave_jsonc_kvpair cave_jsonc_next_kvpair(cave_jsonc_kvpair pair) {
	return pair->next;
}

cave_jsonc_kvpair cave_jsonc_previous_kvpair(cave_jsonc_kvpair pair) {
	return pair->prev;
}

cave_jsonc_kvpair cave_jsonc_take_kvpair_from_object(cave_jsonc_kvpair pair) {
	if(pair->next)
		pair->next->prev = pair->prev;
	else
		pair->object->tail = pair->prev;
	if(pair->prev)
		pair->prev->next = pair->next;
	else
		pair->object->head = pair->next;
	return pair;
}

cave_jsonc_array cave_jsonc_get_array(cave_jsonc_value value) {
	return value->value.array;
}

cave_jsonc_object cave_jsonc_get_object(cave_jsonc_value value) {
	return value->value.object;
}

static _Thread_local int in;
static _Thread_local int (*ffgetc)(void *file);
static _Thread_local void *ffile;
static _Thread_local cave_jsonc_document gdoc;
static _Thread_local cave_jsonc_position pos;
static int next() {
	if(in == '\n') {
		pos.cols = 1;
		pos.row++;
	}
	pos.index++;
	pos.cols++;
	return in = ffgetc(ffile);
}

static void skip() {
	while(in == ' ' || in == '\r' || in == '\n' || in == '\t' || in == '\b')
		next();
}

static _Thread_local char *buf;
static _Thread_local size_t cap = 256, size = 0;
static void put_buf(char c) {
	buf[size++] = c;
	if(size == cap) {
		cap *= 2;
		buf = realloc(buf, cap);
	}
}

static cave_jsonc_value parse_number() {
	cave_jsonc_position p = pos;
	buf = malloc(256);
	cap = 256;
	size = 1;
	buf[0] = in;
	while(next() >= '0' && in <= '9') 
		put_buf(in);
	if(in == '.') {
		if(buf[size - 1] == '-') {
			cave_jsonc_report_error(gdoc, "小数点前必须要有整数部分", pos, 1);
			free(buf);
			return NULL;
		}
		put_buf('.');
		while(next() >= '0' && in <= '9') 
			put_buf(in);
	}
	if(in == 'e' || in == 'E') {
		if(buf[size - 1] == '.') {
			cave_jsonc_report_error(gdoc, "科学计数法小数点后必须要有小数部分", pos, 1);
			free(buf);
			return NULL;
		} else if(buf[size - 1] == '-') {
			cave_jsonc_report_error(gdoc, "科学计数法必须要有有效数位", pos, 1);
			free(buf);
			return NULL;
		}
		put_buf(in);
		if(next() == '-' || (in >= '0' && in <= '9'))
			put_buf(in);
		while(next() >= '0' && in <= '9') 
			put_buf(in);
	}
	if(buf[size - 1] == '.') {
		cave_jsonc_report_error(gdoc, "小数点后必须要有小数部分", pos, 1);
		free(buf);
		return NULL;
	} else if(buf[size - 1] == '-') {
		cave_jsonc_report_error(gdoc, "无意义的负号", pos, 1);
		free(buf);
		return NULL;
	} else if(buf[size - 1] == 'e' || buf[size - 1] == 'E') {
		cave_jsonc_report_error(gdoc, "科学计数法必须要有指数位", pos, 1);
		free(buf);
		return NULL;
	} else if((size >= 2 && buf[0] == '0' && buf[1] >= '0' && buf[1] <= '9') ||
			(size >= 3 && buf[0] == '-' && buf[1] == '0' && buf[2] >= '0' && buf[2] <= '9')) {
		cave_jsonc_report_error(gdoc, "数字不得有前导0", p, 1);
		free(buf);
		return NULL;
	}
	skip();
	put_buf('\0');
	return cave_jsonc_create_number_value(gdoc, buf, CAVE_JSONC_STRING_LIFECYCLE_FREE);
}

static int get_utf16() {
	int ucs = 0;
	for(int i = 0; i < 4; i++)
		if(next() < 0) {
			cave_jsonc_report_error(gdoc, "UTF-16转义字符解析到达文件末尾", pos, 1);
			free(buf);
			return -1;
		} else if(in >= '0' && in <='9') {
			ucs = (ucs << 4) + (in - '0');
		} else if (in >= 'a' && in <= 'f') {
			ucs = (ucs << 4) + (in - 'a' + 10);
		} else if (in >= 'A' && in <= 'F') {
			ucs = (ucs << 4) + (in - 'A' + 10);
		} else {
			cave_jsonc_report_error(gdoc, "UTF-16转义字符必须以四位十六进制数表示", pos, 1);
			free(buf);
			return -1;
		}
	return ucs;
}

static cave_jsonc_string get_string() {
	buf = malloc(256);
	cap = 256;
	size = 0;
	next();// 跳过引号
	do {
		if(in < 0) {
			cave_jsonc_report_error(gdoc, "引号在文件末尾仍未配对", pos, 1);
			free(buf);
			return NULL;
		} else if(in == '\n') {
			cave_jsonc_report_error(gdoc, "不能跨行书写字符串", pos, 1);
			free(buf);
			return NULL;
		}
		if(in == '\\') {
			if(next() == '\\') {
				put_buf('\\');
			} else if(in == 'n') {
				put_buf('\n');
			} else if(in == 'r') {
				put_buf('\r');
			} else if(in == 't') {
				put_buf('\t');
			} else if(in == 'b') {
				put_buf('\b');
			} else if(in == 'f') {
				put_buf('\f');
			} else if(in == '/') {
				put_buf('/');
			} else if(in == '\'') {
				put_buf('\'');
			} else if(in == '"') {
				put_buf('"');
			} else if(in == '0') {
				put_buf('\0');
			} else if(in == 'u') {
				int ucs = get_utf16();
				if(ucs < 0){
					return NULL;
				} else if(ucs > 32767) {
					if(next() != '\\') {
						cave_jsonc_report_error(gdoc, "代理对的转义必须成对存在", pos, 1);
						free(buf);
						return NULL;
					}
					if(next() != 'u') {
						cave_jsonc_report_error(gdoc, "无效的代理对转义", pos, 1);
						free(buf);
						return NULL;
					}
					int unext = get_utf16();
					if(unext < 0)
						return NULL;
					ucs = (((ucs & (~ 0xfc00)) << 10) + 0x10000) | (unext & (~ 0xfc00));
				}
				if(ucs < 0x80) {
					put_buf(ucs);
				} else if(ucs < 0x800) {
					put_buf((ucs >> 6) | 0xc0);
					put_buf((ucs & 0x3f) | 0x80);
				} else if(ucs < 0x10000){
					put_buf((ucs >> 12) | 0xe0);
					put_buf(((ucs >> 6) & 0x3f) | 0x80);
					put_buf((ucs & 0x3f) | 0x80);
				} else {// 受UTF-16编码特征的限制，它无法编码超过4字节的字符，故不再判断
					put_buf((ucs >> 18) | 0xf0);
					put_buf(((ucs >> 12) & 0x3f) | 0x80);
					put_buf(((ucs >> 6) & 0x3f) | 0x80);
					put_buf((ucs & 0x3f) | 0x80);
				}
			} else {
				cave_jsonc_report_error(gdoc, "无效转义", pos, 1);
				free(buf);
				return NULL;
			}
		} else {
			put_buf(in);
		}
		next();
	} while(in != '"');
	next();
	put_buf('\0');
	skip();
	return alloc_string(realloc(buf, size), size - 1, CAVE_JSONC_STRING_LIFECYCLE_FREE);
}

static cave_jsonc_value parse_value() {
	cave_jsonc_position p = pos;
	if(in < 0) {
		cave_jsonc_report_error(gdoc, "意料之外的文件结束", pos, 1);
		return NULL;
	}
	if(in == 'n') {
		if(next() != 'u' || next() != 'l' || next() != 'l') {
			cave_jsonc_report_error(gdoc, "无效内容", pos, 1);
			return NULL;
		}
		next();
		skip();
		cave_jsonc_value rval = cave_jsonc_create_null_value(gdoc);
		cave_jsonc_set_value_position(rval, p);
		return rval;
	} else if(in == 't') {
		if(next() != 'r' || next() != 'u' || next() != 'e') {
			cave_jsonc_report_error(gdoc, "无效内容", pos, 1);
			return NULL;
		}
		next();
		skip();
		cave_jsonc_value rval = cave_jsonc_create_boolean_value(gdoc, 1);
		cave_jsonc_set_value_position(rval, p);
		return rval;
	} else if(in == 'f') {
		if(next() != 'a' || next() != 'l' || next() != 's' || next() != 'e') {
			cave_jsonc_report_error(gdoc, "无效内容", pos, 1);
			return NULL;
		}
		next();
		skip();
		cave_jsonc_value rval = cave_jsonc_create_boolean_value(gdoc, 0);
		cave_jsonc_set_value_position(rval, p);
		return rval;
	} else if(in == '"') {
		cave_jsonc_string string = get_string();
		if(!string) {
			return NULL;
		}
		cave_jsonc_value rval = alloc_value(gdoc, CAVE_JSONC_STRING);
		cave_jsonc_set_value_position(rval, p);
		rval->value.string = string;
		return rval;
	} else if((in >= '0' && in <= '9') || in == '-') {
		cave_jsonc_value rval = parse_number();
		if(!rval) {
			return NULL;
		}
		cave_jsonc_set_value_position(rval, p);
		return rval;
	} else if(in == '{') {
		cave_jsonc_value rval = cave_jsonc_create_object_value(gdoc);
		cave_jsonc_set_value_position(rval, p);
		while(in != '}') {
			next();
			skip();
			if(in == '}') {
				if(rval->value.object->head) {
					cave_jsonc_report_error(gdoc, "多余的逗号", pos, 1);
					return rval;
				} else
					break;
			}
			cave_jsonc_position kp = pos;
			if(in != '"') {
				cave_jsonc_report_error(gdoc, "键只能是字符串", pos, 1);
				return rval;
			}
			cave_jsonc_string key = get_string();
			if(!key)
				return rval;
			if(in < 0) {
				release_string(key);
				cave_jsonc_report_error(gdoc, "达到文件末尾对象键值对未定义完毕", pos, 1);
				return rval;
			} else if(in != ':') {
				release_string(key);
				cave_jsonc_report_error(gdoc, "键值之间应当使用冒号分隔", pos, 1);
				return rval;
			}
			next();
			skip();
			cave_jsonc_value value = parse_value();
			if(value) {
				cave_jsonc_kvpair pair = malloc(sizeof(struct _cave_jsonc_kvpair));
				pair->object = NULL;
				pair->key = key;
				pair->value = value;
				pair->position = kp;
				cave_jsonc_move_kvpair_to_object(pair, rval->value.object);
				cave_jsonc_insert_last_kvpair(rval->value.object, pair);
			} else {
				release_string(key);
				return rval;
			}
			if(cave_jsonc_has_fatal_error(gdoc))
				return rval;
			if(in < 0) {
				cave_jsonc_report_error(gdoc, "达到文件末尾对象花括号仍未配对", pos, 1);
				return rval;
			} else if(in != ',' && in != '}') {
				cave_jsonc_report_error(gdoc, "相邻键值对之间应当使用逗号分隔", pos, 1);
				return rval;
			}
		}
		next();
		skip();
		return rval;
	} else if(in == '[') {
		size_t length = 32, size = 0;
		cave_jsonc_value *values = malloc(sizeof(cave_jsonc_value) * 32);
		while(in != ']'){
			next();
			skip();
			cave_jsonc_value value = parse_value();
			if(value) {
				if(size == length) {
					length *= 2;
					values = realloc(values, sizeof(cave_jsonc_value) * length);
				}
				values[size++] = value;
			}
			if(cave_jsonc_has_fatal_error(gdoc))
				break;
			if(in != ',' && in != ']') {
				cave_jsonc_report_error(gdoc, "数组中相邻键之间应当使用逗号分隔", pos, 1);
				break;
			}
		}
		next();
		skip();
		cave_jsonc_value rval = alloc_value(gdoc, CAVE_JSONC_ARRAY);
		cave_jsonc_set_value_position(rval, p);
		rval->value.array = malloc(sizeof(struct _cave_jsonc_array));
		rval->value.array->values = realloc(values, sizeof(cave_jsonc_value) * size);
		rval->value.array->length = size;
		return rval;
	} else {
		cave_jsonc_report_error(gdoc, "无法理解的内容", pos, 1);
		return NULL;
	}
}

cave_jsonc_document cave_jsonc_parse_document(int (*fgetc)(void *file), void *file) {
	pos = (cave_jsonc_position) {1, 1, 0};
	gdoc = cave_jsonc_create_document();
	ffgetc = fgetc;
	ffile = file;
	next();
	cave_jsonc_set_document_root(gdoc, parse_value());
	if(!cave_jsonc_has_fatal_error(gdoc) && in > 0)
		cave_jsonc_report_error(gdoc, "解析完毕后文本仍有内容", pos, 1);
	return gdoc;
}

static _Thread_local int (*ffputc)(int c, void *file);
static _Thread_local void *fofile;
void sfoprint(const char *str) {
	for(size_t i = 0; str[i]; i++)
		ffputc(str[i], fofile);
}

void serialize_string(cave_jsonc_string string) {
	ffputc('"', fofile);
	size_t p = 0;
	while(p < string->length) {
		if(!strncmp("\u200b", string->value + p, sizeof("\u200b") - 1)) {
			sfoprint("\\u200b");
			p += sizeof("\u200b") - 1;
		} else if(!strncmp("\u200c", string->value + p, sizeof("\u200c") - 1)){
			sfoprint("\\u200c");
			p += sizeof("\u200c") - 1;
		} else if(!strncmp("\u200d", string->value + p, sizeof("\u200d") - 1)) {
			sfoprint("\\u200d");
			p += sizeof("\u200d") - 1;
		} else if(!strncmp("\u202a", string->value + p, sizeof("\u202a") - 1)) {
			sfoprint("\\u202a");
			p += sizeof("\u202a") - 1;
		} else if(!strncmp("\u202b", string->value + p, sizeof("\u202b") - 1)) {
			sfoprint("\\u202b");
			p += sizeof("\u202b") - 1;
		} else if(!strncmp("\u202c", string->value + p, sizeof("\u202c") - 1)) {
			sfoprint("\\u202c");
			p += sizeof("\u202c") - 1;
		} else if(!strncmp("\u200d", string->value + p, sizeof("\u202d") - 1)) {
			sfoprint("\\u202d");
			p += sizeof("\u202d") - 1;
		} else if(!strncmp("\u200e", string->value + p, sizeof("\u202e") - 1)) {
			sfoprint("\\u202e");
			p += sizeof("\u202e") - 1;
		} else {
			char value = string->value[p];
			switch (value) {
				case '\\':
					sfoprint("\\");
					break;
				case '\n':
					sfoprint("\\n");
					break;
				case '\r':
					sfoprint("\\r");
					break;
				case '\t':
					sfoprint("\\t");
					break;
				case '\b':
					sfoprint("\\b");
					break;
				case '\f':
					sfoprint("\\f");
					break;
				case '"':
					sfoprint("\\\"");
					break;
				case '\0':
					sfoprint("\\0");
					break;
				default:
					if(value >= 0 && value <= 40) {
						char head[7] = "\\u00";
						head[4] = (value >> 4) + '0';
						head[5] = (value & 15) >= 10 ? (value & 15) - 10 + 'a' : (value & 15) + '0';
						head[6] = 0;
						sfoprint(head);
					} else
						ffputc(value, fofile);
			}
			p++;
		}
	}
	ffputc('"', fofile);
}

static void print_tab(int count) {
	for(int i = 0; i < count; i++)
		ffputc('\t', ffile);
}

static void serialize_value(cave_jsonc_value value, int mininize, int tab) {
	switch (cave_jsonc_get_value_type(value)) {
		case CAVE_JSONC_UNDEFINED:
		case CAVE_JSONC_NULL:
			sfoprint("null");
			break;
		case CAVE_JSONC_BOOLEAN:
			sfoprint(value->value.boolean ? "true" : "false");
			break;
		case CAVE_JSONC_NUMBER:
			sfoprint(cave_jsonc_get_raw_number(value)->value);
			break;
		case CAVE_JSONC_STRING:
			serialize_string(value->value.string);
			break;
		case CAVE_JSONC_OBJECT:
			ffputc('{', ffile);
			cave_jsonc_object object = value->value.object;
			cave_jsonc_kvpair pair = cave_jsonc_get_first_kvpair(object);
			int first = 1;
			while(pair) {
				if(pair->value != NULL) {
					if(!first)
						ffputc(',', ffile);
					if(!mininize) {
						ffputc('\n', ffile);
						print_tab(tab + 1);
					}
					first = 0;
					serialize_string(pair->key);
					if(!mininize)
						ffputc(' ', ffile);
					ffputc(':', ffile);
					if(!mininize)
						ffputc(' ', ffile);
					serialize_value(pair->value, mininize, tab + 1);
				}
				pair = cave_jsonc_next_kvpair(pair);
			}
			if(!mininize && !first) {
				ffputc('\n', ffile);
				print_tab(tab);
			}
			ffputc('}', ffile);
			break;
		case CAVE_JSONC_ARRAY:
			ffputc('[', ffile);
			cave_jsonc_array array = value->value.array;
			first = 1;
			for(int i = 0; i < array->length; i++) {
				if(!first)
					ffputc(',', ffile);
				if(!mininize)
					ffputc(' ', ffile);
				first = 0;
				serialize_value(array->values[i], mininize, tab);
			}
			ffputc(']', ffile);
			break;
	}
}

int cave_jsonc_serialize_document(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file, int mininize) {
	if(!doc->root)
		return 0;
	ffputc = fputc;
	fofile = file;
	serialize_value(cave_jsonc_get_document_root(doc), mininize, 0);
	return 0;
}

static void ffputs(int (*fputc)(int c, void *file), void *file, const char *str) {
	for(size_t i = 0; str[i]; i++)
		fputc(str[i], file);
}

int cave_jsonc_print_error(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file) {
	cave_jsonc_error err = doc->error_head;
	char head[100];
	size_t count = 0;
	while(err) {
		if(err->fatal)
			sprintf(head, "%ld:%ld: 错误: ", err->position.row, err->position.cols);
		else
			sprintf(head, "%ld:%ld: 警告: ", err->position.row, err->position.cols);
		ffputs(fputc, file, head);
		ffputs(fputc, file, err->message);
		fputc('\n', file);
		err = err->next;
		count++;
	}
	if(count) {
		sprintf(head, "共 %ld 个错误或警告\n", count);
		ffputs(fputc, file, head);
	}
	return count;
}

int cave_jsonc_has_fatal_error(cave_jsonc_document doc) {
	return doc->fatal;
}

int cave_jsonc_report_error(cave_jsonc_document doc, const char *message, cave_jsonc_position position, int fatal) {
	int rval = doc->fatal;
	cave_jsonc_error err = malloc(sizeof(struct _cave_jsonc_error));
	err->fatal = fatal;
	err->message = message;
	err->position = position;
	err->next = NULL;
	if(doc->error_tail)
		doc->error_tail = doc->error_tail->next = err;
	else
		doc->error_head = doc->error_tail = err;
	if(fatal)
		doc->fatal = 1;
	return rval;
}

long long cave_jsonc_get_integer(cave_jsonc_value value) {
	cave_jsonc_number num = value->value.number;
	if(!(num->flag & CAVE_JSONC_NUM_IVAL)) {
		if(num->flag & CAVE_JSONC_NUM_RAW)
			sscanf(value->value.number->raw->value, "%lld", &num->ival);
		else if(num->flag & CAVE_JSONC_NUM_FVAL)
			num->ival = num->fval;
	}
	num->flag |= CAVE_JSONC_NUM_IVAL;
	return num->ival;
}

double cave_jsonc_get_double(cave_jsonc_value value) {
	cave_jsonc_number num = value->value.number;
	if(!(num->flag & CAVE_JSONC_NUM_FVAL)) {
		if(num->flag & CAVE_JSONC_NUM_RAW)
			sscanf(value->value.number->raw->value, "%lf", &num->fval);
		else if(num->flag & CAVE_JSONC_NUM_IVAL)
			num->fval = num->ival;
	}
	num->flag |= CAVE_JSONC_NUM_FVAL;
	return num->fval;
}

static double scale_double(double in, int *out) {
	int e = 0;
	while(in >= 10) {
		in /= 10;
		e++;
	}
	while(in < 1) {
		in *= 10;
		e--;
	}
	*out = e;
	return in;
}

static void internal_stringify_double(double dbl) {
	char head[64];
	sprintf(head, "%.12lf", dbl);
	int end = strlen(head);
	for(end--; head[end] == '0' && head[end - 1] != '.'; end--);
	for(int i = 0; i <= end; i++)
		put_buf(head[i]);
}

static void stringify_int(long long i) {
	if(i == 0) {
		put_buf('0');
		return;
	}
	char stack[48];
	int top = 0;
	if(i < 0) {
		put_buf('-');
		i = -i;
	}
	for(;i; i /= 10)
		stack[top++] = i % 10 + '0';
	for(int j = top - 1; j >= 0; j--)
		put_buf(stack[j]);
}

static void stringify_double(double dbl) {
	if(dbl < 0) {
		put_buf('-');
		dbl = -dbl;
	}
	int out;
	double scaled = scale_double(dbl, &out);
	if(out <= 6 && out >= -6) {
		internal_stringify_double(dbl);
	} else {
		internal_stringify_double(scaled);
		put_buf('E');
		stringify_int(out);
	}
}

cave_jsonc_string cave_jsonc_get_raw_number(cave_jsonc_value value) {
	cave_jsonc_number num = value->value.number;
	if(!(num->flag & CAVE_JSONC_NUM_RAW)) {
		buf = malloc(256);
		cap = 256;
		size = 0;
		if(num->flag & CAVE_JSONC_NUM_FVAL)
			stringify_double(num->fval);
		else if(num->flag & CAVE_JSONC_NUM_IVAL)
			stringify_int(num->ival);
		put_buf('\0');
		num->raw = alloc_string(buf, size - 1, CAVE_JSONC_STRING_LIFECYCLE_FREE);
		num->flag |= CAVE_JSONC_NUM_RAW;
	}
	return num->raw;
}

int cave_jsonc_print_error_full(cave_jsonc_document doc, int (*fputc)(int c, void *file), void *file,
		const char *filename, int (*fseek)(void *, size_t, int), int (*fgetc)(void *file), void *in) {
	cave_jsonc_error err = doc->error_head;
	char head[100];
	size_t count = 0;
	while(err) {
		ffputs(fputc, file, filename);
		if(err->fatal)
			sprintf(head, ":%ld:%ld: 错误: ", err->position.row, err->position.cols);
		else
			sprintf(head, ":%ld:%ld: 警告: ", err->position.row, err->position.cols);
		ffputs(fputc, file, head);
		ffputs(fputc, file, err->message);
		fputc('\n', file);
		fseek(in, err->position.index - err->position.cols + 1, SEEK_SET);
		int out = 0, p = 1;
		while(out != '\n' && out >= 0) {
			fputc(out, file);
			if(p == err->position.cols - 1)
				ffputs(fputc, file, " !!这里!! ->> ");
			p++;
			out = fgetc(in);
		}
		fputc('\n', file);
		fputc('\n', file);
		err = err->next;
		count++;
	}
	if(count) {
		sprintf(head, "共 %ld 个错误或警告\n", count);
		ffputs(fputc, file, head);
	}
	return count;
}

void cave_jsonc_warn_value(cave_jsonc_value value, const char *message, int fatal) {
	cave_jsonc_report_error(value->document, message, value->position, fatal);
}

void cave_jsonc_warn_key(cave_jsonc_kvpair pair, const char *message, int fatal) {
	cave_jsonc_report_error(pair->object->value->document, message, pair->position, fatal);
}
