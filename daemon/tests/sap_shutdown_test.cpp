#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE sap_shutdown_test
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <future>
#include <thread>

#include "../sap.hpp"

BOOST_AUTO_TEST_CASE(terminate_unblocks_pending_receive) {
  // Recreate the socket repeatedly in one process: this catches a receive
  // worker or descriptor left behind by a previous shutdown.
  for (int iteration = 0; iteration < 100; ++iteration) {
    SAP sap("224.2.127.254");
    std::promise<bool> receive_finished;
    auto finished = receive_finished.get_future();

    std::thread receiver([&] {
      bool is_announce = false;
      uint16_t msg_id_hash = 0;
      uint32_t address = 0;
      std::string sdp;
      receive_finished.set_value(
          !sap.receive(is_announce, msg_id_hash, address, sdp, 60));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sap.terminate();

    BOOST_REQUIRE(finished.wait_for(std::chrono::seconds(1)) ==
                  std::future_status::ready);
    BOOST_CHECK(finished.get());
    receiver.join();
  }
}
