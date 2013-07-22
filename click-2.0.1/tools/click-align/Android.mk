LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libclickalign

LOCAL_SRC_FILES :=  \
../../lib/string.cc \
../../lib/straccum.cc \
../../lib/bitvector.cc \
../../lib/vectorv.cc \
../../lib/hashallocator.cc \
../../lib/ipaddress.cc \
../../lib/etheraddress.cc \
../../lib/error.cc \
../../lib/timestamp.cc \
../../lib/glue.cc \
../../lib/archive.cc \
../../lib/confparse.cc \
../../lib/args.cc \
../../lib/userutils.cc \
../../lib/xid.cc \
../../lib/xidpair.cc \
../../lib/xiapath.cc \
../../lib/xiaheader.cc \
../../lib/xiaextheader.cc \
../../lib/xiacontentheader.cc \
../../lib/ip6address.cc \
../../lib/md5.cc \
../../lib/clp.c \
../../lib/nameinfo.cc \
../../lib/variableenv.cc \
../../lib/ip6flowid.cc \
../../lib/router.cc \
../../lib/handlercall.cc \
../../lib/exportstub.cc \
../../lib/driver.cc \
../../lib/notifier.cc \
../../lib/element.cc \
../../lib/lexer.cc \
../../elements/standard/addressinfo.cc \
../../elements/standard/xiaxidinfo.cc \
click-align.cc

LOCAL_CPP_EXTENSION := .c .cc

LOCAL_C_INCLUDES += api/include api/xsocket/ api/xsocket/minini android-deps/protobuf/src click/include click/tools/lib

LOCAL_SHARED_LIBRARIES := \
libdagaddr libprotobuf libz libcutils libutils libpthread
LOCAL_LDLIBS := -lz -lm

LOCAL_CPPFLAGS := -D__MTCLICK__ -DCLICK_USERLEVEL -DHAVE_USER_MULTITHREAD -Wno-psabi -L$(LOCAL_PATH)/src/google/protobuf/io_engine/lib -frtti 
LOCAL_CFLAGS := -Wno-psabi -L$(LOCAL_PATH)/src/google/protobuf/io_engine/lib -frtti 

include $(BUILD_SHARED_LIBRARY)
