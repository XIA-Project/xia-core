%module xsocket
%{
#include  "../Xsocket.h"
%}


%typemap (in) const void *
{
    $1 = (const void*)PyString_AsString($input);
}

/* The "out" map for recvfrom. It packages the data C put into rbuf and dDAG and
   returns them as a python tuple: (rbuf, dDAG). Currently dlen is discarded. Since
   this means the caller of the Python function no longer has access to
   the C function's return value, the status code, we do an error check
   here instead. */
%typemap(argout) (void *rbuf, size_t len, int flags,char * dDAG, size_t *dlen) 
{
    Py_XDECREF($result);
    if (result < 0) {
        free($1);
        free($4);
        free($5);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    PyObject *data, *sender, *return_tuple;
    data = PyString_FromStringAndSize($1, result);
    sender = PyString_FromStringAndSize($4, *$5);
    return_tuple = PyTuple_New(2);
    PyTuple_SetItem(return_tuple, 0, data);
    PyTuple_SetItem(return_tuple, 1, sender);
    $result = return_tuple;
    free($1);
    free($4);
    free($5);
}


/* The "in" map for the receive buffer of receive functions. It converts a single
   integer to the python function into two arguments to the C function:
        $1 (rbuf): a buffuer of that length
        $2 (len): the length itself */
%typemap (in) (void * rbuf, size_t len)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer");
        return NULL;
    }
    $2 = PyInt_AsLong($input);
    if ($2<0) {
        PyErr_SetString(PyExc_ValueError, "Positive integer expected");
        return NULL;
    }
    $1= (void*)malloc($2);

}

/* The "out" map for receive functions. It takes the data C put in rbuf
   and instead returns it as the Python function's return value. Since
   this means the caller of the Python function no longer has access to
   the C function's return value, the status code, we do an error check
   here instead. */
%typemap(argout) (void *rbuf, size_t len) 
{
    Py_XDECREF($result);
    if (result < 0) {
        free($1);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    $result = PyString_FromStringAndSize($1, result);
    free($1);
}


/* The "in" map for the source DAG in recvfrom. It allows calls to the python
   version to omit the buffer and size for the source DAG. Two arguments to the
   C function will be generated:
        $1 (dDAG): a buffer to be filled in with the sender's DAG
        $2 (dlen): a size_t to be filled in with the length of the sender's DAG */
%typemap (in) (int flags, char * dDAG, size_t *dlen)
{
    $1 = (int) PyLong_AsLong($input);  /*TODO: There's no reason to mess with "flags", but we need at least one python input. Better way? */
    $2 = (void*)malloc(4096);  /* TODO: 4096 is arbitrary; is this big enough? */
    $3 = (size_t*)malloc(sizeof(size_t));
}


/* ===== XputChunk ===== */
/* The "in" map: python users don't pass a ChunkInfo pointer;
   we make one here */
%typemap (in) (unsigned length, ChunkInfo *info)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer");
        return NULL;
    }

    $1 = (int)PyInt_AsLong($input);
    $2 = (ChunkInfo*) malloc(sizeof(ChunkInfo));
}

/* The "out" map: Take the data C put in info and instead returns it as 
   the Python function's return value. Since this means the caller of the 
   Python function no longer has access to the C function's return value,
   the status code, we do an error check here instead. */
%typemap(argout) (unsigned length, ChunkInfo *info) 
{
    Py_XDECREF($result);
    if (result < 0) {
        free($2);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    $result = SWIG_NewPointerObj(SWIG_as_voidptr($2), SWIGTYPE_p_ChunkInfo, 0 |  0 );
}


/* ===== XputFile ===== */
/* The "in" map: python users don't pass a ChunkInfo list pointer;
   we make one here */
%typemap (in) (unsigned chunkSize, ChunkInfo **infoList)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer");
        return NULL;
    }

    $1 = (int)PyInt_AsLong($input);
    $2 = (ChunkInfo**) malloc(sizeof(ChunkInfo*));
}

/* The "out" map: Take the data C put in infoList and instead returns it as 
   the Python function's return value. Since this means the caller of the 
   Python function no longer has access to the C function's return value,
   the status code, we do an error check here instead. */
%typemap(argout) (unsigned chunkSize, ChunkInfo **infoList)
{
    Py_XDECREF($result);
    if (result < 0) {
        free($2);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    /* figure out how long list is, and build a python tuple.  */
    PyObject *chunkInfoTuple = PyTuple_New(result);
    int i;
    for (i = 0; i < result; i++) {
        ChunkInfo *chunkInfo = &(*$2)[i];
        PyTuple_SetItem(chunkInfoTuple, i, SWIG_NewPointerObj(SWIG_as_voidptr(chunkInfo), SWIGTYPE_p_ChunkInfo, 0 | 0 ));
    }
    
    $result = chunkInfoTuple;
}



/* Include all of the structs, constants, and function signatures
   in Xsocket.h in our python wrapper */
%include "../Xsocket.h"

/* The next two lines make it possible for python code to pass an
   array of ChunkStatus's or ChunkInfo's to a C function. */
%include "carrays.i"
%array_class(ChunkStatus, ChunkStatusArray);
%array_class(ChunkInfo, ChunkInfoArray);
