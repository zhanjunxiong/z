#ifndef ENV_H_
#define ENV_H_

const char* envGet(const char* key, const char* defaultValue);
int envGetInt(const char* key, int defaultValue);
int envSet(const char* key, const char* value);

int envInit(const char *envFileName);
void envUninit();

#endif


