#include "ProtocolCrypto.h"

#include <QCryptographicHash>
#include <openssl/evp.h>
#include <memory>

namespace ZaloNative::ProtocolCrypto {

QByteArray sign(const QByteArray &operation, const QMap<QByteArray, QByteArray> &values)
{
    QByteArray input = "zsecure" + operation;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        input += it.value();
    return QCryptographicHash::hash(input, QCryptographicHash::Md5).toHex();
}

static std::optional<QByteArray> crypt(const QByteArray &key, const QByteArray &input, bool encode)
{
    if ((key.size() != 16 && key.size() != 24 && key.size() != 32) || input.size() > 1024 * 1024)
        return std::nullopt;
    const EVP_CIPHER *cipher = key.size() == 16 ? EVP_aes_128_cbc()
        : key.size() == 24 ? EVP_aes_192_cbc() : EVP_aes_256_cbc();
    using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    Context ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx)
        return std::nullopt;
    const unsigned char iv[16] = {};
    if (EVP_CipherInit_ex(ctx.get(), cipher, nullptr,
                          reinterpret_cast<const unsigned char *>(key.constData()), iv, encode ? 1 : 0) != 1)
        return std::nullopt;
    QByteArray output(input.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int written = 0;
    int tail = 0;
    if (EVP_CipherUpdate(ctx.get(), reinterpret_cast<unsigned char *>(output.data()), &written,
                         reinterpret_cast<const unsigned char *>(input.constData()), int(input.size())) != 1
        || EVP_CipherFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(output.data()) + written, &tail) != 1)
        return std::nullopt;
    output.resize(written + tail);
    return output;
}

std::optional<QByteArray> encrypt(const QByteArray &key, const QByteArray &plain)
{
    return crypt(key, plain, true);
}

std::optional<QByteArray> decrypt(const QByteArray &key, const QByteArray &cipher)
{
    return crypt(key, cipher, false);
}
}
