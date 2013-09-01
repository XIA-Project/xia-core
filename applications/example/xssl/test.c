#include "xssl.h"

int main() {
	XSSL_CTX *ctx = XSSL_CTX_new();
	if (ctx == NULL) {
		printf("XSSL_CTX is null\n");
		return -1;
	}

	print_keypair(ctx->keypair);
	char *sid = (char*)SID_from_keypair(ctx->keypair);
	printf("SID is: %s\n", sid);

	XSSL_CTX_free(ctx);
	free(sid);
}
