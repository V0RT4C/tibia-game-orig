#include "crypto/rsa/rsa.h"
#include "logging/logging.h"

#include <openssl/err.h>
#include <openssl/pem.h>

static void DumpOpenSSLErrors(const char *Where, const char *What){
	error("OpenSSL error(s) while executing %s at %s:\n", What, Where);
	ERR_print_errors_cb(
		[](const char *str, usize len, void *u) -> int {
			// NOTE(fusion): These error strings already have trailing newlines,
			// for whatever reason.
			error("> %s", str);
			return 1;
		}, NULL);
}

// RsaPrivateKey
// =============================================================================
RsaPrivateKey::RsaPrivateKey(void){
	m_RSA = NULL;
}

RsaPrivateKey::~RsaPrivateKey(void){
	if(m_RSA){
		RSA_free(m_RSA);
		m_RSA = NULL;
	}
}

bool RsaPrivateKey::initFromFile(const char *FileName){
	if(m_RSA != NULL){
		error("RsaPrivateKey::init: Key already initialized.\n");
		return false;
	}

	FILE *File = fopen(FileName, "rb");
	if(File == NULL){
		error("RsaPrivateKey::initFromFile: Failed to open \"%s\".\n", FileName);
		return false;
	}

	m_RSA = PEM_read_RSAPrivateKey(File, NULL, NULL, NULL);
	fclose(File);

	if(m_RSA == NULL){
		error("RsaPrivateKey::initFromFile: Failed to read key from \"%s\".\n", FileName);
		DumpOpenSSLErrors("RsaPrivateKey::initFromFile", "PEM_read_RSAPrivateKey");
	}else if(RSA_size(m_RSA) != 128){
		error("RsaPrivateKey::initFromFile: File \"%s\" doesn't contain a 1024-bit key", FileName);
		RSA_free(m_RSA);
		m_RSA = NULL;
	}

	return (m_RSA != NULL);
}

bool RsaPrivateKey::decrypt(uint8 *Data){
	if(m_RSA == NULL){
		error("RsaPrivateKey::decrypt: Key not initialized.\n");
		return false;
	}

	// TODO(fusion): Pass in the length of `Data` for checking.
	ASSERT(RSA_size(m_RSA) == 128);

	if(RSA_private_decrypt(128, Data, Data, m_RSA, RSA_NO_PADDING) == -1){
		DumpOpenSSLErrors("RsaPrivateKey::decrypt", "RSA_private_decrypt");
		return false;
	}

	return true;
}
