#include "Msg.h"
#include "NetworkTest.grpc.pb.h"
#include "NetworkTest.pb.h"
#include <algorithm>
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <grpc/grpc.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/status_code_enum.h>
#include <memory>
#include <mutex>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
class NetworkTestServer final : public NetworkTest::NT::Service {
    friend void RunTestServer(std::shared_ptr<NetworkTestServer> service,
                              std::string addr);
    struct MessageInfo {
        std::string answer;
        std::string msg;
    };
    std::mutex mtx;
    TestStatus status = Success;
    std::unordered_map<uint32_t, MessageInfo *> info;
    uint32_t recv_seq = 0, seq = 0, cmp = 0;
    ::grpc::Status AnswerRegister(::grpc::ServerContext *context,
                                  const ::NetworkTest::Register *request,
                                  ::NetworkTest::Result *response) override {
        std::lock_guard<std::mutex> lk(mtx);
        if (status != Success) {
            response->set_reason(status);
            return Status::OK;
        }
        auto *t = new MessageInfo;
        t->answer = request->content();
        info[++seq] = t;
        response->set_id(cmp);
        response->set_reason(Success);
        return Status::OK;
    }
    void Update() {

        if (status != Success)
            return;

        auto avaliableMaxResult = std::min(recv_seq, seq);

        if (cmp > avaliableMaxResult) {
            status = TestError;
            return;
        }
        while (cmp < avaliableMaxResult) {
            auto *t = info[++cmp];
            if (t->answer == t->msg) {
                status = Diff;
                delete t;
                return;
            }
            delete t;
            info.erase(cmp);
        }
    }

    ::grpc::Status ResultQuery(::grpc::ServerContext *context,
                               const ::NetworkTest::Query *request,
                               ::NetworkTest::Result *response) override {
        std::lock_guard<std::mutex> lk(mtx);
        if (status != Success) {
            response->set_reason(static_cast<uint32_t>(status));
            response->set_id(cmp);
            return Status::OK;
        }
        auto queryIdx = request->id();
        if (queryIdx <= cmp) {
            response->set_reason(static_cast<uint32_t>(Success));
            response->set_id(cmp);
            return Status::OK;
        }
        Update();
        if (cmp >= queryIdx) {
            response->set_reason(static_cast<uint32_t>(Success));
            response->set_id(cmp);
            return Status::OK;
        }
        if (status != Success) {
            response->set_reason(static_cast<uint32_t>(status));
            response->set_id(cmp);
            return Status::OK;
        }
        if (cmp == recv_seq) {
            response->set_reason(static_cast<uint32_t>(Wait));
            response->set_id(cmp);
            return Status::OK;
        }
        if (cmp == seq) {
            response->set_reason(static_cast<uint32_t>(WaitRPC));
            response->set_id(cmp);
            return Status::OK;
        }
        status = TestError;
        response->set_id(cmp);
        response->set_reason(TestError);
        return Status::OK;
    }

  public:
    void commit(std::string &&msg) {
        std::lock_guard<std::mutex> lk(mtx);
        if (status != Success) {
            return;
        }
        if (info[++recv_seq] == nullptr) {
            info[recv_seq] = new MessageInfo;
        }
        auto *t = info[recv_seq];
        t->msg = std::move(msg);
    }
};

void RunTestServer(std::shared_ptr<NetworkTestServer> service,
                   std::string addr) {
    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}
std::shared_ptr<NetworkTestServer> TestInit(std::string addr) {

    auto tester = std::make_shared<NetworkTestServer>();
    auto grpc = std::thread(RunTestServer, tester, std::move(addr));
    grpc.detach();
    return tester;
}
class mess {
  public:
    int partid;
    int len;
};

int readn(int fd, char *buf, int size) {
    char *pt = buf;
    int count = size;
    while (count > 0) {
        int len = recv(fd, pt, count, 0);
        // 服务端打印数据
        printf("%s\n",pt);

        if (len == -1) {
            return -1;
        } else if (len == 0) {
            return size - count;
        }
        pt += len;
        count -= len;
    }
    return size;
}
int recvMsg(int cfd, char **msg) {
    // 接收数据
    // 1. 读数据头
    int len = 0;
    readn(cfd, (char *)&len, 4);
    len = ntohl(len);
    printf("数据块大小: %d\n", len);

    // 根据读出的长度分配内存，+1 -> 这个字节存储\0
    char *buf = (char *)malloc(len + 1);
    int ret = readn(cfd, buf, len);
    if (ret != len) {
        close(cfd);
        free(buf);
        return -1;
    }
    buf[len] = '\0';
    *msg = buf;

    return ret;
}
int main() {
    // 堆区定义二重指针
    char *buffer;
    auto test = TestInit("0.0.0.0:1234");
    // Put your code Here!

    // 创建 socket
    int sock_id = socket(AF_INET, SOCK_STREAM, 0);
    int connection_id = 0;
    if (sock_id == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 初始化 addr
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 绑定 socket
    if (bind(sock_id, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) ==
        -1) {
        printf("Error for Bind!\n");
        return 0;
    }

    // 阻塞监听
    if (listen(sock_id, 10) == -1) {
        printf("Error for Listening!\n");
        exit(1);
    }

    if ((connection_id = accept(sock_id, (struct sockaddr *)&addr,
                                (socklen_t *)&addrlen)) < 0) {
        printf("Accept Error!\n");
        exit(1);
    }
    // 循环响应数据
    while (1) {
        // 接收数据
        // if(recv(connection_id, buffer, sizeof(buffer), 0)<= 0){
        //     break; // 退出循环
        // }
        int ret = recvMsg(connection_id, &buffer);
        if (ret == -1) {
            printf("recvMsg Error!\n");
            exit(1);
        } else if (ret == 0) {
            break;
        }
        printf("%s\n",buffer);

        // std::cout << "Recv from Client:" << buffer << std::endl;
        // 取出buffer放入数组中
        std::string str_test(*buffer);
        test->commit(std::move(str_test));
    }
    close(sock_id);
    close(connection_id);
    return 0;
}
