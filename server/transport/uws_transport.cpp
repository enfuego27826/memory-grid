// uws_transport.cpp — THE ONE SWAPPABLE FILE.
//
// uWebSockets implements ITransport here and nowhere else. Everything above the
// seam (dispatch/rooms/wire/sessions) is uWS-free, so replacing this single file
// swaps the WebSocket library. ws:// only (LIBUS_NO_SSL); TLS terminates at a
// reverse proxy in deployment.
#include <libusockets.h>

#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "App.h"
#include "transport/itransport.hpp"

namespace mg::tx {
namespace {

constexpr bool SSL = false;
constexpr std::size_t kHttpBodyCap = 64 * 1024;  // bound POST body accumulation
struct PerSocketData { ConnId id = kNoConn; };
using WS = uWS::WebSocket<SSL, true, PerSocketData>;

const char* statusLine(int code) {
    switch (code) {
        case 200: return "200 OK";
        case 400: return "400 Bad Request";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        default:  return "500 Internal Server Error";
    }
}

const char* contentTypeFor(const std::string& path) {
    auto ends = [&](const char* s) {
        size_t n = std::strlen(s);
        return path.size() >= n && path.compare(path.size() - n, n, s) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".js") || ends(".mjs")) return "text/javascript; charset=utf-8";
    if (ends(".css")) return "text/css; charset=utf-8";
    if (ends(".json")) return "application/json";
    if (ends(".svg")) return "image/svg+xml";
    if (ends(".png")) return "image/png";
    if (ends(".ico")) return "image/x-icon";
    if (ends(".woff2")) return "font/woff2";
    return "application/octet-stream";
}

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const char* kSmokePage = R"HTML(<!doctype html><meta charset=utf8>
<title>Memory Grid smoke</title><body style="font:14px monospace">
<h3>Memory Grid — WS smoke test</h3>
<input id=name value=Alice> <button onclick=create()>Create</button>
<input id=code placeholder=CODE> <button onclick=joinRoom()>Join</button>
<button onclick=send({type:'start_game'})>Start</button>
<button onclick=send({type:'press_ready_walk'})>Ready</button>
<pre id=log></pre>
<script>
let ws, L=document.getElementById('log');
const log=(m)=>L.textContent+=m+"\n";
async function create(){
  const r=await fetch('/create',{method:'POST',body:'{}'});
  const j=await r.json(); document.getElementById('code').value=j.code;
  log('created '+j.code); connect(j.code);
}
function joinRoom(){ connect(document.getElementById('code').value); }
function connect(code){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onopen=()=>{ log('ws open'); send({type:'join_room',code,name:document.getElementById('name').value}); };
  ws.onmessage=(e)=>log('< '+e.data);
  ws.onclose=()=>log('ws closed');
}
function send(o){ if(ws&&ws.readyState===1){ ws.send(JSON.stringify(o)); log('> '+JSON.stringify(o)); } }
</script></body>)HTML";

class UwsTransport : public ITransport {
public:
    UwsTransport(ITransportCallbacks& cb, std::string staticDir)
        : cb_(cb), staticDir_(std::move(staticDir)) {}

    void run(uint16_t port) override {
        loop_ = uWS::Loop::get();
        auto cors = [](auto* res) {
            res->writeHeader("Access-Control-Allow-Origin", "*");
            res->writeHeader("Access-Control-Allow-Headers", "content-type");
            res->writeHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        };
        auto writeResp = [cors](auto* res, const HttpResponse& r) {
            res->writeStatus(statusLine(r.status));
            cors(res);
            res->writeHeader("Content-Type", r.contentType);
            res->end(r.body);
        };
        auto httpPost = [this, writeResp](auto* res, auto* req) {
            auto buf = std::make_shared<std::string>();
            std::string path(req->getUrl());
            res->onData([this, res, buf, path, writeResp](std::string_view chunk, bool last) {
                if (buf->size() < kHttpBodyCap) buf->append(chunk);  // bound memory (anti-bloat)
                if (last) writeResp(res, cb_.onHttp({"POST", path, *buf}));
            });
            res->onAborted([] {});
        };

        app_ = std::make_unique<uWS::App>();
        app_->get("/health", [](auto* res, auto*) { res->end("ok"); })
            .options("/*", [cors](auto* res, auto*) { cors(res); res->end(); })
            .post("/create", httpPost)
            .post("/join", httpPost)
            .get("/*", [this](auto* res, auto* req) { serveGet(res, std::string(req->getUrl())); })
            .template ws<PerSocketData>("/ws", makeBehavior())
            .listen(port, [port](auto* token) {
                if (token) printf("[mg] listening on ws://localhost:%u  (smoke: http://localhost:%u/)\n", port, port);
                else printf("[mg] FAILED to listen on port %u\n", port);
            })
            .run();
    }

    void sendOne(ConnId c, std::string_view bytes) override {
        if (auto it = conns_.find(c); it != conns_.end()) it->second->send(bytes, uWS::OpCode::TEXT);
    }
    void closeOne(ConnId c) override {
        if (auto it = conns_.find(c); it != conns_.end()) it->second->end(1000, "closed");
    }
    void subscribe(ConnId c, const RoomKey& room) override {
        if (auto it = conns_.find(c); it != conns_.end()) it->second->subscribe(room);
    }
    void unsubscribe(ConnId c, const RoomKey& room) override {
        if (auto it = conns_.find(c); it != conns_.end()) it->second->unsubscribe(room);
    }
    void publish(const RoomKey& room, std::string_view bytes) override {
        if (app_) app_->publish(room, bytes, uWS::OpCode::TEXT, false);
    }

    void scheduleTimer(const RoomKey& room, uint32_t id, uint32_t delayMs) override {
        cancelTimer(room, id);  // replace any existing
        auto* loop = reinterpret_cast<struct us_loop_t*>(uWS::Loop::get());
        struct us_timer_t* t = us_create_timer(loop, 0, sizeof(TimerCtx*));
        auto* ctx = new TimerCtx{this, room, id};
        *reinterpret_cast<TimerCtx**>(us_timer_ext(t)) = ctx;
        timers_[{room, id}] = t;
        us_timer_set(t, &UwsTransport::onTimerFired, static_cast<int>(delayMs), 0);
    }
    void cancelTimer(const RoomKey& room, uint32_t id) override {
        auto it = timers_.find({room, id});
        if (it == timers_.end()) return;
        struct us_timer_t* t = it->second;
        delete *reinterpret_cast<TimerCtx**>(us_timer_ext(t));
        us_timer_close(t);
        timers_.erase(it);
    }

private:
    template <class Res>
    void serveGet(Res* res, std::string url) {
        if (staticDir_.empty()) {  // dev: serve the built-in smoke page
            res->writeHeader("Content-Type", "text/html")->end(kSmokePage);
            return;
        }
        if (url.find("..") != std::string::npos) url = "/";  // no path traversal
        if (url == "/") url = "/index.html";
        if (auto body = readFile(staticDir_ + url)) {
            res->writeHeader("Content-Type", contentTypeFor(url))->end(*body);
            return;
        }
        // SPA fallback: unknown path → index.html
        if (auto idx = readFile(staticDir_ + "/index.html"))
            res->writeHeader("Content-Type", "text/html; charset=utf-8")->end(*idx);
        else
            res->writeStatus("404 Not Found")->end("not found");
    }

    struct TimerCtx { UwsTransport* self; RoomKey room; uint32_t id; };

    static void onTimerFired(struct us_timer_t* t) {
        auto* ctx = *reinterpret_cast<TimerCtx**>(us_timer_ext(t));
        UwsTransport* self = ctx->self;
        RoomKey room = ctx->room;
        uint32_t id = ctx->id;
        self->cancelTimer(room, id);   // one-shot: free + remove before callback
        self->cb_.onTimer(room, id);
    }

    uWS::App::WebSocketBehavior<PerSocketData> makeBehavior() {
        uWS::App::WebSocketBehavior<PerSocketData> b;
        b.maxPayloadLength = 16 * 1024;
        b.open = [this](WS* ws) {
            ConnId id = nextConn_++;
            ws->getUserData()->id = id;
            conns_[id] = ws;
            cb_.onOpen(id);
        };
        b.message = [this](WS* ws, std::string_view msg, uWS::OpCode) {
            cb_.onMessage(ws->getUserData()->id, msg);
        };
        b.close = [this](WS* ws, int, std::string_view) {
            ConnId id = ws->getUserData()->id;
            conns_.erase(id);
            cb_.onClose(id);
        };
        return b;
    }

    ITransportCallbacks& cb_;
    std::string staticDir_;
    std::unique_ptr<uWS::App> app_;
    uWS::Loop* loop_ = nullptr;
    ConnId nextConn_ = 1;
    std::unordered_map<ConnId, WS*> conns_;
    std::map<std::pair<RoomKey, uint32_t>, struct us_timer_t*> timers_;
};

}  // namespace

// Factory the rest of the program uses; keeps uWS types out of headers.
// staticDir empty ⇒ serve the built-in smoke page; otherwise serve files from it.
std::unique_ptr<ITransport> makeUwsTransport(ITransportCallbacks& cb, std::string staticDir) {
    return std::make_unique<UwsTransport>(cb, std::move(staticDir));
}

}  // namespace mg::tx
