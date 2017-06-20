#include <stdio.h>
#include <unistd.h>
#include <atomic>
#include <sys/time.h>
#include <stdint.h>

#include "include/server_thread.h"
#include "include/pink_conn.h"
#include "include/pb_conn.h"
#include "include/pink_thread.h"
#include "message.pb.h"

using namespace pink;
using namespace std;

uint64_t NowMicros() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

static atomic<int> num(0);

class PingConn : public PbConn {
 public:
  PingConn(int fd, const std::string& ip_port, ServerThread* thread,
      void* worker_specific_data) : 
    PbConn(fd, ip_port, thread) {
  }
  ~PingConn() {}

  int DealMessage() {
    num++;
    request_.ParseFromArray(rbuf_ + cur_pos_ - header_len_, header_len_);

    response_.Clear();
    response_.set_pong("hello " + request_.ping());
    res_ = &response_;

    set_is_reply(true);
  }

 private:

  Ping request_;
  Pong response_;

  PingConn(PingConn&);
  PingConn& operator=(PingConn&);
};


class PingConnFactory : public ConnFactory {
 public:
  virtual PinkConn *NewPinkConn(int connfd, const std::string &ip_port,
      ServerThread *thread, void* worker_specific_data) const {
    return new PingConn(connfd, ip_port, thread, worker_specific_data);
  }
};


int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf ("Usage: ./server ip port\n");
    exit(0);
  }

  std::string ip(argv[1]);
  int port = atoi(argv[2]);

  ConnFactory *my_conn_factory = new PingConnFactory();

  int my_port = (argc > 1) ? atoi(argv[1]) : 8221;

  ServerThread *st_thread = NewStandardServer(ip, port, 24, my_conn_factory, 1000);
  st_thread->StartThread();
  uint64_t st, ed;

  while (1) {
    st = NowMicros();
    int prv = num.load();
    sleep(1);
    printf("num %d\n", num.load());
    ed = NowMicros();
    printf("mmap cost time microsecond(us) %lld\n", ed - st);
    printf("average qps %lf\n", (double)(num.load() - prv) / ((double)(ed - st) / 1000000));
  }
  st_thread->JoinThread();

  delete st_thread;

  return 0;
}
