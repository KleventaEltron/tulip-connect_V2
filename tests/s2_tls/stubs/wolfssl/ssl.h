#ifndef TLS_TEST_SSL_H
#define TLS_TEST_SSL_H
#include <stddef.h>
typedef struct { int unused; } WOLFSSL_CTX;
typedef struct { int unused; } WOLFSSL;
typedef struct { int unused; } WOLFSSL_METHOD;
typedef int (*CallbackIORecv)(WOLFSSL *,char *,int,void *);
typedef CallbackIORecv CallbackIOSend;
#define WOLFSSL_SUCCESS 1
#define WOLFSSL_SHUTDOWN_NOT_DONE 2
int wolfSSL_shutdown(WOLFSSL *);
#define SSL_VERIFY_PEER 1
#define SSL_FILETYPE_ASN1 2
#define WOLFSSL_SNI_HOST_NAME 0
#define WOLFSSL_ERROR_ZERO_RETURN 6
#define WOLFSSL_ERROR_WANT_READ 2
#define WOLFSSL_ERROR_WANT_WRITE 3
#define WOLFSSL_CBIO_ERR_GENERAL (-1)
#define WOLFSSL_CBIO_ERR_WANT_READ (-2)
#define WOLFSSL_CBIO_ERR_WANT_WRITE (-2)
#define WOLFSSL_CBIO_ERR_CONN_CLOSE (-5)
int wolfSSL_Init(void);
int wolfSSL_Cleanup(void);
WOLFSSL_METHOD *wolfTLSv1_2_client_method(void);
WOLFSSL_CTX *wolfSSL_CTX_new(WOLFSSL_METHOD *);
void wolfSSL_CTX_free(WOLFSSL_CTX *);
void wolfSSL_CTX_set_verify(WOLFSSL_CTX *,int,void *);
int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX *,const unsigned char *,long,int);
void wolfSSL_SetIORecv(WOLFSSL_CTX *,CallbackIORecv);
void wolfSSL_SetIOSend(WOLFSSL_CTX *,CallbackIOSend);
WOLFSSL *wolfSSL_new(WOLFSSL_CTX *);
void wolfSSL_free(WOLFSSL *);
int wolfSSL_check_domain_name(WOLFSSL *,const char *);
int wolfSSL_UseSNI(WOLFSSL *,unsigned char,const void *,unsigned short);
void wolfSSL_SetIOReadCtx(WOLFSSL *,void *);
void wolfSSL_SetIOWriteCtx(WOLFSSL *,void *);
int wolfSSL_connect(WOLFSSL *);
long wolfSSL_get_verify_result(const WOLFSSL *);
int wolfSSL_get_error(WOLFSSL *,int);
int wolfSSL_read(WOLFSSL *,void *,int);
int wolfSSL_write(WOLFSSL *,const void *,int);
#endif
