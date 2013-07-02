LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libprotobuf

LOCAL_SRC_FILES :=  \
protobuf/src/google/protobuf/descriptor.cc \
protobuf/src/google/protobuf/descriptor.pb.cc \
protobuf/src/google/protobuf/descriptor_database.cc \
protobuf/src/google/protobuf/dynamic_message.cc \
protobuf/src/google/protobuf/extension_set.cc \
protobuf/src/google/protobuf/extension_set_heavy.cc \
protobuf/src/google/protobuf/generated_message_reflection.cc \
protobuf/src/google/protobuf/generated_message_util.cc \
protobuf/src/google/protobuf/message.cc \
protobuf/src/google/protobuf/message_lite.cc \
protobuf/src/google/protobuf/reflection_ops.cc \
protobuf/src/google/protobuf/repeated_field.cc \
protobuf/src/google/protobuf/service.cc \
protobuf/src/google/protobuf/text_format.cc \
protobuf/src/google/protobuf/unknown_field_set.cc \
protobuf/src/google/protobuf/wire_format.cc \
protobuf/src/google/protobuf/wire_format_lite.cc \
protobuf/src/google/protobuf/io/coded_stream.cc \
protobuf/src/google/protobuf/io/gzip_stream.cc \
protobuf/src/google/protobuf/io/printer.cc \
protobuf/src/google/protobuf/io/tokenizer.cc \
protobuf/src/google/protobuf/io/zero_copy_stream.cc \
protobuf/src/google/protobuf/io/zero_copy_stream_impl.cc \
protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.cc \
protobuf/src/google/protobuf/stubs/common.cc \
protobuf/src/google/protobuf/stubs/once.cc \
protobuf/src/google/protobuf/stubs/structurally_valid.cc \
protobuf/src/google/protobuf/stubs/strutil.cc \
protobuf/src/google/protobuf/stubs/substitute.cc

LOCAL_CPP_EXTENSION := .cc

LOCAL_C_INCLUDES += android-deps/protobuf/src

LOCAL_SHARED_LIBRARIES := \
libz libcutils libutils
LOCAL_LDLIBS := -lz -lm

LOCAL_CFLAGS := -Wno-psabi -frtti -c
LOCAL_CPPFLAGS := -Wno-psabi -frtti -c

include $(BUILD_SHARED_LIBRARY)
