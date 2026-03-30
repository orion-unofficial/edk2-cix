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
#include <time.h>

#define TRUSTED_KEY_OID "1.3.6.1.4.1.4128.2100.303"
#define CERT_COMMON_NAME "Trusted Key Certificate"
#define CERT_VALIDITY_DAYS (365 * 20)

static void print_usage(FILE *stream)
{
  fputs(
    "    ./cix_regen_trusted_key_cert <option>\n\n"
    "    options:\n"
    "        -h, --help         show the help information\n"
    "        -o, --output       output file/path\n"
    "        -p, --publickey    public key file/path\n"
    "        -s, --signkey      sign key file/path\n"
    "        -v, --certfile     cert file/path\n\n"
    "    examples:\n"
    "        If you want to replace no-trusted key file extension, you can run:\n"
    "            ./cix_regen_trusted_key_cert -p PUBLIC_KEY_FILE -s PRIVATE_KEY_FILE -o OUTPUT_FILE\n"
    "        If you want to verify no-trusted key file extension, you can run:\n"
    "            ./cix_regen_trusted_key_cert -p CIX_PUBLIC_KEY_FILE -v CERT_FILE -o OEM_PUBLIC_KEY_FILE\n",
    stream
  );
}

static void print_openssl_errors(void)
{
  ERR_print_errors_fp(stderr);
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

static EVP_PKEY *load_public_key(const char *path)
{
  BIO *bio = NULL;
  EVP_PKEY *key = NULL;

  bio = BIO_new_file(path, "r");
  if (bio == NULL) {
    return NULL;
  }

  key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  BIO_free(bio);
  return key;
}

static EVP_PKEY *load_private_key(const char *path)
{
  BIO *bio = NULL;
  EVP_PKEY *key = NULL;

  bio = BIO_new_file(path, "r");
  if (bio == NULL) {
    return NULL;
  }

  key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
  BIO_free(bio);
  return key;
}

static X509 *load_certificate(const char *path)
{
  BIO *bio = NULL;
  X509 *cert = NULL;

  bio = BIO_new_file(path, "rb");
  if (bio == NULL) {
    return NULL;
  }

  cert = d2i_X509_bio(bio, NULL);
  if (cert != NULL) {
    BIO_free(bio);
    return cert;
  }

  (void)BIO_reset(bio);
  ERR_clear_error();
  cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  BIO_free(bio);
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

static int write_public_key_pem(const char *path, EVP_PKEY *key)
{
  BIO *bio = NULL;
  int ok = 0;

  bio = BIO_new_file(path, "w");
  if (bio == NULL) {
    return 0;
  }

  ok = PEM_write_bio_PUBKEY(bio, key);
  BIO_free(bio);
  return ok == 1;
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

static int parse_hex_bytes(const char *hex, unsigned char **bytes, size_t *bytes_len)
{
  size_t hex_len = 0;
  unsigned char *buf = NULL;
  size_t index = 0;

  if (hex == NULL || bytes == NULL || bytes_len == NULL) {
    return 0;
  }

  hex_len = strlen(hex);
  if (hex_len == 0 || (hex_len % 2) != 0) {
    return 0;
  }

  buf = OPENSSL_malloc(hex_len / 2);
  if (buf == NULL) {
    return 0;
  }

  for (index = 0; index < hex_len; index += 2) {
    unsigned int value = 0;
    if (sscanf(hex + index, "%2x", &value) != 1) {
      OPENSSL_free(buf);
      return 0;
    }
    buf[index / 2] = (unsigned char)value;
  }

  *bytes = buf;
  *bytes_len = hex_len / 2;
  return 1;
}

static int set_serial_from_env(X509 *cert)
{
  const char *serial_hex = getenv("CIX_REGEN_TRUSTED_KEY_CERT_SERIAL_HEX");
  unsigned char *serial_bytes = NULL;
  size_t serial_len = 0;
  int ok = 0;

  if (serial_hex == NULL || serial_hex[0] == '\0') {
    return 0;
  }

  if (!parse_hex_bytes(serial_hex, &serial_bytes, &serial_len) || serial_len == 0) {
    return 0;
  }

  serial_bytes[0] &= 0x7f;
  if (serial_bytes[0] == 0 && serial_len == 1) {
    serial_bytes[0] = 1;
  }

  ok = set_serial_from_bytes(cert, serial_bytes, serial_len);
  OPENSSL_free(serial_bytes);
  return ok;
}

static int set_deterministic_serial(X509 *cert, EVP_PKEY *public_key, EVP_PKEY *sign_key)
{
  const char *source_date_epoch = getenv("SOURCE_DATE_EPOCH");
  unsigned char *public_der = NULL;
  unsigned char *sign_der = NULL;
  int public_der_len = 0;
  int sign_der_len = 0;
  EVP_MD_CTX *ctx = NULL;
  unsigned char digest[SHA256_DIGEST_LENGTH];
  unsigned int digest_len = 0;
  unsigned char serial_bytes[8];
  int ok = 0;

  if (source_date_epoch == NULL || source_date_epoch[0] == '\0') {
    return 0;
  }

  if (!pkey_to_der(public_key, &public_der, &public_der_len) ||
      !pkey_to_der(sign_key, &sign_der, &sign_der_len)) {
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL ||
      EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
      EVP_DigestUpdate(ctx, public_der, (size_t)public_der_len) != 1 ||
      EVP_DigestUpdate(ctx, sign_der, (size_t)sign_der_len) != 1 ||
      EVP_DigestUpdate(ctx, source_date_epoch, strlen(source_date_epoch)) != 1 ||
      EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 ||
      digest_len < sizeof(serial_bytes)) {
    goto out;
  }

  memcpy(serial_bytes, digest, sizeof(serial_bytes));
  serial_bytes[0] &= 0x7f;
  if (serial_bytes[0] == 0 && serial_bytes[1] == 0 && serial_bytes[2] == 0 &&
      serial_bytes[3] == 0 && serial_bytes[4] == 0 && serial_bytes[5] == 0 &&
      serial_bytes[6] == 0 && serial_bytes[7] == 0) {
    serial_bytes[7] = 1;
  }

  ok = set_serial_from_bytes(cert, serial_bytes, sizeof(serial_bytes));

out:
  EVP_MD_CTX_free(ctx);
  OPENSSL_free(sign_der);
  OPENSSL_free(public_der);
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
  const char *source_date_epoch = getenv("SOURCE_DATE_EPOCH");
  time_t now = (time_t)-1;
  char *endptr = NULL;

  if (source_date_epoch != NULL && source_date_epoch[0] != '\0') {
    long long parsed = strtoll(source_date_epoch, &endptr, 10);
    if (endptr == source_date_epoch || *endptr != '\0' || parsed < 0) {
      return 0;
    }
    now = (time_t)parsed;
  } else {
    now = time(NULL);
    if (now == (time_t)-1) {
      return 0;
    }
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

  public_key = load_public_key(public_key_path);
  if (public_key == NULL) {
    status = failf("Failed to load public key: %s", public_key_path);
    goto out;
  }

  sign_key = load_private_key(sign_key_path);
  if (sign_key == NULL) {
    status = failf("Failed to load sign key: %s", sign_key_path);
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

  if (!set_serial_from_env(cert) &&
      !set_deterministic_serial(cert, public_key, sign_key) &&
      !set_random_serial(cert)) {
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
    status = failf("Failed to write certificate: %s", output_path);
    goto out;
  }

  status = 0;

out:
  if (status != 0) {
    print_openssl_errors();
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
  EVP_PKEY *subject_key = NULL;
  EVP_PKEY *embedded_key = NULL;
  X509 *cert = NULL;
  int status = 255;

  signer_key = load_public_key(public_key_path);
  if (signer_key == NULL) {
    status = failf("Failed to load public key: %s", public_key_path);
    goto out;
  }

  cert = load_certificate(cert_path);
  if (cert == NULL) {
    status = failf("Failed to load certificate: %s", cert_path);
    goto out;
  }

  subject_key = X509_get_pubkey(cert);
  if (subject_key == NULL) {
    status = failf("Certificate does not contain a public key");
    goto out;
  }

  if (!compare_public_keys(subject_key, signer_key)) {
    status = failf("Certificate public key does not match supplied signer key");
    goto out;
  }

  if (X509_verify(cert, signer_key) != 1) {
    status = failf("Certificate signature verification failed");
    goto out;
  }

  if (!extract_embedded_public_key(cert, &embedded_key)) {
    status = failf("Failed to extract embedded OEM public key");
    goto out;
  }

  if (!write_public_key_pem(output_path, embedded_key)) {
    status = failf("Failed to write extracted public key: %s", output_path);
    goto out;
  }

  status = 0;

out:
  if (status != 0) {
    print_openssl_errors();
  }

  EVP_PKEY_free(embedded_key);
  EVP_PKEY_free(subject_key);
  X509_free(cert);
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
    {"certfile", required_argument, NULL, 'v'},
    {0, 0, 0, 0}
  };

  const char *output_path = NULL;
  const char *public_key_path = NULL;
  const char *sign_key_path = NULL;
  const char *cert_path = NULL;
  int opt;

  while ((opt = getopt_long(argc, argv, "ho:p:s:v:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        print_usage(stdout);
        return 0;
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
        print_usage(stderr);
        return 255;
    }
  }

  if (public_key_path == NULL || output_path == NULL) {
    print_usage(stderr);
    return 255;
  }

  if ((sign_key_path == NULL && cert_path == NULL) ||
      (sign_key_path != NULL && cert_path != NULL)) {
    print_usage(stderr);
    return 255;
  }

  if (sign_key_path != NULL) {
    return generate_certificate(public_key_path, sign_key_path, output_path);
  }

  return verify_and_extract(public_key_path, cert_path, output_path);
}
