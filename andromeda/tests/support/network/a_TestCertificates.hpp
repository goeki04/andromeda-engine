#pragma once

/**
 * @file a_TestCertificates.hpp
 * @brief A throwaway certificate authority and server certificate for TLS tests.
 */

#include <boost/asio/ssl/context.hpp>
#include <memory>
#include <string>

typedef struct evp_pkey_st EVP_PKEY;
typedef struct x509_st X509;

namespace Andromeda::Test {

    /**
     * @class TestCertificates
     * @brief A certificate authority and a server certificate signed by it, created at runtime.
     * @details Generated fresh for every test instead of being checked in: no private key in the
     *          repository and nothing that expires. The client trusts the CA only, so the tests
     *          exercise the real verification path - chain, validity and host name - just as against
     *          a production server.
     */
    class TestCertificates {
    public:
        /**
         * @param subjectAltNames The names the server certificate is valid for, in OpenSSL's
         *        notation. The default matches a server reached as 127.0.0.1 or localhost; pass e.g.
         *        "DNS:other.invalid" to get a certificate that belongs to a different host.
         */
        explicit TestCertificates(const std::string& subjectAltNames = "IP:127.0.0.1,DNS:localhost");
        ~TestCertificates();
        TestCertificates(const TestCertificates&) = delete;
        TestCertificates& operator=(const TestCertificates&) = delete;

        /** @brief Installs the server certificate and its key into a server-side context. */
        void useAsServer(boost::asio::ssl::context& context) const;

        /** @brief Makes a client-side context trust the CA and verify the server against it. */
        void trustIn(boost::asio::ssl::context& context) const;

    private:
        struct KeyDeleter { void operator()(EVP_PKEY* key) const; };
        struct CertificateDeleter { void operator()(X509* certificate) const; };

        std::unique_ptr<EVP_PKEY, KeyDeleter> m_CaKey;
        std::unique_ptr<X509, CertificateDeleter> m_CaCertificate;
        std::unique_ptr<EVP_PKEY, KeyDeleter> m_ServerKey;
        std::unique_ptr<X509, CertificateDeleter> m_ServerCertificate;
    };
}
