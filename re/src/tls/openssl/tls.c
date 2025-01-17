/**
 * @file openssl/tls.c TLS backend using OpenSSL
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <re_types.h>
#include <re_fmt.h>
#include <re_mem.h>
#include <re_mbuf.h>
#include <re_main.h>
#include <re_sa.h>
#include <re_net.h>
#include <re_srtp.h>
#include <re_sys.h>
#include <re_tcp.h>
#include <re_tls.h>
#include "tls.h"


#define DEBUG_MODULE "tls"
#define DEBUG_LEVEL 5
#include <re_dbg.h>


#if defined(TRACE_SSL) && (OPENSSL_VERSION_NUMBER >= 0x10101000L)
/**
 * Global flag if key material must be appended to file
 */
static bool fresh_keylog_file = true;


/**
 * SSL Key logger callback function
 *
 * @param ssl  OpenSSL SSL object
 * @param line Key material in NSS format
 */
static void tls_keylogger_cb(const SSL *ssl,
	const char *line)
{
	FILE *f = NULL;

	(void) ssl;

	if (fresh_keylog_file) {
		f = fopen(TRACE_SSL, "w");
		fresh_keylog_file = false;
	}
	else {
		f = fopen(TRACE_SSL, "a");
	}

	if (!f)
		return;

	(void)re_fprintf(f, "%s\n", line);

	if (f)
		(void)fclose(f);

}
#endif


/* NOTE: shadow struct defined in tls_*.c */
struct tls_conn {
	SSL *ssl;
	struct tls *tls;
};


static void destructor(void *data)
{
	struct tls *tls = data;

	if (tls->ctx)
		SSL_CTX_free(tls->ctx);

	if (tls->cert)
		X509_free(tls->cert);

	mem_deref(tls->pass);
}


/*The password code is not thread safe*/
static int password_cb(char *buf, int size, int rwflag, void *userdata)
{
	struct tls *tls = userdata;

	(void)rwflag;

	DEBUG_NOTICE("password callback\n");

	if (size < (int)strlen(tls->pass)+1)
		return 0;

	strncpy(buf, tls->pass, size);

	return (int)strlen(tls->pass);
}


static int keytype2int(enum tls_keytype type)
{
	switch (type) {
	case TLS_KEYTYPE_EC:
		return EVP_PKEY_EC;
	case TLS_KEYTYPE_RSA:
		return EVP_PKEY_RSA;
	default:
		return EVP_PKEY_NONE;
	}
}


#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
	!defined(LIBRESSL_VERSION_NUMBER)
static int verify_handler(int ok, X509_STORE_CTX *ctx)
{
	int err, depth;
	char    buf[128];
	X509   *err_cert;

	err = X509_STORE_CTX_get_error(ctx);

#if 1 //(DEBUG_LEVEL >= 6)
	err_cert = X509_STORE_CTX_get_current_cert(ctx);

	X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 128);
	DEBUG_WARNING("TLS subject_name = %s\n", buf);

	X509_NAME_oneline(X509_get_issuer_name(err_cert), buf, 128);
	DEBUG_WARNING("TLS issuer_name  = %s\n", buf);
#endif

	depth = X509_STORE_CTX_get_error_depth(ctx);
	
	if (err) {
		DEBUG_WARNING("%s: depth = %d, err = %d (%s)\n", __FUNC__, depth, err, X509_verify_cert_error_string(err));
	}

#if 1 //(DEBUG_LEVEL >= 6)
	DEBUG_WARNING("TLS depth %d verify ok = %d\n", depth, ok);
#endif

	return ok;
}
#endif


/**
 * Allocate a new TLS context
 *
 * @param tlsp    Pointer to allocated TLS context
 * @param method  TLS method
 * @param keyfile Optional private key file
 * @param pwd     Optional password
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_alloc(struct tls **tlsp, enum tls_method method, const char *keyfile,
	      const char *pwd)
{
	struct tls *tls;
	int r, err;

	if (!tlsp)
		return EINVAL;

	tls = mem_zalloc(sizeof(*tls), destructor);
	if (!tls)
		return ENOMEM;

	tls->verify_server = true;
	switch (method) {

	case TLS_METHOD_SSLV23:
		tls->ctx = SSL_CTX_new(SSLv23_method());
		break;

#ifdef USE_OPENSSL_DTLS
	case TLS_METHOD_DTLSV1:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
	!defined(LIBRESSL_VERSION_NUMBER)

		tls->ctx = SSL_CTX_new(DTLS_method());
#else
		tls->ctx = SSL_CTX_new(DTLSv1_method());
#endif
		break;

#ifdef SSL_OP_NO_DTLSv1_2
		/* DTLS v1.2 is available in OpenSSL 1.0.2 and later */

	case TLS_METHOD_DTLS:
		tls->ctx = SSL_CTX_new(DTLS_method());
		break;

	case TLS_METHOD_DTLSV1_2:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
	!defined(LIBRESSL_VERSION_NUMBER)

		tls->ctx = SSL_CTX_new(DTLS_method());
#else
		tls->ctx = SSL_CTX_new(DTLSv1_2_method());
#endif
		break;
#endif

#endif

	default:
		DEBUG_WARNING("tls method %d not supported\n", method);
		err = ENOSYS;
		goto out;
	}

	if (!tls->ctx) {
		ERR_clear_error();
		err = ENOMEM;
		goto out;
	}

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(tls->ctx, 1);
#endif

#if defined(TRACE_SSL) && (OPENSSL_VERSION_NUMBER >= 0x10101000L)
	SSL_CTX_set_keylog_callback(tls->ctx, tls_keylogger_cb);
#endif

	/* Load our keys and certificates */
	if (keyfile) {
		if (pwd) {
			err = str_dup(&tls->pass, pwd);
			if (err)
				goto out;

			SSL_CTX_set_default_passwd_cb(tls->ctx, password_cb);
			SSL_CTX_set_default_passwd_cb_userdata(tls->ctx, tls);
		}

		r = SSL_CTX_use_certificate_chain_file(tls->ctx, keyfile);
		if (r <= 0) {
			DEBUG_WARNING("Can't read certificate file: %s (%d)\n",
				      keyfile, r);
			ERR_clear_error();
			err = EINVAL;
			goto out;
		}

		r = SSL_CTX_use_PrivateKey_file(tls->ctx, keyfile,
						SSL_FILETYPE_PEM);
		if (r <= 0) {
			DEBUG_WARNING("Can't read key file: %s (%d)\n",
				      keyfile, r);
			ERR_clear_error();
			err = EINVAL;
			goto out;
		}
	}

	err = 0;
 out:
	if (err)
		mem_deref(tls);
	else
		*tlsp = tls;

	return err;
}

#if defined(WIN32) || defined(__WIN32__)
int tls_add_windows_ca(struct tls *tls)
{
	X509_STORE *store;
	HCERTSTORE hStore;
    int count = 0;

	if (!tls || !tls->ctx)
		return EINVAL;

	store = SSL_CTX_get_cert_store(tls->ctx);
	hStore = CertOpenSystemStore(NULL, "ROOT");

    if(hStore) {
		PCCERT_CONTEXT pContext = NULL;
		FILETIME now;

        GetSystemTimeAsFileTime(&now);		
		
		for(;;) {
			X509 *x509;
            BYTE key_usage[2];
            DWORD req_size;
            const unsigned char *encoded_cert;
			char cert_name[256];

            pContext = CertEnumCertificatesInStore(hStore, pContext);
            if(!pContext)
                break;

			if(!CertGetNameStringA(pContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, cert_name, sizeof(cert_name))) {
                strcpy(cert_name, "Unknown");
            }
			DEBUG_INFO("TLS: Checking Windows store cert [%s]\n", cert_name);

            encoded_cert = (const unsigned char *)pContext->pbCertEncoded;
            if(!encoded_cert)
                continue;

            if(CompareFileTime(&pContext->pCertInfo->NotBefore, &now) > 0 || CompareFileTime(&now, &pContext->pCertInfo->NotAfter) > 0)
                continue;

            /* If key usage exists check for signing attribute */
            if(CertGetIntendedKeyUsage(pContext->dwCertEncodingType,
                                       pContext->pCertInfo,
                                       key_usage, sizeof(key_usage))) {
                if(!(key_usage[0] & CERT_KEY_CERT_SIGN_KEY_USAGE))
                    continue;
            } else if(GetLastError())
                continue;

            x509 = d2i_X509(NULL, &encoded_cert, pContext->cbCertEncoded);
            if(!x509)
                continue;

            if(X509_STORE_add_cert(store, x509) == 1) {
				DEBUG_INFO("TLS: Imported cert [%s] from Windows store\n", cert_name);
				count++;
			}
            X509_free(x509);
        }

        CertFreeCertificateContext(pContext);
		CertCloseStore(hStore, 0);
	}
	DEBUG_WARNING("TLS: add %d certificate(s) from Windows store\n", count);
	return 0;
}
#endif

/**
 * Set default locations for trusted CA certificates
 *
 * @param tls    TLS Context
 * @param cafile PEM file with CA certificates
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_add_ca(struct tls *tls, const char *cafile)
{
	return tls_add_cafile_path(tls, cafile, NULL);
}


/**
 * Set default file and path for trusted CA certificates
 *
 * @param tls    TLS Context
 * @param cafile PEM file with CA certificate(s)
 * @param capath Path containing CA certificates files
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_add_cafile_path(struct tls *tls, const char *cafile,
	const char *capath)
{
	if (!tls || (!cafile && !capath) || !tls->ctx)
		return EINVAL;

	if (capath && !fs_isdir(capath)) {
		DEBUG_WARNING("capath is not a directory\n");
		return ENOTDIR;
	}

	/* Load the CAs we trust */
	if (!(SSL_CTX_load_verify_locations(tls->ctx, cafile, capath))) {
		if (str_isset(cafile))
			DEBUG_WARNING("Can't read CA file: %s\n", cafile);

		ERR_clear_error();
		return ENOENT;
	}

	return 0;
}


/**
 * Add trusted CA certificates given as string.
 *
 * @param tls    TLS Context
 * @param capem  The trusted CA as null-terminated string given in PEM format.
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_add_capem(struct tls *tls, const char *capem)
{
	X509_STORE *store;
	X509 *x509;
	BIO *bio;
	int ok;
	int err = 0;

	if (!tls || !capem || !tls->ctx)
		return EINVAL;

	store = SSL_CTX_get_cert_store(tls->ctx);
	if (!store)
		return EINVAL;

	bio  = BIO_new_mem_buf((char *)capem, (int)strlen(capem));
	if (!bio)
		return EINVAL;

	x509 = PEM_read_bio_X509(bio, NULL, 0, NULL);
	if (!x509) {
		err = EINVAL;
		DEBUG_WARNING("Could not read certificate capem\n");
		goto out;
	}

	ok = X509_STORE_add_cert(store, x509);
	if (!ok) {
		err = EINVAL;
		DEBUG_WARNING("Could not add certificate capem\n");
	}

out:
	X509_free(x509);
	BIO_free(bio);

	return err;
}


/**
 * Set SSL verification of the certificate purpose
 *
 * @param tls     TLS Context
 * @param purpose Certificate purpose as string
 *
 * @return int    0 if success, errorcode otherwise
 */
int tls_set_verify_purpose(struct tls *tls, const char *purpose)
{
	int err;
	int i;
	X509_PURPOSE *xptmp;

	if (!tls || !purpose)
		return EINVAL;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	i = X509_PURPOSE_get_by_sname(purpose);
#else
	i = X509_PURPOSE_get_by_sname((char *) purpose);
#endif

	if (i < 0)
		return EINVAL;

	/* purpose index -> purpose object */
	/* purpose object -> purpose value */
	xptmp = X509_PURPOSE_get0(i);
	i = X509_PURPOSE_get_id(xptmp);
	err = SSL_CTX_set_purpose(tls->ctx, i);

	return err == 1 ? 0 : EINVAL;
}


/**
 * Generate and set selfsigned certificate on TLS context
 *
 * @param tls TLS Context
 * @param cn  Common Name
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_selfsigned(struct tls *tls, const char *cn)
{
	return tls_set_selfsigned_rsa(tls, cn, 2048);
}


static int tls_generate_cert(X509 **pcert, const char *cn)
{
	X509 *cert = NULL;
	X509_NAME *subj = NULL;
	int e = 0;

	if (!pcert || !cn)
		goto err;

	cert = X509_new();
	if (!cert)
		goto err;

	if (!X509_set_version(cert, 2))
		goto err;

	if (!ASN1_INTEGER_set(X509_get_serialNumber(cert), rand_u32()))
		goto err;

	subj = X509_NAME_new();
	if (!subj)
		goto err;

	if (!X509_NAME_add_entry_by_txt(subj, "CN", MBSTRING_ASC,
					(unsigned char *)cn,
					(int)strlen(cn), -1, 0))
		goto err;

	if (!X509_set_issuer_name(cert, subj) ||
	    !X509_set_subject_name(cert, subj))
		goto err;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	if (!X509_gmtime_adj(X509_getm_notBefore(cert), -3600*24*365) ||
	    !X509_gmtime_adj(X509_getm_notAfter(cert),   3600*24*365*10))
		goto err;
#else
	if (!X509_gmtime_adj(X509_get_notBefore(cert), -3600*24*365) ||
	    !X509_gmtime_adj(X509_get_notAfter(cert),   3600*24*365*10))
		goto err;
#endif

	goto out;

 err:
	e = 1;

 out:
	if (e)
		X509_free(cert);
	else
		*pcert = cert;

	X509_NAME_free(subj);
	return e;
}


/**
 * Create a selfsigned X509 certificate using EC
 *
 * @param tls      TLS Contect
 * @param cn       Common Name
 * @param curve_n  Known EC curve name
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_selfsigned_ec(struct tls *tls, const char *cn, const char *curve_n)
{
	EC_KEY *eckey = NULL;
	EVP_PKEY *key = NULL;
	X509 *cert = NULL;
	int r, eccgrp, err = ENOMEM;

	if (!tls || !cn)
		return EINVAL;

	eccgrp = OBJ_txt2nid(curve_n);
	if (eccgrp == NID_undef)
		return ENOTSUP;

	eckey = EC_KEY_new_by_curve_name(eccgrp);
	if (!eckey)
		goto out;

	if (!EC_KEY_generate_key(eckey))
		goto out;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);
#else
	EC_KEY_set_asn1_flag(eckey, 0);
#endif

	key = EVP_PKEY_new();
	if (!key)
		goto out;

	if (!EVP_PKEY_set1_EC_KEY(key, eckey))
		goto out;

	if (tls_generate_cert(&cert, cn))
		goto out;

	if (!X509_set_pubkey(cert, key))
		goto out;

	if (!X509_sign(cert, key, EVP_sha256()))
		goto out;

	r = SSL_CTX_use_certificate(tls->ctx, cert);
	if (r != 1)
		goto out;

	r = SSL_CTX_use_PrivateKey(tls->ctx, key);
	if (r != 1)
		goto out;

	if (tls->cert)
		X509_free(tls->cert);

	tls->cert = cert;
	cert = NULL;

	err = 0;

 out:
	if (eckey)
		EC_KEY_free(eckey);

	if (key)
		EVP_PKEY_free(key);

	if (cert)
		X509_free(cert);


	return err;
}


int tls_set_selfsigned_rsa(struct tls *tls, const char *cn, size_t bits)
{
	EVP_PKEY *key = NULL;
	X509 *cert = NULL;
	BIGNUM *bn = NULL;
	RSA *rsa = NULL;
	int r, err = ENOMEM;

	if (!tls || !cn)
		return EINVAL;

	rsa = RSA_new();
	if (!rsa)
		goto out;

	bn = BN_new();
	if (!bn)
		goto out;

	BN_set_word(bn, RSA_F4);
	if (!RSA_generate_key_ex(rsa, (int)bits, bn, NULL))
		goto out;

	key = EVP_PKEY_new();
	if (!key)
		goto out;

	if (!EVP_PKEY_set1_RSA(key, rsa))
		goto out;

	if (tls_generate_cert(&cert, cn))
		goto out;

	if (!X509_set_pubkey(cert, key))
		goto out;

	if (!X509_sign(cert, key, EVP_sha1()))
		goto out;

	r = SSL_CTX_use_certificate(tls->ctx, cert);
	if (r != 1)
		goto out;

	r = SSL_CTX_use_PrivateKey(tls->ctx, key);
	if (r != 1)
		goto out;

	if (tls->cert)
		X509_free(tls->cert);

	tls->cert = cert;
	cert = NULL;

	err = 0;

 out:
	if (cert)
		X509_free(cert);

	if (key)
		EVP_PKEY_free(key);

	if (rsa)
		RSA_free(rsa);

	if (bn)
		BN_free(bn);

	if (err)
		ERR_clear_error();

	return err;
}


/**
 * Set the certificate and private key on a TLS context
 *
 * @param tls      TLS Context
 * @param cert     Certificate in PEM format
 * @param len_cert Length of certificate PEM string
 * @param key      Private key in PEM format, will be read from cert if NULL
 * @param len_key  Length of private key PEM string
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_certificate_pem(struct tls *tls, const char *cert, size_t len_cert,
			    const char *key, size_t len_key)
{
	BIO *bio = NULL, *kbio = NULL;
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;
	int r, err = ENOMEM;

	if (!tls || !cert || !len_cert || (key && !len_key))
		return EINVAL;

	if (!key) {
		key = cert;
		len_key = len_cert;
	}

	bio  = BIO_new_mem_buf((char *)cert, (int)len_cert);
	kbio = BIO_new_mem_buf((char *)key, (int)len_key);
	if (!bio || !kbio)
		goto out;

	x509 = PEM_read_bio_X509(bio, NULL, 0, NULL);
	pkey = PEM_read_bio_PrivateKey(kbio, NULL, 0, NULL);
	if (!x509 || !pkey)
		goto out;

	r = SSL_CTX_use_certificate(tls->ctx, x509);
	if (r != 1)
		goto out;

	r = SSL_CTX_use_PrivateKey(tls->ctx, pkey);
	if (r != 1) {
		DEBUG_WARNING("set_certificate_pem: use_PrivateKey failed\n");
		goto out;
	}

	if (tls->cert)
		X509_free(tls->cert);

	tls->cert = x509;
	x509 = NULL;

	err = 0;

 out:
	if (x509)
		X509_free(x509);
	if (pkey)
		EVP_PKEY_free(pkey);
	if (bio)
		BIO_free(bio);
	if (kbio)
		BIO_free(kbio);
	if (err)
		ERR_clear_error();

	return err;
}


/**
 * Set the certificate and private key on a TLS context
 *
 * @param tls      TLS Context
 * @param keytype  Private key type
 * @param cert     Certificate in DER format
 * @param len_cert Length of certificate DER bytes
 * @param key      Private key in DER format, will be read from cert if NULL
 * @param len_key  Length of private key DER bytes
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_certificate_der(struct tls *tls, enum tls_keytype keytype,
			    const uint8_t *cert, size_t len_cert,
			    const uint8_t *key, size_t len_key)
{
	const uint8_t *buf_cert;
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;
	int r, type, err = ENOMEM;

	if (!tls || !cert || !len_cert || (key && !len_key))
		return EINVAL;

	type = keytype2int(keytype);
	if (type == EVP_PKEY_NONE)
		return EINVAL;

	buf_cert = cert;

	x509 = d2i_X509(NULL, &buf_cert, len_cert);
	if (!x509)
		goto out;

	if (!key) {
		key = buf_cert;
		len_key = len_cert - (buf_cert - cert);
	}

	pkey = d2i_PrivateKey(type, NULL, &key, len_key);
	if (!pkey)
		goto out;

	r = SSL_CTX_use_certificate(tls->ctx, x509);
	if (r != 1)
		goto out;

	r = SSL_CTX_use_PrivateKey(tls->ctx, pkey);
	if (r != 1) {
		DEBUG_WARNING("set_certificate_der: use_PrivateKey failed\n");
		goto out;
	}

	if (tls->cert)
		X509_free(tls->cert);

	tls->cert = x509;
	x509 = NULL;

	err = 0;

 out:
	if (x509)
		X509_free(x509);
	if (pkey)
		EVP_PKEY_free(pkey);
	if (err)
		ERR_clear_error();

	return err;
}


/**
 * Set the certificate and private key on a TLS context
 *
 * @param tls TLS Context
 * @param pem Certificate and private key in PEM format
 * @param len Length of PEM string
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_certificate(struct tls *tls, const char *pem, size_t len)
{
	return tls_set_certificate_pem(tls, pem, len, NULL, 0);
}


static int verify_trust_all(int ok, X509_STORE_CTX *ctx)
{
	(void)ok;
	(void)ctx;

	return 1;    /* We trust the certificate from peer */
}


/**
 * Set TLS server context to request certificate from client
 *
 * @param tls    TLS Context
 */
void tls_set_verify_client(struct tls *tls)
{
	if (!tls)
		return;

	SSL_CTX_set_verify_depth(tls->ctx, 0);
	SSL_CTX_set_verify(tls->ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE,
			   verify_trust_all);
}


/**
 * Set SRTP suites on TLS context
 *
 * @param tls    TLS Context
 * @param suites Secure-RTP Profiles
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_srtp(struct tls *tls, const char *suites)
{
#ifdef USE_OPENSSL_SRTP
	if (!tls || !suites)
		return EINVAL;

	if (0 != SSL_CTX_set_tlsext_use_srtp(tls->ctx, suites)) {
		ERR_clear_error();
		return ENOSYS;
	}

	return 0;
#else
	(void)tls;
	(void)suites;

	return ENOSYS;
#endif
}


static int cert_fingerprint(X509 *cert, enum tls_fingerprint type,
			    uint8_t *md, size_t size)
{
	unsigned int len = (unsigned int)size;
	int n;

	switch (type) {

	case TLS_FINGERPRINT_SHA1:
		if (size < 20)
			return EOVERFLOW;

		n = X509_digest(cert, EVP_sha1(), md, &len);
		break;

	case TLS_FINGERPRINT_SHA256:
		if (size < 32)
			return EOVERFLOW;

		n = X509_digest(cert, EVP_sha256(), md, &len);
		break;

	default:
		return ENOSYS;
	}

	if (n != 1) {
		ERR_clear_error();
		return ENOENT;
	}

	return 0;
}


/**
 * Get fingerprint of local certificate
 *
 * @param tls  TLS Context
 * @param type Digest type
 * @param md   Buffer for fingerprint digest
 * @param size Buffer size
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_fingerprint(const struct tls *tls, enum tls_fingerprint type,
		    uint8_t *md, size_t size)
{
	if (!tls || !tls->cert || !md)
		return EINVAL;

	return cert_fingerprint(tls->cert, type, md, size);
}


/**
 * Get fingerprint of peer certificate of a TLS connection
 *
 * @param tc   TLS Connection
 * @param type Digest type
 * @param md   Buffer for fingerprint digest
 * @param size Buffer size
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_peer_fingerprint(const struct tls_conn *tc, enum tls_fingerprint type,
			 uint8_t *md, size_t size)
{
	X509 *cert;
	int err;

	if (!tc || !md)
		return EINVAL;

	cert = SSL_get_peer_certificate(tc->ssl);
	if (!cert)
		return ENOENT;

	err = cert_fingerprint(cert, type, md, size);

	X509_free(cert);

	return err;
}


/**
 * Get common name of peer certificate of a TLS connection
 *
 * @param tc   TLS Connection
 * @param cn   Returned common name
 * @param size Size of common name
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_peer_common_name(const struct tls_conn *tc, char *cn, size_t size)
{
	X509 *cert;
	int n;

	if (!tc || !cn || !size)
		return EINVAL;

	cert = SSL_get_peer_certificate(tc->ssl);
	if (!cert)
		return ENOENT;

	n = X509_NAME_get_text_by_NID(X509_get_subject_name(cert),
				      NID_commonName, cn, (int)size);

	X509_free(cert);

	if (n < 0) {
		ERR_clear_error();
		return ENOENT;
	}

	return 0;
}


/**
 * Verify peer certificate of a TLS connection
 *
 * @param tc TLS Connection
 *
 * @return 0 if verified, otherwise errorcode
 */
int tls_peer_verify(const struct tls_conn *tc)
{
	if (!tc)
		return EINVAL;

	if (SSL_get_verify_result(tc->ssl) != X509_V_OK)
		return EAUTH;

	return 0;
}


/**
 * Get SRTP suite and keying material of a TLS connection
 *
 * @param tc           TLS Connection
 * @param suite        Returned SRTP suite
 * @param cli_key      Client key
 * @param cli_key_size Client key size
 * @param srv_key      Server key
 * @param srv_key_size Server key size
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_srtp_keyinfo(const struct tls_conn *tc, enum srtp_suite *suite,
		     uint8_t *cli_key, size_t cli_key_size,
		     uint8_t *srv_key, size_t srv_key_size)
{
#ifdef USE_OPENSSL_SRTP
	static const char *label = "EXTRACTOR-dtls_srtp";
	size_t key_size, salt_size, size;
	SRTP_PROTECTION_PROFILE *sel;
	uint8_t keymat[256], *p;

	if (!tc || !suite || !cli_key || !srv_key)
		return EINVAL;

	sel = SSL_get_selected_srtp_profile(tc->ssl);
	if (!sel)
		return ENOENT;

	switch (sel->id) {

	case SRTP_AES128_CM_SHA1_80:
		*suite = SRTP_AES_CM_128_HMAC_SHA1_80;
		key_size  = 16;
		salt_size = 14;
		break;

	case SRTP_AES128_CM_SHA1_32:
		*suite = SRTP_AES_CM_128_HMAC_SHA1_32;
		key_size  = 16;
		salt_size = 14;
		break;

#ifdef SRTP_AEAD_AES_128_GCM
	case SRTP_AEAD_AES_128_GCM:
		*suite = SRTP_AES_128_GCM;
		key_size  = 16;
		salt_size = 12;
		break;
#endif

#ifdef SRTP_AEAD_AES_256_GCM
	case SRTP_AEAD_AES_256_GCM:
		*suite = SRTP_AES_256_GCM;
		key_size  = 32;
		salt_size = 12;
		break;
#endif

	default:
		return ENOSYS;
	}

	size = key_size + salt_size;

	if (cli_key_size < size || srv_key_size < size)
		return EOVERFLOW;

	if (sizeof(keymat) < 2*size)
		return EOVERFLOW;

	if (1 != SSL_export_keying_material(tc->ssl, keymat, 2*size, label,
					    strlen(label), NULL, 0, 0)) {
		ERR_clear_error();
		return ENOENT;
	}

	p = keymat;

	memcpy(cli_key,            p, key_size);  p += key_size;
	memcpy(srv_key,            p, key_size);  p += key_size;
	memcpy(cli_key + key_size, p, salt_size); p += salt_size;
	memcpy(srv_key + key_size, p, salt_size);

	return 0;
#else
	(void)tc;
	(void)suite;
	(void)cli_key;
	(void)cli_key_size;
	(void)srv_key;
	(void)srv_key_size;

	return ENOSYS;
#endif
}


/**
 * Get cipher name of a TLS connection
 *
 * @param tc TLS Connection
 *
 * @return name of cipher actually used.
 */
const char *tls_cipher_name(const struct tls_conn *tc)
{
	if (!tc)
		return NULL;

	return SSL_get_cipher_name(tc->ssl);
}


/**
 * Set the ciphers to use for this TLS context
 *
 * @param tls      TLS Context
 * @param cipherv  Vector of cipher names, in order of priority
 * @param count    Number of cipher names in the vector
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_ciphers(struct tls *tls, const char *cipherv[], size_t count)
{
	struct mbuf *mb;
	int r, err;
	size_t i;

	if (!tls || !cipherv || !count)
		return EINVAL;

	mb = mbuf_alloc(32 * count);
	if (!mb)
		return ENOMEM;

	for (i=0; i<count; i++) {

		err = mbuf_printf(mb, "%s%s", i>0 ? ":" : "", cipherv[i]);
		if (err)
			goto out;
	}

	err = mbuf_write_u8(mb, '\0');
	if (err)
		goto out;

	r = SSL_CTX_set_cipher_list(tls->ctx, (char *)mb->buf);
	if (r <= 0) {
		ERR_clear_error();
		err = EPROTO;
		goto out;
	}

 out:
	mem_deref(mb);

	return err;
}


/**
 * Enable verification of server certificate and hostname (SNI)
 *
 * @param tc   TLS Connection
 * @param host Server hostname
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_set_verify_server(struct tls_conn *tc, const char *host)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && \
	!defined(LIBRESSL_VERSION_NUMBER)
	struct sa sa;

	if (!tc || !host)
		return EINVAL;

	if (!tc->tls->verify_server)
		return 0;

	if (sa_set_str(&sa, host, 0)) {
		SSL_set_hostflags(tc->ssl,
				X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

		if (!SSL_set1_host(tc->ssl, host)) {
			DEBUG_WARNING("SSL_set1_host error\n");
			ERR_clear_error();
			return EPROTO;
		}

		if (!SSL_set_tlsext_host_name(tc->ssl, host)) {
			DEBUG_WARNING("SSL_set_tlsext_host_name error\n");
			ERR_clear_error();
			return EPROTO;
		}
	}

	SSL_set_verify(tc->ssl, SSL_VERIFY_PEER, verify_handler);

	return 0;
#else
	(void)tc;
	(void)host;

	return ENOSYS;
#endif
}


static int print_error(const char *str, size_t len, void *unused)
{
	(void)unused;
	DEBUG_WARNING("%b", str, len);

	return 1;
}


void tls_flush_error(void)
{
	ERR_print_errors_cb(print_error, NULL);
}


/**
 * Get the backend-specific (OpenSSL) context
 *
 * @param tls  Generic TLS Context
 *
 * @return OpenSSL context
 */
struct ssl_ctx_st *tls_openssl_context(const struct tls *tls)
{
	return tls ? tls->ctx : NULL;
}


/**
 * Convert a X509_NAME object into a human-readable form placed in an mbuf
 *
 * @param field  X509_NAME of Cert field
 * @param mb     Memorybuffer to store the readable format
 * @param flags  X509_NAME_print_ex flags
 *
 * @return 0 if success, otherwise errorcode
 */
static int convert_X509_NAME_to_mbuf(X509_NAME *field, struct mbuf *mb,
	unsigned long flags)
{
	BIO *outbio;
	char *p;
	long size;
	int err = ENOMEM;

	if (!field || !mb)
		return EINVAL;

	outbio = BIO_new(BIO_s_mem());
	if (!outbio)
		return ENOMEM;

	if (X509_NAME_print_ex(outbio, field, 1, flags) <= 0)
		goto out;

	if (BIO_eof(outbio))
		goto out;

	size = BIO_get_mem_data(outbio, &p);
	err = mbuf_write_mem(mb, (uint8_t *)p, size);
	if (err)
		goto out;

	err = 0;

 out:
	if (outbio)
		BIO_free(outbio);

	return err;
}


/**
 * Extract a X509 certficate issuer/subject and write the result into an mbuf
 *
 * @param tls           TLS Object
 * @param mb            Memory buffer
 * @param field_getter  Functionpointer to the X509 getter function
 * @param flags         X509_NAME_print_ex flags
 *
 * @return 0 if success, othewise errorcode
 */
static int tls_get_ca_chain_field(struct tls *tls, struct mbuf *mb,
	tls_get_certfield_h *field_getter, unsigned long flags)
{
	X509 *crt = NULL;
	X509_NAME *field;

	crt = SSL_CTX_get0_certificate(tls->ctx);
	if (!crt)
		return ENOENT;

	field = field_getter(crt);
	if (!field)
		return ENOTSUP;

	return convert_X509_NAME_to_mbuf(field, mb, flags);
}


/**
 * Get the issuers fields of a certificate chain
 *
 * @param tls  TLS Object
 * @param mb   Memory Buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_get_issuer(struct tls *tls, struct mbuf *mb)
{
	if (!tls || !tls->ctx || !mb)
		return EINVAL;

	return tls_get_ca_chain_field(tls, mb, &X509_get_issuer_name,
		XN_FLAG_RFC2253);
}


/**
 * Get the subject fields of a certificate chain
 *
 * @param tls  TLS Object
 * @param mb   Memory Buffer
 *
 * @return 0 if success, otherwise errorcode
 */
int tls_get_subject(struct tls *tls, struct mbuf *mb)
{
	if (!tls || !tls->ctx || !mb)
		return EINVAL;

	return tls_get_ca_chain_field(tls, mb, &X509_get_subject_name,
		XN_FLAG_RFC2253);
}


/**
 * Disables SIP TLS server verifications for following requests
 *
 * @param tls     TLS Object
 */
void tls_disable_verify_server(struct tls *tls)
{
	if (!tls)
		return;

	tls->verify_server = false;
}
