LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libxiasocket

LOCAL_SRC_FILES :=  \
Xaccept.c \
Xbind.c \
Xclose.c \
Xconnect.c \
XrequestChunk.c \
XgetChunkStatus.c \
XreadChunk.c \
XputChunk.c \
Xrecv.c \
Xrecvfrom.c \
Xfcntl.c \
Xsend.c \
Xsendto.c \
Xsocket.c \
Xsetsockopt.c \
Xutil.c \
state.c \
Xinit.c \
XupdateAD.c \
XupdateNameServerDAG.c \
XgetDAGbyName.c \
xia.pb.cc \
../../../xia-core/api/xsocket/minini/minIni.c 

LOCAL_CPP_EXTENSION := .c .cc

LOCAL_C_INCLUDES += api/include api/xsocket/ api/xsocket/minini android-deps/protobuf/src

LOCAL_SHARED_LIBRARIES := \
libdagaddr libprotobuf libz libcutils libutils libpthread
LOCAL_LDLIBS := -lz -lm

LOCAL_CPPFLAGS := -D__MTCLICK__ -DCLICK_USERLEVEL -DHAVE_USER_MULTITHREAD -Wno-psabi -L$(LOCAL_PATH)/src/google/protobuf/io_engine/lib -frtti 
LOCAL_CFLAGS := -Wno-psabi -L$(LOCAL_PATH)/src/google/protobuf/io_engine/lib -frtti 

include $(BUILD_SHARED_LIBRARY)
