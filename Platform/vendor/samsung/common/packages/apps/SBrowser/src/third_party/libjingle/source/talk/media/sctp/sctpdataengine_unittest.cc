/*
 * libjingle SCTP
 * Copyright 2013 Google Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

#include "talk/base/buffer.h"
#include "talk/base/criticalsection.h"
#include "talk/base/gunit.h"
#include "talk/base/helpers.h"
#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/thread.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/sctp/sctpdataengine.h"

enum {
  MSG_PACKET = 1,
};

// Fake NetworkInterface that sends/receives sctp packets.  The one in
// talk/media/base/fakenetworkinterface.h only works with rtp/rtcp.
class SctpFakeNetworkInterface : public cricket::MediaChannel::NetworkInterface,
                                 public talk_base::MessageHandler {
 public:
  explicit SctpFakeNetworkInterface(talk_base::Thread* thread)
    : thread_(thread),
      dest_(NULL) {
  }

  void SetDestination(cricket::DataMediaChannel* dest) { dest_ = dest; }

 protected:
  // Called to send raw packet down the wire (e.g. SCTP an packet).
  virtual bool SendPacket(talk_base::Buffer* packet) {
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::SendPacket";

    // TODO(ldixon): Can/should we use Buffer.TransferTo here?
    // Note: this assignment does a deep copy of data from packet.
    talk_base::Buffer* buffer = new talk_base::Buffer(packet->data(),
                                                      packet->length());
    thread_->Post(this, MSG_PACKET, talk_base::WrapMessageData(buffer));
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::SendPacket, Posted message.";
    return true;
  }

  // Called when a raw packet has been recieved. This passes the data to the
  // code that will interpret the packet. e.g. to get the content payload from
  // an SCTP packet.
  virtual void OnMessage(talk_base::Message* msg) {
    LOG(LS_VERBOSE) << "SctpFakeNetworkInterface::OnMessage";
    talk_base::Buffer* buffer =
        static_cast<talk_base::TypedMessageData<talk_base::Buffer*>*>(
            msg->pdata)->data();
    if (dest_) {
      dest_->OnPacketReceived(buffer);
    }
    delete buffer;
  }

  // Unsupported functions required to exist by NetworkInterface.
  // TODO(ldixon): Refactor parent NetworkInterface class so these are not
  // required. They are RTC specific and should be in an appropriate subclass.
  virtual bool SendRtcp(talk_base::Buffer* packet) {
    LOG(LS_WARNING) << "Unsupported: SctpFakeNetworkInterface::SendRtcp.";
    return false;
  }
  virtual int SetOption(SocketType type, talk_base::Socket::Option opt,
                        int option) {
    LOG(LS_WARNING) << "Unsupported: SctpFakeNetworkInterface::SetOption.";
    return 0;
  }

 private:
  // Not owned by this class.
  talk_base::Thread* thread_;
  cricket::DataMediaChannel* dest_;
};

// This is essentially a buffer to hold recieved data. It stores only the last
// received data. Calling OnDataReceived twice overwrites old data with the
// newer one.
// TODO(ldixon): Implement constraints, and allow new data to be added to old
// instead of replacing it.
class SctpFakeDataReceiver : public sigslot::has_slots<> {
 public:
  SctpFakeDataReceiver() : received_(false) {}

  void Clear() {
    received_ = false;
    last_data_ = "";
    last_params_ = cricket::ReceiveDataParams();
  }

  virtual void OnDataReceived(const cricket::ReceiveDataParams& params,
                              const char* data, size_t length) {
    received_ = true;
    last_data_ = std::string(data, length);
    last_params_ = params;
  }

  bool received() const { return received_; }
  std::string last_data() const { return last_data_; }
  cricket::ReceiveDataParams last_params() const { return last_params_; }

 private:
  bool received_;
  std::string last_data_;
  cricket::ReceiveDataParams last_params_;
};

// SCTP Data Engine testing framework.
class SctpDataMediaChannelTest : public testing::Test {
 protected:
  virtual void SetUp() {
    engine_.reset(new cricket::SctpDataEngine());
  }

  cricket::SctpDataMediaChannel* CreateChannel(
        SctpFakeNetworkInterface* net, SctpFakeDataReceiver* recv) {
    cricket::SctpDataMediaChannel* channel =
        static_cast<cricket::SctpDataMediaChannel*>(engine_->CreateChannel(""));
    channel->SetInterface(net);
    // When data is received, pass it to the SctpFakeDataReceiver.
    channel->SignalDataReceived.connect(
        recv, &SctpFakeDataReceiver::OnDataReceived);
    return channel;
  }

  bool SendData(cricket::SctpDataMediaChannel* chan, uint32 ssrc,
                const std::string& msg,
                cricket::SendDataResult* result) {
    cricket::SendDataParams params;
    params.ssrc = ssrc;
    return chan->SendData(params, talk_base::Buffer(msg.data(), msg.length()), result);
  }

  bool ReceivedData(const SctpFakeDataReceiver* recv, uint32 ssrc,
                    const std::string& msg ) {
    return (recv->received() &&
            recv->last_params().ssrc == ssrc &&
            recv->last_data() == msg);
  }

  bool ProcessMessagesUntilIdle() {
    talk_base::Thread* thread = talk_base::Thread::Current();
    while (!thread->empty()) {
      talk_base::Message msg;
      if (thread->Get(&msg, talk_base::kForever)) {
        thread->Dispatch(&msg);
      }
    }
    return !thread->IsQuitting();
  }

 private:
  talk_base::scoped_ptr<cricket::SctpDataEngine> engine_;
};

#ifdef WIN32
TEST_F(SctpDataMediaChannelTest, DISABLED_SendData) {
#else
TEST_F(SctpDataMediaChannelTest, SendData) {
#endif
  talk_base::scoped_ptr<SctpFakeNetworkInterface> net1(
      new SctpFakeNetworkInterface(talk_base::Thread::Current()));
  talk_base::scoped_ptr<SctpFakeNetworkInterface> net2(
      new SctpFakeNetworkInterface(talk_base::Thread::Current()));
  talk_base::scoped_ptr<SctpFakeDataReceiver> recv1(
      new SctpFakeDataReceiver());
  talk_base::scoped_ptr<SctpFakeDataReceiver> recv2(
      new SctpFakeDataReceiver());
  talk_base::scoped_ptr<cricket::SctpDataMediaChannel> chan1(
      CreateChannel(net1.get(), recv1.get()));
  chan1->set_debug_name("chan1/connector");
  talk_base::scoped_ptr<cricket::SctpDataMediaChannel> chan2(
      CreateChannel(net2.get(), recv2.get()));
  chan2->set_debug_name("chan2/listener");

  net1->SetDestination(chan2.get());
  net2->SetDestination(chan1.get());

  LOG(LS_VERBOSE) << "Channel setup ----------------------------- ";
  chan1->AddSendStream(cricket::StreamParams::CreateLegacy(1));
  chan2->AddRecvStream(cricket::StreamParams::CreateLegacy(1));

  chan2->AddSendStream(cricket::StreamParams::CreateLegacy(2));
  chan1->AddRecvStream(cricket::StreamParams::CreateLegacy(2));

  LOG(LS_VERBOSE) << "Connect the channels -----------------------------";
  // chan1 wants to setup a data connection.
  chan1->SetReceive(true);
  // chan1 will have sent chan2 a request to setup a data connection. After
  // chan2 accepts the offer, chan2 connects to chan1 with the following.
  chan2->SetReceive(true);
  chan2->SetSend(true);
  // Makes sure that network packets are delivered and simulates a
  // deterministic and realistic small timing delay between the SetSend calls.
  ProcessMessagesUntilIdle();

  // chan1 and chan2 are now connected so chan1 enables sending to complete
  // the creation of the connection.
  chan1->SetSend(true);

  cricket::SendDataResult result;
  LOG(LS_VERBOSE) << "chan1 sending: 'hello?' -----------------------------";
  ASSERT_TRUE(SendData(chan1.get(), 1, "hello?", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(recv2.get(), 1, "hello?"), 1000);
  LOG(LS_VERBOSE) << "recv2.received=" << recv2->received()
                  << "recv2.last_params.ssrc=" << recv2->last_params().ssrc
                  << "recv2.last_params.timestamp="
                  << recv2->last_params().ssrc
                  << "recv2.last_params.seq_num="
                  << recv2->last_params().seq_num
                  << "recv2.last_data=" << recv2->last_data();

  LOG(LS_VERBOSE) << "chan2 sending: 'hi chan1' -----------------------------";
  ASSERT_TRUE(SendData(chan2.get(), 2, "hi chan1", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(ReceivedData(recv1.get(), 2, "hi chan1"), 1000);
  LOG(LS_VERBOSE) << "recv1.received=" << recv1->received()
                  << "recv1.last_params.ssrc=" << recv1->last_params().ssrc
                  << "recv1.last_params.timestamp="
                  << recv1->last_params().ssrc
                  << "recv1.last_params.seq_num="
                  << recv1->last_params().seq_num
                  << "recv1.last_data=" << recv1->last_data();

  LOG(LS_VERBOSE) << "Closing down. -----------------------------";
  // Disconnects and closes socket, including setting receiving to false.
  chan1->SetSend(false);
  chan2->SetSend(false);
  LOG(LS_VERBOSE) << "Cleaning up. -----------------------------";
}
