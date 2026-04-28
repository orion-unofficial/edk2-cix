#include <errno.h>
#include <getopt.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define TRUSTED_KEY_OID "1.3.6.1.4.1.4128.2100.303"
#define CERT_COMMON_NAME "Trusted Key Certificate"
#define CERT_VALIDITY_DAYS (365 * 20)

static void print_usage(FILE *stream)
{
  fputs(
    "\033[1;33m    ./cix_regen_trusted_key_cert <option>\n\n"
    "\033[m    options:\n"
    "        -h, --help         show the help information\n"
    "        -o, --output       output file/path\n"
    "        -p, --publickey    public key file/path\n"
    "        -s, --signkey      sign key file/pathary\n"
    "        -v, --certfile     cert file/pathary\n\n"
    "    examples:\n"
    "        If you want to replace no-trusted key file extension, you can run:\n"
    "\033[0;32;32m            ./cix_regen_trusted_key_cert -p PUBLIC_KEY_FILE -s PRIVATE_KEY_FILE -o OUTPUT_FILE\n"
    "\033[0;32;32m        If you want to verify no-trusted key file extension, you can run:\n"
    "\033[0;32;32m            ./cix_regen_trusted_key_cert -p CIX_PUBLIC_KEY_FILE -v CERT_FILE -o OEM_PUBLIC_KEY_FILE\n"
    "\033[m",
    stream
  );
}

static int path_is_directory(const char *path)
{
  struct stat st;

  if (path == NULL) {
    return 0;
  }

  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int fail_public_key_load(const char *path, const char *open_message, const char *pem_message)
{
  if (open_message != NULL) {
    printf(open_message, path == NULL ? "(null)" : path);
  } else if (pem_message != NULL) {
    printf("%s\n", pem_message);
  }
  ERR_clear_error();
  printf("Get public key file fail\n");
  printf("Fail to add extesnsion into cert\n");
  return 255;
}

static int failf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  return 255;
}

static EVP_PKEY *load_public_key_for_extension(const char *path, int *open_failed, int *pem_failed)
{
  BIO *bio = NULL;
  EVP_PKEY *key = NULL;

  if (open_failed != NULL) {
    *open_failed = 0;
  }
  if (pem_failed != NULL) {
    *pem_failed = 0;
  }

  if (path == NULL) {
    if (open_failed != NULL) {
      *open_failed = 1;
    }
    return NULL;
  }

  if (path_is_directory(path)) {
    if (pem_failed != NULL) {
      *pem_failed = 1;
    }
    return NULL;
  }

  bio = BIO_new_file(path, "r");
  if (bio == NULL) {
    if (open_failed != NULL) {
      *open_failed = 1;
    }
    ERR_clear_error();
    return NULL;
  }

  key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  BIO_free(bio);
  if (key == NULL) {
    if (pem_failed != NULL) {
      *pem_failed = 1;
    }
    ERR_clear_error();
  }
  return key;
}

static EVP_PKEY *load_public_key_for_verify(const char *path)
{
  BIO *bio = NULL;
  EVP_PKEY *key = NULL;

  if (path == NULL) {
    printf("Can't open file of (null)\n");
    return NULL;
  }

  if (path_is_directory(path)) {
    printf("PEM_read_RSA_PUBKEY fail\n");
    return NULL;
  }

  bio = BIO_new_file(path, "r");
  if (bio == NULL) {
    ERR_clear_error();
    printf("Can't open file of %s\n", path);
    return NULL;
  }

  key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  BIO_free(bio);
  if (key == NULL) {
    ERR_clear_error();
    printf("PEM_read_RSA_PUBKEY fail\n");
  }
  return key;
}

static EVP_PKEY *load_private_key_for_vendor(const char *path, int *open_failed)
{
  BIO *bio = NULL;
  EVP_PKEY *key = NULL;

  if (open_failed != NULL) {
    *open_failed = 0;
  }

  if (path == NULL) {
    if (open_failed != NULL) {
      *open_failed = 1;
    }
    return NULL;
  }

  if (path_is_directory(path)) {
    return NULL;
  }

  bio = BIO_new_file(path, "r");
  if (bio == NULL) {
    if (open_failed != NULL) {
      *open_failed = 1;
    }
    ERR_clear_error();
    return NULL;
  }

  key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
  BIO_free(bio);
  if (key == NULL) {
    ERR_clear_error();
  }
  return key;
}

static X509 *load_der_certificate_for_verify(const char *path, int *malformed)
{
  FILE *stream = NULL;
  X509 *cert = NULL;

  if (malformed != NULL) {
    *malformed = 0;
  }

  if (path == NULL) {
    printf("Open File((null)) fail\n");
    printf("Read data from certificate fail\n");
    return NULL;
  }

  stream = fopen(path, "rb");
  if (stream == NULL) {
    printf("Open File(%s) fail\n", path);
    printf("Read data from certificate fail\n");
    return NULL;
  }

  cert = d2i_X509_fp(stream, NULL);
  fclose(stream);
  if (cert == NULL) {
    if (malformed != NULL) {
      *malformed = 1;
    }
    fprintf(stderr, "d2i_X509 fail\n");
    printf("transfer data to der format fail\n");
    ERR_clear_error();
  }
  return cert;
}

static int write_certificate_der(const char *path, X509 *cert)
{
  BIO *bio = NULL;
  int ok = 0;

  bio = BIO_new_file(path, "wb");
  if (bio == NULL) {
    return 0;
  }

  ok = i2d_X509_bio(bio, cert);
  BIO_free(bio);
  return ok == 1;
}

static int write_empty_failed_certificate(const char *path)
{
  FILE *stream = NULL;

  if (path == NULL) {
    printf("Can't open output file: (null)\n");
    printf("Can't write cert into file\n");
    return 255;
  }

  stream = fopen(path, "wb");
  if (stream != NULL) {
    fclose(stream);
  } else {
    printf("Can't open output file: %s\n", path);
  }
  printf("Can't write cert into file\n");
  return stream == NULL ? 255 : 0;
}

static int fail_output_certificate_write(const char *path)
{
  ERR_clear_error();
  printf("Can't open output file: %s\n", path == NULL ? "(null)" : path);
  printf("Fail to add extesnsion into cert\n");
  return 255;
}

static int pkey_to_der(EVP_PKEY *key, unsigned char **der, int *der_len)
{
  int len;
  unsigned char *buf = NULL;
  unsigned char *tmp = NULL;

  len = i2d_PUBKEY(key, NULL);
  if (len <= 0) {
    return 0;
  }

  buf = OPENSSL_malloc((size_t)len);
  if (buf == NULL) {
    return 0;
  }

  tmp = buf;
  if (i2d_PUBKEY(key, &tmp) != len) {
    OPENSSL_free(buf);
    return 0;
  }

  *der = buf;
  *der_len = len;
  return 1;
}

static int add_v3_extension(X509 *cert, int nid, const char *value)
{
  X509V3_CTX ctx;
  X509_EXTENSION *ext = NULL;
  int ok = 0;

  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);

  ext = X509V3_EXT_conf_nid(NULL, &ctx, nid, (char *)value);
  if (ext == NULL) {
    return 0;
  }

  ok = X509_add_ext(cert, ext, -1);
  X509_EXTENSION_free(ext);
  return ok == 1;
}

static int add_custom_extension(X509 *cert, EVP_PKEY *embedded_key)
{
  ASN1_OBJECT *obj = NULL;
  ASN1_OCTET_STRING *octets = NULL;
  X509_EXTENSION *ext = NULL;
  unsigned char *der = NULL;
  int der_len = 0;
  int ok = 0;

  if (!pkey_to_der(embedded_key, &der, &der_len)) {
    goto out;
  }

  obj = OBJ_txt2obj(TRUSTED_KEY_OID, 1);
  octets = ASN1_OCTET_STRING_new();
  if (obj == NULL || octets == NULL) {
    goto out;
  }

  if (ASN1_OCTET_STRING_set(octets, der, der_len) != 1) {
    goto out;
  }

  ext = X509_EXTENSION_create_by_OBJ(NULL, obj, 1, octets);
  if (ext == NULL) {
    goto out;
  }

  ok = X509_add_ext(cert, ext, -1) == 1;

out:
  X509_EXTENSION_free(ext);
  ASN1_OCTET_STRING_free(octets);
  ASN1_OBJECT_free(obj);
  OPENSSL_free(der);
  return ok;
}

static int set_serial_from_bytes(X509 *cert, const unsigned char *serial_bytes, size_t serial_len)
{
  ASN1_INTEGER *serial = NULL;
  BIGNUM *bn = NULL;
  int ok = 0;

  bn = BN_bin2bn(serial_bytes, (int)serial_len, NULL);
  if (bn == NULL) {
    goto out;
  }

  serial = BN_to_ASN1_INTEGER(bn, NULL);
  if (serial == NULL) {
    goto out;
  }

  ok = X509_set_serialNumber(cert, serial) == 1;

out:
  ASN1_INTEGER_free(serial);
  BN_free(bn);
  return ok;
}

static int set_random_serial(X509 *cert)
{
  unsigned char buf[8];

  if (RAND_bytes(buf, (int)sizeof(buf)) != 1) {
    return 0;
  }

  buf[0] &= 0x7f;
  if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0 &&
      buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 0) {
    buf[7] = 1;
  }

  return set_serial_from_bytes(cert, buf, sizeof(buf));
}

static int set_validity_window(X509 *cert)
{
  time_t now = (time_t)-1;

  now = time(NULL);
  if (now == (time_t)-1) {
    return 0;
  }

  if (ASN1_TIME_adj(X509_getm_notBefore(cert), now, 0, 0) == NULL) {
    return 0;
  }

  if (ASN1_TIME_adj(X509_getm_notAfter(cert), now, CERT_VALIDITY_DAYS, 0) == NULL) {
    return 0;
  }

  return 1;
}

static int set_common_name(X509_NAME *name)
{
  return X509_NAME_add_entry_by_txt(
           name,
           "CN",
           MBSTRING_UTF8,
           (const unsigned char *)CERT_COMMON_NAME,
           -1,
           -1,
           0
         ) == 1;
}

static int generate_certificate(const char *public_key_path, const char *sign_key_path, const char *output_path)
{
  EVP_PKEY *public_key = NULL;
  EVP_PKEY *sign_key = NULL;
  X509 *cert = NULL;
  X509_NAME *name = NULL;
  EVP_MD_CTX *md_ctx = NULL;
  EVP_PKEY_CTX *pkey_ctx = NULL;
  int status = 255;
  int open_failed = 0;
  int pem_failed = 0;

  public_key = load_public_key_for_extension(public_key_path, &open_failed, &pem_failed);
  if (public_key == NULL) {
    status = fail_public_key_load(
               public_key_path,
               open_failed ? "Can't open key file: %s\n" : NULL,
               pem_failed ? "Fail to read public key file by PEM format" : NULL
             );
    goto out;
  }

  printf("Sign key: %s\n", sign_key_path == NULL ? "(null)" : sign_key_path);
  sign_key = load_private_key_for_vendor(sign_key_path, &open_failed);
  if (sign_key == NULL) {
    if (open_failed) {
      printf("Can't open private key file: %s\n", sign_key_path == NULL ? "(null)" : sign_key_path);
    }
    status = write_empty_failed_certificate(output_path);
    goto out;
  }

  cert = X509_new();
  if (cert == NULL) {
    status = failf("Failed to allocate X509 certificate");
    goto out;
  }

  if (X509_set_version(cert, 2L) != 1 ||
      !set_validity_window(cert) ||
      X509_set_pubkey(cert, sign_key) != 1) {
    status = failf("Failed to initialise certificate fields");
    goto out;
  }

  name = X509_NAME_new();
  if (name == NULL || !set_common_name(name)) {
    status = failf("Failed to create certificate name");
    goto out;
  }

  if (X509_set_subject_name(cert, name) != 1 ||
      X509_set_issuer_name(cert, name) != 1) {
    status = failf("Failed to set certificate subject or issuer");
    goto out;
  }

  if (!add_v3_extension(cert, NID_subject_key_identifier, "hash") ||
      !add_v3_extension(cert, NID_authority_key_identifier, "keyid:always") ||
      !add_v3_extension(cert, NID_basic_constraints, "CA:FALSE") ||
      !add_custom_extension(cert, public_key)) {
    status = failf("Failed to add certificate extensions");
    goto out;
  }

  if (!set_random_serial(cert)) {
    status = failf("Failed to initialise certificate serial number");
    goto out;
  }

  md_ctx = EVP_MD_CTX_new();
  if (md_ctx == NULL ||
      EVP_DigestSignInit(md_ctx, &pkey_ctx, EVP_sha256(), NULL, sign_key) != 1 ||
      EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) != 1 ||
      EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, 32) != 1 ||
      EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, EVP_sha256()) != 1 ||
      X509_sign_ctx(cert, md_ctx) <= 0) {
    status = failf("Failed to sign certificate");
    goto out;
  }

  if (!write_certificate_der(output_path, cert)) {
    status = fail_output_certificate_write(output_path);
    goto out;
  }

  status = 0;

out:
  if (status != 0) {
    ERR_clear_error();
  }

  EVP_MD_CTX_free(md_ctx);
  X509_NAME_free(name);
  X509_free(cert);
  EVP_PKEY_free(sign_key);
  EVP_PKEY_free(public_key);
  return status;
}

static int compare_public_keys(EVP_PKEY *left, EVP_PKEY *right)
{
  unsigned char *left_der = NULL;
  unsigned char *right_der = NULL;
  int left_len = 0;
  int right_len = 0;
  int same = 0;

  if (!pkey_to_der(left, &left_der, &left_len) ||
      !pkey_to_der(right, &right_der, &right_len)) {
    goto out;
  }

  same = (left_len == right_len) && (memcmp(left_der, right_der, (size_t)left_len) == 0);

out:
  OPENSSL_free(left_der);
  OPENSSL_free(right_der);
  return same;
}

static int extract_embedded_public_key(X509 *cert, EVP_PKEY **embedded_key)
{
  ASN1_OBJECT *obj = NULL;
  int index = -1;
  X509_EXTENSION *ext = NULL;
  ASN1_OCTET_STRING *data = NULL;
  const unsigned char *cursor = NULL;
  long data_len = 0;
  EVP_PKEY *key = NULL;
  int ok = 0;

  obj = OBJ_txt2obj(TRUSTED_KEY_OID, 1);
  if (obj == NULL) {
    goto out;
  }

  index = X509_get_ext_by_OBJ(cert, obj, -1);
  if (index < 0) {
    goto out;
  }

  ext = X509_get_ext(cert, index);
  data = X509_EXTENSION_get_data(ext);
  if (data == NULL) {
    goto out;
  }

  cursor = ASN1_STRING_get0_data(data);
  data_len = ASN1_STRING_length(data);
  key = d2i_PUBKEY(NULL, &cursor, data_len);
  if (key == NULL || cursor == NULL) {
    goto out;
  }

  if ((long)(cursor - ASN1_STRING_get0_data(data)) != data_len) {
    goto out;
  }

  *embedded_key = key;
  key = NULL;
  ok = 1;

out:
  EVP_PKEY_free(key);
  ASN1_OBJECT_free(obj);
  return ok;
}

static int verify_and_extract(const char *public_key_path, const char *cert_path, const char *output_path)
{
  EVP_PKEY *signer_key = NULL;
  EVP_PKEY *expected_oem_key = NULL;
  EVP_PKEY *subject_key = NULL;
  EVP_PKEY *embedded_key = NULL;
  X509 *cert = NULL;
  int status = 255;
  int malformed_cert = 0;

  signer_key = load_public_key_for_verify(public_key_path);
  if (signer_key == NULL) {
    goto out;
  }

  expected_oem_key = load_public_key_for_verify(output_path);
  if (expected_oem_key == NULL) {
    goto out;
  }

  cert = load_der_certificate_for_verify(cert_path, &malformed_cert);
  if (cert == NULL) {
    status = malformed_cert ? 0 : 255;
    goto out;
  }

  subject_key = X509_get_pubkey(cert);
  if (subject_key == NULL) {
    printf("Get pubkey from cert fail\n");
    status = 255;
    goto out;
  }

  if (X509_verify(cert, subject_key) != 1) {
    printf("Verify cerfificate fail\n");
    status = 255;
    goto out;
  }

  if (!compare_public_keys(subject_key, signer_key)) {
    printf("Compare cix & cert public key data fail\n");
    goto out;
  }

  if (!extract_embedded_public_key(cert, &embedded_key)) {
    printf("Read data from certificate fail\n");
    goto out;
  }

  if (!compare_public_keys(embedded_key, expected_oem_key)) {
    printf("Compare extension & oem public key data fail\n");
    goto out;
  }

  printf("X509_verify successful\n");
  status = 0;

out:
  if (status != 0) {
    ERR_clear_error();
  }

  EVP_PKEY_free(embedded_key);
  EVP_PKEY_free(subject_key);
  X509_free(cert);
  EVP_PKEY_free(expected_oem_key);
  EVP_PKEY_free(signer_key);
  return status;
}

int main(int argc, char **argv)
{
  static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"output", required_argument, NULL, 'o'},
    {"publickey", required_argument, NULL, 'p'},
    {"signkey", required_argument, NULL, 's'},
    {0, 0, 0, 0}
  };

  const char *output_path = NULL;
  const char *public_key_path = NULL;
  const char *sign_key_path = NULL;
  const char *cert_path = NULL;
  int opt;

  while ((opt = getopt_long(argc, argv, "v:o:p:hs:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        print_usage(stdout);
        return 255;
      case 'o':
        output_path = optarg;
        break;
      case 'p':
        public_key_path = optarg;
        break;
      case 's':
        sign_key_path = optarg;
        break;
      case 'v':
        cert_path = optarg;
        break;
      default:
        printf("unknown option : ?\n");
        print_usage(stdout);
        return 255;
    }
  }

  if (cert_path != NULL) {
    return verify_and_extract(public_key_path, cert_path, output_path);
  }

  return generate_certificate(public_key_path, sign_key_path, output_path);
}
