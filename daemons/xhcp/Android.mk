LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := xhcp_client

LOCAL_SRC_FILES := \
xhcp_client.cc

LOCAL_CPP_EXTENSION := .c .cc

LCOAL_C_INCLUDES += api/include api/xsocket api/dagaddr click-2.0.1/tools/click-align

LOCAL_SHARED_LIBRARIES := \
libdagaddr libprotobuf libz libcutils libutils libpthread libclickalign libxiasocket libarmeabi

LOCAL_LDLIBS := -lz -lm

include $(BUILD_EXECUTABLE)
