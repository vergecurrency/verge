// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Wraps dumb protocol buffer paymentRequest
// with some extra methods
//

#include <qt/paymentrequestplus.h>

#include <util/system.h>

#include <memory>
#include <stdexcept>

#include <openssl/x509_vfy.h>

#include <QDateTime>
#include <QDebug>
#include <QSslCertificate>

class SSLVerifyError : public std::runtime_error
{
public:
    explicit SSLVerifyError(std::string err) : std::runtime_error(err) { }
};

namespace {
struct X509Deleter {
    void operator()(X509* cert) const { X509_free(cert); }
};

struct X509StackDeleter {
    void operator()(STACK_OF(X509)* chain) const { sk_X509_free(chain); }
};

struct X509StoreCtxDeleter {
    void operator()(X509_STORE_CTX* ctx) const { X509_STORE_CTX_free(ctx); }
};

struct EVPKeyDeleter {
    void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};

#if HAVE_DECL_EVP_MD_CTX_NEW
struct EVPMdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const { EVP_MD_CTX_free(ctx); }
};
#endif

QString GetCommonNameFromCert(const X509* cert)
{
    if (!cert) {
        return QString();
    }

    const X509_NAME* certname = X509_get_subject_name(cert);
    if (!certname) {
        return QString();
    }

    const int commonNameIndex = X509_NAME_get_index_by_NID(certname, NID_commonName, -1);
    if (commonNameIndex < 0) {
        return QString();
    }

    const X509_NAME_ENTRY* commonNameEntry = X509_NAME_get_entry(certname, commonNameIndex);
    if (!commonNameEntry) {
        return QString();
    }

    const ASN1_STRING* commonNameData = X509_NAME_ENTRY_get_data(commonNameEntry);
    if (!commonNameData) {
        return QString();
    }

    unsigned char* utf8Data = nullptr;
    const int utf8Length = ASN1_STRING_to_UTF8(&utf8Data, commonNameData);
    if (utf8Length <= 0 || utf8Data == nullptr) {
        OPENSSL_free(utf8Data);
        return QString();
    }

    const QString commonName = QString::fromUtf8(reinterpret_cast<const char*>(utf8Data), utf8Length);
    OPENSSL_free(utf8Data);
    return commonName;
}
} // namespace

bool PaymentRequestPlus::parse(const QByteArray& data)
{
    bool parseOK = paymentRequest.ParseFromArray(data.data(), data.size());
    if (!parseOK) {
        qWarning() << "PaymentRequestPlus::parse: Error parsing payment request";
        return false;
    }
    if (paymentRequest.payment_details_version() > 1) {
        qWarning() << "PaymentRequestPlus::parse: Received up-version payment details, version=" << paymentRequest.payment_details_version();
        return false;
    }

    parseOK = details.ParseFromString(paymentRequest.serialized_payment_details());
    if (!parseOK)
    {
        qWarning() << "PaymentRequestPlus::parse: Error parsing payment details";
        paymentRequest.Clear();
        return false;
    }
    return true;
}

bool PaymentRequestPlus::SerializeToString(std::string* output) const
{
    return paymentRequest.SerializeToString(output);
}

bool PaymentRequestPlus::IsInitialized() const
{
    return paymentRequest.IsInitialized();
}

bool PaymentRequestPlus::getMerchant(X509_STORE* certStore, QString& merchant) const
{
    merchant.clear();

    if (!IsInitialized())
        return false;

    // One day we'll support more PKI types, but just
    // x509 for now:
    const EVP_MD* digestAlgorithm = nullptr;
    if (paymentRequest.pki_type() == "x509+sha256") {
        digestAlgorithm = EVP_sha256();
    }
    else if (paymentRequest.pki_type() == "x509+sha1") {
        digestAlgorithm = EVP_sha1();
    }
    else if (paymentRequest.pki_type() == "none") {
        qWarning() << "PaymentRequestPlus::getMerchant: Payment request: pki_type == none";
        return false;
    }
    else {
        qWarning() << "PaymentRequestPlus::getMerchant: Payment request: unknown pki_type " << QString::fromStdString(paymentRequest.pki_type());
        return false;
    }

    payments::X509Certificates certChain;
    if (!certChain.ParseFromString(paymentRequest.pki_data())) {
        qWarning() << "PaymentRequestPlus::getMerchant: Payment request: error parsing pki_data";
        return false;
    }

    std::vector<std::unique_ptr<X509, X509Deleter>> certs;
    const QDateTime currentTime = QDateTime::currentDateTime();
    for (int i = 0; i < certChain.certificate_size(); i++) {
        QByteArray certData(certChain.certificate(i).data(), certChain.certificate(i).size());
        QSslCertificate qCert(certData, QSsl::Der);
        if (currentTime < qCert.effectiveDate() || currentTime > qCert.expiryDate()) {
            qWarning() << "PaymentRequestPlus::getMerchant: Payment request: certificate expired or not yet active: " << qCert;
            return false;
        }
#if QT_VERSION >= 0x050000
        if (qCert.isBlacklisted()) {
            qWarning() << "PaymentRequestPlus::getMerchant: Payment request: certificate blacklisted: " << qCert;
            return false;
        }
#endif
        const unsigned char *data = (const unsigned char *)certChain.certificate(i).data();
        std::unique_ptr<X509, X509Deleter> cert(d2i_X509(nullptr, &data, certChain.certificate(i).size()));
        if (cert)
            certs.push_back(std::move(cert));
    }
    if (certs.empty()) {
        qWarning() << "PaymentRequestPlus::getMerchant: Payment request: empty certificate chain";
        return false;
    }

    // The first cert is the signing cert, the rest are untrusted certs that chain
    // to a valid root authority. OpenSSL needs them separately.
    std::unique_ptr<STACK_OF(X509), X509StackDeleter> chain(sk_X509_new_null());
    if (!chain) {
        qWarning() << "PaymentRequestPlus::getMerchant: error allocating certificate chain";
        return false;
    }
    for (int i = certs.size() - 1; i > 0; i--) {
        if (!sk_X509_push(chain.get(), certs[i].get())) {
            qWarning() << "PaymentRequestPlus::getMerchant: error building certificate chain";
            return false;
        }
    }
    X509 *signing_cert = certs[0].get();

    // Now create a "store context", which is a single use object for checking,
    // load the signing cert into it and verify.
    std::unique_ptr<X509_STORE_CTX, X509StoreCtxDeleter> store_ctx(X509_STORE_CTX_new());
    if (!store_ctx) {
        qWarning() << "PaymentRequestPlus::getMerchant: Payment request: error creating X509_STORE_CTX";
        return false;
    }

    bool fResult = true;
    try
    {
        if (!X509_STORE_CTX_init(store_ctx.get(), certStore, signing_cert, chain.get()))
        {
            int error = X509_STORE_CTX_get_error(store_ctx.get());
            throw SSLVerifyError(X509_verify_cert_error_string(error));
        }

        // Now do the verification!
        int result = X509_verify_cert(store_ctx.get());
        if (result != 1) {
            int error = X509_STORE_CTX_get_error(store_ctx.get());
            // For testing payment requests, we allow self signed root certs!
            // This option is just shown in the UI options, if -help-debug is enabled.
            if (!(error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT && gArgs.GetBoolArg("-allowselfsignedrootcertificates", DEFAULT_SELFSIGNED_ROOTCERTS))) {
                throw SSLVerifyError(X509_verify_cert_error_string(error));
            } else {
               qDebug() << "PaymentRequestPlus::getMerchant: Allowing self signed root certificate, because -allowselfsignedrootcertificates is true.";
            }
        }
        // Valid cert; check signature:
        payments::PaymentRequest rcopy(paymentRequest); // Copy
        rcopy.set_signature(std::string(""));
        std::string data_to_verify;                     // Everything but the signature
        rcopy.SerializeToString(&data_to_verify);

#if HAVE_DECL_EVP_MD_CTX_NEW
        std::unique_ptr<EVP_MD_CTX, EVPMdCtxDeleter> ctx_holder(EVP_MD_CTX_new());
        EVP_MD_CTX *ctx = ctx_holder.get();
        if (!ctx) throw SSLVerifyError("Error allocating OpenSSL context.");
#else
        EVP_MD_CTX _ctx;
        EVP_MD_CTX *ctx;
        ctx = &_ctx;
#endif
        std::unique_ptr<EVP_PKEY, EVPKeyDeleter> pubkey(X509_get_pubkey(signing_cert));
        if (!pubkey) throw SSLVerifyError("Error extracting certificate public key.");
        EVP_MD_CTX_init(ctx);
        if (!EVP_VerifyInit_ex(ctx, digestAlgorithm, nullptr) ||
            !EVP_VerifyUpdate(ctx, data_to_verify.data(), data_to_verify.size()) ||
            !EVP_VerifyFinal(ctx, (const unsigned char*)paymentRequest.signature().data(), (unsigned int)paymentRequest.signature().size(), pubkey.get())) {
            throw SSLVerifyError("Bad signature, invalid payment request.");
        }

        merchant = GetCommonNameFromCert(signing_cert);
        if (merchant.isEmpty()) {
            throw SSLVerifyError("Bad certificate, missing common name.");
        }
        // TODO: detect EV certificates and set merchant = business name instead of unfriendly NID_commonName ?
    }
    catch (const SSLVerifyError& err) {
        fResult = false;
        qWarning() << "PaymentRequestPlus::getMerchant: SSL error: " << err.what();
    }

    return fResult;
}

QList<std::pair<CScript,CAmount> > PaymentRequestPlus::getPayTo() const
{
    QList<std::pair<CScript,CAmount> > result;
    for (int i = 0; i < details.outputs_size(); i++)
    {
        const unsigned char* scriptStr = (const unsigned char*)details.outputs(i).script().data();
        CScript s(scriptStr, scriptStr+details.outputs(i).script().size());

        result.append(std::make_pair(s, details.outputs(i).amount()));
    }
    return result;
}
