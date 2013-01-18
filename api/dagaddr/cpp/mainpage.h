/*!
  @file mainpage.h
  @brief dummy file to contain main API documentation
*/  
/*!
  @mainpage

<h1>DAG Manipulation Overview</h1>

In XIA, addresses are represented as directed acyclic graphs (DAGs). The XIA
socket API manipulates DAGs as strings in a particular format; creating and
working with DAGs in this string format can be tedious. This library provides
an object-oriented API (in C++ and Python) for creating and manipulating DAGs.

<h2>Creating a DAG</h2>

Here we describe building a DAG from the ground up. Most simple DAGs can be
seen one or more possible paths for reaching the final intent; that is, most
DAGs you work with will consist of and handful of paths from source to sink.
Consider this simple example: we are constructing a DAG for retrieving a piece
of content with ID "CID". We want to make a DAG that offers the network three
possible paths to the content, each more specific than the last:
<ol>
	<li>SRC -> CID</li>
	<li>SRC -> AD -> CID</li>
	<li>SRC -> AD -> HID -> CID</li>
</ol>

In the subsections that follow, we show how to construct a DAG for each of
these paths and then how to combine those three DAGs into a single DAG, where
paths 2 and 3 are fallbacks.

A DAG is represented by a Graph object in our library. Graphs are composed of
Node objects, so we begin by instantiating the nodes we'll need:

C++
\code
Node n_src;
Node n_ad(Node::XID_TYPE_AD, "0606060606060606060606060606060606060606");
Node n_hid(Node::XID_TYPE_HID, "0101010101010101010101010101010101010101");
Node n_cid(Node::XID_TYPE_CID, "0202020202020202020202020202020202020202");
\endcode

Python
\code
n_src = Node()
n_ad = Node(Node.XID_TYPE_AD, "0606060606060606060606060606060606060606")
n_hid = Node(Node.XID_TYPE_HID, "0101010101010101010101010101010101010101")
n_cid = Node(Node.XID_TYPE_CID, "0202020202020202020202020202020202020202")
\endcode

<h3>Making a Path</h3>

The simplest way to combine Nodes to make a Graph is to append
them using the * operator:

C++
\code
// Path directly to n_cid
// n_src -> n_cid
Graph g0 = n_src * n_cid;

// Path to n_cid through n_hid
// n_src -> n_hid -> n_cid
Graph g1 = n_src * n_ad * n_cid;

// Path to n_cid through n_ad then n_hid
// n_src -> n_ad -> n_hid -> n_cid
Graph g2 = n_src * n_ad * n_hid * n_cid;
\endcode

Python
\code
# Path directly to n_cid
# n_src -> n_cid
g0 = n_src * n_cid

# Path to n_cid through n_hid
# n_src -> n_hid -> n_cid
g1 = n_src * n_hid * n_cid

# Path to n_cid through n_ad then n_hid
# n_src -> n_ad -> n_hid -> n_cid
g2 = n_src * n_ad * n_hid * n_cid

\endcode


<h3>Combining Paths</h3>

Paths can be combined into a single DAG using the + operator. The easiest way
to make a DAG with a fallback route from one node to another is to combine
paths that share the same source and sink; in our case, we are combining three
paths with source node n_src and sink node n_cid:

C++
\code
// Combine the above three paths into a single DAG;
// g1 and g2 become fallback paths from n_src to n_cid

Graph g3 = g0 + g1 + g2;
\endcode

Python
\code
# Combine the above three paths into a single DAG;
# g1 and g2 become fallback paths from n_src to n_cid

g3 = g0 + g1 + g2
\endcode


<p>&nbsp;</p>
<p>&nbsp;</p>
<h2>Converting to a DAG string</h2>

To use a DAG constructed with this library in an XSocket API call, use the
Graph::dag_string() method:

C++
\code
// Get a DAG string version of the graph that could be used in an
// XSocket API call

const char* dag_string =  g3.dag_string().c_str();
\endcode

Python
\code
# Get a DAG string version of the graph that could be used in an
# XSocket API call

dag_string = g3.dag_string()
\endcode

In this example, dag_string will have the value:
<pre>
DAG 2 0 - 
AD:0606060606060606060606060606060606060606 2 1 - 
HID:0101010101010101010101010101010101010101 2 - 
CID:0202020202020202020202020202020202020202
</pre>


<p>&nbsp;</p>
<p>&nbsp;</p>
<h2>Converting from a DAG string</h2>

At times, you may want to create a Graph object from a DAG string you get from
an XSocket API call (like XrecvFrom()) to make it easier to manipulate. This is
easy to do, as the Graph class has a constructor that takes in a DAG string:

C++
\code
// Create a DAG from a string (which we might have gotten from an Xsocket
// API call like XrecvFrom)

Graph g4 = Graph(dag_string);
\endcode

Python
\code
# Create a DAG from a string (which we might have gotten from an Xsocket
# API call like XrecvFrom)

g4 = Graph(dag_string)
\endcode


<p>&nbsp;</p>
<p>&nbsp;</p>
<h2>Visualizing a DAG</h2>

It is not always easy to tell if the DAG you created is really what you wanted;
inspecting the DAG string to make sure there are edges in the right places can
be tedious. To help with this, we created a <a target="_blank"
href="http://www.cs.cmu.edu/~dnaylor/dagtool/">web-based tool for displaying
DAGs</a>.  Simply paste the DAG string (like the one we obtained above with the
Graph::dag_string() method) into the tool and click "Draw DAG."

*/

