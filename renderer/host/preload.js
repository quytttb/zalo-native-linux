/*
 * Bounded renderer-side host APIs for the Zalo PC core.
 *
 * This file is deliberately not a catch-all shim. It implements only the
 * host surface that the native host can serve under its app-data policy and
 * fails explicitly for unknown operations. It never loads remote code, reads
 * cookies directly or exposes the user's home directory.
 *
 * Transport, in order of preference:
 *   1. window.__zalonativeHostNative(json) -> json, for an embedding host/test.
 *   2. A synchronous GET to /__zalonative_host on the runtime origin.
 *
 * The synchronous transport is a bounded first step. It is intentionally
 * limited to small file/store operations and must be replaced or augmented by
 * an asynchronous IPC path for long-running work.
 */
(function (global) {
    "use strict";

    if (!global || (global.$znode && global.$znode.__zalonativeHostApi === 1))
        return;

    var config = global.__zalonativeHostConfig || {};
    var hostEndpoint = config.hostEndpoint || "/__zalonative_host";
    var cwd = typeof config.cwd === "string" && config.cwd.charAt(0) === "/"
        ? config.cwd
        : "/";

    function hostError(code, message) {
        var error = new Error(message || code);
        error.code = code;
        return error;
    }

    function callHost(op, args) {
        var request = { op: op, args: args || {} };
        var response;

        if (typeof global.__zalonativeHostNative === "function") {
            try {
                response = JSON.parse(global.__zalonativeHostNative(JSON.stringify(request)));
            } catch (error) {
                throw hostError("ERR_HOST_UNAVAILABLE", "native host call failed");
            }
        } else {
            if (typeof global.XMLHttpRequest !== "function")
                throw hostError("ERR_HOST_UNAVAILABLE", "no host transport is available");
            var requestJson = JSON.stringify(request);
            // Qt's custom-scheme handler does not deliver a body for this
            // synchronous XHR path, so the bounded request travels in the query
            // string. Keep it small; large binary work needs the async bridge.
            if (requestJson.length > 512 * 1024)
                throw hostError("ERR_HOST_REQUEST_TOO_LARGE",
                    "host request exceeds the synchronous transport limit");
            var separator = hostEndpoint.indexOf("?") >= 0 ? "&" : "?";
            var xhr = new global.XMLHttpRequest();
            xhr.open("GET", hostEndpoint + separator + "request="
                + encodeURIComponent(requestJson), false);
            try {
                xhr.send(null);
            } catch (error) {
                throw hostError("ERR_HOST_UNAVAILABLE", "host request failed");
            }
            if (xhr.status !== 200)
                throw hostError("ERR_HOST_UNAVAILABLE", "host returned HTTP " + xhr.status);
            try {
                response = JSON.parse(xhr.responseText);
            } catch (error) {
                throw hostError("ERR_HOST_PROTOCOL", "host returned invalid JSON");
            }
        }

        if (!response || response.ok !== true) {
            var detail = (response && response.error) || {};
            throw hostError(detail.code || "ERR_HOST_FAILED",
                detail.message || "host request failed");
        }
        return response.value;
    }

    // ---------------------------------------------------------------------
    // POSIX path helper (pure JS, no host call)
    // ---------------------------------------------------------------------

    var pathModule = (function () {
        function assertPath(path) {
            if (typeof path !== "string")
                throw new TypeError("Path must be a string");
        }

        function normalize(path) {
            assertPath(path);
            if (path.length === 0)
                return ".";
            var isAbsolute = path.charCodeAt(0) === 47;
            var trailingSlash = path.charCodeAt(path.length - 1) === 47;
            var parts = path.split("/");
            var out = [];
            for (var i = 0; i < parts.length; ++i) {
                var part = parts[i];
                if (!part || part === ".")
                    continue;
                if (part === "..") {
                    if (out.length > 0 && out[out.length - 1] !== "..")
                        out.pop();
                    else if (!isAbsolute)
                        out.push("..");
                } else {
                    out.push(part);
                }
            }
            var result = out.join("/");
            if (result.length === 0)
                result = isAbsolute ? "/" : ".";
            else if (isAbsolute)
                result = "/" + result;
            if (trailingSlash && result !== "/")
                result += "/";
            return result;
        }

        function join() {
            var parts = [];
            for (var i = 0; i < arguments.length; ++i) {
                assertPath(arguments[i]);
                if (arguments[i].length > 0)
                    parts.push(arguments[i]);
            }
            if (parts.length === 0)
                return ".";
            return normalize(parts.join("/"));
        }

        function resolve() {
            var resolved = "";
            var isAbsolute = false;
            for (var i = arguments.length - 1; i >= -1; --i) {
                var path = i >= 0 ? arguments[i] : cwd;
                assertPath(path);
                if (path.length === 0)
                    continue;
                resolved = path + "/" + resolved;
                isAbsolute = path.charCodeAt(0) === 47;
                if (isAbsolute)
                    break;
            }
            if (!isAbsolute)
                resolved = cwd + "/" + resolved;
            var result = normalize(resolved);
            if (result.length > 1 && result.charAt(result.length - 1) === "/")
                result = result.slice(0, -1);
            return result;
        }

        function isAbsolute(path) {
            assertPath(path);
            return path.charCodeAt(0) === 47;
        }

        function dirname(path) {
            assertPath(path);
            if (path.length === 0)
                return ".";
            var hasRoot = path.charCodeAt(0) === 47;
            var end = -1;
            var matchedSlash = true;
            for (var i = path.length - 1; i >= 1; --i) {
                if (path.charCodeAt(i) === 47) {
                    if (!matchedSlash) {
                        end = i;
                        break;
                    }
                } else {
                    matchedSlash = false;
                }
            }
            if (end === -1)
                return hasRoot ? "/" : ".";
            if (hasRoot && end === 1)
                return "//";
            return path.slice(0, end);
        }

        function basename(path, ext) {
            assertPath(path);
            if (ext !== undefined && typeof ext !== "string")
                throw new TypeError("Extension must be a string");
            var start = 0;
            var end = -1;
            var matchedSlash = true;
            for (var i = path.length - 1; i >= 0; --i) {
                if (path.charCodeAt(i) === 47) {
                    if (!matchedSlash) {
                        start = i + 1;
                        break;
                    }
                } else if (end === -1) {
                    matchedSlash = false;
                    end = i + 1;
                }
            }
            if (end === -1)
                return "";
            if (ext && end - start > ext.length
                && path.slice(end - ext.length, end) === ext)
                end -= ext.length;
            return path.slice(start, end);
        }

        function extname(path) {
            assertPath(path);
            var startDot = -1;
            var startPart = 0;
            var end = -1;
            var matchedSlash = true;
            var preDotState = 0;
            for (var i = path.length - 1; i >= 0; --i) {
                var code = path.charCodeAt(i);
                if (code === 47) {
                    if (!matchedSlash) {
                        startPart = i + 1;
                        break;
                    }
                    continue;
                }
                if (end === -1) {
                    matchedSlash = false;
                    end = i + 1;
                }
                if (code === 46) {
                    if (startDot === -1)
                        startDot = i;
                    else if (preDotState !== 1)
                        preDotState = 1;
                } else if (startDot !== -1) {
                    preDotState = -1;
                }
            }
            if (startDot === -1 || end === -1 || preDotState === 0
                || (preDotState === 1 && startDot === end - 1 && startDot === startPart + 1))
                return "";
            return path.slice(startDot, end);
        }

        function trimArray(parts) {
            var start = 0;
            while (start < parts.length && parts[start] === "")
                ++start;
            var end = parts.length;
            while (end > start && parts[end - 1] === "")
                --end;
            return parts.slice(start, end);
        }

        function relative(from, to) {
            assertPath(from);
            assertPath(to);
            var fromResolved = resolve(from);
            var toResolved = resolve(to);
            if (fromResolved === toResolved)
                return "";
            var fromParts = trimArray(fromResolved.split("/"));
            var toParts = trimArray(toResolved.split("/"));
            var length = Math.min(fromParts.length, toParts.length);
            var samePartsLength = length;
            for (var i = 0; i < length; ++i) {
                if (fromParts[i] !== toParts[i]) {
                    samePartsLength = i;
                    break;
                }
            }
            var out = [];
            for (var j = samePartsLength; j < fromParts.length; ++j)
                out.push("..");
            out = out.concat(toParts.slice(samePartsLength));
            return out.join("/");
        }

        function parse(path) {
            assertPath(path);
            var result = { root: "", dir: "", base: "", ext: "", name: "" };
            if (path.length === 0)
                return result;
            var isAbs = path.charCodeAt(0) === 47;
            if (isAbs)
                result.root = "/";
            result.base = basename(path);
            result.ext = extname(path);
            result.name = result.ext
                ? result.base.slice(0, -result.ext.length)
                : result.base;
            result.dir = dirname(path);
            if (result.dir === "." && !isAbs)
                result.dir = "";
            return result;
        }

        function format(pathObject) {
            if (pathObject === null || typeof pathObject !== "object")
                throw new TypeError('The "pathObject" argument must be of type object');
            var dir = pathObject.dir || pathObject.root;
            var base = pathObject.base
                || ((pathObject.name || "") + (pathObject.ext || ""));
            if (!dir)
                return base;
            if (dir === pathObject.root)
                return dir + base;
            return dir + "/" + base;
        }

        function toNamespacedPath(path) {
            assertPath(path);
            return path;
        }

        var module = {
            sep: "/",
            delimiter: ":",
            normalize: normalize,
            join: join,
            resolve: resolve,
            isAbsolute: isAbsolute,
            dirname: dirname,
            basename: basename,
            extname: extname,
            relative: relative,
            parse: parse,
            format: format,
            toNamespacedPath: toNamespacedPath
        };
        module.posix = module;
        return module;
    })();

    // ---------------------------------------------------------------------
    // Encoding helpers
    // ---------------------------------------------------------------------

    function normalizeEncoding(options, fallback) {
        if (typeof options === "string")
            return options.toLowerCase();
        if (options && typeof options === "object"
            && typeof options.encoding === "string")
            return options.encoding.toLowerCase();
        return fallback;
    }

    function encodeData(data, options) {
        var encoding = normalizeEncoding(options, "utf8");
        if (typeof data === "string") {
            if (encoding === "base64")
                return { data: data, encoding: "base64" };
            return { data: data, encoding: "utf8" };
        }
        if (data && typeof data === "object") {
            var bytes = null;
            if (typeof ArrayBuffer !== "undefined" && data instanceof ArrayBuffer)
                bytes = new Uint8Array(data);
            else if (typeof Uint8Array !== "undefined" && data instanceof Uint8Array)
                bytes = data;
            else if (typeof global.Buffer !== "undefined"
                && typeof global.Buffer.isBuffer === "function"
                && global.Buffer.isBuffer(data))
                bytes = data;
            if (bytes) {
                var base64;
                if (typeof global.Buffer !== "undefined"
                    && typeof global.Buffer.from === "function") {
                    base64 = global.Buffer.from(bytes).toString("base64");
                } else if (typeof global.btoa === "function") {
                    var binary = "";
                    for (var i = 0; i < bytes.length; ++i)
                        binary += String.fromCharCode(bytes[i]);
                    base64 = global.btoa(binary);
                } else {
                    throw hostError("EINVAL", "binary data is not supported by this host");
                }
                return { data: base64, encoding: "base64" };
            }
        }
        throw hostError("EINVAL", "data must be a string, ArrayBuffer or Uint8Array");
    }

    function decodeBase64(value) {
        if (typeof global.Buffer !== "undefined"
            && typeof global.Buffer.from === "function")
            return global.Buffer.from(value, "base64");
        if (typeof global.atob === "function") {
            var binary = global.atob(value);
            var bytes = new Uint8Array(binary.length);
            for (var i = 0; i < binary.length; ++i)
                bytes[i] = binary.charCodeAt(i);
            return bytes;
        }
        return value;
    }

    // ---------------------------------------------------------------------
    // fs, fsExtra and the bounded host store
    // ---------------------------------------------------------------------

    function nodeCallback(callback, action) {
        var result;
        var error = null;
        try {
            result = action();
        } catch (caught) {
            error = caught;
        }
        if (typeof callback === "function") {
            var invoke = function () { callback(error, result); };
            if (typeof global.setTimeout === "function")
                global.setTimeout(invoke, 0);
            else
                invoke();
            return undefined;
        }
        if (error)
            return Promise.reject(error);
        return Promise.resolve(result);
    }

    function unsupportedFs(name) {
        return function () {
            throw hostError("ERR_HOST_UNSUPPORTED", "fs." + name + " is not implemented");
        };
    }

    var fs = {};

    fs.constants = {
        F_OK: 0,
        R_OK: 4,
        W_OK: 2,
        X_OK: 1,
        O_RDONLY: 0,
        O_WRONLY: 1,
        O_RDWR: 2,
        O_CREAT: 64,
        O_EXCL: 128,
        O_NOCTTY: 256,
        O_TRUNC: 512,
        O_APPEND: 1024,
        O_DIRECTORY: 65536,
        O_NOFOLLOW: 131072,
        S_IFMT: 61440,
        S_IFREG: 32768,
        S_IFDIR: 16384,
        S_IFLNK: 40960,
        COPYFILE_EXCL: 1
    };

    fs.accessSync = function (path, mode) {
        callHost("fs.access", { path: path, mode: typeof mode === "number" ? mode : 0 });
    };

    fs.existsSync = function (path) {
        return callHost("fs.exists", { path: path }) === true;
    };

    fs.statSync = function (path) {
        return callHost("fs.stat", { path: path });
    };

    fs.lstatSync = function (path) {
        return callHost("fs.lstat", { path: path });
    };

    fs.readFileSync = function (path, options) {
        var encoding = normalizeEncoding(options, "");
        if (!encoding) {
            var base64 = callHost("fs.readFile", { path: path, encoding: "base64" });
            return decodeBase64(base64);
        }
        return callHost("fs.readFile", { path: path, encoding: encoding });
    };

    fs.writeFileSync = function (path, data, options) {
        var encoded = encodeData(data, options);
        callHost("fs.writeFile", {
            path: path,
            data: encoded.data,
            encoding: encoded.encoding
        });
    };

    fs.appendFileSync = function (path, data, options) {
        var encoded = encodeData(data, options);
        callHost("fs.appendFile", {
            path: path,
            data: encoded.data,
            encoding: encoded.encoding
        });
    };

    fs.mkdirSync = function (path, options) {
        var recursive = !!(options && typeof options === "object" && options.recursive);
        callHost("fs.mkdir", { path: path, recursive: recursive });
    };

    fs.readdirSync = function (path) {
        return callHost("fs.readdir", { path: path });
    };

    fs.unlinkSync = function (path) {
        callHost("fs.unlink", { path: path, recursive: false });
    };

    fs.rmdirSync = function (path, options) {
        if (options && options.recursive) {
            callHost("fs.unlink", { path: path, recursive: true });
            return;
        }
        callHost("fs.rmdir", { path: path });
    };

    fs.rmSync = function (path, options) {
        var force = !!(options && options.force);
        var recursive = !!(options && options.recursive);
        if (!fs.existsSync(path)) {
            if (force)
                return;
            throw hostError("ENOENT", "no such file or directory");
        }
        callHost("fs.unlink", { path: path, recursive: recursive });
    };

    fs.renameSync = function (from, to) {
        callHost("fs.rename", { from: from, to: to });
    };

    fs.copyFileSync = function (from, to) {
        callHost("fs.copyFile", { from: from, to: to });
    };

    fs.realpathSync = function (path) {
        return callHost("fs.realpath", { path: path });
    };

    fs.access = function (path, mode, callback) {
        if (typeof mode === "function") {
            callback = mode;
            mode = 0;
        }
        return nodeCallback(callback, function () { return fs.accessSync(path, mode); });
    };

    fs.stat = function (path, callback) {
        return nodeCallback(callback, function () { return fs.statSync(path); });
    };

    fs.lstat = function (path, callback) {
        return nodeCallback(callback, function () { return fs.lstatSync(path); });
    };

    fs.readFile = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.readFileSync(path, options); });
    };

    fs.writeFile = function (path, data, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () {
            return fs.writeFileSync(path, data, options);
        });
    };

    fs.appendFile = function (path, data, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () {
            return fs.appendFileSync(path, data, options);
        });
    };

    fs.mkdir = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.mkdirSync(path, options); });
    };

    fs.readdir = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.readdirSync(path); });
    };

    fs.unlink = function (path, callback) {
        return nodeCallback(callback, function () { return fs.unlinkSync(path); });
    };

    fs.rmdir = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.rmdirSync(path, options); });
    };

    fs.rename = function (from, to, callback) {
        return nodeCallback(callback, function () { return fs.renameSync(from, to); });
    };

    fs.copyFile = function (from, to, flags, callback) {
        if (typeof flags === "function") {
            callback = flags;
            flags = 0;
        }
        return nodeCallback(callback, function () { return fs.copyFileSync(from, to); });
    };

    fs.realpath = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.realpathSync(path); });
    };

    fs.rm = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fs.rmSync(path, options); });
    };

    fs.promises = {};
    ["access", "stat", "lstat", "readFile", "writeFile", "appendFile", "mkdir",
        "readdir", "unlink", "rmdir", "rename", "copyFile", "realpath", "rm"
    ].forEach(function (name) {
        var syncName = name + "Sync";
        fs.promises[name] = function () {
            var args = Array.prototype.slice.call(arguments);
            return Promise.resolve().then(function () {
                return fs[syncName].apply(fs, args);
            });
        };
    });

    [
        "createReadStream", "createWriteStream", "watch", "watchFile", "unwatchFile",
        "readlink", "symlink", "link", "chmod", "chown", "utimes", "truncate",
        "open", "close", "read", "write", "writev", "readv", "fsync", "fdatasync",
        "ftruncate", "futimes", "mkdtemp", "opendir"
    ].forEach(function (name) {
        fs[name] = unsupportedFs(name);
        fs[name + "Sync"] = unsupportedFs(name + "Sync");
    });

    var fsExtra = {};
    Object.keys(fs).forEach(function (key) {
        fsExtra[key] = fs[key];
    });

    fsExtra.ensureDirSync = function (path) {
        fs.mkdirSync(path, { recursive: true });
    };

    fsExtra.ensureDir = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fsExtra.ensureDirSync(path); });
    };

    fsExtra.ensureFileSync = function (path) {
        var parent = pathModule.dirname(path);
        if (parent && parent !== "." && parent !== "/")
            fs.mkdirSync(parent, { recursive: true });
        if (!fs.existsSync(path))
            fs.writeFileSync(path, "");
    };

    fsExtra.ensureFile = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () { return fsExtra.ensureFileSync(path); });
    };

    fsExtra.pathExistsSync = function (path) {
        return fs.existsSync(path);
    };

    fsExtra.pathExists = function (path, callback) {
        return nodeCallback(callback, function () { return fsExtra.pathExistsSync(path); });
    };

    fsExtra.readJsonSync = function (path, options) {
        return JSON.parse(fs.readFileSync(path, "utf8"));
    };

    fsExtra.readJson = function (path, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () {
            return fsExtra.readJsonSync(path, options);
        });
    };

    fsExtra.writeJsonSync = function (path, value, options) {
        fs.writeFileSync(path, JSON.stringify(value, null, 2) + "\n", "utf8");
    };

    fsExtra.writeJson = function (path, value, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () {
            return fsExtra.writeJsonSync(path, value, options);
        });
    };

    fsExtra.outputFileSync = function (path, data, options) {
        var parent = pathModule.dirname(path);
        if (parent && parent !== "." && parent !== "/")
            fs.mkdirSync(parent, { recursive: true });
        fs.writeFileSync(path, data, options);
    };

    fsExtra.outputFile = function (path, data, options, callback) {
        if (typeof options === "function") {
            callback = options;
            options = undefined;
        }
        return nodeCallback(callback, function () {
            return fsExtra.outputFileSync(path, data, options);
        });
    };

    fsExtra.removeSync = function (path) {
        if (!fs.existsSync(path))
            return;
        fs.rmSync(path, { recursive: true, force: true });
    };

    fsExtra.remove = function (path, callback) {
        return nodeCallback(callback, function () { return fsExtra.removeSync(path); });
    };

    fsExtra.emptyDirSync = function (path) {
        fsExtra.ensureDirSync(path);
        fs.readdirSync(path).forEach(function (entry) {
            fsExtra.removeSync(pathModule.join(path, entry));
        });
    };

    fsExtra.emptyDir = function (path, callback) {
        return nodeCallback(callback, function () { return fsExtra.emptyDirSync(path); });
    };

    // A small fs-extra subset that is explicitly unsupported is better than a
    // generic proxy: callers get a stable error code instead of silent success.
    ["copy", "copySync", "move", "moveSync", "outputJson", "outputJsonSync"].forEach(
        function (name) {
            if (!fsExtra[name]) {
                fsExtra[name] = function () {
                    throw hostError("ERR_HOST_UNSUPPORTED", "fsExtra." + name + " is not implemented");
                };
            }
        });

    // ---------------------------------------------------------------------
    // Electron host values and restricted store
    // ---------------------------------------------------------------------

    function electronPath(name) {
        if (typeof name !== "string" || name.length === 0)
            throw hostError("EINVAL", "path name must be a string");
        return callHost("electron.path", { name: name });
    }

    var stores = {};

    function cloneStoreValue(value) {
        return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
    }

    function ElectronStore(name) {
        this.name = typeof name === "string" && name.length > 0 ? name : "default";
        this.listeners = [];
        this.anyListeners = [];
    }

    ElectronStore.prototype.snapshot = function () {
        var result = {};
        var self = this;
        var keys = callHost("store.keys", { name: self.name });
        if (Array.isArray(keys)) {
            keys.forEach(function (key) {
                var entry = callHost("store.get", { name: self.name, key: key });
                if (entry && entry.found)
                    result[key] = entry.value;
            });
        }
        return result;
    };

    ElectronStore.prototype.notifyKey = function (key, newValue, oldValue) {
        this.listeners.slice().forEach(function (listener) {
            if (listener.key !== key)
                return;
            try {
                listener.callback(cloneStoreValue(newValue), cloneStoreValue(oldValue));
            } catch (error) { console.error("ElectronStore listener failed", error); }
        });
    };

    ElectronStore.prototype.notifyAny = function (newStore, oldStore) {
        if (this.anyListeners.length === 0)
            return;
        this.anyListeners.slice().forEach(function (listener) {
            try {
                listener.callback(cloneStoreValue(newStore), cloneStoreValue(oldStore));
            } catch (error) { console.error("ElectronStore listener failed", error); }
        });
    };

    ElectronStore.prototype.get = function (key, defaultValue) {
        var result = callHost("store.get", { name: this.name, key: String(key) });
        if (result && result.found)
            return result.value;
        return defaultValue;
    };

    ElectronStore.prototype.set = function (key, value) {
        var name = String(key);
        var trackAny = this.anyListeners.length > 0;
        var oldValue;
        var oldStore;
        if (this.listeners.length > 0)
            oldValue = this.get(name);
        if (trackAny)
            oldStore = this.snapshot();
        callHost("store.set", { name: this.name, key: name, value: value });
        this.notifyKey(name, value, oldValue);
        if (trackAny)
            this.notifyAny(this.snapshot(), oldStore);
    };

    ElectronStore.prototype.has = function (key) {
        return callHost("store.has", { name: this.name, key: String(key) }) === true;
    };

    ElectronStore.prototype.delete = function (key) {
        var name = String(key);
        var trackAny = this.anyListeners.length > 0;
        var oldValue;
        var oldStore;
        if (this.listeners.length > 0)
            oldValue = this.get(name);
        if (trackAny)
            oldStore = this.snapshot();
        callHost("store.delete", { name: this.name, key: name });
        this.notifyKey(name, undefined, oldValue);
        if (trackAny)
            this.notifyAny(this.snapshot(), oldStore);
    };

    ElectronStore.prototype.clear = function () {
        var track = this.listeners.length > 0 || this.anyListeners.length > 0;
        var oldStore = track ? this.snapshot() : {};
        callHost("store.clear", { name: this.name });
        var self = this;
        Object.keys(oldStore).forEach(function (key) {
            self.notifyKey(key, undefined, oldStore[key]);
        });
        this.notifyAny({}, oldStore);
    };

    ElectronStore.prototype.reset = function () {
        throw hostError("ERR_HOST_UNSUPPORTED", "store.reset defaults are not implemented");
    };

    ElectronStore.prototype.onDidChange = function (key, callback) {
        if (typeof callback !== "function")
            throw new TypeError("onDidChange requires a callback");
        var listener = { key: String(key), callback: callback };
        this.listeners.push(listener);
        var self = this;
        return function () {
            var index = self.listeners.indexOf(listener);
            if (index >= 0)
                self.listeners.splice(index, 1);
        };
    };

    ElectronStore.prototype.onDidAnyChange = function (callback) {
        if (typeof callback !== "function")
            throw new TypeError("onDidAnyChange requires a callback");
        this.anyListeners.push({ callback: callback });
        var listener = this.anyListeners[this.anyListeners.length - 1];
        var self = this;
        return function () {
            var index = self.anyListeners.indexOf(listener);
            if (index >= 0)
                self.anyListeners.splice(index, 1);
        };
    };

    Object.defineProperty(ElectronStore.prototype, "size", {
        get: function () {
            var keys = callHost("store.keys", { name: this.name });
            return Array.isArray(keys) ? keys.length : 0;
        }
    });

    function getElectronStore(name) {
        var key = typeof name === "string" && name.length > 0 ? name : "default";
        if (!Object.prototype.hasOwnProperty.call(stores, key))
            stores[key] = new ElectronStore(key);
        return stores[key];
    }

    function unsupportedOperation(name) {
        return function () {
            throw hostError("ERR_HOST_UNSUPPORTED", name + " is not implemented");
        };
    }

    function unsupportedAsync(name) {
        return function () {
            return Promise.reject(hostError("ERR_HOST_UNSUPPORTED", name + " is not implemented"));
        };
    }

    var fileUtilities = {
        getDirectorySizeAsync: unsupportedAsync("nativelibs.fileUtilities.getDirectorySizeAsync"),
        getDirectorySizeByGlobAsync:
            unsupportedAsync("nativelibs.fileUtilities.getDirectorySizeByGlobAsync"),
        getDirectorySizeSync: unsupportedOperation("nativelibs.fileUtilities.getDirectorySizeSync")
    };

    var nativelibs = {
        fileUtilities: function () { return fileUtilities; }
    };

    var electronNative = {
        getResourceUsage: unsupportedOperation("zelectronNative.getResourceUsage"),
        clearCache: unsupportedOperation("zelectronNative.clearCache"),
        getAvailableHeapSize: unsupportedOperation("zelectronNative.getAvailableHeapSize"),
        setZoomFactor: unsupportedOperation("zelectronNative.setZoomFactor"),
        lockWebframeZoom: unsupportedOperation("zelectronNative.lockWebframeZoom"),
        getClipboardText: unsupportedOperation("zelectronNative.getClipboardText"),
        writeTextToClipboard: unsupportedOperation("zelectronNative.writeTextToClipboard"),
        getClipboardImage: unsupportedOperation("zelectronNative.getClipboardImage"),
        writeImageToClipboard: unsupportedOperation("zelectronNative.writeImageToClipboard")
    };

    var electron = {
        getPath: electronPath,
        getPathSync: electronPath,
        getAppPath: function () { return callHost("electron.appPath", {}); },
        getAppPathSync: function () { return callHost("electron.appPath", {}); },
        getAppVersion: function () { return callHost("electron.appVersion", {}); },
        getElectronStore: getElectronStore
    };

    [
        "setCookie", "getCookie", "clearStorageData", "setProxy", "openExternal",
        "showOpenDialog", "showSaveDialog"
    ].forEach(function (name) {
        electron[name] = function () {
            throw hostError("ERR_HOST_UNSUPPORTED", "zelectron." + name + " is not implemented");
        };
    });

    var node = {
        path: pathModule,
        fs: fs,
        fsExtra: fsExtra,
        // Electron's preload exposes this alias; the runtime uses it in login
        // startup. It stays inside the same bounded fs policy.
        fsPromise: fs.promises,
        nativelibs: nativelibs,
        os: {
            EOL: "\n",
            homedir: function () { return callHost("electron.path", { name: "home" }); },
            tmpdir: function () { return callHost("electron.path", { name: "temp" }); },
            platform: function () { return config.platform || "linux"; },
            type: function () { return config.osType || "Linux"; },
            arch: function () { return config.arch || "x64"; },
            release: unsupportedOperation("os.release"),
            hostname: unsupportedOperation("os.hostname"),
            networkInterfaces: unsupportedOperation("os.networkInterfaces"),
            cpus: unsupportedOperation("os.cpus")
        }
    };

    function install(name, value) {
        try {
            Object.defineProperty(global, name, {
                value: value,
                writable: false,
                configurable: false,
                enumerable: true
            });
        } catch (error) {
            global[name] = value;
        }
    }

    Object.defineProperty(node, "__zalonativeHostApi", {
        value: 1,
        enumerable: false
    });

    install("$znode", node);
    install("$zelectron", electron);
    install("$zelectronNative", electronNative);
})(typeof globalThis !== "undefined" ? globalThis : this);
