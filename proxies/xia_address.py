XIDS = dict(hid0="HID:0000000000000000000000000000000000000000",
hid1= "HID:0000000000000000000000000000000000000001",
ad0=  "AD:1000000000000000000000000000000000000000",
ad1=  "AD:1000000000000000000000000000000000000001",
rhid0="HID:0000000000000000000000000000000000000002",
rhid1="HID:0000000000000000000000000000000000000003",
sid0= "SID:0f00000000000000000000000000000000000055",
sid1= "SID:1f10000001111111111111111111111110000056",
sid_stock= "SID:0f03333333333333333333333333330000000055")

HID0= XIDS['hid0'] # "HID:0000000000000000000000000000000000000000"
HID1= XIDS['hid1']#"HID:0000000000000000000000000000000000000001"
AD0=  XIDS['ad0'] #"AD:1000000000000000000000000000000000000000"
AD1=  XIDS['ad1'] #"AD:1000000000000000000000000000000000000001"
RHID0=XIDS['rhid0'] #"HID:0000000000000000000000000000000000000002"
RHID1=XIDS['rhid1'] #"HID:0000000000000000000000000000000000000003"
SID0= XIDS['sid0'] #"SID:0f00000000000000000000000000000000000055"
SID1= XIDS['sid1'] #"SID:1f10000001111111111111111111111110000056"
SID_STOCK= XIDS['sid_stock']#"SID:0f03333333333333333333333333330000000055"

IP1 = "IP:128.2.208.167"
IP0 = "IP:128.2.208.168"



# Note: this is temporary. Eventually URLs will be formatted
# differently and name translation will use DNS
#
# Fallbacks are very limited; for a primary intent of an SID,
# the fallback must be AD:HID:SID; likewise, for CID, fallback
# must be AD:HID:CID
def dag_from_url(url): 
    #url_segments[0] = primary intent
    #url_segments[1] = fallback (if supplied)
    url_segments = url.split('.fallback.')

    # First segment is primary intent
    if url_segments[0][0:3] == 'sid':
        primary_XID = xid_from_name(url_segments[0][4:])
    elif url_segments[0][0:3] == 'cid':
        primary_XID = xid_from_name(url_segments[0][4:])
    else:
        print 'ERROR: dag_from_url: unsupported primary intent principal type'
        return

    # Check if a fallback was supplied
    # NOTE: this is very brittle; once we settle on
    # a URL format, we need better code here
    if len(url_segments) > 1:
        # Fallback provided
        #url_segments[1] = url_segments[1:-1] # strip off parens

        #fallback_segments[0] = 'ad'
        #fallback_segments[1] = 'ad_name'
        #fallback_segments[2] = 'hid'
        #fallback_segments[3] = 'hid_name'
        #fallback_segments[4] = 'cid' or 'sid'
        #fallback_segments[5] = 'cid/sid name'
        fallback_segments = url_segments[1].split('.')

        ad = xid_from_name(fallback_segments[1])
        hid = xid_from_name(fallback_segments[3])

        # NOTE: we are hard-coding in IP1 node even though it's not specified in URL
        return "DAG 3 0 1 - \n %s 2 - \n %s 2 - \n %s 3 - \n %s" % (ad, IP1, hid, primary_XID)
    else:
        # No fallback
        return 'RE %s' % primary_XID
    
def xid_from_name(name):
   return XIDS[name] # TODO check if name is in dict
