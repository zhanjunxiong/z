#ifndef CONFIG_H_
#define CONFIG_H_

const char* configGet(const char* key, const char* defaultValue);
int configGetInt(const char* key, int defaultValue);
int configSet(const char* key, const char* value);

int configInit(const char* fileName);
void configUninit();


#endif


