#ifndef NAME_MGR_H_
#define NAME_MGR_H_

int nameMgrRegist(const char *name, int id);
int nameMgrUnRegist(const char *name);

int nameMgrGetId(const char *name);

int nameMgrInit();
void nameMgrUninit();

#endif


