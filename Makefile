SHARED := -fPIC --shared -lrt
PLAT = linux
LUA_CLIB_PATH ?= luaclib
CSERVICE_PATH ?= cservice
EXPORT := -Wl,-E  -lrt -lm
BUILD_PATH ?= .
CC = gcc
# lua

LUA_STATICLIB := 3rd/lua/src/liblua.a
LUA_LIB ?= $(LUA_STATICLIB)
LUA_INC ?= 3rd/lua/src

# mysql
MYSQL_STATICLIB := 3rd/mysql-connector-c/lib/libmysqlclient.a
MYSQL_LIB ?= $(MYSQL_STATICLIB)
MYSQL_INC ?= 3rd/mysql-connector-c/include

CC = g++
CFLAGS = -g -O2 -Wall -lpthread  -I$(LUA_INC) $(MYCFLAGS) 


# z
#
CSERVICE = zlua logger
LUA_CLIB = z tcpserver tcpclient session  dir zmysql luapb

SRC = ae.c \
	  anet.c \
	  blockqueue.c \
	  buffer.c \
	  circqueue.c \
	  circqueue.c \
	  config.c \
	  context.c \
	  ctx_mgr.c \
	  daemon.c \
	  dict.c \
	  event_msgqueue.c \
	  eventloop.c \
	  globalqueue.c \
	  log.c \
	  main.c \
	  message.c \
	  module.c \
	  protocol.c \
	  queue.c \
	  session.c \
	  tcp_client.c \
	  tcp_server.c \
	  thread.c \
	  timer.c \
	  z.c \
	  zmalloc.c \
	  
all : \
  $(BUILD_PATH)/z \
  $(foreach v, $(CSERVICE), $(CSERVICE_PATH)/$(v).so) \
  $(foreach v, $(LUA_CLIB), $(LUA_CLIB_PATH)/$(v).so) 

$(BUILD_PATH)/z : $(foreach v, $(SRC), src/$(v)) $(LUA_LIB) $(MALLOC_STATICLIB)
	$(CC) $(CFLAGS) -o $@ $^ -Isrc  -ldl $(LDFLAGS) $(EXPORT) $(LIBS) $(LUA_LIB)  

$(LUA_CLIB_PATH) :
	mkdir $(LUA_CLIB_PATH)

$(CSERVICE_PATH) :
	mkdir $(CSERVICE_PATH)

define CSERVICE_TEMP
  $$(CSERVICE_PATH)/$(1).so : service-src/service_$(1).c | $$(CSERVICE_PATH)
	$(CC) $$(CFLAGS) $$(SHARED) $$< $$(LUA_LIB) -o $$@ -Isrc -lm
endef

$(foreach v, $(CSERVICE), $(eval $(call CSERVICE_TEMP,$(v))))

$(LUA_CLIB_PATH)/z.so: lualib-src/lua-z.c lualib-src/lua-seri.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Isrc -Iservice-src -Ilualib-src

$(LUA_CLIB_PATH)/tcpserver.so: lualib-src/lua-tcpserver.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src

$(LUA_CLIB_PATH)/tcpclient.so: lualib-src/lua-tcpclient.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src

$(LUA_CLIB_PATH)/buffer.so: lualib-src/lua-buffer.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src


$(LUA_CLIB_PATH)/luapb.so: lualib-src/pbc/LuaPB.cc lualib-src/pbc/ProtoImporter.cc  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src -lprotobuf

$(LUA_CLIB_PATH)/session.so: lualib-src/lua-session.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src -Ilualib-src/pbc

$(LUA_CLIB_PATH)/dir.so: lualib-src/lua-dir.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  -Isrc -Iservice-src -Ilualib-src 

$(LUA_CLIB_PATH)/zmysql.so: lualib-src/lua-zmysql.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@  $(MYSQL_LIB) -Isrc -Iservice-src -Ilualib-src -I$(MYSQL_INC)

lua :
	cd 3rd/lua && $(MAKE) CC=$(CC) $(PLAT) 

clean :
	rm -f $(BUILD_PATH)/z $(CSERVICE_PATH)/*.so $(LUA_CLIB_PATH)/*.so

cleanall: clean
	rm -f $(LUA_STATICLIB)

