# Walkthrough — lõi Zalo chính thức + Qt 6/QML

Tài liệu phân biệt hướng đã chốt, code đang có và các thử nghiệm lịch sử.
Roadmap hiện hành: [PLAN.md](PLAN.md); checklist: [TODO.md](TODO.md); bằng chứng
runtime gần nhất: [RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md).

## 1. Mục tiêu và trạng thái

Dự án theo **mô hình Olaz**: tận dụng lõi JavaScript/renderer Zalo PC do người
dùng cung cấp, host trong **Qt WebEngine ẩn**, còn giao diện là **Qt 6 Quick/QML
native**. C++ làm host, bridge/model và tích hợp Linux; không tự dựng lại giao
thức Zalo.

**Đã có:**

- shell QML/demo, model, diagnostics và bridge demo có allowlist;
- `zalort inspect/import/verify`: nhận diện app directory, entry HTML, preload,
  native library; copy cách ly, ghim version/tree hash và kiểm tra toàn vẹn cục bộ;
- ba chế độ tách biệt: native runtime shell (mặc định), demo (`--demo`) và Web
  wrapper thử nghiệm (`--web`);
- `zalo-bootstrap-probe`: load entry HTML trong Qt WebEngine **không inject host
  API**, so sánh các chiến lược CSP, chặn mạng khi cần và ghi blocker
  preload/network/storage;
- `RuntimePagePolicy::allowSchemeInCsp`: thêm scheme local vào CSP fetch
  directive trong bộ nhớ, có test synthetic; đã cho script chạy ở mức probe;
- spike chứng minh XHR đồng bộ tới Qt custom scheme hoạt động ở mức transport.

**Chưa có:** runtime từ nguồn Zalo chính thức, preload/IPC bridge tương thích,
tích hợp CSP vào host production, network/cookie/storage của lõi thật, QR/login
qua runtime và chat thật trong QML. Không dùng demo hoặc Web wrapper như client chính.

Runtime duy nhất đã thử là **AppImage cộng đồng**, không phải bản Zalo Linux
chính thức. Kết quả chỉ là bằng chứng chẩn đoán, không xác thực vendor.

Probe C++ trực tiếp đã từng xác nhận được phiên Web, nhưng hướng đó **đã dừng**
và không chứng minh runtime Zalo PC đã được tích hợp.

## 2. Thành phần hiện có

- `src/app/`, `qml/`: shell Qt/QML, model, demo, chẩn đoán và chọn chế độ.
- `qml/RuntimeShell.qml`: màn hình native mặc định; hiển thị runtime phát hiện
  được, preload/native dependency và cảnh báo. Chưa chạy lõi Zalo.
- `qml/WebShell.qml`: Web wrapper thử nghiệm, chỉ mở khi có `--web`.
- `qml/RendererHost.qml`: khung WebEngine/bridge demo; chưa host lõi chính thức.
- `src/core/`: ASAR reader, runtime locator/manifest, importer, preload inventory,
  scheme resolver, model và bridge demo.
- `src/cli/`: `zalort` cho `inspect`, `import`, `verify`, `list`, `extract`,
  `bridge-spec`.
- `tools/bootstrap-probe/`: probe chẩn đoán Qt WebEngine, không login.
- `src/probe/`, `tests/test_qr_protocol.cpp`: probe/crypto giao thức trực tiếp,
  giữ làm lịch sử; build mặc định tắt (`ZALO_BUILD_QR_PROBE=OFF`).
- `docs/M1-PROBE.md`: kết quả probe cũ, không phải mốc M1 hiện hành.

Working tree vẫn còn WIP chưa commit. Tài liệu này không tách nhánh, không xóa
mã lịch sử và không đăng nhập bằng tài khoản thật.

## 3. Build và kiểm thử offline

Trên máy phát triển: GCC 15 / Qt 6.10.2. Nền cần CMake/Ninja, Qt Core, Gui,
QML, Quick, Quick Controls, Network và Test. Host đích cần thêm
WebEngineQuick/WebChannel cùng phiên bản. Dùng build directory riêng:

```bash
# Có WebEngine: shell native + Web wrapper + bootstrap probe.
cmake -S . -B build-web -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-15 -DZALO_WEBENGINE=ON -DZALO_BUILD_QR_PROBE=OFF
cmake --build build-web --parallel 4
ctest --test-dir build-web --output-on-failure

# Không WebEngine: core/demo/tooling; không có client runtime.
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-15 -DZALO_WEBENGINE=OFF -DZALO_BUILD_QR_PROBE=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Kết quả ngày 2026-10-30:

- build có WebEngine: **20/20** CTest pass;
- build không WebEngine: **17/17** CTest pass;
- các test `test_sync_scheme`, `test_csp_scheme` và `test_host_preload` chỉ có
  trong cấu hình WebEngine;
- `test_csp_scheme` xác nhận script cùng origin bị chặn khi không rewrite và chạy
  khi `RuntimePagePolicy` thêm scheme vào CSP;
- `test_host_api` kiểm tra containment/traversal/symlink, giới hạn file/store và
  lỗi unsupported; `test_host_preload` chạy preload thật qua scheme test;
- `app_qml_warning_negative` cố tình phát cảnh báo `Cannot assign` và phải thất
  bại; nó kiểm tra biểu thức bắt lỗi, không bảo đảm bắt mọi lỗi QML.

Smoke test chạy profile test, không dùng profile Zalo thật. Nếu thiếu WebEngine,
CMake vẫn có thể build tiếp và in cảnh báo; đừng nhầm build đó với runtime host.

## 4. Chế độ chạy và lệnh runtime

```bash
# Mặc định: màn hình chẩn đoán runtime native, không mở Zalo Web.
./build-web/zalonative

# Chỉ định runtime đã giải nén hoặc app.asar.
./build-web/zalonative --runtime /path/to/AppDir/app

# Demo dữ liệu giả; composer chỉ ghi RAM.
./build-web/zalonative --demo

# Web wrapper thử nghiệm, phải opt-in rõ ràng.
./build-web/zalonative --web
./build-web/zalonative --web --url https://chat.zalo.me/
```

`--url` bị bỏ qua nếu không có `--web`. Không có chế độ nào tự kết luận đã đăng
nhập chỉ từ URL. `--runtime` hiện chỉ nạp manifest/chẩn đoán; nó chưa bootstrap
lõi Zalo.

Công cụ runtime:

```bash
# Đọc cấu trúc, entry, preload và native library; không chạy mã runtime.
./build-web/src/cli/zalort inspect /path/to/AppDir/app

# Copy cách ly, ghim version và hash, từ chối ghi đè ngoài import cũ.
./build-web/src/cli/zalort import /path/to/AppDir/app \
  "$HOME/.local/share/zalo-native/runtime" \
  --expected-version 26.10.20 \
  --expected-sha256 <tree-sha256>

# Kiểm tra lại hash cục bộ so với runtime.json.
./build-web/src/cli/zalort verify "$HOME/.local/share/zalo-native/runtime"
```

Kết quả thật với app `26.10.20` (xem chi tiết trong
[RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md)):

```text
entry HTML         pc-dist/index.html
preload APIs       46
sync call sites    21 (2 literal, 19 computed)
Linux IPC channels 12
native libraries   15
files / bytes      13,369 / 255,047,692
tree SHA-256       f46e7998416732498476f6134234edf0d20c3b75fe1d8e54ef5d7dd84a3d2a3a
```

`verify` chỉ chứng minh bản copy khớp `runtime.json`; `sourceVerified` vẫn là
`false`. Hash cục bộ không thay thế chữ ký hay hash từ vendor.

## 5. Bootstrap probe — từ blocker CSP đến preload

```bash
QT_QPA_PLATFORM=offscreen \
QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" \
  --rewrite-csp --block-remote --wait-ms 8000
```

Probe không inject preload/host API và không login. Kết quả baseline:

1. Trang load được (`loadFinished(true)`, tiêu đề đăng nhập Zalo), nhưng CSP của
   runtime từ chối stylesheet/script phục vụ qua `zalo-app://` dù chúng cùng
   scheme.
2. Cả 46 tên preload đều `undefined`; lỗi JS duy nhất là `Cannot read properties
   of undefined (reading 'loadShellQos')`.
3. `localStorage` đọc/ghi được; `indexedDB`, `caches`, `serviceWorker` tồn tại;
   `crossOriginIsolated` là `false`. Chưa kiểm chứng persistence/quota/SQLite.
4. Không có request host từ xa nào; hai file icon nhỏ bị thiếu nhưng không chặn.

Các thử nghiệm CSP:

- `--scheme-mode standard` (bỏ cờ `LocalScheme`): CSP vẫn chặn; cờ này không
  phải nguyên nhân.
- `--scheme-mode csp-ignored`: script chạy, chứng minh CSP là blocker, nhưng đây
  là bypass toàn phần nên không dùng làm mặc định.
- `--rewrite-csp`: thêm `zalo-app:` **chỉ vào các fetch directive** của meta CSP
  trong bộ nhớ. Không còn lỗi CSP, script/worker chạy, origin bên ngoài vẫn bị
  giới hạn như CSP gốc.

Sau `--rewrite-csp`, lỗi còn lại là thiếu preload: `$zlogger.loadShellQos` và
`$zwindow`. Không có request mạng nào được thử trong thời gian quan sát, nên
network/TLS/cookie vẫn chưa kiểm chứng.

### Shim preload chẩn đoán

`--stub-preload` inject `$zlogger`/`$zwindow` không chức năng và proxy ghi log cho
các tên preload còn lại. Đây **không phải bridge** và không chứng minh tương
thích; mọi call chỉ được ghi lại, không lưu, không login, không gửi mạng. Chế độ
này bắt buộc đi kèm `--block-remote`.

```bash
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" \
  --rewrite-csp --block-remote --stub-preload --wait-ms 8000
```

Kết quả:

- `$zlogger.loadShellQos()` chạy; lỗi TypeError đầu tiên biến mất.
- Startup quét toàn bộ preload surface; thiếu `$zuri` chỉ chuyển lỗi sang tên kế
  tiếp, nên cần cả danh sách để đi tiếp.
- Runtime gọi `$zFileManager.getSentryConfig`, `MetriczGlobal.startCPUMonitor`,
  `$zMetric`, `$zelectron.getAppVersion/getPath/getElectronStore`,
  `$znode.path/fs/fsExtra`.
- Khi thêm `$znode.path` và `$zelectron.getPath` tối thiểu, lỗi `p.join` biến
  mất; blocker kế tiếp là `$znode.fsExtra.ensureDir`, `$znode.fs.access`,
  `$znode.fs.constants`.
- Namespace `$zFeatures.*`/`$zTFModels.ss` dạng proxy làm mất lỗi
  `E[t.name] is not a function`; lỗi còn lại là DI `bind` trong
  `vendors-login...`, được tìm thấy ở các phương thức store thiếu trong lần thử trước.
- Probe mở IndexedDB test version 1 thành công, nên các dòng
  `do open indexeddb zlocalstorage undefined` là log nội bộ của runtime, không
  phải bằng chứng IndexedDB hỏng.
- `blockedRemoteCount: 0`; chỉ origin `zalo-app` xuất hiện.

### Host API hẹp (`--host-api`)

```bash
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" \
  --rewrite-csp --block-remote --stub-preload --host-api --wait-ms 8000
```

`--host-api` inject `renderer/host/preload.js` thật, dùng `HostApi` và một
`QTemporaryDir` làm app-data root; bắt buộc có `--rewrite-csp` và
`--block-remote`. Kết quả đo được:

- `hostApisInjected: true`, `hostApiMode: bounded-diagnostic`;
- trang vẫn load, tiêu đề đăng nhập Zalo, `blockedRemoteCount: 0`;
- `hostRequestCount: 18`, operation gồm `electron.appVersion`, `electron.path`,
  `store.get`, `store.set`, `fs.mkdir`, `fs.access`, `fs.exists`;
- còn một rejection dependency injection; subset chưa đủ cho preload bridge hoàn chỉnh.

Test WebEngine `test_host_preload` chứng minh thêm `$znode.path`, file
UTF-8/base64, có `fsPromise`, `$zelectron` path/version/store, chặn path ngoài
root, `ERR_HOST_PATH_UNSUPPORTED` và `ENOENT`. Đây là policy layer, không phải
Electron/IPC production hay OS sandbox.

Bảng quyết định API nào cần host thật, API nào trả lỗi unsupported nằm trong
[BRIDGE.md](BRIDGE.md). Không dùng proxy catch-all trong production.

Logic CSP nằm trong `src/core/RuntimePagePolicy.{h,cpp}`, có test synthetic
trong `tests/test_scheme.cpp` và test WebEngine `tests/test_csp_scheme.cpp`
(script cùng origin bị chặn nếu không rewrite, chạy khi có rewrite). Đây mới là
bằng chứng probe, chưa tích hợp vào host production. Bước tiếp theo là
preload/IPC bridge đầy đủ và điều tra dependency injection; spike sync đã chứng minh XHR đồng
bộ tới custom scheme trả `sync:200:pong`, nhưng đó chỉ là transport, chưa phải
`sendSync`/IPC đầy đủ.

## 6. Lịch sử thử QR C++ trực tiếp — đã dừng

1. QR được tạo và điện thoại xác nhận, nhưng kiểm tra phiên ban đầu thất bại.
2. Đối chiếu [zca-js](https://github.com/RFS-ADRENO/zca-js) cho thấy cần theo
   chuyển hướng sau `checksession`; probe được sửa với allowlist HTTPS giới hạn.
3. Một chuyển hướng đến `jr.zaloapp.com` bị chặn; sau khi thêm **đúng host đó**,
   lần thử tiếp theo xác nhận được phiên Web trong bộ nhớ.
4. Không ghi lại mã QR, cookie, token, tên hay ID tài khoản. Sau khi cửa sổ
   probe đóng, phiên trong RAM kết thúc; chưa thử khôi phục phiên.

Nguồn tham khảo được ghim tại [zca-js `loginQR.ts` (MIT), commit
`2a7e360`](https://github.com/RFS-ADRENO/zca-js/blob/2a7e360257dfe98e2eb48dc46bbf389834be7e0b/src/apis/loginQR.ts).
Không kèm mã Zalo độc quyền, adapter Olaz hay mã GPLv3 của Zalo10. Mã probe vẫn
còn nhưng không nằm trong build mặc định.

## 7. Tiếp theo — chỉ theo lõi chính thức

- M1 còn mở: chọn runtime từ nguồn chính thức, tích hợp/review CSP vào host
  production, thiết kế preload bridge và phân loại dependency Linux; chưa đạt
  điều kiện hoàn thành M1.
- M2: scheme handler/bridge có giới hạn, renderer ẩn, QR/state từ runtime vào
  QML, profile riêng; kiểm chứng login/restart/logout qua lõi chính thức.
- M3–M4: adapter state/action, model và UI chat/file thật, đồng bộ qua lõi Zalo.
- M5: chỉ tích hợp call backend Linux có sẵn nếu khả thi/được phép; không tự
  reverse-engineer signaling/media. Không có backend thì để chưa hỗ trợ gọi.
- M6: pin runtime/adapter, package có WebEngine, test quyền/profile và đo tài nguyên.

Kết quả thử nghiệm cũ được giữ như bằng chứng lịch sử; không tiếp tục các task
codec/API/WebSocket giao thức trực tiếp. Hướng Olaz không còn là phương án dự phòng.
