#include "RuntimePagePolicy.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace ZaloNative::RuntimePagePolicy {

namespace {

const QSet<QString> &fetchDirectiveNames()
{
    static const QSet<QString> names{
        QStringLiteral("default-src"),
        QStringLiteral("script-src"),
        QStringLiteral("script-src-elem"),
        QStringLiteral("script-src-attr"),
        QStringLiteral("style-src"),
        QStringLiteral("style-src-elem"),
        QStringLiteral("style-src-attr"),
        QStringLiteral("img-src"),
        QStringLiteral("font-src"),
        QStringLiteral("connect-src"),
        QStringLiteral("media-src"),
        QStringLiteral("object-src"),
        QStringLiteral("frame-src"),
        QStringLiteral("child-src"),
        QStringLiteral("worker-src"),
        QStringLiteral("manifest-src"),
        QStringLiteral("prefetch-src"),
    };
    return names;
}

bool rewriteContent(QString &content, const QString &source)
{
    bool changed = false;
    QStringList directives = content.split(QLatin1Char(';'));
    for (QString &directive : directives) {
        directive = directive.trimmed();
        if (directive.isEmpty())
            continue;

        const QString name = directive.section(QLatin1Char(' '), 0, 0).toLower();
        if (!fetchDirectiveNames().contains(name))
            continue;
        if (directive.contains(source, Qt::CaseInsensitive))
            continue;

        if (directive.contains(QLatin1String("'none'"), Qt::CaseInsensitive))
            directive.replace(QLatin1String("'none'"), source, Qt::CaseInsensitive);
        else
            directive += QLatin1Char(' ') + source;
        changed = true;
    }
    content = directives.join(QLatin1String("; "));
    return changed;
}

} // namespace

CspRewriteResult allowSchemeInCsp(const QByteArray &html, const QByteArray &scheme)
{
    CspRewriteResult result{html, false};

    const QString schemeName = QString::fromLatin1(scheme);
    static const QRegularExpression schemeRe(QStringLiteral("^[a-z][a-z0-9+.-]*$"),
                                             QRegularExpression::CaseInsensitiveOption);
    if (!schemeRe.match(schemeName).hasMatch())
        return result;

    const QString source = schemeName + QLatin1Char(':');
    QString text = QString::fromUtf8(html);

    static const QRegularExpression metaRe(
        QStringLiteral("<meta\\b[^>]*http-equiv\\s*=\\s*[\"']Content-Security-Policy[\"'][^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression contentRe(
        QStringLiteral("content\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)')"),
        QRegularExpression::CaseInsensitiveOption);

    struct Replacement
    {
        qsizetype start = 0;
        qsizetype length = 0;
        QString tag;
    };
    QVector<Replacement> replacements;

    auto matches = metaRe.globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch metaMatch = matches.next();
        QString tag = metaMatch.captured(0);
        const QRegularExpressionMatch contentMatch = contentRe.match(tag);
        if (!contentMatch.hasMatch())
            continue;

        const bool doubleQuoted = contentMatch.capturedStart(1) >= 0;
        const int contentGroup = doubleQuoted ? 1 : 2;
        QString content = contentMatch.captured(contentGroup);
        if (!rewriteContent(content, source))
            continue;

        tag.replace(contentMatch.capturedStart(contentGroup),
                    contentMatch.capturedLength(contentGroup), content);
        replacements.append(
            Replacement{metaMatch.capturedStart(0), metaMatch.capturedLength(0), tag});
    }

    for (int i = replacements.size() - 1; i >= 0; --i) {
        const Replacement &replacement = replacements.at(i);
        text.replace(replacement.start, replacement.length, replacement.tag);
    }

    result.html = text.toUtf8();
    result.changed = !replacements.isEmpty();
    return result;
}

} // namespace ZaloNative::RuntimePagePolicy
