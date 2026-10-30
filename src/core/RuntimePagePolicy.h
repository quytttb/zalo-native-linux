#pragma once

#include <QByteArray>

namespace ZaloNative::RuntimePagePolicy {

struct CspRewriteResult
{
    QByteArray html;
    bool changed = false;
};

// Adds a local URL scheme (for example "zalo-app:") to the fetch directives of
// Content-Security-Policy meta tags in an HTML document. This is an in-memory
// host adaptation for a runtime served through a custom scheme; it does not
// disable CSP, add external origins, or modify the file on disk.
//
// Non-fetch directives such as frame-ancestors, base-uri, form-action, sandbox
// and reporting directives are left untouched. An invalid scheme leaves the
// document unchanged.
CspRewriteResult allowSchemeInCsp(const QByteArray &html, const QByteArray &scheme);

} // namespace ZaloNative::RuntimePagePolicy
