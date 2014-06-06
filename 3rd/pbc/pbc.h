#ifndef PROTOBUF_C_H
#define PROTOBUF_C_H

#include <stdio.h>
#include <stdint.h>

#define PBC_ARRAY_CAP 64

#define PBC_NOEXIST -1
#define PBC_INT 1
#define PBC_REAL 2
#define PBC_BOOL 3
#define PBC_ENUM 4
#define PBC_STRING 5
#define PBC_MESSAGE 6
#define PBC_FIXED64 7
#define PBC_FIXED32 8
#define PBC_BYTES 9
#define PBC_INT64 10
#define PBC_UINT 11
#define PBC_UNKNOWN 12
#define PBC_REPEATED 128

typedef struct _pbc_array { char _data[PBC_ARRAY_CAP]; } pbc_array[1];

struct pbc_slice {
	void *buffer;
	int len;
};

struct pbc_pattern;
struct pbc_env;
struct pbc_rmessage;
struct pbc_wmessage;

extern "C" struct pbc_env * pbc_new(void);
extern "C" void pbc_delete(struct pbc_env *);
extern "C" int pbc_register(struct pbc_env *, struct pbc_slice * slice);
extern "C" int pbc_type(struct pbc_env *, const char * type_name , const char * key , const char ** type);
extern "C" const char * pbc_error(struct pbc_env *);

// callback api
union pbc_value {
	struct {
		uint32_t low;
		uint32_t hi;
	} i;
	double f;
	struct pbc_slice s;
	struct {
		int id;
		const char * name;
	} e;
};

typedef void (*pbc_decoder)(void *ud, int type, const char * type_name, union pbc_value *v, int id, const char *key);
extern "C" int pbc_decode(struct pbc_env * env, const char * type_name , struct pbc_slice * slice, pbc_decoder f, void *ud);

// message api

extern "C" struct pbc_rmessage * pbc_rmessage_new(struct pbc_env * env, const char * type_name , struct pbc_slice * slice);
extern "C" void pbc_rmessage_delete(struct pbc_rmessage *);

extern "C" uint32_t pbc_rmessage_integer(struct pbc_rmessage * , const char *key , int index, uint32_t *hi);
extern "C" double pbc_rmessage_real(struct pbc_rmessage * , const char *key , int index);
extern "C" const char * pbc_rmessage_string(struct pbc_rmessage * , const char *key , int index, int *sz);
extern "C" struct pbc_rmessage * pbc_rmessage_message(struct pbc_rmessage *, const char *key, int index);
extern "C" int pbc_rmessage_size(struct pbc_rmessage *, const char *key);
extern "C" int pbc_rmessage_next(struct pbc_rmessage *, const char **key);

extern "C" struct pbc_wmessage * pbc_wmessage_new(struct pbc_env * env, const char *type_name);
extern "C" void pbc_wmessage_delete(struct pbc_wmessage *);

// for negative integer, pass -1 to hi
extern "C" int pbc_wmessage_integer(struct pbc_wmessage *, const char *key, uint32_t low, uint32_t hi);
extern "C" int pbc_wmessage_real(struct pbc_wmessage *, const char *key, double v);
extern "C" int pbc_wmessage_string(struct pbc_wmessage *, const char *key, const char * v, int len);
extern "C" struct pbc_wmessage * pbc_wmessage_message(struct pbc_wmessage *, const char *key);
extern "C" void * pbc_wmessage_buffer(struct pbc_wmessage *, struct pbc_slice * slice);

// array api 

extern "C" int pbc_array_size(pbc_array);
extern "C" uint32_t pbc_array_integer(pbc_array array, int index, uint32_t *hi);
extern "C" double pbc_array_real(pbc_array array, int index);
extern "C" struct pbc_slice * pbc_array_slice(pbc_array array, int index);

extern "C" void pbc_array_push_integer(pbc_array array, uint32_t low, uint32_t hi);
extern "C" void pbc_array_push_slice(pbc_array array, struct pbc_slice *);
extern "C" void pbc_array_push_real(pbc_array array, double v);

extern "C" struct pbc_pattern * pbc_pattern_new(struct pbc_env * , const char * message, const char *format, ...);
extern "C" void pbc_pattern_delete(struct pbc_pattern *);

// return unused bytes , -1 for error
extern "C" int pbc_pattern_pack(struct pbc_pattern *, void *input, struct pbc_slice * s);

// <0 for error
extern "C" int pbc_pattern_unpack(struct pbc_pattern *, struct pbc_slice * s , void * output);

extern "C" void pbc_pattern_set_default(struct pbc_pattern * , void *data);
extern "C" void pbc_pattern_close_arrays(struct pbc_pattern *, void *data);

#endif
