/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_P2P_BASE_TRANSPORTCHANNEL_H_
#define TALK_P2P_BASE_TRANSPORTCHANNEL_H_

#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/socket.h"
#include "talk/base/sslidentity.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/transport.h"
#include "talk/p2p/base/transportdescription.h"

namespace cricket {

class Candidate;

// Stats that we can return about the connections for this transport channel.
struct ConnectionInfo {
  bool best_connection;        // Is this the best connection we have?
  bool writable;               // Has this connection received a STUN response?
  bool readable;               // Has this connection received a STUN request?
  bool timeout;                // Has this connection timed out?
  bool new_connection;         // Is this a newly created connection?
  size_t rtt;                  // The STUN RTT for this connection.
  size_t sent_total_bytes;     // Total bytes sent on this connection.
  size_t sent_bytes_second;    // Bps over the last measurement interval.
  size_t recv_total_bytes;     // Total bytes received on this connection.
  size_t recv_bytes_second;    // Bps over the last measurement interval.
  Candidate local_candidate;   // The local candidate for this connection.
  Candidate remote_candidate;  // The remote candidate for this connection.
  void* key;                   // A static value that identifies this conn.
};
typedef std::vector<ConnectionInfo> ConnectionInfos;

// Flags for SendPacket/SignalReadPacket.
enum PacketFlags {
  PF_NORMAL       = 0x00,  // A normal packet.
  PF_SRTP_BYPASS  = 0x01,  // An encrypted SRTP packet; bypass any additional
                           // crypto provided by the transport (e.g. DTLS)
};

// A TransportChannel represents one logical stream of packets that are sent
// between the two sides of a session.
class TransportChannel : public sigslot::has_slots<> {
 public:
  explicit TransportChannel(const std::string& content_name, int component)
      : content_name_(content_name),
        component_(component),
        readable_(false), writable_(false) {}
  virtual ~TransportChannel() {}

  // Returns the session id of this channel.
  virtual const std::string& SessionId() const { return session_id_; }
  // Sets session id which created this transport channel.
  // This is called from TransportProxy::GetOrCreateImpl.
  virtual void SetSessionId(const std::string& session_id) {
    session_id_ = session_id;
  }
  const std::string& content_name() const { return content_name_; }
  int component() const { return component_; }

  // Returns the readable and states of this channel.  Each time one of these
  // states changes, a signal is raised.  These states are aggregated by the
  // TransportManager.
  bool readable() const { return readable_; }
  bool writable() const { return writable_; }
  sigslot::signal1<TransportChannel*> SignalReadableState;
  sigslot::signal1<TransportChannel*> SignalWritableState;
  // Emitted when the TransportChannel's ability to send has changed.
  sigslot::signal1<TransportChannel*> SignalReadyToSend;

  // Attempts to send the given packet.  The return value is < 0 on failure.
  // TODO: Remove the default argument once channel code is updated.
  virtual int SendPacket(const char* data, size_t len, int flags = 0) = 0;

  // Sets a socket option on this channel.  Note that not all options are
  // supported by all transport types.
  virtual int SetOption(talk_base::Socket::Option opt, int value) = 0;

  // Returns the most recent error that occurred on this channel.
  virtual int GetError() = 0;

  // Returns current transportchannel ICE role.
  virtual TransportRole GetRole() const = 0;

  // Returns the current stats for this connection.
  virtual bool GetStats(ConnectionInfos* infos) {
    return false;
  }

  // Is DTLS active?
  virtual bool IsDtlsActive() const {
    return false;
  }

  // Set up the ciphers to use for DTLS-SRTP.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) {
    return false;
  }

  // Find out which DTLS-SRTP cipher was negotiated
  virtual bool GetSrtpCipher(std::string* cipher) {
    return false;
  }

  // Allows key material to be extracted for external encryption.
  virtual bool ExportKeyingMaterial(const std::string& label,
      const uint8* context,
      size_t context_len,
      bool use_context,
      uint8* result,
      size_t result_len) {
    return false;
  }

  // Signalled each time a packet is received on this channel.
  sigslot::signal4<TransportChannel*, const char*,
                   size_t, int> SignalReadPacket;

  // This signal occurs when there is a change in the way that packets are
  // being routed, i.e. to a different remote location. The candidate
  // indicates where and how we are currently sending media.
  sigslot::signal2<TransportChannel*, const Candidate&> SignalRouteChange;

  // Invoked when the channel is being destroyed.
  sigslot::signal1<TransportChannel*> SignalDestroyed;

  // Debugging description of this transport channel.
  std::string ToString() const;

 protected:
  // Sets the readable state, signaling if necessary.
  void set_readable(bool readable);

  // Sets the writable state, signaling if necessary.
  void set_writable(bool writable);


 private:
  std::string session_id_;
  // Used mostly for debugging.
  std::string content_name_;
  int component_;
  bool readable_;
  bool writable_;

  DISALLOW_EVIL_CONSTRUCTORS(TransportChannel);
};

}  // namespace cricket

#endif  // TALK_P2P_BASE_TRANSPORTCHANNEL_H_
