%module c_xsocket
%{
#include "Xsocket.h"
#include "xia.h"
#include "dagaddr.hpp"
%}


/*
** generic void* to python string converter
*/
%typemap (in) const void *
{
    $1 = PyString_AsString($input);
}

/* The "out" map for recvfrom. It packages the data C put into rbuf and addr and
   returns them as a python tuple: (rbuf, addr string). Currently dlen is discarded. Since
   this means the caller of the Python function no longer has access to
   the C function's return value, the status code, we do an error check
   here instead. */
%typemap(argout) (void *rbuf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen) 
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
    return_tuple = PyTuple_New(2);
    data = PyString_FromStringAndSize((const char *)$1, result);
    sockaddr_x *a = (sockaddr_x*)$4;

    Graph g(a);
    sender = PyString_FromString(g.dag_string().c_str());

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
    $result = PyString_FromStringAndSize((const char *)$1, result);
    free($1);
}


/* The "in" map for the source DAG in recvfrom. It allows calls to the python
   version to omit the buffer and size for the source DAG. Two arguments to the
   C function will be generated:
        $1 (dDAG): a buffer to be filled in with the sender's DAG
        $2 (dlen): a size_t to be filled in with the length of the sender's DAG */
%typemap (in) (int flags, struct sockaddr* addr, socklen_t *addrlen)
{
    $1 = (int) PyLong_AsLong($input);  /*TODO: There's no reason to mess with "flags", but we need at least one python input. Better way? */
    $2 = (struct sockaddr *)malloc(sizeof(sockaddr_x));
    $3 = (socklen_t*)malloc(sizeof(socklen_t));

    *$3 = sizeof(sockaddr_x);
}

%typemap (in) (int sockfd, struct sockaddr* addr, socklen_t *addrlen)
{
    $1 = (int) PyLong_AsLong($input);
    $2 = (struct sockaddr *)calloc(1, sizeof(sockaddr_x));
    $3 = (socklen_t*)malloc(sizeof(socklen_t));

    *$3 = sizeof(sockaddr_x);
}

%typemap (in) (const char *name, sockaddr_x *addr, socklen_t *addrlen)
{
    $1 = PyString_AsString($input);
    $2 = (sockaddr_x*)calloc(1, sizeof(sockaddr_x));
    $3 = (socklen_t*)malloc(sizeof(socklen_t));

    *$3 = sizeof(sockaddr_x);
}


%typemap (in) (const struct sockaddr *addr, socklen_t addrlen)
{
    PyObject *dag = $input;
    if (!PyString_Check(dag)) {
        PyErr_SetString(PyExc_ValueError, "expected a dag string");
        return NULL;
    }

    $2 = sizeof(sockaddr_x);
    Graph g(PyString_AsString(dag));
    $1 = (struct sockaddr*)malloc($2);
    g.fill_sockaddr((sockaddr_x*)$1);
}

/*
** used by Xaccept
*/
%typemap (argout) (int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    Py_XDECREF($result);
    if (result < 0) {
        free($2);
        free($3);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    PyObject *accept_sock, *peer, *return_tuple;
    return_tuple = PyTuple_New(2);

    accept_sock = PyLong_FromLong(result);

    sockaddr_x *a = (sockaddr_x*)$2;
    if (a) {
        Graph g(a);
        if (g.num_nodes() != 0)
            peer = PyString_FromString(g.dag_string().c_str());
        else
            peer = PyString_FromString("");
    }
    else
        peer = PyString_FromString("");

    PyTuple_SetItem(return_tuple, 0, accept_sock);
    PyTuple_SetItem(return_tuple, 1, peer);
    $result = return_tuple;
    free($2);
    free($3);
}
/*
** used by:
**  XgetDAGbyName
*/
%typemap (argout) (struct sockaddr *addr, socklen_t *addrlen)
{
    sockaddr_x *a = (sockaddr_x*)$1;
    Graph g(a);

    if (g.num_nodes() != 0)
        $result = PyString_FromString(g.dag_string().c_str());
    else
        $result = PyString_FromString("");

    free($1);
    free($2);
}

%typemap (argout) (sockaddr_x *addr, socklen_t *addrlen)
{
    Graph g($1);
 
    if (g.num_nodes() != 0)
    	$result = PyString_FromString(g.dag_string().c_str());
    else
    	$result = PyString_FromString("");

    free($1);
    free($2);
}



/*
** used by:
**  XgetDAGbyName
*/
%typemap (in) (sockaddr_x *addr)
{
    if (!PyString_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "expected a dag string");
        return NULL;
    }

    Graph g(PyString_AsString($input));

    $1 = (sockaddr_x*)malloc(sizeof(sockaddr_x));
    g.fill_sockaddr((sockaddr_x*)$1);
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
    free($2);
}


/* ===== XputFile and XputBuffer ===== */
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
    free ($2);
}



/* ===== XreadLocalHostAddr ===== */
/* The "in" map: python users don't pass in pointers for localhostAD or
   localhostHID, so we allocate them here and pass them to the C function. */
%typemap (in) (int sockfd, char *localhostAD, unsigned lenAD, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID)
{
    $1 = (int) PyLong_AsLong($input);  /*TODO: There's no reason to mess with "sockfd", but we need at least one python input. Better way? */
    $2 = (char*)malloc(44); /* "AD:" + 40-byte XID + null byte */
    $3 = 44;
    $4 = (char*)malloc(45); /* "HID:" + 40-byte XID + null byte */
    $5 = 45;
    $6 = (char*)malloc(44); /* "IP:" + 40-byte XID + null byte */
    $7 = 44;
}

%typemap (argout) (int sockfd, char *localhostAD, unsigned lenAD, char *localhostHID, unsigned lenHID, char *local4ID, unsigned len4ID)
{
    Py_XDECREF($result);
    if (result < 0) {
        free($2);
        free($4);
        free($6);
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    
    PyObject *ad, *hid, *fid, *return_tuple;
    ad = PyString_FromStringAndSize($2, 43);  /* don't give the null byte to python */
    hid = PyString_FromStringAndSize($4, 44);
    fid = PyString_FromStringAndSize($6, 43);
    return_tuple = PyTuple_New(3);
    PyTuple_SetItem(return_tuple, 0, ad);
    PyTuple_SetItem(return_tuple, 1, hid);
    PyTuple_SetItem(return_tuple, 2, fid);
    $result = return_tuple;
    free($2);
    free($4); 
    free($6);
}

/* ===== Xgetsockopt ===== */
%typemap (in) (int optname, void* optval, socklen_t* optlen)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer (optname)");
        return NULL;
    }
    $1 = PyInt_AsLong($input);
    $2= (void*)malloc(4096); /* TODO: 4096 is arbitrary; better buffer size? */
    $3 = (socklen_t*)malloc(sizeof(socklen_t));
    (*$3) = 4096;
}
%typemap(argout) (int optname, void* optval, socklen_t* optlen)
{
    Py_XDECREF($result);
    if (result < 0) {
        free($2);
        free($3);
        PyErr_SetFromErrno(PyExc_Exception);
        return NULL;
    }
    $result = PyLong_FromLong(*(int*)$2);
    free($2);
    free($3);
}

/* ===== Xsetsockopt ===== */
%typemap (in) (const void* optval, socklen_t optlen)
{
    if (!PyInt_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting an integer (optval)");
        return NULL;
    }
    $1 = (void*)malloc(sizeof(int));
    *(int*)($1) = PyInt_AsLong($input);
    $2 = sizeof(int);
}



/* Include all of the structs, constants, and function signatures
   in Xsocket.h in our python wrapper 
   NOTE: It matters to swig where everything else in this file is
   with relation to this include, so don't move it. */
%include "../../include/Xsocket.h"
%include "../../include/xia.h"

%pythoncode %{
def Xsocket(type):
    return _c_xsocket.Xsocket(AF_XIA, type, 0)
%}

/* ===== Xsend ===== */
/* eliminate the need for python users to pass the length of the data, since we can calculate it */
%pythoncode %{
def Xsend(sock, data, flags):
    return _c_xsocket.Xsend(sock, data, len(data), flags)
%}

/* ===== Xsendto ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def Xsendto(sock, data, flags, dest_dag):
    return _c_xsocket.Xsendto(sock, data, len(data), flags, dest_dag)
%}

/* ===== XgetChunkStatus ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def XgetChunkStatus(sock, dag):
    return _c_xsocket.XgetChunkStatus(sock, dag, len(dag))
%}

/* ===== XreadChunk ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def XreadChunk(sock, length, flags, dag):
    return _c_xsocket.XreadChunk(sock, length, flags, dag, len(dag))
%}

/* ===== XrequestChunk ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def XrequestChunk(sock, dag):
    return _c_xsocket.XrequestChunk(sock, dag, len(dag))
%}

/* ===== XputBuffer ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def XputBuffer(context, data, chunk_size):
    return _c_xsocket.XputBuffer(context, data, len(data), chunk_size)
%}

/* ===== XputChunk ===== */
/* eliminate the need for python users to pass the length of the data or the DAG, 
   since we can calculate it */
%pythoncode %{
def XputChunk(context, data):
    return _c_xsocket.XputChunk(context, data, len(data))
%}


/* The next two lines make it possible for python code to pass an
   array of ChunkStatus's or ChunkInfo's to a C function. */
%include "carrays.i"
%array_class(ChunkStatus, ChunkStatusArray);
/*%array_class(ChunkInfo, ChunkInfoArray);*/
