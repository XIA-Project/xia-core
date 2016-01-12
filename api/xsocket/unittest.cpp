#include "Xsocket.h"
#include "dagaddr.hpp"
#include "gtest/gtest.h"

/*
** FIXME:
** the getaddrinfo tests will fail with the wrong number of nodes if the host running the test
** has 4ids. We may need to do a test for whether we are on a dualstack router and adjust the
** expected counts in the tests.
**
** Need to make a decision on the correct return values for some of the getaddrinfo tests
*/

#define XID_LEN 50 // 40 bytes plus space for HID: or whatever

// for XregisterName and XgetDAGbyName
#define TEST_DAG "RE AD:1122334455667788990011223344556677889900 HID:1111222233334444555566667777888899990000 SID:1234567890123456789012345678901234567890"
#define TEST_NAME "test.name"
#define BAD_NAME "nonexistant.name"

// for Xgetaddrinfo
#define FULL_DAG	"RE %s %s %s"
#define HOST_DAG	"RE %s %s"
#define FULL_NAME	"fullname.test.xia"
#define HOST_NAME	"test.xia"
#define SID			"SID:0987654321098765432109876543210987654321"


// Xsocket *******************************************************************
TEST(Xsocket, Stream)
{
	int sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	EXPECT_GT(sock, -1);
	Xclose(sock);
}

TEST(Xsocket, Dgram)
{
	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	EXPECT_GT(sock, -1);
	Xclose(sock);
}

TEST(Xsocket, Chunk)
{
	int sock = Xsocket(AF_XIA, XSOCK_CHUNK, 0);
	EXPECT_GT(sock, -1);
	Xclose(sock);
}

TEST(Xsocket, Raw)
{
	int sock = Xsocket(AF_XIA, SOCK_RAW, 0);
	EXPECT_GT(sock, -1);
	Xclose(sock);
}

TEST(Xsocket, InvalidType)
{
	int sock = Xsocket(AF_XIA, 7, 0);
	EXPECT_EQ(sock, -1);
}

TEST(Xsocket, InvalidFamily)
{
	int sock = Xsocket(AF_INET, SOCK_STREAM, 0);
	EXPECT_EQ(sock, -1);
}

TEST(Xsocket, InvalidProtocol)
{
	int sock = Xsocket(AF_XIA, SOCK_STREAM, 10);
	EXPECT_EQ(sock, -1);
}

// Xclose *********************************************************************
TEST(Xclose, ValidSocket)
{
	int sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	EXPECT_EQ(0, Xclose(sock));
}

TEST(Xclose, InvalidSocket)
{
	EXPECT_EQ(-1, Xclose(0));
}

// Xgetsockopt / Xsetsockopt ***********************************************
class XsockoptTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	}

	virtual void TearDown() {
		Xclose(sock);
	}

	void integerSet(int opt, int val)
	{
		EXPECT_EQ(0, Xsetsockopt(sock, opt, (const void *)&val, sizeof(val)));
	}

	void integerSetInvalid(int opt, int val)
	{
		EXPECT_EQ(-1, Xsetsockopt(sock, opt, (const void *)&val, sizeof(val)));
	}

	void integerGet(int opt, int val)
	{
		int s = Xsocket(AF_XIA, SOCK_DGRAM, 0);
		socklen_t sz = sizeof(int);
		int v = 999;
		EXPECT_EQ(0, Xsetsockopt(sock, opt, (const void *)&val, sz));
		EXPECT_EQ(0, Xgetsockopt(sock, opt, (void *)&v, &sz));
		EXPECT_EQ(sizeof(int), sz);
		EXPECT_EQ(v, val);
		Xclose(s);
	}


	int sock;
};


TEST_F(XsockoptTest, Set_InvalidSocket)
{
	int val = 1;
	socklen_t sz = sizeof(val);
	EXPECT_EQ(-1, Xsetsockopt(1, SO_DEBUG, (const void *)&val, sz));
}

TEST_F(XsockoptTest, Set_InvalidOpt)
{
	int val;
	EXPECT_EQ(-1, Xsetsockopt(sock, 999999999, (const void *)&val, sizeof(val)));
}

TEST_F(XsockoptTest, Get_InvalidOpt)
{
	int val;
	socklen_t sz;
	EXPECT_EQ(-1, Xgetsockopt(sock, 999999999, (void *)&val, &sz));
}

TEST_F(XsockoptTest, Set_NullValue)
{
	int val;
	EXPECT_EQ(-1, Xsetsockopt(sock, SO_DEBUG, NULL, sizeof(val)));
}

TEST_F(XsockoptTest, Get_NullValue)
{
	socklen_t sz;
	EXPECT_EQ(-1, Xgetsockopt(sock, SO_DEBUG, NULL, &sz));
}

TEST_F(XsockoptTest, Get_NullSize)
{
	int val;
	EXPECT_EQ(-1, Xgetsockopt(sock, SO_DEBUG, (void *)&val, NULL));
}

TEST_F(XsockoptTest, SetSmallSize)
{
	int val = 1;
	EXPECT_EQ(-1, Xsetsockopt(sock, SO_DEBUG, (const void *)&val, 1));
}

TEST_F(XsockoptTest, SetLargeSize)
{
	int val = 1;
	EXPECT_EQ(0, Xsetsockopt(sock, SO_DEBUG, (const void *)&val, 10));
}

TEST_F(XsockoptTest, GetSmallSize)
{
	int val = 1;
	socklen_t sz = 1;
	EXPECT_EQ(-1, Xgetsockopt(sock, SO_DEBUG, (void *)&val, &sz));
	EXPECT_EQ(sizeof(int), sz);
}

TEST_F(XsockoptTest, GetLargeSize)
{
	int val = 1;
	socklen_t sz = 10;
	EXPECT_EQ(0, Xgetsockopt(sock, SO_DEBUG, (void *)&val, &sz));
	EXPECT_EQ(sizeof(int), sz);
}

TEST_F(XsockoptTest, SO_DEBUG_Set)
{
	integerSet(SO_DEBUG, 1);
}

TEST_F(XsockoptTest, SO_DEBUG_Get)
{
	integerGet(SO_DEBUG, 2);
}

TEST_F(XsockoptTest, SO_ERROR_Set)
{
	integerSet(SO_ERROR, 1);
}

TEST_F(XsockoptTest, SO_ERROR_Get)
{
	integerGet(SO_ERROR, 5);
}

TEST_F(XsockoptTest, SO_TYPE_Set)
{
	int val = 1;
	socklen_t sz = sizeof(val);
	EXPECT_EQ(-1, Xsetsockopt(sock, SO_TYPE, (const void *)&val, sz));
}

TEST_F(XsockoptTest, SO_TYPE_Get)
{
	int val;
	socklen_t sz = sizeof(val);
	EXPECT_EQ(0, Xgetsockopt(sock, SO_TYPE, (void *)&val, &sz));
	EXPECT_EQ(SOCK_DGRAM, val);
}

TEST_F(XsockoptTest, XOPT_HLIM_Set)
{
	integerSet(XOPT_HLIM, 0);
	integerSet(XOPT_HLIM, 255);
}

TEST_F(XsockoptTest, XOPT_HLIM_SetInvalid)
{
	integerSetInvalid(XOPT_HLIM, -1);
	integerSetInvalid(XOPT_HLIM, 256);
}

TEST_F(XsockoptTest, XOPT_HLIM_Get)
{
	integerGet(XOPT_HLIM, 20);
}

TEST_F(XsockoptTest, XOPT_NEXT_PROTO_Set)
{
	integerSet(XOPT_HLIM, XPROTO_XCMP);
}

TEST_F(XsockoptTest, XOPT_NEXT_PROTO_SetInvalid)
{
	integerSetInvalid(XOPT_HLIM, 1234);
}

TEST_F(XsockoptTest, XOPT_NEXT_PROTO_Get)
{
	integerGet(XOPT_HLIM, XPROTO_XCMP);
}


// XreadLocalHostAddr *********************************************************
TEST(XreadLocalHostAddr, ValidParameters)
{
	char ad[XID_LEN], hid[XID_LEN], fid[XID_LEN];
	int sock = Xsocket(AF_XIA, XSOCK_STREAM, 0);
	int rc = XreadLocalHostAddr(sock, ad, XID_LEN, hid, XID_LEN, fid, XID_LEN);
	ASSERT_EQ(0, rc);
	EXPECT_STRNE(ad, "");
	EXPECT_STRNE(hid, "");
	EXPECT_STRNE(fid, "");
	EXPECT_EQ(0, strncmp(ad, "AD:", 3));
	EXPECT_EQ(0, strncmp(hid, "HID:", 4));
	EXPECT_EQ(0, strncmp(fid, "IP:", 3));
}

TEST(XreadLocalHostAddr, ShortBuffers)
{
	char ad[XID_LEN], hid[XID_LEN], fid[XID_LEN];
	int sock = Xsocket(AF_XIA, XSOCK_STREAM, 0);
	int rc = XreadLocalHostAddr(sock, ad, 10, hid, 10, fid, 10);
	ASSERT_EQ(0, rc);
	EXPECT_STRNE(ad, "");
	EXPECT_STRNE(hid, "");
	EXPECT_STRNE(fid, "");
}

TEST(XreadLocalHostAddr, InvalidSocket)
{
	char ad[XID_LEN], hid[XID_LEN], fid[XID_LEN];
	int rc = XreadLocalHostAddr(0, ad, XID_LEN, hid, XID_LEN, fid, XID_LEN);
	ASSERT_EQ(-1, rc);
}

TEST(XreadLocalHostAddr, InvalidPointers)
{
	int sock = Xsocket(AF_XIA, XSOCK_STREAM, 0);
	int rc = XreadLocalHostAddr(sock, NULL, XID_LEN, NULL, XID_LEN, NULL, XID_LEN);
	ASSERT_EQ(-1, rc);
	Xclose(sock);
}

// XregisterName *************************************************************
// NOTE: these tests assume the DAG library generates valid sockaddr_x structures
//  also, they don't check invalid XIDs and other bad input as the dag library
//  should be handling that
TEST(XregisterName, ValidParameters)
{
	sockaddr_x sa;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	EXPECT_EQ(0, XregisterName(TEST_NAME, &sa));
}

TEST(XregisterName, NullName)
{
	sockaddr_x sa;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	EXPECT_EQ(-1, XregisterName(NULL, &sa));
}

TEST(XregisterName, EmptyName)
{
	sockaddr_x sa;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	EXPECT_EQ(-1, XregisterName("", &sa));
}

TEST(XregisterName, NullSockaddr)
{
	EXPECT_EQ(-1, XregisterName(TEST_NAME, NULL));
}

TEST(XregisterName, BadFamily)
{
	sockaddr_x sa;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	sa.sx_family = AF_INET;
	EXPECT_EQ(-1, XregisterName(TEST_NAME, &sa));
}

TEST(XregisterName, NoNodes)
{
	sockaddr_x sa;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	sa.sx_addr.s_count = 0;
	EXPECT_EQ(-1, XregisterName(TEST_NAME, &sa));
}

// XgetDAGbyName **************************************************************
TEST(XgetDAGbyName, Valid)
{
	sockaddr_x sa;
	socklen_t len = sizeof(sa);
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	XregisterName(TEST_NAME, &sa);
	memset(&sa, 0, sizeof(sa));
	EXPECT_EQ(0, XgetDAGbyName(TEST_NAME, &sa, &len));
	EXPECT_EQ(len, sizeof(sa));
	Graph g1(&sa);
	// TEST_DAG was in form of AD:... HID:... SID:...
	EXPECT_EQ(3, g1.num_nodes());
}

TEST(XgetDAGbyName, ExtraLen)
{
	sockaddr_x sa;
	socklen_t len = sizeof(sa) + 10;
	Graph g(TEST_DAG);
	g.fill_sockaddr(&sa);
	XregisterName(TEST_NAME, &sa);
	memset(&sa, 0, sizeof(sa));
	EXPECT_EQ(0, XgetDAGbyName(TEST_NAME, &sa, &len));
	EXPECT_EQ(len, sizeof(sa));
	Graph g1(&sa);
	// TEST_DAG was in form of AD:... HID:... SID:...
	EXPECT_EQ(3, g1.num_nodes());
}

TEST(XgetDAGbyName, NullName)
{
	sockaddr_x sa;
	socklen_t len = sizeof(sa);
	EXPECT_EQ(-1, XgetDAGbyName(NULL, &sa, &len));
}

TEST(XgetDAGbyName, NoName)
{
	sockaddr_x sa;
	socklen_t len = sizeof(sa);
	EXPECT_EQ(-1, XgetDAGbyName("", &sa, &len));
}

TEST(XgetDAGbyName, NullSockaddr)
{
	socklen_t len = sizeof(sockaddr_x);
	EXPECT_EQ(-1, XgetDAGbyName(TEST_NAME, NULL, &len));
}

TEST(XgetDAGbyName, NullLen)
{
	sockaddr_x sa;
	EXPECT_EQ(-1, XgetDAGbyName(TEST_NAME, &sa, NULL));
}

TEST(XgetDAGbyName, ShortLen)
{
	sockaddr_x sa;
	socklen_t len = 10;
	EXPECT_EQ(-1, XgetDAGbyName(TEST_NAME, &sa, &len));
}

TEST(XgetDAGbyName, BadName)
{
	sockaddr_x sa;
	socklen_t len = sizeof(sa);
	EXPECT_EQ(-1, XgetDAGbyName(BAD_NAME, &sa, &len));
}

// Xgetaddrinfo ***************************************************************

class XgetaddrinfoTest : public ::testing::Test {
protected:
	virtual void SetUp() {
		sockaddr_x sa;

		int sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
		XreadLocalHostAddr(sock, ad, XID_LEN, hid, XID_LEN, fid, XID_LEN);
		sprintf(fdag, FULL_DAG, ad, hid, SID);
		Graph gf(fdag);
		gf.fill_sockaddr(&sa);
		XregisterName(FULL_NAME, &sa);

		sprintf(hdag, HOST_DAG, ad, hid);
		Graph gh(hdag);
		gh.fill_sockaddr(&sa);
		XregisterName(HOST_NAME, &sa);
		Xclose(sock);

		nad = new Node(ad);
		nhid = new Node(hid);
		nsid = new Node(SID);

		ai = NULL;
	}

	virtual void TearDown() {
		delete nad;
		delete nhid;
		delete nsid;
		if (ai)
			Xfreeaddrinfo(ai);
	}

	struct addrinfo *ai;
	char fdag[512], hdag[512];
	char ad[XID_LEN], hid[XID_LEN], fid[XID_LEN];
	Node *nad, *nhid, *nsid;
};

TEST_F(XgetaddrinfoTest, FullDag)
{
	sockaddr_x *sa;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, NULL, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, HostDag)
{
	sockaddr_x *sa;
	ASSERT_EQ(0, Xgetaddrinfo(HOST_NAME, NULL, NULL, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(2, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, HostPlusSid)
{
	// FIXME: should this fail if the hints struct isn't configured?
	sockaddr_x *sa;
	ASSERT_EQ(0, Xgetaddrinfo(HOST_NAME, SID, NULL, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
	EXPECT_EQ(g.get_node(2).id_string(), nsid->id_string());
}

TEST_F(XgetaddrinfoTest, EmptyName)
{
	// FIXME: should this act the same as a NULL name?
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV;
	ASSERT_NE(Xgetaddrinfo("", NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, EmptyHints)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, EmptyHintsPlusSid)
{
	// FIXME: same question as HostPlusSid above
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	ASSERT_EQ(0, Xgetaddrinfo(HOST_NAME, SID, &hints, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, SidInHints)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV;
	ASSERT_EQ(0, Xgetaddrinfo(HOST_NAME, SID, &hints, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	EXPECT_EQ(AF_XIA, sa->sx_family);
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, HintsWithNullSid)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV;
	ASSERT_NE(Xgetaddrinfo(HOST_NAME, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, StreamHints)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
}

TEST_F(XgetaddrinfoTest, DgramHints)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
}

TEST_F(XgetaddrinfoTest, ChunkHints)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = XSOCK_CHUNK;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
}

TEST_F(XgetaddrinfoTest, RawHints)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_RAW;
	ASSERT_NE(Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, Protocol)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_protocol = 1;
	ASSERT_NE(Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, ValidFamily)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_XIA;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
}

TEST_F(XgetaddrinfoTest, InvalidFamily)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	ASSERT_NE(Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, UnspecFamily)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	ASSERT_EQ(0, Xgetaddrinfo(FULL_NAME, NULL, &hints, &ai));
}

TEST_F(XgetaddrinfoTest, CanonNameError)
{
	// can't be set with a NULL name
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	ASSERT_NE(Xgetaddrinfo(NULL, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, NullNameDagInHints)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_DAGHOST;
	ASSERT_NE(Xgetaddrinfo(NULL, NULL, &hints, &ai), 0);
}

TEST_F(XgetaddrinfoTest, DagInHints)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_DAGHOST;
	ASSERT_EQ(Xgetaddrinfo(hdag, NULL, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(2, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, DagInHintsiPlusSid)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_DAGHOST | XAI_XIDSERV;
	ASSERT_EQ(Xgetaddrinfo(hdag, SID, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
}

TEST_F(XgetaddrinfoTest, LocalAddr)
{
	sockaddr_x *sa;
	ASSERT_EQ(Xgetaddrinfo(NULL, NULL, NULL, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(2, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
}

TEST_F(XgetaddrinfoTest, LocalAddrPlusPassive)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	ASSERT_EQ(Xgetaddrinfo(NULL, NULL, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(2, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
}

TEST_F(XgetaddrinfoTest, LocalAddrPlusPassivePlusSid)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE | XAI_XIDSERV;
	ASSERT_EQ(Xgetaddrinfo(NULL, SID, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
	EXPECT_EQ(g.get_node(2).id_string(), nsid->id_string());
}

TEST_F(XgetaddrinfoTest, LocalAddrPlusSid)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV;
	ASSERT_EQ(Xgetaddrinfo(NULL, SID, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
	EXPECT_EQ(g.get_node(2).id_string(), nsid->id_string());
}

TEST_F(XgetaddrinfoTest, LocalAddrPlusFallback)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_FALLBACK;
	ASSERT_EQ(0, Xgetaddrinfo(NULL, NULL, &hints, &ai));
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(2, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
}

TEST_F(XgetaddrinfoTest, LocalAddrPlusFallbackPlusSid)
{
	struct addrinfo hints;
	sockaddr_x *sa;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = XAI_XIDSERV | XAI_FALLBACK;
	ASSERT_EQ(Xgetaddrinfo(NULL, SID, &hints, &ai), 0);
	sa = (sockaddr_x*)ai->ai_addr;
	Graph g(sa);
	EXPECT_EQ(3, g.num_nodes());
	EXPECT_EQ(g.get_node(0).id_string(), nad->id_string());
	EXPECT_EQ(g.get_node(1).id_string(), nhid->id_string());
	EXPECT_EQ(g.get_node(2).id_string(), nsid->id_string());
}




int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
