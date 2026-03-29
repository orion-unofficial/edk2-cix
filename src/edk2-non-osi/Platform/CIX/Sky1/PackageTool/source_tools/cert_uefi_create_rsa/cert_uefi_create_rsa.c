#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NON_TRUSTED_FW_NVCOUNTER_OID "1.3.6.1.4.1.4128.2100.2"
#define NON_TRUSTED_FW_CONTENT_CERT_PK_OID "1.3.6.1.4.1.4128.2100.1101"
#define NON_TRUSTED_WORLD_BOOTLOADER_HASH_OID "1.3.6.1.4.1.4128.2100.1201"
#define NON_TRUSTED_FW_CONFIG_HASH_OID "1.3.6.1.4.1.4128.2100.1202"
#define NON_TRUSTED_FW_KEY_CERT_COMMON_NAME "Non-Trusted Firmware Key Certificate"
#define NON_TRUSTED_FW_CONTENT_CERT_COMMON_NAME "Non-Trusted Firmware Content Certificate"
#define CERT_VALIDITY_DAYS (365 * 20)
#define DEFAULT_KEY_ALG "rsa"
#define DEFAULT_HASH_ALG "sha256"
#define DEFAULT_KEY_SIZE 3072
#define NTFW_KEY_CERT_SERIAL_ENV "CERT_UEFI_CREATE_RSA_NT_FW_KEY_CERT_SERIAL_HEX"
#define NTFW_CERT_SERIAL_ENV "CERT_UEFI_CREATE_RSA_NT_FW_CERT_SERIAL_HEX"

typedef struct {
  const char *key_alg;
  int key_size;
  const char *hash_alg;
  const char *nt_fw_cert_path;
  const char *nt_fw_key_cert_path;
  const char *nt_fw_key_path;
  const char *non_trusted_world_key_path;
  const char *nt_fw_path;
  const char *nt_fw_config_path;
  uint64_t ntfw_nvctr;
  bool saw_ntfw_nvctr;
} tool_options_t;

typedef struct {
  const char *common_name;
  const char *output_path;
  const char *serial_env_name;
  EVP_PKEY *subject_key;
  EVP_PKEY *sign_key;
  const unsigned char *nvcounter_der;
  size_t nvcounter_der_len;
  const unsigned char *public_key_der;
  size_t public_key_der_len;
  const unsigned char *fw_hash_der;
  size_t fw_hash_der_len;
  const unsigned char *fw_config_hash_der;
  size_t fw_config_hash_der_len;
} cert_request_t;

static void print_usage(FILE *stream)
{
  fputs(
    "Usage: cert_uefi_create_rsa [options]\n\n"
    "Required options:\n"
    "  --nt-fw-cert PATH            output path for the non-trusted FW content cert\n"
    "  --nt-fw-key-cert PATH        output path for the non-trusted FW key cert\n"
    "  --nt-fw-key PATH             PEM private key used for the content cert\n"
    "  --non-trusted-world-key PATH PEM private key used for the key cert\n"
    "  --nt-fw PATH                 BL33/UEFI payload to hash into the content cert\n"
    "  --ntfw-nvctr VALUE           non-trusted firmware NV counter value\n\n"
    "Optional compatibility flags:\n"
    "  --nt-fw-config PATH          optional config blob hashed into the content cert\n"
    "  --key-alg rsa                accepted for compatibility; only rsa is supported\n"
    "  --key-size 3072              accepted for compatibility; defaults to 3072\n"
    "  --hash-alg sha256            accepted for compatibility; only sha256 is supported\n"
    "  -p                           accepted for compatibility; no-op\n"
    "  -h, --help                   show this help text\n\n"
    "Reproducibility knobs:\n"
    "  SOURCE_DATE_EPOCH sets certificate validity start and deterministic serial seed.\n"
    "  " NTFW_KEY_CERT_SERIAL_ENV " overrides the key-cert serial as hex.\n"
    "  " NTFW_CERT_SERIAL_ENV " overrides the content-cert serial as hex.\n",
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

static int validate_rsa_private_key(const char *label, EVP_PKEY *key, int expected_bits)
{
  if (key == NULL) {
    return failf("Missing required key material for %s", label);
  }

  if (EVP_PKEY_get_base_id(key) != EVP_PKEY_RSA) {
    return failf("%s must be an RSA private key", label);
  }

  if (expected_bits > 0 && EVP_PKEY_bits(key) != expected_bits) {
    return failf(
             "%s must be %d bits, got %d bits",
             label,
             expected_bits,
             EVP_PKEY_bits(key)
           );
  }

  return 0;
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

static int pkey_to_der(EVP_PKEY *key, unsigned char **der, size_t *der_len)
{
  int len = 0;
  unsigned char *buf = NULL;
  unsigned char *cursor = NULL;

  if (key == NULL || der == NULL || der_len == NULL) {
    return 0;
  }

  len = i2d_PUBKEY(key, NULL);
  if (len <= 0) {
    return 0;
  }

  buf = OPENSSL_malloc((size_t)len);
  if (buf == NULL) {
    return 0;
  }

  cursor = buf;
  if (i2d_PUBKEY(key, &cursor) != len) {
    OPENSSL_free(buf);
    return 0;
  }

  *der = buf;
  *der_len = (size_t)len;
  return 1;
}

static int digest_file_sha256(const char *path, unsigned char digest[SHA256_DIGEST_LENGTH])
{
  EVP_MD_CTX *ctx = NULL;
  FILE *stream = NULL;
  unsigned char buffer[8192];
  size_t bytes_read = 0;
  unsigned int digest_len = 0;
  int ok = 0;

  if (digest == NULL) {
    return 0;
  }

  stream = fopen(path, "rb");
  if (stream == NULL) {
    return 0;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    goto out;
  }

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), stream)) > 0) {
    if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
      goto out;
    }
  }

  if (ferror(stream)) {
    goto out;
  }

  if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 ||
      digest_len != SHA256_DIGEST_LENGTH) {
    goto out;
  }

  ok = 1;

out:
  EVP_MD_CTX_free(ctx);
  fclose(stream);
  return ok;
}

static int encode_sha256_digest_info(
  const unsigned char digest[SHA256_DIGEST_LENGTH],
  unsigned char **der,
  size_t *der_len
)
{
  X509_SIG *sig = NULL;
  X509_ALGOR *alg = NULL;
  ASN1_OCTET_STRING *digest_value = NULL;
  int len = 0;
  unsigned char *buf = NULL;
  unsigned char *cursor = NULL;
  int ok = 0;

  if (digest == NULL || der == NULL || der_len == NULL) {
    return 0;
  }

  sig = X509_SIG_new();
  if (sig == NULL) {
    goto out;
  }

  X509_SIG_getm(sig, &alg, &digest_value);
  if (alg == NULL || digest_value == NULL) {
    goto out;
  }

  if (X509_ALGOR_set0(alg, OBJ_nid2obj(NID_sha256), V_ASN1_NULL, NULL) != 1) {
    goto out;
  }
  if (ASN1_OCTET_STRING_set(digest_value, digest, SHA256_DIGEST_LENGTH) != 1) {
    goto out;
  }

  len = i2d_X509_SIG(sig, NULL);
  if (len <= 0) {
    goto out;
  }

  buf = OPENSSL_malloc((size_t)len);
  if (buf == NULL) {
    goto out;
  }

  cursor = buf;
  if (i2d_X509_SIG(sig, &cursor) != len) {
    goto out;
  }

  *der = buf;
  *der_len = (size_t)len;
  buf = NULL;
  ok = 1;

out:
  OPENSSL_free(buf);
  X509_SIG_free(sig);
  return ok;
}

static int encode_nvcounter_der(uint64_t value, unsigned char **der, size_t *der_len)
{
  ASN1_INTEGER *asn1_integer = NULL;
  int len = 0;
  unsigned char *buf = NULL;
  unsigned char *cursor = NULL;
  int ok = 0;

  if (der == NULL || der_len == NULL) {
    return 0;
  }

  asn1_integer = ASN1_INTEGER_new();
  if (asn1_integer == NULL || ASN1_INTEGER_set_uint64(asn1_integer, value) != 1) {
    goto out;
  }

  len = i2d_ASN1_INTEGER(asn1_integer, NULL);
  if (len <= 0) {
    goto out;
  }

  buf = OPENSSL_malloc((size_t)len);
  if (buf == NULL) {
    goto out;
  }

  cursor = buf;
  if (i2d_ASN1_INTEGER(asn1_integer, &cursor) != len) {
    goto out;
  }

  *der = buf;
  *der_len = (size_t)len;
  buf = NULL;
  ok = 1;

out:
  OPENSSL_free(buf);
  ASN1_INTEGER_free(asn1_integer);
  return ok;
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

static int add_custom_extension(
  X509 *cert,
  const char *oid,
  int critical,
  const unsigned char *payload,
  size_t payload_len
)
{
  ASN1_OBJECT *obj = NULL;
  ASN1_OCTET_STRING *octets = NULL;
  X509_EXTENSION *ext = NULL;
  int ok = 0;

  if (cert == NULL || oid == NULL || payload == NULL || payload_len == 0) {
    return 0;
  }

  obj = OBJ_txt2obj(oid, 1);
  octets = ASN1_OCTET_STRING_new();
  if (obj == NULL || octets == NULL) {
    goto out;
  }

  if (ASN1_OCTET_STRING_set(octets, payload, (int)payload_len) != 1) {
    goto out;
  }

  ext = X509_EXTENSION_create_by_OBJ(NULL, obj, critical, octets);
  if (ext == NULL) {
    goto out;
  }

  ok = X509_add_ext(cert, ext, -1) == 1;

out:
  X509_EXTENSION_free(ext);
  ASN1_OCTET_STRING_free(octets);
  ASN1_OBJECT_free(obj);
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

static int set_serial_from_env(X509 *cert, const char *env_name)
{
  const char *serial_hex = NULL;
  unsigned char *serial_bytes = NULL;
  size_t serial_len = 0;
  int ok = 0;

  if (env_name == NULL) {
    return 0;
  }

  serial_hex = getenv(env_name);
  if (serial_hex == NULL || serial_hex[0] == '\0') {
    return 0;
  }

  if (!parse_hex_bytes(serial_hex, &serial_bytes, &serial_len) || serial_len == 0) {
    return 0;
  }

  serial_bytes[0] &= 0x7f;
  if (serial_len == 1 && serial_bytes[0] == 0) {
    serial_bytes[0] = 1;
  }

  ok = set_serial_from_bytes(cert, serial_bytes, serial_len);
  OPENSSL_free(serial_bytes);
  return ok;
}

static int set_deterministic_serial(
  X509 *cert,
  const cert_request_t *request
)
{
  const char *source_date_epoch = getenv("SOURCE_DATE_EPOCH");
  EVP_MD_CTX *ctx = NULL;
  unsigned char *subject_key_der = NULL;
  size_t subject_key_der_len = 0;
  unsigned char *sign_key_der = NULL;
  size_t sign_key_der_len = 0;
  unsigned char digest[SHA256_DIGEST_LENGTH];
  unsigned int digest_len = 0;
  unsigned char serial_bytes[8];
  int ok = 0;

  if (cert == NULL || request == NULL ||
      source_date_epoch == NULL || source_date_epoch[0] == '\0') {
    return 0;
  }

  if (!pkey_to_der(request->subject_key, &subject_key_der, &subject_key_der_len) ||
      !pkey_to_der(request->sign_key, &sign_key_der, &sign_key_der_len)) {
    goto out;
  }

  ctx = EVP_MD_CTX_new();
  if (ctx == NULL || EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
    goto out;
  }

  if (EVP_DigestUpdate(ctx, request->common_name, strlen(request->common_name)) != 1 ||
      EVP_DigestUpdate(ctx, subject_key_der, subject_key_der_len) != 1 ||
      EVP_DigestUpdate(ctx, sign_key_der, sign_key_der_len) != 1 ||
      EVP_DigestUpdate(ctx, request->nvcounter_der, request->nvcounter_der_len) != 1 ||
      EVP_DigestUpdate(ctx, source_date_epoch, strlen(source_date_epoch)) != 1) {
    goto out;
  }

  if (request->public_key_der != NULL && request->public_key_der_len > 0 &&
      EVP_DigestUpdate(ctx, request->public_key_der, request->public_key_der_len) != 1) {
    goto out;
  }

  if (request->fw_hash_der != NULL && request->fw_hash_der_len > 0 &&
      EVP_DigestUpdate(ctx, request->fw_hash_der, request->fw_hash_der_len) != 1) {
    goto out;
  }

  if (request->fw_config_hash_der != NULL && request->fw_config_hash_der_len > 0 &&
      EVP_DigestUpdate(ctx, request->fw_config_hash_der, request->fw_config_hash_der_len) != 1) {
    goto out;
  }

  if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1 ||
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
  OPENSSL_free(sign_key_der);
  OPENSSL_free(subject_key_der);
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

static int set_common_name(X509_NAME *name, const char *common_name)
{
  return X509_NAME_add_entry_by_txt(
           name,
           "CN",
           MBSTRING_UTF8,
           (const unsigned char *)common_name,
           -1,
           -1,
           0
         ) == 1;
}

static int generate_certificate(const cert_request_t *request)
{
  X509 *cert = NULL;
  X509_NAME *name = NULL;
  EVP_MD_CTX *md_ctx = NULL;
  EVP_PKEY_CTX *pkey_ctx = NULL;
  int status = 255;

  if (request == NULL) {
    return failf("Internal error: missing certificate request");
  }

  cert = X509_new();
  if (cert == NULL) {
    status = failf("Failed to allocate X509 certificate");
    goto out;
  }

  if (X509_set_version(cert, 2L) != 1 ||
      !set_validity_window(cert) ||
      X509_set_pubkey(cert, request->subject_key) != 1) {
    status = failf("Failed to initialise certificate fields for %s", request->output_path);
    goto out;
  }

  name = X509_NAME_new();
  if (name == NULL || !set_common_name(name, request->common_name)) {
    status = failf("Failed to create certificate name for %s", request->output_path);
    goto out;
  }

  if (X509_set_subject_name(cert, name) != 1 ||
      X509_set_issuer_name(cert, name) != 1) {
    status = failf("Failed to set certificate subject or issuer for %s", request->output_path);
    goto out;
  }

  if (!add_v3_extension(cert, NID_subject_key_identifier, "hash") ||
      !add_v3_extension(cert, NID_authority_key_identifier, "keyid:always") ||
      !add_v3_extension(cert, NID_basic_constraints, "CA:FALSE") ||
      !add_custom_extension(
         cert,
         NON_TRUSTED_FW_NVCOUNTER_OID,
         1,
         request->nvcounter_der,
         request->nvcounter_der_len
       )) {
    status = failf("Failed to add base certificate extensions for %s", request->output_path);
    goto out;
  }

  if (request->public_key_der != NULL &&
      !add_custom_extension(
        cert,
        NON_TRUSTED_FW_CONTENT_CERT_PK_OID,
        1,
        request->public_key_der,
        request->public_key_der_len
      )) {
    status = failf("Failed to add public-key extension for %s", request->output_path);
    goto out;
  }

  if (request->fw_hash_der != NULL &&
      !add_custom_extension(
        cert,
        NON_TRUSTED_WORLD_BOOTLOADER_HASH_OID,
        1,
        request->fw_hash_der,
        request->fw_hash_der_len
      )) {
    status = failf("Failed to add firmware hash extension for %s", request->output_path);
    goto out;
  }

  if (request->fw_config_hash_der != NULL &&
      !add_custom_extension(
        cert,
        NON_TRUSTED_FW_CONFIG_HASH_OID,
        1,
        request->fw_config_hash_der,
        request->fw_config_hash_der_len
      )) {
    status = failf("Failed to add firmware-config hash extension for %s", request->output_path);
    goto out;
  }

  if (!set_serial_from_env(cert, request->serial_env_name) &&
      !set_deterministic_serial(cert, request) &&
      !set_random_serial(cert)) {
    status = failf("Failed to initialise certificate serial number for %s", request->output_path);
    goto out;
  }

  md_ctx = EVP_MD_CTX_new();
  if (md_ctx == NULL ||
      EVP_DigestSignInit(md_ctx, &pkey_ctx, EVP_sha256(), NULL, request->sign_key) != 1 ||
      EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) != 1 ||
      EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, 32) != 1 ||
      EVP_PKEY_CTX_set_rsa_mgf1_md(pkey_ctx, EVP_sha256()) != 1 ||
      X509_sign_ctx(cert, md_ctx) <= 0) {
    status = failf("Failed to sign certificate %s", request->output_path);
    goto out;
  }

  if (!write_certificate_der(request->output_path, cert)) {
    status = failf("Failed to write certificate: %s", request->output_path);
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
  return status;
}

static int parse_key_size(const char *text, int *value)
{
  long parsed = 0;
  char *endptr = NULL;

  if (text == NULL || value == NULL) {
    return 0;
  }

  errno = 0;
  parsed = strtol(text, &endptr, 10);
  if (errno != 0 || endptr == text || *endptr != '\0' || parsed <= 0 ||
      parsed > INT32_MAX) {
    return 0;
  }

  *value = (int)parsed;
  return 1;
}

static int parse_uint64_value(const char *text, uint64_t *value)
{
  unsigned long long parsed = 0;
  char *endptr = NULL;

  if (text == NULL || value == NULL) {
    return 0;
  }

  errno = 0;
  parsed = strtoull(text, &endptr, 10);
  if (errno != 0 || endptr == text || *endptr != '\0') {
    return 0;
  }

  *value = (uint64_t)parsed;
  return 1;
}

static int parse_arguments(int argc, char **argv, tool_options_t *options)
{
  enum {
    OPT_KEY_ALG = 1000,
    OPT_KEY_SIZE,
    OPT_HASH_ALG,
    OPT_NTFW_NVCTR,
    OPT_NT_FW_CERT,
    OPT_NT_FW_KEY_CERT,
    OPT_NT_FW_KEY,
    OPT_NON_TRUSTED_WORLD_KEY,
    OPT_NT_FW,
    OPT_NT_FW_CONFIG
  };
  static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"key-alg", required_argument, NULL, OPT_KEY_ALG},
    {"key-size", required_argument, NULL, OPT_KEY_SIZE},
    {"hash-alg", required_argument, NULL, OPT_HASH_ALG},
    {"ntfw-nvctr", required_argument, NULL, OPT_NTFW_NVCTR},
    {"nt-fw-cert", required_argument, NULL, OPT_NT_FW_CERT},
    {"nt-fw-key-cert", required_argument, NULL, OPT_NT_FW_KEY_CERT},
    {"nt-fw-key", required_argument, NULL, OPT_NT_FW_KEY},
    {"non-trusted-world-key", required_argument, NULL, OPT_NON_TRUSTED_WORLD_KEY},
    {"nt-fw", required_argument, NULL, OPT_NT_FW},
    {"nt-fw-config", required_argument, NULL, OPT_NT_FW_CONFIG},
    {NULL, 0, NULL, 0}
  };
  int opt = 0;

  if (options == NULL) {
    return failf("Internal error: missing options storage");
  }

  options->key_alg = DEFAULT_KEY_ALG;
  options->key_size = DEFAULT_KEY_SIZE;
  options->hash_alg = DEFAULT_HASH_ALG;

  optind = 1;
  while ((opt = getopt_long(argc, argv, "hp", long_options, NULL)) != -1) {
    switch (opt) {
      case 'h':
        print_usage(stdout);
        return 1;
      case 'p':
        break;
      case OPT_KEY_ALG:
        options->key_alg = optarg;
        break;
      case OPT_KEY_SIZE:
        if (!parse_key_size(optarg, &options->key_size)) {
          return failf("Invalid --key-size value: %s", optarg);
        }
        break;
      case OPT_HASH_ALG:
        options->hash_alg = optarg;
        break;
      case OPT_NTFW_NVCTR:
        if (!parse_uint64_value(optarg, &options->ntfw_nvctr)) {
          return failf("Invalid --ntfw-nvctr value: %s", optarg);
        }
        options->saw_ntfw_nvctr = true;
        break;
      case OPT_NT_FW_CERT:
        options->nt_fw_cert_path = optarg;
        break;
      case OPT_NT_FW_KEY_CERT:
        options->nt_fw_key_cert_path = optarg;
        break;
      case OPT_NT_FW_KEY:
        options->nt_fw_key_path = optarg;
        break;
      case OPT_NON_TRUSTED_WORLD_KEY:
        options->non_trusted_world_key_path = optarg;
        break;
      case OPT_NT_FW:
        options->nt_fw_path = optarg;
        break;
      case OPT_NT_FW_CONFIG:
        options->nt_fw_config_path = optarg;
        break;
      default:
        print_usage(stderr);
        return failf("Unrecognised option");
    }
  }

  if (optind != argc) {
    return failf("Unexpected positional argument: %s", argv[optind]);
  }

  if (strcmp(options->key_alg, "rsa") != 0) {
    return failf("Unsupported --key-alg=%s; only rsa is supported", options->key_alg);
  }

  if (strcmp(options->hash_alg, "sha256") != 0) {
    return failf("Unsupported --hash-alg=%s; only sha256 is supported", options->hash_alg);
  }

  if (options->nt_fw_cert_path == NULL ||
      options->nt_fw_key_cert_path == NULL ||
      options->nt_fw_key_path == NULL ||
      options->non_trusted_world_key_path == NULL ||
      options->nt_fw_path == NULL ||
      !options->saw_ntfw_nvctr) {
    print_usage(stderr);
    return failf("Missing one or more required options");
  }

  return 0;
}

int main(int argc, char **argv)
{
  tool_options_t options = {0};
  EVP_PKEY *nt_fw_key = NULL;
  EVP_PKEY *non_trusted_world_key = NULL;
  unsigned char *nt_fw_key_public_der = NULL;
  size_t nt_fw_key_public_der_len = 0;
  unsigned char *nvcounter_der = NULL;
  size_t nvcounter_der_len = 0;
  unsigned char *nt_fw_hash_der = NULL;
  size_t nt_fw_hash_der_len = 0;
  unsigned char *nt_fw_config_hash_der = NULL;
  size_t nt_fw_config_hash_der_len = 0;
  unsigned char nt_fw_digest[SHA256_DIGEST_LENGTH];
  unsigned char nt_fw_config_digest[SHA256_DIGEST_LENGTH];
  cert_request_t nt_fw_key_cert_request;
  cert_request_t nt_fw_cert_request;
  int status = 255;

  status = parse_arguments(argc, argv, &options);
  if (status != 0) {
    return status > 0 ? 0 : status;
  }

  memset(nt_fw_config_digest, 0, sizeof(nt_fw_config_digest));

  nt_fw_key = load_private_key(options.nt_fw_key_path);
  status = validate_rsa_private_key("nt-fw-key", nt_fw_key, options.key_size);
  if (status != 0) {
    goto out;
  }

  non_trusted_world_key = load_private_key(options.non_trusted_world_key_path);
  status = validate_rsa_private_key(
             "non-trusted-world-key",
             non_trusted_world_key,
             options.key_size
           );
  if (status != 0) {
    goto out;
  }

  if (!pkey_to_der(nt_fw_key, &nt_fw_key_public_der, &nt_fw_key_public_der_len)) {
    status = failf("Failed to encode nt-fw-key public key");
    goto out;
  }

  if (!encode_nvcounter_der(options.ntfw_nvctr, &nvcounter_der, &nvcounter_der_len)) {
    status = failf("Failed to encode --ntfw-nvctr value");
    goto out;
  }

  if (!digest_file_sha256(options.nt_fw_path, nt_fw_digest) ||
      !encode_sha256_digest_info(nt_fw_digest, &nt_fw_hash_der, &nt_fw_hash_der_len)) {
    status = failf("Failed to hash firmware payload: %s", options.nt_fw_path);
    goto out;
  }

  if (options.nt_fw_config_path != NULL) {
    if (!digest_file_sha256(options.nt_fw_config_path, nt_fw_config_digest)) {
      status = failf("Failed to hash firmware config: %s", options.nt_fw_config_path);
      goto out;
    }
  }

  if (!encode_sha256_digest_info(
         nt_fw_config_digest,
         &nt_fw_config_hash_der,
         &nt_fw_config_hash_der_len
       )) {
    status = failf("Failed to encode firmware-config hash extension");
    goto out;
  }

  nt_fw_key_cert_request = (cert_request_t){
    .common_name = NON_TRUSTED_FW_KEY_CERT_COMMON_NAME,
    .output_path = options.nt_fw_key_cert_path,
    .serial_env_name = NTFW_KEY_CERT_SERIAL_ENV,
    .subject_key = non_trusted_world_key,
    .sign_key = non_trusted_world_key,
    .nvcounter_der = nvcounter_der,
    .nvcounter_der_len = nvcounter_der_len,
    .public_key_der = nt_fw_key_public_der,
    .public_key_der_len = nt_fw_key_public_der_len,
    .fw_hash_der = NULL,
    .fw_hash_der_len = 0,
    .fw_config_hash_der = NULL,
    .fw_config_hash_der_len = 0
  };
  status = generate_certificate(&nt_fw_key_cert_request);
  if (status != 0) {
    goto out;
  }

  nt_fw_cert_request = (cert_request_t){
    .common_name = NON_TRUSTED_FW_CONTENT_CERT_COMMON_NAME,
    .output_path = options.nt_fw_cert_path,
    .serial_env_name = NTFW_CERT_SERIAL_ENV,
    .subject_key = nt_fw_key,
    .sign_key = nt_fw_key,
    .nvcounter_der = nvcounter_der,
    .nvcounter_der_len = nvcounter_der_len,
    .public_key_der = NULL,
    .public_key_der_len = 0,
    .fw_hash_der = nt_fw_hash_der,
    .fw_hash_der_len = nt_fw_hash_der_len,
    .fw_config_hash_der = nt_fw_config_hash_der,
    .fw_config_hash_der_len = nt_fw_config_hash_der_len
  };
  status = generate_certificate(&nt_fw_cert_request);

out:
  if (status != 0 && ERR_peek_error() != 0) {
    print_openssl_errors();
  }
  OPENSSL_free(nt_fw_config_hash_der);
  OPENSSL_free(nt_fw_hash_der);
  OPENSSL_free(nvcounter_der);
  OPENSSL_free(nt_fw_key_public_der);
  EVP_PKEY_free(non_trusted_world_key);
  EVP_PKEY_free(nt_fw_key);
  return status;
}
