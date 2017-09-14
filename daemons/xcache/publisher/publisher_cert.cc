#include "publisher_cert.h"

PublisherCert::PublisherCert(std::string certpath)
{
	char rootdir[MAX_KEYDIR_PATH_LEN];
	std::string ca_file = "/ca/ca.cert";

	_certpath = certpath;

	// CA cert must be in xia-core/ca/ca.cert
	if(XrootDir((char *)rootdir, MAX_KEYDIR_PATH_LEN) == NULL) {
		std::cout << "Publisher couldn't find XIA root directory" << std::endl;
		assert(-1);
	}

	// Protect against buffer overflow
	assert(strlen(rootdir) + ca_file.size() < MAX_KEYDIR_PATH_LEN);

	// Everything checked out, we have a CA certificate
	_cafilepath = strcat(rootdir, ca_file.c_str());

	// Verify provided cert and CA cert are regular files we can read
	if(!isRegularFile(_cafilepath) || !isRegularFile(certpath)) {
		std::cout << "Publisher or CA cert argument invalid" << std::endl;
		assert(-1);
	}
}

bool PublisherCert::isRegularFile(std::string path)
{
	struct stat statbuf;

	// Verify that the path points to a regular file
	if(stat(path.c_str(), &statbuf)) {
		std::cout << path << ": Not found" << std::endl;
		return false;
	}
	if(!S_ISREG(statbuf.st_mode)) {
		std::cout << path << ": Not a regular file" << std::endl;
		return false;
	}
	return true;
}

bool PublisherCert::is_valid()
{
	bool ret = false;
	int state = 0;
	X509_STORE *cert_store = NULL;
	X509_LOOKUP *cert_lookup = NULL;

	cert_store=X509_STORE_new();
	if (cert_store == NULL) {
		goto verify_done;
	}
	state = 1; // cert_store must be freed

	OpenSSL_add_all_algorithms();

	// Prepare to add an x509 cert for the CA to lookup context
	cert_lookup = X509_STORE_add_lookup(cert_store, X509_LOOKUP_file());
	if (cert_lookup == NULL) {
		goto verify_done;
	}

	// Include CA x509 certificate to the lookup context
	if(!X509_LOOKUP_load_file(
				cert_lookup, _cafilepath.c_str(), X509_FILETYPE_PEM)) {
		goto verify_done;
	}

	// Prepare to add a directory containing hashes
	cert_lookup = X509_STORE_add_lookup(
			cert_store, X509_LOOKUP_hash_dir());
	if (cert_lookup == NULL) {
		goto verify_done;
	}

	// Add all valid x509 certificates from the default hash directory
	X509_LOOKUP_add_dir(cert_lookup, NULL, X509_FILETYPE_DEFAULT);

	ret = check(cert_store);

verify_done:
	switch(state) {
		case 1:
			X509_STORE_free(cert_store);
	}

	return ret;
}

X509 *PublisherCert::load()
{
    X509 *x = NULL;
    BIO *cert;
	int state = 0;

    if ((cert=BIO_new(BIO_s_file())) == NULL) {
        goto load_cert_done;
	}
	state = 1;

    if (BIO_read_filename(cert, _certpath.c_str()) <= 0) {
        goto load_cert_done;
	}

    x = PEM_read_bio_X509_AUX(cert, NULL, NULL, NULL);

load_cert_done:
	switch(state) {
		case 1:
			BIO_free(cert);
	}
    return x;
}

bool PublisherCert::check(X509_STORE *store)
{
	X509 *x = NULL;
	bool ret = false;
	int state = 0;
	X509_STORE_CTX *context;

	x = load();
	if (x == NULL) {
		goto check_done;
	}
	state = 1;

	context = X509_STORE_CTX_new();
	if (context == NULL) {
		goto check_done;
	}
	state = 2;

	X509_STORE_set_flags(store, 0);
	if(!X509_STORE_CTX_init(context, store, x, 0)) {
		goto check_done;
	}

	if(X509_verify_cert(context) == 1) {
		ret = true;
	}

check_done:
	switch(state) {
		case 2:
			X509_STORE_CTX_free(context);
		case 1:
			X509_free(x);
	}

	return ret;
}

bool PublisherCert::extract_pubkey()
{
	bool retval = false;
	int ret;
	int state = 0;
	BIO *certbio = NULL;
	BIO *outbio = NULL;
	X509 *cert = NULL;
	EVP_PKEY *pubkey = NULL;

	OpenSSL_add_all_algorithms();
	// Pubkeypath = certpath - strlen(".cert") + ".pub"
	std::string pubkeypath = _certpath.substr(0, _certpath.size()-5);
	pubkeypath += ".pub";

	FILE *pubkeyfile = fopen(pubkeypath.c_str(), "w");
	if(!pubkeyfile) {
		std::cout << "ERROR writing to " << pubkeypath << std::endl;
		goto extract_pubkey_done;
	}
	state = 1;

	certbio = BIO_new(BIO_s_file());
	if(certbio == NULL) {
		std::cout << "ERROR creating certbio" << std::endl;
		goto extract_pubkey_done;
	}
	state = 2;

	outbio = BIO_new_fp(pubkeyfile, BIO_NOCLOSE);
	if(outbio == NULL) {
		std::cout << "ERROR creating outbio" << std::endl;
		goto extract_pubkey_done;
	}
	state = 3;

	ret = BIO_read_filename(certbio, _certpath.c_str());
	if(ret != 1) {
		std::cout << "ERROR setting BIO to read cert" << std::endl;
		goto extract_pubkey_done;
	}

	cert = PEM_read_bio_X509(certbio, NULL, 0, NULL);
	if(cert == NULL) {
		std::cout << "ERROR unable to read cert: " << _certpath << std::endl;
		goto extract_pubkey_done;
	}
	state = 4;

	pubkey = X509_get_pubkey(cert);
	if(pubkey == NULL) {
		std::cout << "ERROR extracting pubkey from cert" << std::endl;
		goto extract_pubkey_done;
	}
	state = 5;

	if(!PEM_write_bio_PUBKEY(outbio, pubkey)) {
		std::cout << "ERROR writing out pubkey" << std::endl;
		goto extract_pubkey_done;
	}

	// Public key successfully written to file
	retval = true;

extract_pubkey_done:
	switch(state) {
		case 5: EVP_PKEY_free(pubkey);
		case 4: X509_free(cert);
		case 3: BIO_free_all(outbio);
		case 2: BIO_free_all(certbio);
		case 1: fclose(pubkeyfile);
	}
	return retval;
}
