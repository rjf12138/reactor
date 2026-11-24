/*
    WebSocket 客户端发送测试（自动化，返回 0 表示通过）。

    流程：
    1. 本地 raw TCP 服务器模拟 WebSocket 服务端，完成 HTTP Upgrade；
    2. WSNetClient 发送文本帧（不显式指定 mask，使用默认值，验证 RFC 6455 的掩码）；
    3. 服务端校验帧的 mask/opcode/payload，并回显一帧；
    4. 客户端 handle_msg 收到回显，验证发送和接收双向正常。
*/
#include "reactor.h"
#include "algorithm/algorithm.h"

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace reactor;

namespace {

const int kListenPort = 12192;
const char *kWebsocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char *kTestPayload = "hello-ws";

void msleep(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int create_listen_socket(int port)
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(fd, 16) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

bool recv_exact(int fd, void *buf, size_t size, int timeout_ms)
{
    char *data = static_cast<char*>(buf);
    size_t received = 0;
    for (int i = 0; i < timeout_ms / 10 && received < size; ++i) {
        ssize_t ret = ::recv(fd, data + received, size - received, MSG_DONTWAIT);
        if (ret > 0) {
            received += static_cast<size_t>(ret);
            continue;
        }
        if (ret == 0) {
            return false;
        }
        msleep(10);
    }
    return received == size;
}

bool recv_until(int fd, const std::string &mark, std::string &out, int timeout_ms)
{
    out.clear();
    for (int i = 0; i < timeout_ms / 10; ++i) {
        char buffer[2048];
        ssize_t ret = ::recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (ret > 0) {
            out.append(buffer, static_cast<size_t>(ret));
            if (out.find(mark) != std::string::npos) {
                return true;
            }
            continue;
        }
        if (ret == 0) {
            return false;
        }
        msleep(10);
    }
    return false;
}

std::string get_header(const std::string &request, const std::string &name)
{
    std::string lower = request;
    std::string target = name;
    for (size_t i = 0; i < lower.size(); ++i) {
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
    }
    for (size_t i = 0; i < target.size(); ++i) {
        target[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(target[i])));
    }

    size_t pos = lower.find(target + ":");
    if (pos == std::string::npos) {
        return "";
    }
    pos = lower.find(':', pos) + 1;
    size_t end = lower.find("\r\n", pos);
    std::string value = request.substr(pos, end - pos);
    size_t begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    size_t last = value.find_last_not_of(" \t");
    return value.substr(begin, last - begin + 1);
}

std::string make_accept(const std::string &key)
{
    ByteBuffer input(key);
    input.write_string(kWebsocketGuid);

    ByteBuffer digest;
    if (algorithm::sha1(input, digest) != 0) {
        return "";
    }

    ByteBuffer base64;
    if (algorithm::encode_base64(digest, base64) <= 0) {
        return "";
    }
    return base64.str();
}


// 读取一个完整 WebSocket 帧（用于校验客户端发送的帧）
bool read_ws_frame(int fd, int &opcode, std::string &payload, bool &masked, int timeout_ms)
{
    unsigned char header[2] = {0};
    if (!recv_exact(fd, header, sizeof(header), timeout_ms)) {
        return false;
    }

    opcode = header[0] & 0x0F;
    masked = (header[1] & 0x80) != 0;

    uint64_t payload_len = header[1] & 0x7F;
    if (payload_len == 126) {
        unsigned char ext[2] = {0};
        if (!recv_exact(fd, ext, sizeof(ext), timeout_ms)) {
            return false;
        }
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8] = {0};
        if (!recv_exact(fd, ext, sizeof(ext), timeout_ms)) {
            return false;
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (payload_len > 1024 * 1024) {
        return false;
    }

    unsigned char mask_key[4] = {0};
    if (masked && !recv_exact(fd, mask_key, sizeof(mask_key), timeout_ms)) {
        return false;
    }

    payload.assign(static_cast<size_t>(payload_len), '\0');
    if (payload_len > 0 &&
        !recv_exact(fd, &payload[0], static_cast<size_t>(payload_len), timeout_ms)) {
        return false;
    }

    if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(payload[i] ^ mask_key[i % 4]);
        }
    }

    return true;
}

// 服务端发送文本帧（服务端发出的帧不能掩码）
bool send_ws_text_frame(int fd, const std::string &payload)
{
    std::string frame;
    frame.push_back(static_cast<char>(0x81)); // FIN=1, opcode=TEXT
    if (payload.size() < 126) {
        frame.push_back(static_cast<char>(payload.size()));
    } else {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((payload.size() >> 8) & 0xFF));
        frame.push_back(static_cast<char>(payload.size() & 0xFF));
    }
    frame += payload;

    return ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL) ==
           static_cast<ssize_t>(frame.size());
}

class WSSendTestClient : public WSNetClient {
public:
    int handle_msg(ptl::WebsocketPtl &ptl, ptl::WebsocketParse_ErrorCode err) override
    {
        if (err == ptl::WebsocketParse_OK) {
            std::lock_guard<std::mutex> lock(mutex_);
            received_ += ptl.get_content().str();
        }
        return 0;
    }

    std::string received(void)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

private:
    std::mutex mutex_;
    std::string received_;
};

struct ServerResult {
    bool ok = false;
    std::string error;
    int received_opcode = -1;
    bool received_masked = false;
    std::string received_payload;
};

void ws_send_server(int listen_fd, ServerResult *result)
{
    int client_fd = ::accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
        result->error = "accept failed";
        return;
    }

    std::string request;
    if (!recv_until(client_fd, "\r\n\r\n", request, 3000)) {
        result->error = "upgrade request not received";
        ::close(client_fd);
        return;
    }

    std::string key = get_header(request, "Sec-WebSocket-Key");
    std::string accept = make_accept(key);
    if (key.empty() || accept.empty()) {
        result->error = "invalid Sec-WebSocket-Key";
        ::close(client_fd);
        return;
    }

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    if (::send(client_fd, response.data(), response.size(), MSG_NOSIGNAL) < 0) {
        result->error = "upgrade response send failed";
        ::close(client_fd);
        return;
    }

    result->ok = read_ws_frame(client_fd, result->received_opcode,
                               result->received_payload, result->received_masked, 3000);
    if (!result->ok) {
        result->error = "client frame not received";
        ::close(client_fd);
        return;
    }

    if (result->received_opcode != ptl::WEBSOCKET_OPCODE_TEXT_FRAME ||
        result->received_payload != kTestPayload ||
        result->received_masked == false) {
        result->ok = false;
        result->error = "client frame invalid (opcode/mask/payload)";
        ::close(client_fd);
        return;
    }

    if (!send_ws_text_frame(client_fd, result->received_payload)) {
        result->ok = false;
        result->error = "echo frame send failed";
    }

    msleep(300);
    ::close(client_fd);
}

} // namespace

int main(void)
{
    int failed = 0;

    ReactorConfig_t config;
    config.sub_reactor_size_ = 5;
    if (reactor_start(config) != 0) {
        fprintf(stderr, "reactor_start failed\n");
        return 1;
    }

    int listen_fd = create_listen_socket(kListenPort);
    if (listen_fd < 0) {
        fprintf(stderr, "listen on port %d failed\n", kListenPort);
        reactor_stop();
        return 1;
    }

    ServerResult server_result;
    std::thread server_thread(ws_send_server, listen_fd, &server_result);
    msleep(100);

    WSSendTestClient client;
    ByteBuffer content;
    std::string url = "ws://127.0.0.1:" + std::to_string(kListenPort) + "/chat";
    if (client.connect(url, content) != 0) {
        fprintf(stderr, "ws client connect failed\n");
        ++failed;
    } else {
        for (int i = 0; i < 300 && client.get_state() != NetConnectState_Connected; ++i) {
            msleep(10);
        }
        if (client.get_state() != NetConnectState_Connected) {
            fprintf(stderr, "ws upgrade failed, state=%d\n",
                    static_cast<int>(client.get_state()));
            ++failed;
        } else {
            ByteBuffer message(kTestPayload);
            // 不显式传 is_mask，使用默认值：客户端帧必须带 mask
            ssize_t ret = client.send_data(message,
                                           static_cast<int8_t>(ptl::WEBSOCKET_OPCODE_TEXT_FRAME));
            if (ret <= 0) {
                fprintf(stderr, "ws send frame failed, ret=%ld\n", static_cast<long>(ret));
                ++failed;
            }

            for (int i = 0; i < 300 && client.received() != kTestPayload; ++i) {
                msleep(10);
            }
            if (client.received() != kTestPayload) {
                fprintf(stderr, "ws echo not received, got='%s'\n", client.received().c_str());
                ++failed;
            }
        }
        client.NetClient::disconnect();
    }

    server_thread.join();
    ::close(listen_fd);

    if (server_result.ok == false) {
        fprintf(stderr, "ws server check failed: %s\n", server_result.error.c_str());
        ++failed;
    }

    int ret = reactor_stop();
    printf("ws send test: ret=%d, failed=%d, server_opcode=%d, masked=%d, payload='%s'\n",
           ret, failed, server_result.received_opcode,
           server_result.received_masked ? 1 : 0,
           server_result.received_payload.c_str());

    return failed == 0 ? 0 : 1;
}

