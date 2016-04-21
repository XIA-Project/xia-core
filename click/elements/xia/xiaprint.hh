#ifndef CLICK_XIAPRINT_HH
#define CLICK_XIAPRINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

XIAPrint([LABEL, I<KEYWORDS>])

=s ip

pretty-prints XIA packets

=d

Expects XIA packets as input.  Should be placed downstream of a
MarkXIAHeader or equivalent element.

Prints out XIA packets in a human-readable tcpdump-like format, preceded by
the LABEL text.

Keyword arguments are:

=over 8

=item CONTENTS

Determines whether the packet data is printed. It may be `NONE' (do not print
packet data), `HEX' (print packet data in hexadecimal), or `ASCII' (print
packet data in plaintext). Default is `NONE'.

=item PAYLOAD

Like CONTENTS, but prints only the packet payload, rather than the entire
packet. Specify at most one of CONTENTS and PAYLOAD.

=item MAXLENGTH

If CONTENTS or PAYLOAD printing is on, then MAXLENGTH determines the maximum
number of bytes to print. -1 means print the entire packet or payload. Default
is 1500.

=item HLIM 

Boolean. Determines whether to print each packet's XIA hlim field. Default is
false.

=item LENGTH

Boolean. Determines whether to print each packet's XIA length field. Default is
false.

=item TIMESTAMP

Boolean. Determines whether to print each packet's timestamp in seconds since
1970. Default is true.

=item AGGREGATE

Boolean. Determines whether to print each packet's aggregate annotation.
Default is false.

=item PAINT

Boolean. Determines whether to print each packet's paint annotation. Default is false.

=item OUTFILE

String. Only available at user level. PrintV<> information to the file specified
by OUTFILE instead of standard error.

=item ACTIVE

Boolean.  If false, then don't print messages.  Default is true.

=back

=a Print, CheckXIAHeader */

class XIAPrint : public Element { public:

    XIAPrint();
    ~XIAPrint();
  
    const char *class_name() const		{ return "XIAPrint"; }
    const char *port_count() const		{ return PORTS_1_1; }
    const char *processing() const		{ return AGNOSTIC; }
  
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();
  
    Packet *simple_action(Packet *);
  
  private:
    bool _active;
    String _label;
    int _bytes;			// Number of bytes to dump
    bool _print_timestamp : 1;
    bool _print_paint : 1;
    bool _print_hlim : 1;
    bool _print_len : 1;
    bool _print_aggregate : 1;
    bool _payload : 1;		// '_contents' refers to payload
    unsigned _contents : 2;	// Whether to dump packet contents
	int verbosity;
  
#if CLICK_USERLEVEL
    String _outfilename;
    FILE *_outfile;
#endif

	int set_verbosity(int v);
	int get_verbosity();
	bool should_print(Packet *p);

    ErrorHandler *_errh;

	enum{VERBOSITY};
    void print_xids(StringAccum &sa, const struct click_xia *xiah);
	static int write_handler(const String &str, Element *e, void *thunk, ErrorHandler *errh);
    static String read_handler(Element *e, void *thunk);
};

CLICK_ENDDECLS
#endif
