%module xsocket
%{
#include  "../Xsocket.h"
%}


%typemap (in) const void *
{
    $1 = (const void*)PyString_AsString($input);
}

/* The "in" map for receive functions. It converts a single
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

/* Include all of the structs, constants, and function signatures
   in Xsocket.h in our python wrapper */
%include "../Xsocket.h"

/* The next two lines make it possible for python code to pass an
   array of cDAGvec's to a C function. */
%include "carrays.i"
%array_class(struct cDAGvec, cDAGvecArray);
