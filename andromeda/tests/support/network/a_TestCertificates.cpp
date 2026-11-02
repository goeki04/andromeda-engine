#include "a_TestCertificates.hpp"
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdexcept>

namespace Andromeda::Test {
    namespace {
        void check(const bool ok, const char* step) {
            if (!ok) {
                throw std::runtime_error(std::string("TestCertificates: ") + step + " failed");
            }
        }

        EVP_PKEY* makeKey() {
            // An EC key is generated in a fraction of the time an RSA key needs.
            EVP_PKEY* key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
            check(key != nullptr, "key generation");
            return key;
        }

        void addExtension(X509* certificate, X509V3_CTX& context, const int nid, const std::string& value) {
            X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, &context, nid, value.c_str());
            check(extension != nullptr, "creating an extension");
            const bool added = X509_add_ext(certificate, extension, -1) == 1;
            X509_EXTENSION_free(extension);
            check(added, "adding an extension");
        }

        /**
         * @brief A certificate for @p key, signed by @p issuerKey (or self-signed when the issuer is null).
         * @param subjectAltNames Only for the server certificate; empty for the CA.
         */
        X509* makeCertificate(EVP_PKEY* key, const char* commonName, const long serial,
                              X509* issuer, EVP_PKEY* issuerKey, const std::string& subjectAltNames) {
            const bool isAuthority = issuer == nullptr;
            X509* certificate = X509_new();
            check(certificate != nullptr, "X509_new");
            X509_set_version(certificate, 2); // version 3, required for extensions
            ASN1_INTEGER_set(X509_get_serialNumber(certificate), serial);
            // Valid from an hour ago, so a clock that is slightly off cannot fail the tests.
            X509_gmtime_adj(X509_getm_notBefore(certificate), -60L * 60);
            X509_gmtime_adj(X509_getm_notAfter(certificate), 24L * 60 * 60);
            X509_set_pubkey(certificate, key);

            // Built separately and copied in: OpenSSL 4 only hands out the certificate's own name as const.
            X509_NAME* name = X509_NAME_new();
            X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(commonName), -1, -1, 0);
            X509_set_subject_name(certificate, name);
            X509_set_issuer_name(certificate, isAuthority ? name : X509_get_subject_name(issuer));
            X509_NAME_free(name);

            X509V3_CTX context;
            X509V3_set_ctx_nodb(&context);
            X509V3_set_ctx(&context, isAuthority ? certificate : issuer, certificate, nullptr, nullptr, 0);
            if (isAuthority) {
                addExtension(certificate, context, NID_basic_constraints, "critical,CA:TRUE");
                addExtension(certificate, context, NID_key_usage, "critical,keyCertSign,cRLSign");
            } else {
                addExtension(certificate, context, NID_basic_constraints, "critical,CA:FALSE");
                addExtension(certificate, context, NID_key_usage, "critical,digitalSignature");
                addExtension(certificate, context, NID_ext_key_usage, "serverAuth");
                addExtension(certificate, context, NID_subject_alt_name, subjectAltNames);
            }

            check(X509_sign(certificate, isAuthority ? key : issuerKey, EVP_sha256()) > 0, "signing");
            return certificate;
        }
    }

    void TestCertificates::KeyDeleter::operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
    void TestCertificates::CertificateDeleter::operator()(X509* certificate) const { X509_free(certificate); }

    TestCertificates::TestCertificates(const std::string& subjectAltNames)
        : m_CaKey(makeKey()),
          m_CaCertificate(makeCertificate(m_CaKey.get(), "Andromeda Test CA", 1, nullptr, nullptr, {})),
          m_ServerKey(makeKey()),
          m_ServerCertificate(makeCertificate(m_ServerKey.get(), "Andromeda Test Server", 2,
                                              m_CaCertificate.get(), m_CaKey.get(), subjectAltNames)) {}

    TestCertificates::~TestCertificates() = default;

    void TestCertificates::useAsServer(boost::asio::ssl::context& context) const {
        check(SSL_CTX_use_certificate(context.native_handle(), m_ServerCertificate.get()) == 1, "installing the server certificate");
        check(SSL_CTX_use_PrivateKey(context.native_handle(), m_ServerKey.get()) == 1, "installing the server key");
    }

    void TestCertificates::trustIn(boost::asio::ssl::context& context) const {
        check(X509_STORE_add_cert(SSL_CTX_get_cert_store(context.native_handle()), m_CaCertificate.get()) == 1, "trusting the CA");
        context.set_verify_mode(boost::asio::ssl::verify_peer);
    }
}
