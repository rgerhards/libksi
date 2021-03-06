/*
 * Copyright 2013-2015 Guardtime, Inc.
 *
 * This file is part of the Guardtime client SDK.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES, CONDITIONS, OR OTHER LICENSES OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 * "Guardtime" and "KSI" are trademarks or registered trademarks of
 * Guardtime, Inc., and no license to trademarks is granted; Guardtime
 * reserves and retains all trademark rights.
 */

#include "internal.h"

#if KSI_PKI_TRUSTSTORE_IMPL == KSI_IMPL_CRYPTOAPI

#include <string.h>
#include <limits.h>

#include <windows.h>
#include <Wincrypt.h>

#include "tlv.h"
#include "pkitruststore.h"

const char* getMSError(DWORD error, char *buf, unsigned len){
    LPVOID lpMsgBuf;
    char *tmp = NULL;
    char *ret = NULL;

	if(buf == NULL) goto cleanup;

    if(!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf,
					0, NULL)){
		goto cleanup;
	}

	tmp = (char*)lpMsgBuf;
	tmp[strlen(tmp)-2] = 0;

	if(KSI_strncpy(buf, tmp, len) == NULL)
		goto cleanup;

	ret = buf;

cleanup:

    LocalFree(lpMsgBuf);

	return ret;
}

static int KSI_PKITruststore_global_initCount = 0;

struct KSI_PKITruststore_st {
	KSI_CTX *ctx;
	HCERTSTORE collectionStore;
};

struct KSI_PKICertificate_st {
	KSI_CTX *ctx;
	PCCERT_CONTEXT x509;
};

struct KSI_PKISignature_st {
	KSI_CTX *ctx;
	CRYPT_INTEGER_BLOB pkcs7;
};

static int cryptopapiGlobal_init(void) {
	if (KSI_PKITruststore_global_initCount++ > 0) {
		/* Nothing to do */
	} else {
		;
	}

	return KSI_OK;
}

static void cryptopapiGlobal_cleanup(void) {
	if (--KSI_PKITruststore_global_initCount > 0) {
		/* Nothing to do */
	} else {
		;
	}
}

static ALG_ID algIdFromOID(const char *OID){
	if (strcmp(OID, szOID_RSA_SHA256RSA) == 0) return CALG_SHA_256;
	else if (strcmp(OID, szOID_RSA_SHA1RSA) == 0) return CALG_SHA1;
	else if (strcmp(OID, szOID_RSA_SHA384RSA) == 0) return CALG_SHA_384;
	else if (strcmp(OID, szOID_RSA_SHA512RSA) == 0) return CALG_SHA_512;
	else return 0;
	}


void KSI_PKITruststore_free(KSI_PKITruststore *trust) {
	char buf[1024];

	if (trust != NULL) {
		if (trust->collectionStore != NULL){
			if (!CertCloseStore(trust->collectionStore, CERT_CLOSE_STORE_CHECK_FLAG)){
				KSI_LOG_debug(trust->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
				}
		}
		KSI_free(trust);
	}
}

int KSI_PKICertificate_fromTlv(KSI_TLV *tlv, KSI_PKICertificate **cert) {
	KSI_CTX *ctx = NULL;
	int res = KSI_UNKNOWN_ERROR;

	KSI_PKICertificate *tmp = NULL;
	const unsigned char *raw = NULL;
	unsigned int raw_len = 0;

	if (tlv == NULL || cert == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	ctx = KSI_TLV_getCtx(tlv);
	KSI_ERR_clearErrors(ctx);

	res = KSI_TLV_getRawValue(tlv, &raw, &raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_PKICertificate_new(ctx, raw, raw_len, &tmp);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*cert = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_nofree(raw);

	KSI_PKICertificate_free(tmp);

	return res;
}

int KSI_PKICertificate_toTlv(KSI_CTX *ctx, KSI_PKICertificate *cert, unsigned tag, int isNonCritical, int isForward, KSI_TLV **tlv) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_TLV *tmp = NULL;
	unsigned char *raw = NULL;
	unsigned raw_len = 0;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || cert == NULL || tlv == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	res = KSI_TLV_new(ctx, KSI_TLV_PAYLOAD_RAW, tag, isNonCritical, isForward, &tmp);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_PKICertificate_serialize(cert, &raw, &raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_TLV_setRawValue(tmp, raw, raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*tlv = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_nofree(raw);
	KSI_TLV_free(tmp);

	return res;
}
/*TODO: Not supported*/
int KSI_PKITruststore_addLookupDir(KSI_PKITruststore *trust, const char *path) {
	KSI_LOG_debug(trust->ctx, "CryptoAPI: Not implemented.");
	return KSI_OK;
}

/*TODO: Not supported*/
int KSI_PKITruststore_addLookupFile(KSI_PKITruststore *trust, const char *path) {
	int res = KSI_UNKNOWN_ERROR;
	HCERTSTORE tmp_FileTrustStore = NULL;
	char buf[1024];

	if (trust == NULL || path == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	KSI_ERR_clearErrors(trust->ctx);

	/*Open new store */
	tmp_FileTrustStore = CertOpenStore(CERT_STORE_PROV_FILENAME_A, 0, 0, 0, path);
	if (tmp_FileTrustStore == NULL) {
		KSI_LOG_debug(trust->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(trust->ctx, res = KSI_INVALID_FORMAT, NULL);
		goto cleanup;
	}

	/*Update with priority 0 store*/
	if (!CertAddStoreToCollection(trust->collectionStore, tmp_FileTrustStore, 0, 0)) {
		KSI_LOG_debug(trust->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(trust->ctx, res = KSI_INVALID_FORMAT, NULL);
		goto cleanup;
	}

	tmp_FileTrustStore = NULL;

	res = KSI_OK;

cleanup:

	if (tmp_FileTrustStore) CertCloseStore(tmp_FileTrustStore, CERT_CLOSE_STORE_CHECK_FLAG);
	return res;
}

int KSI_PKITruststore_new(KSI_CTX *ctx, int setDefaults, KSI_PKITruststore **trust) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_PKITruststore *tmp = NULL;
	HCERTSTORE collectionStore = NULL;
	char buf[1024];

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || trust == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}


	res = KSI_CTX_registerGlobals(ctx, cryptopapiGlobal_init, cryptopapiGlobal_cleanup);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	//TODO: Will be removed
	/*Open certificate store as collection of other stores*/
	collectionStore = CertOpenStore(CERT_STORE_PROV_COLLECTION, PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, 0, 0, NULL);
	if (collectionStore == NULL) {
		KSI_LOG_debug(ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, NULL);
		goto cleanup;
	}

	tmp = KSI_new(KSI_PKITruststore);
	if (tmp == NULL) {
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->collectionStore = collectionStore;

	*trust = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_PKITruststore_free(tmp);

	return res;
}

void KSI_PKICertificate_free(KSI_PKICertificate *cert) {
	if (cert != NULL) {
		if (cert->x509 != NULL) CertFreeCertificateContext(cert->x509);
		KSI_free(cert);
	}
}

void KSI_PKISignature_free(KSI_PKISignature *sig) {
	if (sig != NULL) {
		if (sig->pkcs7.pbData != NULL) KSI_free(sig->pkcs7.pbData);
		KSI_free(sig);
	}
}

int KSI_PKISignature_serialize(KSI_PKISignature *sig, unsigned char **raw, unsigned *raw_len) {
	int res = KSI_UNKNOWN_ERROR;
	unsigned char *tmp = NULL;

	if (sig == NULL || raw == NULL || raw_len == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	KSI_ERR_clearErrors(sig->ctx);


	tmp = KSI_malloc(sig->pkcs7.cbData);
	if (tmp == NULL) {
		KSI_pushError(sig->ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	memcpy(tmp, sig->pkcs7.pbData, sig->pkcs7.cbData);

	*raw = tmp;
	*raw_len = (unsigned)sig->pkcs7.cbData;

	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_free(tmp);

	return res;
}

int KSI_PKISignature_fromTlv(KSI_TLV *tlv, KSI_PKISignature **sig) {
	KSI_CTX *ctx = NULL;
	int res = KSI_UNKNOWN_ERROR;

	KSI_PKISignature *tmp = NULL;
	const unsigned char *raw = NULL;
	unsigned int raw_len = 0;

	if (tlv == NULL || sig == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = KSI_TLV_getCtx(tlv);
	KSI_ERR_clearErrors(ctx);


	res = KSI_TLV_getRawValue(tlv, &raw, &raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_PKISignature_new(ctx, raw, raw_len, &tmp);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	*sig = tmp;
	tmp = NULL;

	res = KSI_OK;


cleanup:

	KSI_nofree(raw);

	KSI_PKISignature_free(tmp);

	return res;
}

int KSI_PKISignature_toTlv(KSI_CTX *ctx, KSI_PKISignature *sig, unsigned tag, int isNonCritical, int isForward, KSI_TLV **tlv) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_TLV *tmp = NULL;
	unsigned char *raw = NULL;
	unsigned raw_len = 0;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || sig == NULL || tlv == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}


	res = KSI_TLV_new(ctx, KSI_TLV_PAYLOAD_RAW, tag, isNonCritical, isForward, &tmp);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_PKISignature_serialize(sig, &raw, &raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_TLV_setRawValue(tmp, raw, raw_len);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}


	*tlv = tmp;
	tmp = NULL;

	res = KSI_OK;


cleanup:

	KSI_nofree(raw);
	KSI_TLV_free(tmp);

	return res;
}

int KSI_PKISignature_new(KSI_CTX *ctx, const void *raw, unsigned raw_len, KSI_PKISignature **signature) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_PKISignature *tmp = NULL;

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || raw == NULL || raw_len == 0 || signature == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}


	tmp = KSI_new(KSI_PKISignature);
	if (tmp == NULL) {
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}
	tmp->ctx = ctx;
	tmp->pkcs7.pbData = NULL;
	tmp->pkcs7.cbData = 0;

	if (raw_len > UINT_MAX) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Length is more than MAX_INT.");
		goto cleanup;
	}

	tmp->pkcs7.pbData = KSI_malloc(raw_len);
	if (tmp->pkcs7.pbData == NULL) {
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->pkcs7.cbData = raw_len;
	memcpy(tmp->pkcs7.pbData, raw, raw_len);

	*signature = tmp;
	tmp = NULL;

	res = KSI_OK;

cleanup:

	KSI_PKISignature_free(tmp);

	return res;
}

int KSI_PKICertificate_new(KSI_CTX *ctx, const void *der, size_t der_len, KSI_PKICertificate **cert) {
	int res = KSI_UNKNOWN_ERROR;
	PCCERT_CONTEXT x509 = NULL;
	KSI_PKICertificate *tmp = NULL;
	char buf[1024];

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || der == NULL || der_len == 0 || cert == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (der_len > UINT_MAX) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Length is more than MAX_INT.");
		goto cleanup;
	}

	x509 = CertCreateCertificateContext(X509_ASN_ENCODING, der, (unsigned)der_len);
	if (x509 == NULL) {
		DWORD error = GetLastError();
		const char *errmsg = getMSError(GetLastError(), buf, sizeof(buf));
		KSI_LOG_debug(ctx, "%s", errmsg);

		if (error == CRYPT_E_ASN1_EOD)
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "Invalid PKI certificate. ASN.1 unexpected end of data.");
		else if (error == CRYPT_E_ASN1_MEMORY	)
			KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		else
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errmsg);

		goto cleanup;
	}

	tmp = KSI_new(KSI_PKICertificate);
	if (tmp == NULL) {
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	tmp->ctx = ctx;
	tmp->x509 = x509;
	x509 = NULL;

	*cert = tmp;
	tmp = NULL;

	res = KSI_OK;


cleanup:

	if (x509 != NULL) CertFreeCertificateContext(x509);
	KSI_PKICertificate_free(tmp);

	return res;
}

int KSI_PKICertificate_serialize(KSI_PKICertificate *cert, unsigned char **raw, unsigned *raw_len) {
	int res = KSI_UNKNOWN_ERROR;
	unsigned char *tmp_serialized = NULL;
	DWORD len = 0;
	char buf[1024];

	if (cert == NULL || raw == NULL || raw_len == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	KSI_ERR_clearErrors(cert->ctx);

	if (!CertSerializeCertificateStoreElement(cert->x509, 0, NULL, &len)) {
		KSI_LOG_debug(cert->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(cert->ctx, res = KSI_CRYPTO_FAILURE, "Unable to serialize PKI certificate.");
		goto cleanup;
	}

	tmp_serialized = KSI_malloc(len);
	if (tmp_serialized == NULL) {
		KSI_pushError(cert->ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	if (!CertSerializeCertificateStoreElement(cert->x509, 0, (BYTE*)tmp_serialized, &len)){
		KSI_LOG_debug(cert->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(cert->ctx, res = KSI_CRYPTO_FAILURE, "Unable to serialize PKI certificate.");
		goto cleanup;
	}

	*raw = tmp_serialized;
	*raw_len = (unsigned)len;
	tmp_serialized = NULL;

	res = KSI_OK;

cleanup:

	KSI_free(tmp_serialized);

	return res;
}

char* KSI_PKICertificate_toString(KSI_PKICertificate *cert, char *buf, unsigned buf_len){
	char *ret = NULL;
	char strSubjectname[256];
	char strIssuerName[256];

	if (cert == NULL  || buf == NULL) goto cleanup;

	CertGetNameString(cert->x509, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, strSubjectname, sizeof(strSubjectname));
	CertGetNameString(cert->x509, CERT_NAME_SIMPLE_DISPLAY_TYPE , CERT_NAME_ISSUER_FLAG, 0,strIssuerName, sizeof(strIssuerName));
	KSI_snprintf(buf, buf_len, "Subject: '%s',  Issuer '%s'.", strSubjectname, strIssuerName);

	ret = buf;

cleanup:

return ret;
}

/*TODO: for debugging*/
static void printCertInfo(PCCERT_CONTEXT cert){
	char strMail[256];
	char strData[256];
	char strIssuerName[256];

	if (cert == NULL){
		printf("Certificate is nullptr.\n");
		return;
	}

	CertGetNameString(cert, CERT_NAME_EMAIL_TYPE, 0, NULL, strMail, sizeof(strMail));
	CertGetNameString(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, strData, sizeof(strData));
	CertGetNameString(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE , CERT_NAME_ISSUER_FLAG, 0,strIssuerName, sizeof(strIssuerName));
	printf("Cert: '%s' Mail '%s' Issuer '%s'.\n", strData,strMail, strIssuerName);

	return;
	}

/*TODO: for debugging*/
static void printCertsInStore(HCERTSTORE certStore){
	PCCERT_CONTEXT certFound = NULL;
	DWORD i =0;

	if (certStore == NULL){
		printf("Cert store is nullptr\n");
		return;
	}

	do{
		certFound = CertEnumCertificatesInStore(certStore,certFound);


		if (certFound != NULL){
			printf("  >>%2i)",i++);
			printCertInfo(certFound);

		}
		else{
			printf("  >>No more certs to print.\n");
		}

	}
	while (certFound != NULL);

}

/*TODO: for debugging*/
static void printCertChain(const PCCERT_CHAIN_CONTEXT pChainContext){
	DWORD i=0;
	DWORD j=0;

	if (pChainContext == NULL){
		printf("Certificate chain is nullptr\n");
		return;
	}

	printf("Certificate chains (%i)", pChainContext->cChain);
	for (i=0; i< pChainContext->cChain; i++){
		printf("\n Chain (%i)::\n", pChainContext->rgpChain[i]->cElement);
		for (j=0; j<pChainContext->rgpChain[i]->cElement; j++){
			PCERT_CHAIN_ELEMENT element = pChainContext->rgpChain[i]->rgpElement[j];
			printf("\t %i) ", j);
			if ((element->TrustStatus.dwInfoStatus)&CERT_TRUST_IS_SELF_SIGNED)
				printf("*ROOT* ");
				printCertInfo(element->pCertContext);
		}

	}
}

/*cert obj must be freed*/
static int extractSigningCertificate(const KSI_PKISignature *signature, PCCERT_CONTEXT *cert) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_CTX *ctx = NULL;
	HCERTSTORE certStore = NULL;
	DWORD signerCount = 0;
	PCERT_INFO pSignerCertInfo = NULL;
	HCRYPTMSG signaturMSG = NULL;
	PCCERT_CONTEXT signing_cert = NULL;
	BYTE *dataRecieved = NULL;
	char buf[1024];
	DWORD dataLen = 0;

	if (signature == NULL || cert == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = signature->ctx;
	KSI_ERR_clearErrors(ctx);


	/*Get Signature certificates as a certificate store*/
	certStore = CryptGetMessageCertificates(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, (HCRYPTPROV_LEGACY)NULL, 0, signature->pkcs7.pbData, signature->pkcs7.cbData);
	if (certStore == NULL){
		KSI_LOG_debug(signature->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "Unable to get signatures PKI certificates.");
		goto cleanup;
	 }

	/*Counting signing certificates*/
	signerCount = CryptGetMessageSignerCount(PKCS_7_ASN_ENCODING, signature->pkcs7.pbData, signature->pkcs7.cbData);
	if (signerCount == -1){
		KSI_LOG_debug(signature->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "Unable to count PKI signatures certificates.");
		goto cleanup;
	}

	/*Is there exactly 1 signing cert?*/
	if (signerCount != 1){
		KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "PKI signature certificate count is not 1.");
		goto cleanup;
	}

	/*Open signature for decoding*/
	signaturMSG = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING, 0, 0,0, NULL, NULL);
	if (signaturMSG == NULL){
		DWORD error = GetLastError();
		const char *errmsg = getMSError(GetLastError(), buf, sizeof(buf));
		KSI_LOG_debug(signature->ctx, "%s", errmsg);

		if (error == E_INVALIDARG)
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errmsg);
		else if (error == E_OUTOFMEMORY)
			KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		else
			KSI_pushError(ctx, res = KSI_UNKNOWN_ERROR, errmsg);

		goto cleanup;
	}

	if (!CryptMsgUpdate(signaturMSG, signature->pkcs7.pbData, signature->pkcs7.cbData, TRUE)){
		DWORD error = GetLastError();
		const char *errmsg = getMSError(GetLastError(), buf, sizeof(buf));
		KSI_LOG_debug(signature->ctx, "%s", errmsg);

		if (error == E_OUTOFMEMORY)
			KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		else if (error == CRYPT_E_UNEXPECTED_ENCODING)
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "The PKI signature is not encoded as PKCS7.");
		else if (error == CRYPT_E_MSG_ERROR)
			KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, errmsg);
		else
			KSI_pushError(ctx, res = KSI_UNKNOWN_ERROR, errmsg);

		goto cleanup;
	}

	/*Get signatures signing cert id*/
	if (!CryptMsgGetParam (signaturMSG, CMSG_SIGNER_CERT_INFO_PARAM, 0, NULL, &dataLen)){
		DWORD error = GetLastError();
		const char *errmsg = getMSError(GetLastError(), buf, sizeof(buf));
		KSI_LOG_debug(signature->ctx, "%s", errmsg);

		if (error == CRYPT_E_ATTRIBUTES_MISSING)
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "The PKI signature does not contain signing certificate id.");
		else
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errmsg);

		goto cleanup;
	}

	dataRecieved = KSI_malloc(dataLen);
	if (dataRecieved == NULL){
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	if (!CryptMsgGetParam (signaturMSG, CMSG_SIGNER_CERT_INFO_PARAM, 0, dataRecieved, &dataLen)){
		KSI_LOG_debug(signature->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_INVALID_FORMAT, "Unable to get PKI signatures signing certificate id.");
		goto cleanup;
	}

	pSignerCertInfo = (PCERT_INFO)dataRecieved;

	/*Get signing cert*/
	signing_cert = CertGetSubjectCertificateFromStore(certStore, X509_ASN_ENCODING, pSignerCertInfo);
	if (signing_cert == NULL){
		KSI_LOG_debug(signature->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to get PKI signatures signer certificate.");
		goto cleanup;
	}

	/*The copy of the object is NOT created. Just its reference value is incremented*/
	signing_cert = CertDuplicateCertificateContext(signing_cert);

	*cert = signing_cert;
	signing_cert = NULL;


	res = KSI_OK;

cleanup:

	if (signing_cert) CertFreeCertificateContext(signing_cert);
	if (certStore) CertCloseStore(certStore, CERT_CLOSE_STORE_CHECK_FLAG);
	if (signaturMSG) CryptMsgClose(signaturMSG);
	KSI_free(dataRecieved);

	return res;
}

static const char* getCertificateChainErrorStr(PCCERT_CHAIN_CONTEXT pChainContext){
	if (pChainContext == NULL)
		return "Certificate chain is nullptr";

	switch (pChainContext->TrustStatus.dwErrorStatus){
		case CERT_TRUST_NO_ERROR: return "No error found for this certificate or chain.";
		case CERT_TRUST_IS_NOT_TIME_VALID:return "This certificate or one of the certificates in the certificate chain is not time valid.";
		case CERT_TRUST_IS_REVOKED:return "Trust for this certificate or one of the certificates in the certificate chain has been revoked.";
		case CERT_TRUST_IS_NOT_SIGNATURE_VALID: return "The certificate or one of the certificates in the certificate chain does not have a valid signature.";
		case CERT_TRUST_IS_NOT_VALID_FOR_USAGE:return "The certificate or certificate chain is not valid for its proposed usage.";
		case CERT_TRUST_IS_UNTRUSTED_ROOT: return "The certificate or certificate chain is based on an untrusted root.";
		case CERT_TRUST_REVOCATION_STATUS_UNKNOWN: return "The revocation status of the certificate or one of the certificates in the certificate chain is unknown.";
		case CERT_TRUST_IS_CYCLIC: return "One of the certificates in the chain was issued by a certification authority that the original certificate had certified.";
		case CERT_TRUST_INVALID_EXTENSION: return "One of the certificates has an extension that is not valid.";
		case CERT_TRUST_INVALID_POLICY_CONSTRAINTS: return "The certificate or one of the certificates in the certificate chain has a policy constraints extension, and one of the issued certificates has a disallowed policy mapping extension or does not have a required issuance policies extension.";
		case CERT_TRUST_INVALID_BASIC_CONSTRAINTS: return "The certificate or one of the certificates in the certificate chain has a basic constraints extension, and either the certificate cannot be used to issue other certificates, or the chain path length has been exceeded.";
		case CERT_TRUST_INVALID_NAME_CONSTRAINTS: return "The certificate or one of the certificates in the certificate chain has a name constraints extension that is not valid.";
		case CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT: return "The certificate or one of the certificates in the certificate chain has a name constraints extension that contains unsupported fields.";
		case CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT: return "The certificate or one of the certificates in the certificate chain has a name constraints extension and a name constraint is missing for one of the name choices in the end certificate.";
		case CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT: return "The certificate or one of the certificates in the certificate chain has a name constraints extension, and there is not a permitted name constraint for one of the name choices in the end certificate.";
		case CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT: return "The certificate or one of the certificates in the certificate chain has a name constraints extension, and one of the name choices in the end certificate is explicitly excluded.";
		case CERT_TRUST_IS_OFFLINE_REVOCATION: return "The revocation status of the certificate or one of the certificates in the certificate chain is either offline or stale.";
		case CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY: return "The end certificate does not have any resultant issuance policies, and one of the issuing certification authority certificates has a policy constraints extension requiring it.";
		case CERT_TRUST_IS_EXPLICIT_DISTRUST: return "The certificate is explicitly distrusted.";
		case CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT: return "The certificate does not support a critical extension.";
		default: return "Unknown certificate chain error";
	}
}

//TODO: Will be removed
static int isUntrustedRootCertInStore(const KSI_PKITruststore *pki, const PCCERT_CHAIN_CONTEXT pChainContext){
	DWORD j=0;
	PCCERT_CONTEXT pUntrustedRootCert = NULL;
	PCCERT_CONTEXT certFound = NULL;

	if (pChainContext == NULL) return false;
	if (pChainContext->cChain > 1) return false;

	for (j=0; j<pChainContext->rgpChain[0]->cElement; j++){
		PCERT_CHAIN_ELEMENT element = pChainContext->rgpChain[0]->rgpElement[j];

		if (element->TrustStatus.dwErrorStatus&CERT_TRUST_IS_UNTRUSTED_ROOT && element->TrustStatus.dwInfoStatus&CERT_TRUST_IS_SELF_SIGNED){
			pUntrustedRootCert = element->pCertContext;

			while ((certFound = CertEnumCertificatesInStore(pki->collectionStore, certFound)) != NULL){
				if (certFound->cbCertEncoded == pUntrustedRootCert->cbCertEncoded){
						if (memcmp(certFound->pbCertEncoded, pUntrustedRootCert->pbCertEncoded, certFound->cbCertEncoded)==0){
							CertFreeCertificateContext(certFound);
							return true;
					}
				}
			}
		}
	}

	return false;
}

static int KSI_PKITruststore_verifyCertificate(const KSI_PKITruststore *pki, const PCCERT_CONTEXT cert){
	int res = KSI_UNKNOWN_ERROR;
	KSI_CTX *ctx = NULL;
	CERT_ENHKEY_USAGE enhkeyUsage;
	CERT_USAGE_MATCH certUsage;
	CERT_CHAIN_PARA chainPara;
	PCCERT_CHAIN_CONTEXT pChainContext = NULL;
	CERT_CHAIN_POLICY_PARA policyPara;
	CERT_CHAIN_POLICY_STATUS policyStatus;
	char buf[1024];

	if (pki == NULL || cert == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = pki->ctx;
	KSI_ERR_clearErrors(ctx);

	/* Get the certificate chain of certificate under verification. */
	/*OID List for certificate trust list extensions*/
	enhkeyUsage.cUsageIdentifier = 0;
	enhkeyUsage.rgpszUsageIdentifier = NULL;
	/*Criteria for identifying issuer certificate for chain building*/
	certUsage.dwType = USAGE_MATCH_TYPE_AND;
	certUsage.Usage = enhkeyUsage;
	/*Searching and matching criteria for chain building*/
	chainPara.cbSize = sizeof(CERT_CHAIN_PARA);
	chainPara.RequestedUsage = certUsage;

	/*Use CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL for no automatic cert store update by windows.
	 It is useful when there is need to remove default cert from system store*/
	/*Build Certificate Chain from top to root certificate*/
	if (!CertGetCertificateChain(NULL, cert, NULL, pki->collectionStore, &chainPara, 0, NULL, &pChainContext)) {
		KSI_LOG_debug(pki->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to get PKI certificate chain");
		goto cleanup;
	}
//TODO: debugging
//	printCertChain(pChainContext);

	/*TODO: REMOVE*/
	/*If chain is based on untrusted root, determine if it's in pki->collectionStore.
	 If it is, enable chain verification to trust untrusted roots*/
	if (pChainContext->TrustStatus.dwErrorStatus&CERT_TRUST_IS_UNTRUSTED_ROOT){
		KSI_LOG_debug(ctx, "CryptoAPI: Root certificate is not present under Windows 'Trusted Root Certification Authorities'.");
		KSI_LOG_debug(ctx, "CryptoAPI: Searching if it is present under PKI Trust Store from files.");
		if (isUntrustedRootCertInStore(pki, pChainContext)){
			policyPara.dwFlags =  CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG;
			KSI_LOG_debug(ctx, "CryptoAPI: Certificate is present. Allow untrusted root certificates");
		}
		else{
			policyPara.dwFlags = 0;
			KSI_LOG_debug(ctx, "CryptoAPI: Certificate is not present.");
		}
	}
	else if (pChainContext->TrustStatus.dwErrorStatus != CERT_TRUST_NO_ERROR) {
		KSI_LOG_debug(ctx, "%s", getCertificateChainErrorStr(pChainContext));
		KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, getCertificateChainErrorStr(pChainContext));
		goto cleanup;
	}
	else{
		policyPara.dwFlags = 0;
	}

	/* Verify certificate chain. */
	policyPara.cbSize = sizeof(CERT_CHAIN_POLICY_PARA);
	policyPara.pvExtraPolicyPara = 0;

	if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, pChainContext, &policyPara, &policyStatus)) {
		KSI_LOG_debug(pki->ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, NULL);
		goto cleanup;
	}

	if (policyStatus.dwError) {
		KSI_LOG_debug(ctx, "CryptoAPI: PKI chain policy error %X.", policyStatus.dwError);
 		KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	if (pChainContext) CertFreeCertificateChain(pChainContext);

	return res;
}

static int KSI_PKITruststore_verifySignatureCertificate(const KSI_PKITruststore *pki, const KSI_PKISignature *signature) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_CTX *ctx = NULL;
	PCCERT_CONTEXT subjectCert = NULL;
	char tmp[256];
	char *magicEmail = NULL;

	if (pki == NULL || signature == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = pki->ctx;
	KSI_ERR_clearErrors(ctx);

	res = extractSigningCertificate(signature, &subjectCert);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	//printCertInfo(subjectCert);

	res=KSI_CTX_getPublicationCertEmail(ctx, &magicEmail);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	if (magicEmail != NULL){
		if (CertGetNameString(subjectCert, CERT_NAME_EMAIL_TYPE, 0, NULL, tmp, sizeof(tmp))==1){
			KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to get subjects name from PKI certificate.");
			goto cleanup;
		}

		KSI_LOG_debug(ctx, "CryptoAPI: Subjects E-mail: %s.", tmp);

		if (strcmp(tmp, magicEmail) != 0) {
			KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, "Wrong subject name.");
			goto cleanup;
		}
	}

	res = KSI_PKITruststore_verifyCertificate(pki, subjectCert);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_OK;

cleanup:

	if (subjectCert) CertFreeCertificateContext(subjectCert);

	return res;
}

int KSI_PKITruststore_verifySignature(KSI_PKITruststore *pki, const unsigned char *data, size_t data_len, const KSI_PKISignature *signature) {
	int res = KSI_UNKNOWN_ERROR;
	KSI_CTX *ctx = NULL;
	PCCERT_CONTEXT subjectCert = NULL;
	CRYPT_VERIFY_MESSAGE_PARA msgPara;
	DWORD dLen;
	char buf[1024];

	if (pki == NULL || data == NULL || signature == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}
	ctx = pki->ctx;
	KSI_ERR_clearErrors(ctx);

	KSI_LOG_debug(ctx, "CryptoAPI: Start PKI signature verification.");

	if (data_len > INT_MAX) {
		KSI_pushError(ctx, res = KSI_INVALID_ARGUMENT, "Data too long (more than MAX_INT).");
		goto cleanup;
	}

	/*Verify signature and signed data. Certificate is extracted from signature*/
	msgPara.cbSize = sizeof(CRYPT_VERIFY_MESSAGE_PARA);
    msgPara.dwMsgAndCertEncodingType = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    msgPara.hCryptProv = 0;
    msgPara.pfnGetSignerCertificate = NULL;
    msgPara.pvGetArg = NULL;
	dLen = (DWORD)data_len;

	if (!CryptVerifyDetachedMessageSignature(&msgPara, 0, signature->pkcs7.pbData, signature->pkcs7.cbData, 1, &data, &dLen, &subjectCert)){
		DWORD error = GetLastError();
		const char *errmsg = getMSError(error, buf, sizeof(buf));
		KSI_LOG_debug(pki->ctx, "%s", errmsg);

		if (error == E_INVALIDARG || error == CRYPT_E_UNEXPECTED_MSG_TYPE || error == CRYPT_E_NO_SIGNER)
			KSI_pushError(ctx, res = KSI_INVALID_FORMAT, errmsg);
		else if (error == NTE_BAD_ALGID)
			KSI_pushError(ctx, res = KSI_INVALID_PKI_SIGNATURE, errmsg);
		else if (error == NTE_BAD_SIGNATURE)
			KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Verification of PKI signature failed.");
		else
			KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, errmsg);

		goto cleanup;
	}

	res = KSI_PKITruststore_verifyCertificate(pki, subjectCert);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}

	res = KSI_PKITruststore_verifySignatureCertificate(pki, signature);
	if (res != KSI_OK){
		KSI_pushError(ctx, res, NULL);
		goto cleanup;
	}


	KSI_LOG_debug(ctx, "CryptoAPI: PKI signature verified successfully.");

	res = KSI_OK;

cleanup:

	if (subjectCert) CertFreeCertificateContext(subjectCert);

	return res;
}

int KSI_PKITruststore_verifyRawSignature(KSI_CTX *ctx, const unsigned char *data, unsigned data_len, const char *algoOid, const unsigned char *signature, unsigned signature_len, const KSI_PKICertificate *certificate) {
	int res = KSI_UNKNOWN_ERROR;
	ALG_ID algorithm=0;
	HCRYPTPROV hCryptProv = 0;
    PCCERT_CONTEXT subjectCert = NULL;
	HCRYPTKEY publicKey = 0;
	DWORD i=0;
	BYTE *little_endian_pkcs1= NULL;
	DWORD pkcs1_len = 0;
	HCRYPTHASH hash = 0;
	char buf[1024];

	KSI_ERR_clearErrors(ctx);
	if (ctx == NULL || data == NULL || signature == NULL ||
		signature_len >= UINT_MAX || algoOid == NULL || certificate == NULL){
		res = KSI_INVALID_ARGUMENT;
		goto cleanup;
	}


	algorithm = algIdFromOID(algoOid);
	if (algorithm == 0) {
		KSI_pushError(ctx, res = KSI_UNAVAILABLE_HASH_ALGORITHM, NULL);
		goto cleanup;
	}

	// Get the CSP context
	if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)){
		KSI_LOG_debug(ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to get cryptographic provider.");
		goto cleanup;
	}

	// Get the public key from the issuer certificate
	subjectCert = certificate->x509;
	if (!CryptImportPublicKeyInfo(hCryptProv, X509_ASN_ENCODING,&subjectCert->pCertInfo->SubjectPublicKeyInfo,&publicKey)){
		KSI_LOG_debug(ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, "Failed to read PKI public key.");
		goto cleanup;
	}

	/*Convert big-endian to little-endian PKCS#1 signature*/
	pkcs1_len = signature_len;
	little_endian_pkcs1 = (BYTE*)KSI_malloc(pkcs1_len);

	if (little_endian_pkcs1 == NULL){
		KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, NULL);
		goto cleanup;
	}

	for (i=0; i<pkcs1_len; i++){
		little_endian_pkcs1[pkcs1_len-1-i] = signature[i];
	}

	// Create the hash object and hash input data.
	if (!CryptCreateHash(hCryptProv, algorithm, 0, 0, &hash)){
		KSI_LOG_debug(ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to create hasher.");
		goto cleanup;
	}

	if (!CryptHashData(hash, (BYTE*)data, data_len,0)){
		KSI_LOG_debug(ctx, "%s", getMSError(GetLastError(), buf, sizeof(buf)));
		KSI_pushError(ctx, res = KSI_CRYPTO_FAILURE, "Unable to hash data.");
		goto cleanup;
	}

	/*Verify the signature. The format MUST be PKCS#1*/
	if (!CryptVerifySignature(hash, (BYTE*)little_endian_pkcs1, pkcs1_len, publicKey, NULL, 0)){
		DWORD error = GetLastError();
		const char *errmsg = getMSError(GetLastError(), buf, sizeof(buf));
		KSI_LOG_debug(ctx, "%s", errmsg);

		if (error == NTE_BAD_SIGNATURE)
			KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, "Invalid PKI signature.");
		else if (error == NTE_NO_MEMORY)
			KSI_pushError(ctx, res = KSI_OUT_OF_MEMORY, "Unable to verify PKI signature. CSP out of memory.");
		else
			KSI_pushError(ctx, res = KSI_PKI_CERTIFICATE_NOT_TRUSTED, errmsg);

		goto cleanup;
	}

	KSI_LOG_debug(certificate->ctx, "CryptoAPI: PKI signature verified successfully.");

	res = KSI_OK;

cleanup:

	KSI_free(little_endian_pkcs1);
	if (hCryptProv) CryptReleaseContext(hCryptProv, 0);
	if (hash) CryptDestroyHash(hash);

	return res;
}

#endif
